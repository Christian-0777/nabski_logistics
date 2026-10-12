#include "SyncConfiguration.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUrl>

#include <algorithm>
#include <utility>

#include <windows.h>
#include <wincrypt.h>

namespace nlsi::sync {
namespace {

QString ConfigurationPath(const QString& user_data_directory) {
    return QDir(user_data_directory).filePath(QStringLiteral("sync/config.dpapi"));
}

QString DriverKeyConfigurationPath(const QString& user_data_directory) {
    return QDir(user_data_directory).filePath(QStringLiteral("sync/driver-key.dpapi"));
}

bool Protect(const QByteArray& input, QByteArray* output, QString* error) {
    DATA_BLOB source{
        static_cast<DWORD>(input.size()),
        reinterpret_cast<BYTE*>(const_cast<char*>(input.constData())),
    };
    DATA_BLOB protected_data{};
    if (!CryptProtectData(
            &source,
            L"NLSI Exclusive Logbook protected settings",
            nullptr,
            nullptr,
            nullptr,
            CRYPTPROTECT_UI_FORBIDDEN,
            &protected_data)) {
        if (error) {
            *error = QStringLiteral("Windows could not protect the saved credential.");
        }
        return false;
    }
    *output = QByteArray(
        reinterpret_cast<const char*>(protected_data.pbData),
        static_cast<qsizetype>(protected_data.cbData));
    LocalFree(protected_data.pbData);
    return true;
}

bool Unprotect(const QByteArray& input, QByteArray* output, QString* error) {
    DATA_BLOB source{
        static_cast<DWORD>(input.size()),
        reinterpret_cast<BYTE*>(const_cast<char*>(input.constData())),
    };
    DATA_BLOB clear_data{};
    if (!CryptUnprotectData(
            &source,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            CRYPTPROTECT_UI_FORBIDDEN,
            &clear_data)) {
        if (error) {
            *error = QStringLiteral(
                "Windows could not unlock the saved settings for this user.");
        }
        return false;
    }
    *output = QByteArray(
        reinterpret_cast<const char*>(clear_data.pbData),
        static_cast<qsizetype>(clear_data.cbData));
    SecureZeroMemory(clear_data.pbData, clear_data.cbData);
    LocalFree(clear_data.pbData);
    return true;
}

} // namespace

bool SyncConfigurationStore::Load(
    const QString& user_data_directory,
    SyncConfiguration* configuration,
    QString* error) {
    if (!configuration) {
        if (error) {
            *error = QStringLiteral("Sync configuration output is unavailable.");
        }
        return false;
    }
    const QString path = ConfigurationPath(user_data_directory);
    QFile file(path);
    if (!file.exists()) {
        *configuration = {};
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Could not read protected sync settings: %1")
                .arg(file.errorString());
        }
        return false;
    }

    QByteArray clear_data;
    if (!Unprotect(file.readAll(), &clear_data, error)) {
        return false;
    }
    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(clear_data, &parse_error);
    SecureZeroMemory(clear_data.data(), static_cast<SIZE_T>(clear_data.size()));
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) {
            *error = QStringLiteral("Protected sync settings are invalid.");
        }
        return false;
    }
    const QJsonObject object = document.object();
    SyncConfiguration loaded{
        object.value(QStringLiteral("api_base_url")).toString(),
        object.value(QStringLiteral("device_api_key")).toString(),
        object.value(QStringLiteral("last_successful_sync")).toString(),
    };
    if (!IsValidApiBaseUrl(loaded.api_base_url)
        || !IsValidDeviceApiKey(loaded.device_api_key)) {
        if (error) {
            *error = QStringLiteral("Protected sync settings contain invalid values.");
        }
        return false;
    }
    *configuration = std::move(loaded);
    return true;
}

bool SyncConfigurationStore::Save(
    const QString& user_data_directory,
    const SyncConfiguration& configuration,
    QString* error) {
    if (!IsValidApiBaseUrl(configuration.api_base_url)
        || !IsValidDeviceApiKey(configuration.device_api_key)) {
        if (error) {
            *error = QStringLiteral(
                "Enter an HTTPS or loopback HTTP API base URL and a valid NLSI driver or legacy device key.");
        }
        return false;
    }

    QByteArray clear_data = QJsonDocument(QJsonObject{
        {QStringLiteral("api_base_url"), configuration.api_base_url.trimmed()},
        {QStringLiteral("device_api_key"), configuration.device_api_key},
        {QStringLiteral("last_successful_sync"), configuration.last_successful_sync},
    }).toJson(QJsonDocument::Compact);
    QByteArray protected_data;
    const bool protected_ok = Protect(clear_data, &protected_data, error);
    SecureZeroMemory(clear_data.data(), static_cast<SIZE_T>(clear_data.size()));
    if (!protected_ok) {
        return false;
    }

    const QString path = ConfigurationPath(user_data_directory);
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (error) {
            *error = QStringLiteral("Could not create the per-user sync settings directory.");
        }
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(protected_data) != protected_data.size()
        || !file.commit()) {
        if (error) {
            *error = QStringLiteral("Could not save protected sync settings: %1")
                .arg(file.errorString());
        }
        return false;
    }
    return true;
}

bool SyncConfigurationStore::IsValidApiBaseUrl(const QString& api_base_url) {
    const QUrl url(api_base_url.trimmed(), QUrl::StrictMode);
    const QString scheme = url.scheme().toLower();
    const QString host = url.host().toLower();
    const bool secure = scheme == QStringLiteral("https");
    const bool local_http = scheme == QStringLiteral("http")
        && (host == QStringLiteral("localhost")
            || host == QStringLiteral("127.0.0.1")
            || host == QStringLiteral("::1"));
    return url.isValid()
        && (secure || local_http)
        && !host.isEmpty()
        && url.userInfo().isEmpty()
        && url.query().isEmpty()
        && url.fragment().isEmpty();
}

bool SyncConfigurationStore::IsValidDeviceApiKey(const QString& device_api_key) {
    const bool driver_key = device_api_key.size() == 12
        && std::all_of(device_api_key.cbegin(), device_api_key.cend(), [](QChar character) {
               return (character >= QLatin1Char('0') && character <= QLatin1Char('9'))
                   || (character >= QLatin1Char('a') && character <= QLatin1Char('z'))
                   || (character >= QLatin1Char('A') && character <= QLatin1Char('Z'));
           });
    const bool legacy_device_key = device_api_key.size() == 64
        && std::all_of(device_api_key.cbegin(), device_api_key.cend(), [](QChar character) {
               return (character >= QLatin1Char('0') && character <= QLatin1Char('9'))
                   || (character >= QLatin1Char('a') && character <= QLatin1Char('f'))
                   || (character >= QLatin1Char('A') && character <= QLatin1Char('F'));
           });
    return driver_key || legacy_device_key;
}

bool SyncConfigurationStore::LoadDriverKey(
    const QString& user_data_directory,
    DriverKeyConfiguration* configuration,
    QString* error) {
    if (!configuration) {
        if (error) {
            *error = QStringLiteral("Driver key configuration output is unavailable.");
        }
        return false;
    }

    QFile file(DriverKeyConfigurationPath(user_data_directory));
    if (!file.exists()) {
        *configuration = {};
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Could not read protected driver key settings: %1")
                .arg(file.errorString());
        }
        return false;
    }

    QByteArray clear_data;
    if (!Unprotect(file.readAll(), &clear_data, error)) {
        return false;
    }
    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(clear_data, &parse_error);
    SecureZeroMemory(clear_data.data(), static_cast<SIZE_T>(clear_data.size()));
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) {
            *error = QStringLiteral("Protected driver key settings are invalid.");
        }
        return false;
    }
    const QJsonObject object = document.object();
    DriverKeyConfiguration loaded{
        object.value(QStringLiteral("website_api_base_url")).toString(),
        object.value(QStringLiteral("driver_key")).toString(),
    };
    if (!IsValidApiBaseUrl(loaded.website_api_base_url)
        || !IsValidDriverKey(loaded.driver_key)) {
        if (error) {
            *error = QStringLiteral("Protected driver key settings contain invalid values.");
        }
        return false;
    }
    *configuration = std::move(loaded);
    return true;
}

bool SyncConfigurationStore::SaveDriverKey(
    const QString& user_data_directory,
    const DriverKeyConfiguration& configuration,
    QString* error) {
    if (!IsValidApiBaseUrl(configuration.website_api_base_url)
        || !IsValidDriverKey(configuration.driver_key)) {
        if (error) {
            *error = QStringLiteral(
                "Enter an HTTPS or loopback HTTP website API URL and a 12-character alphanumeric driver key.");
        }
        return false;
    }

    QByteArray clear_data = QJsonDocument(QJsonObject{
        {QStringLiteral("website_api_base_url"), configuration.website_api_base_url.trimmed()},
        {QStringLiteral("driver_key"), configuration.driver_key},
    }).toJson(QJsonDocument::Compact);
    QByteArray protected_data;
    const bool protected_ok = Protect(clear_data, &protected_data, error);
    SecureZeroMemory(clear_data.data(), static_cast<SIZE_T>(clear_data.size()));
    if (!protected_ok) {
        return false;
    }

    const QString path = DriverKeyConfigurationPath(user_data_directory);
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (error) {
            *error = QStringLiteral("Could not create the per-user driver key settings directory.");
        }
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(protected_data) != protected_data.size()
        || !file.commit()) {
        if (error) {
            *error = QStringLiteral("Could not save protected driver key settings: %1")
                .arg(file.errorString());
        }
        return false;
    }
    return true;
}

bool SyncConfigurationStore::ClearDriverKey(
    const QString& user_data_directory,
    QString* error) {
    const QString path = DriverKeyConfigurationPath(user_data_directory);
    if (!QFile::exists(path)) {
        return true;
    }
    if (QFile::remove(path)) {
        return true;
    }
    if (error) {
        *error = QStringLiteral("Could not remove the protected driver key settings file.");
    }
    return false;
}

bool SyncConfigurationStore::IsValidDriverKey(const QString& driver_key) {
    return driver_key.size() == 12
        && std::all_of(driver_key.cbegin(), driver_key.cend(), [](QChar character) {
               return (character >= QLatin1Char('0') && character <= QLatin1Char('9'))
                   || (character >= QLatin1Char('a') && character <= QLatin1Char('z'))
                   || (character >= QLatin1Char('A') && character <= QLatin1Char('Z'));
           });
}

} // namespace nlsi::sync
