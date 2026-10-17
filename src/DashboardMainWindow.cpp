#include "DashboardMainWindow.h"
#include "DisplayFormatting.h"
#include "DashboardTypography.h"
#include "JobReport.h"
#include "MeasurementFormatting.h"
#include "JobSyncProtocol.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QCoreApplication>
#include <QApplication>
#include <QDate>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QComboBox>
#include <QClipboard>
#include <QFileInfo>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QFrame>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSizePolicy>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QThreadPool>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QUrl>
#include <QUrlQuery>
#include <QtConcurrent/QtConcurrentRun>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <TlHelp32.h>
#endif

namespace nlsi {
namespace {

const QString unavailable = QString::fromUtf8("—");
const QString profileUnavailable = "Not connected";

bool isSupportedDriverRank(const QString& rank)
{
    return rank == "Trainee" || rank == "Driver" || rank == "Elder" ||
        rank == "Veteran" || rank == "Expert" || rank == "Legend" ||
        rank == "Master" || rank == "Ultimate" || rank == "Mythic";
}

double rankLowerThreshold(const QString& rank)
{
    if (rank == "Driver") return 5000.0;
    if (rank == "Elder") return 15000.0;
    if (rank == "Veteran") return 25000.0;
    if (rank == "Expert") return 50000.0;
    if (rank == "Legend") return 100000.0;
    if (rank == "Master") return 250000.0;
    if (rank == "Ultimate") return 500000.0;
    if (rank == "Mythic") return 1000000.0;
    return 0.0;
}

double nextRankThreshold(const QString& rank)
{
    if (rank == "Trainee") return 5000.0;
    if (rank == "Driver") return 15000.0;
    if (rank == "Elder") return 25000.0;
    if (rank == "Veteran") return 50000.0;
    if (rank == "Expert") return 100000.0;
    if (rank == "Legend") return 250000.0;
    if (rank == "Master") return 500000.0;
    if (rank == "Ultimate") return 1000000.0;
    return 0.0;
}

QString maskedEmail(const QString& email)
{
    const int separator = email.indexOf('@');
    if (separator <= 0)
    {
        return email;
    }
    const QString localPart = email.left(separator);
    const QString visiblePrefix = localPart.left(2);
    const int maskLength = (std::max)(3,
        static_cast<int>(localPart.size()) - static_cast<int>(visiblePrefix.size()));
    return visiblePrefix + QString(maskLength, '*') + email.mid(separator);
}

QString canonicalValueText(const QString& text)
{
    if (text.isEmpty())
    {
        return unavailable;
    }
    const QString trimmed = text.trimmed();
    return trimmed.isEmpty() ? unavailable : trimmed;
}

QUrl apiEndpointUrl(const QString& baseUrl, const QString& endpoint)
{
    QString normalized;
    if (!normalizeApiBaseUrl(baseUrl, normalized))
    {
        return {};
    }
    QUrl url(normalized);
    QString path = url.path();
    if (!path.endsWith('/'))
    {
        path += '/';
    }
    url.setPath(path + endpoint);
    return url;
}

void addNgrokWarningBypassHeader(QNetworkRequest& request)
{
    const QString host = request.url().host().toLower();
    if (host == "ngrok-free.dev" || host.endsWith(".ngrok-free.dev") ||
        host == "ngrok-free.app" || host.endsWith(".ngrok-free.app") ||
        host.endsWith(".ngrok.io"))
    {
        request.setRawHeader("ngrok-skip-browser-warning", "1");
    }
}

bool hasJsonContentType(const QNetworkReply* reply)
{
    const QString contentType = reply->header(QNetworkRequest::ContentTypeHeader)
        .toString().section(';', 0, 0).trimmed().toLower();
    return contentType == "application/json" || contentType.endsWith("+json");
}

QString safeApiUrl(QUrl url)
{
    url.setUserInfo(QString());
    QUrlQuery query(url);
    if (!query.isEmpty())
    {
        QList<QPair<QString, QString>> safeItems;
        for (const auto& item : query.queryItems(QUrl::FullyDecoded))
        {
            const QString key = item.first.toLower();
            safeItems.append({item.first,
                key == "limit" || key == "offset" ? item.second : "[REDACTED]"});
        }
        query.setQueryItems(safeItems);
        url.setQuery(query);
    }
    return url.toString(QUrl::FullyEncoded);
}

bool sensitiveApiJsonKey(const QString& key)
{
    const QString normalized = key.toLower();
    return normalized.contains("key") || normalized.contains("token") ||
        normalized.contains("secret") || normalized.contains("password") ||
        normalized.contains("authorization") || normalized.contains("cookie") ||
        normalized == "username" || normalized == "email" ||
        normalized == "display_name" || normalized.endsWith("_id") ||
        normalized == "records" || normalized == "jobs";
}

QJsonValue redactApiJson(const QJsonValue& value, const QString& key = QString())
{
    if (!key.isEmpty() && sensitiveApiJsonKey(key))
    {
        if ((key == "records" || key == "jobs") && value.isArray())
        {
            return QString("[REDACTED: %1 records]").arg(value.toArray().size());
        }
        return QString("[REDACTED]");
    }
    if (value.isObject())
    {
        QJsonObject redacted;
        const QJsonObject object = value.toObject();
        for (auto item = object.constBegin(); item != object.constEnd(); ++item)
        {
            redacted.insert(item.key(), redactApiJson(item.value(), item.key()));
        }
        return redacted;
    }
    if (value.isArray())
    {
        QJsonArray redacted;
        for (const QJsonValue& item : value.toArray())
        {
            redacted.append(redactApiJson(item));
        }
        return redacted;
    }
    return value;
}

QString apiResponseShape(const QByteArray& responseBytes)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(responseBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        return responseBytes.trimmed().isEmpty() ? "empty body" : "non-JSON body";
    }
    if (document.isObject())
    {
        QStringList keys = document.object().keys();
        keys.sort();
        return "JSON object keys=[" + keys.join(",") + "]";
    }
    if (document.isArray())
    {
        return QString("JSON array (%1 entries)").arg(document.array().size());
    }
    return "JSON scalar";
}

QString safeApiBodyExcerpt(const QByteArray& responseBytes, const QString& apiKey)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(responseBytes, &parseError);
    if (parseError.error == QJsonParseError::NoError && document.isObject())
    {
        return QString::fromUtf8(QJsonDocument(
            redactApiJson(document.object()).toObject()).toJson(QJsonDocument::Compact)
            .left(512));
    }
    if (parseError.error == QJsonParseError::NoError && document.isArray())
    {
        return QString::fromUtf8(QJsonDocument(
            redactApiJson(document.array()).toArray()).toJson(QJsonDocument::Compact)
            .left(512));
    }

    QString excerpt = QString::fromUtf8(responseBytes.left(2048));
    if (!apiKey.isEmpty())
    {
        excerpt.replace(apiKey, "[REDACTED]");
    }
    excerpt.replace(QRegularExpression(
        "(?i)(bearer\\s+)[A-Za-z0-9._~+/-]+=*"), "\\1[REDACTED]");
    excerpt.replace(QRegularExpression(
        "(?i)((?:password|passwd|secret|token|api[_-]?key)\\s*[=:]\\s*)[^\\s<>\"']+"),
        "\\1[REDACTED]");
    excerpt.replace(QRegularExpression("\\s+"), " ");
    return excerpt.left(512);
}

void logApiResponseFailure(QNetworkReply* reply, const QByteArray& responseBytes,
                           const QString& configuredBaseUrl, const QString& apiKey,
                           const QString& expectedSchema)
{
    if (!reply)
    {
        return;
    }
    const QUrl configuredUrl = reply->request().url();
    const QUrl responseUrl = reply->url();
    const QString location = QString::fromUtf8(reply->rawHeader("Location"));
    const QString contentType = reply->header(QNetworkRequest::ContentTypeHeader)
        .toString().trimmed();
    const QString method = reply->operation() == QNetworkAccessManager::PostOperation
        ? "POST"
        : reply->operation() == QNetworkAccessManager::GetOperation ? "GET" : "OTHER";
    const QString redirect = location.isEmpty()
        ? "[none]"
        : safeApiUrl(responseUrl.resolved(QUrl(location)));
    const QStringList details = {
        "configured_base=" + safeApiUrl(QUrl(configuredBaseUrl)),
        "request_url=" + safeApiUrl(configuredUrl),
        "response_url=" + safeApiUrl(responseUrl),
        "method=" + method,
        "status=" + QString::number(reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt()),
        "redirect_location=" + redirect,
        "content_type=" + (contentType.isEmpty() ? "[none]" : contentType),
        "expected=" + expectedSchema,
        "actual=" + apiResponseShape(responseBytes),
        "body_redacted=" + safeApiBodyExcerpt(responseBytes, apiKey),
    };
    qWarning().noquote() << "NLSI API response failure:" << details.join(' ');
}

bool parseDoubleValue(const QString& text, double& value)
{
    if (text.isEmpty())
    {
        return false;
    }
    bool ok = false;
    value = QLocale::c().toDouble(text, &ok);
    if (!ok)
    {
        QString normalized = text;
        normalized.replace(',', '.');
        value = normalized.toDouble(&ok);
    }
    return ok && std::isfinite(value);
}

QLabel* makeValue(QWidget* parent)
{
    auto* label = new QLabel(unavailable, parent);
    label->setObjectName("telemetryValue");
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

QString formattedField(const TelemetrySnapshot& snapshot, const std::string& key)
{
    const auto it = snapshot.values.find(key);
    if (it == snapshot.values.end() || !it->second.available || it->second.value.empty())
    {
        return unavailable;
    }

    QString result = QString::fromStdString(it->second.value);
    if (!it->second.unit.empty())
    {
        result += " " + QString::fromStdString(it->second.unit);
    }
    return result;
}

QString processStateText(GameProcessState state)
{
    switch (state)
    {
    case GameProcessState::NotRunning: return "NOT RUNNING";
    case GameProcessState::Ets2Running: return "ETS2 RUNNING";
    case GameProcessState::AtsRunning: return "ATS RUNNING";
    case GameProcessState::BothRunning: return "BOTH RUNNING — ETS2 / ATS";
    case GameProcessState::DetectionError: return "DETECTION ERROR";
    }
    return "DETECTION ERROR";
}

QString gameDisplayName(const QString& gameId)
{
    if (gameId.compare("eut2", Qt::CaseInsensitive) == 0)
    {
        return "EURO TRUCK SIMULATOR 2";
    }
    if (gameId.compare("ats", Qt::CaseInsensitive) == 0)
    {
        return "AMERICAN TRUCK SIMULATOR";
    }
    return gameId.trimmed().isEmpty() ? unavailable : gameId.toUpper();
}

QString gameCurrencyDenomination(const QString& gameId, const QString& denomination)
{
    if (denomination.trimmed().compare("game currency", Qt::CaseInsensitive) != 0)
    {
        return denomination;
    }
    if (gameId.compare("eut2", Qt::CaseInsensitive) == 0)
    {
        return "EUR";
    }
    if (gameId.compare("ats", Qt::CaseInsensitive) == 0)
    {
        return "USD";
    }
    return denomination;
}

QString jobStatusLabel(JobStatus status)
{
    return status == JobStatus::Unknown
        ? "PENDING"
        : QString::fromStdString(jobStatusToString(status));
}

QString recordValue(const JobRecord& record, const std::string& key)
{
    const auto it = record.snapshot.values.find(key);
    if (it == record.snapshot.values.end() || !it->second.available || it->second.value.empty())
    {
        return unavailable;
    }
    return QString::fromStdString(it->second.value);
}

QString recordText(const JobRecord& record, const std::string& key)
{
    const QString value = recordValue(record, key);
    return uppercaseDisplayText(value);
}

QString recordNumber(const JobRecord& record, const std::string& key,
                     const UserPreferences& preferences, const QString& unit = QString(),
                     std::optional<double> eurUsdRate = std::nullopt)
{
    const auto it = record.snapshot.values.find(key);
    if (it == record.snapshot.values.end() || !it->second.available)
    {
        return unavailable;
    }
    double value = 0.0;
    if (!parseDoubleValue(QString::fromStdString(it->second.value), value))
    {
        return unavailable;
    }
    const QString sourceUnit = QString::fromStdString(it->second.unit);
    if (key == "job.max_speed_kmh" || key == "job.average_speed_kmh")
    {
        return formatSpeed(value, sourceUnit, preferences);
    }
    if (key == "job.refueled" || key == "job.total_fuel_consumed")
    {
        return formatVolume(value, preferences);
    }
    if (key == "job.cancel_penalty")
    {
        return formatCurrencyAmount(
            value,
            gameCurrencyDenomination(QString::fromStdString(record.snapshot.gameId), sourceUnit),
            preferences,
            eurUsdRate);
    }
    const QString suffix = unit.isEmpty() ? sourceUnit : unit;
    return formatFixedNumber(QString::number(value, 'g', 16), suffix, 2);
}

QString recordDistance(const JobRecord& record, const std::string& key,
                       const UserPreferences& preferences)
{
    const auto it = record.snapshot.values.find(key);
    if (it == record.snapshot.values.end() || !it->second.available)
    {
        return unavailable;
    }
    double value = 0.0;
    if (!parseDoubleValue(QString::fromStdString(it->second.value), value))
    {
        return unavailable;
    }
    return formatDistance(value, QString::fromStdString(it->second.unit), preferences);
}

QString recordCurrency(const JobRecord& record, const UserPreferences& preferences,
                       std::optional<double> eurUsdRate)
{
    double value = 0.0;
    const auto it = record.snapshot.values.find("job.income");
    return it != record.snapshot.values.end() && it->second.available &&
            parseDoubleValue(QString::fromStdString(it->second.value), value)
        ? formatCurrencyAmount(
            value,
            gameCurrencyDenomination(QString::fromStdString(record.snapshot.gameId),
                                     QString::fromStdString(it->second.unit)),
            preferences,
            eurUsdRate)
        : unavailable;
}

QString recordDamage(const JobRecord& record)
{
    QStringList parts;
    for (const auto& item : {std::pair<std::string, QString>{"job.cargo_damage", "CARGO"},
                             {"job.vehicle_damage", "VEHICLE"}})
    {
        double value = 0.0;
        const QString raw = recordValue(record, item.first);
        if (raw != unavailable && parseDoubleValue(raw, value))
        {
            parts.push_back(QString("%1 %2%").arg(item.second, formatDisplayNumber(value * 100.0, 2)));
        }
    }
    return parts.isEmpty() ? unavailable : parts.join(" / ");
}

QString recordDuration(const JobRecord& record, const std::string& key, double multiplier = 1.0)
{
    double value = 0.0;
    const QString raw = recordValue(record, key);
    return raw != unavailable && parseDoubleValue(raw, value)
        ? formatDurationHhmmss(value * multiplier)
        : unavailable;
}

QString normalizedLocationValue(const QString& value)
{
    const QString trimmed = value.trimmed();
    return trimmed.isEmpty() || trimmed.compare(unavailable, Qt::CaseInsensitive) == 0
        ? QString()
        : trimmed;
}

QString formatCityCountry(const QString& city, const QString& country)
{
    const QString normalizedCity = normalizedLocationValue(city);
    const QString normalizedCountry = normalizedLocationValue(country);

    if (normalizedCity.isEmpty() && normalizedCountry.isEmpty())
    {
        return unavailable;
    }
    if (normalizedCity.isEmpty())
    {
        return normalizedCountry;
    }
    if (normalizedCountry.isEmpty())
    {
        return normalizedCity;
    }
    if (normalizedCountry.compare(normalizedCity, Qt::CaseInsensitive) == 0)
    {
        return normalizedCity;
    }
    if (normalizedCity.contains(normalizedCountry, Qt::CaseInsensitive) ||
        normalizedCountry.contains(normalizedCity, Qt::CaseInsensitive))
    {
        return normalizedCity;
    }
    return QString("%1, %2").arg(normalizedCity, normalizedCountry);
}

QString recordLocation(const JobRecord& record, const std::string& cityKey, const std::string& countryKey)
{
    const QString city = recordValue(record, cityKey);
    const QString country = recordValue(record, countryKey);
    const QString formatted = formatCityCountry(city, country);
    return formatted == unavailable ? unavailable : uppercaseDisplayText(formatted);
}

QString currentJobCompanyCity(const TelemetrySnapshot& snapshot,
                              const std::string& companyKey,
                              const std::string& cityKey,
                              const std::string& countryKey = std::string())
{
    const auto availableValue = [&](const std::string& key)
    {
        const auto it = snapshot.values.find(key);
        return it == snapshot.values.end() || !it->second.available
            ? unavailable
            : uppercaseDisplayText(QString::fromStdString(it->second.value).trimmed());
    };
    const QString company = availableValue(companyKey);
    const QString city = availableValue(cityKey);
    QString country;
    if (!countryKey.empty())
    {
        country = availableValue(countryKey);
    }
    const QString location = uppercaseDisplayText(formatCityCountry(city, country));
    if (company == unavailable)
    {
        return location;
    }
    if (location == unavailable)
    {
        return company;
    }
    return QString("%1 - %2").arg(company, location);
}

QString recordAverageFuelConsumption(const JobRecord& record, const UserPreferences& preferences)
{
    const QString raw = recordValue(record, "truck.fuel.average_consumption");
    double value = 0.0;
    if (raw == unavailable || !parseDoubleValue(raw, value))
    {
        return unavailable;
    }
    const QString unit = QString::fromStdString(
        record.snapshot.values.at("truck.fuel.average_consumption").unit).toLower();
    if (unit == "l/km")
    {
        value *= 100.0;
    }
    if (preferences.volume == VolumeUnit::UsGallons)
    {
        value = litersToUsGallons(value *
            (preferences.distance == DistanceUnit::Miles ? milesToKilometres(1.0) : 1.0));
    }
    else if (preferences.volume == VolumeUnit::ImperialGallons)
    {
        value = litersToImperialGallons(value *
            (preferences.distance == DistanceUnit::Miles ? milesToKilometres(1.0) : 1.0));
    }
    else if (preferences.distance == DistanceUnit::Miles)
    {
        value *= milesToKilometres(1.0);
    }
    return QString("%1 %2/100 %3")
        .arg(formatDisplayNumber(value, 2),
             volumeUnitLabel(preferences.volume),
             distanceUnitLabel(preferences.distance));
}

QString recordSpecialCargo(const JobRecord& record)
{
    const QString value = recordValue(record, "job.special_cargo");
    if (value == unavailable)
    {
        return unavailable;
    }
    return value.compare("true", Qt::CaseInsensitive) == 0 ? "SUPPORTED" : "NOT SPECIAL CARGO";
}

QJsonObject syncRecordToJson(const JobRecord& record)
{
    QJsonObject values;
    for (const auto& entry : record.snapshot.values)
    {
        const FieldValue& value = entry.second;
        values.insert(QString::fromStdString(entry.first), QJsonObject{
            {"label", QString::fromStdString(value.label)},
            {"value", QString::fromStdString(value.value)},
            {"unit", QString::fromStdString(value.unit)},
            {"available", value.available}
        });
    }
    QString status = "unknown";
    switch (record.status)
    {
    case JobStatus::Active: status = "active"; break;
    case JobStatus::Delivered: status = "delivered"; break;
    case JobStatus::Cancelled: status = "cancelled"; break;
    case JobStatus::Unknown: break;
    }
    return {
        {"local_job_id", QString::fromStdString(record.id)},
        {"status", status},
        {"started_at", record.startedAt.toUTC().toString(Qt::ISODateWithMs)},
        {"completed_at", record.completedAt.isValid()
            ? QJsonValue(record.completedAt.toUTC().toString(Qt::ISODateWithMs))
            : QJsonValue(QJsonValue::Null)},
        {"details", QJsonObject{
            {"format", 1},
            {"game_id", QString::fromStdString(record.snapshot.gameId)},
            {"game", QString::fromStdString(record.snapshot.gameName)},
            {"api_version", QString::fromStdString(record.snapshot.gameApiVersion)},
            {"values", values}
        }}
    };
}

QDateTime syncDateTime(const QJsonValue& value)
{
    if (!value.isString())
    {
        return {};
    }
    const QString text = value.toString();
    QDateTime result = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!result.isValid())
    {
        result = QDateTime::fromString(text, "yyyy-MM-dd hh:mm:ss.zzz");
        if (!result.isValid())
        {
            result = QDateTime::fromString(text, "yyyy-MM-dd hh:mm:ss");
        }
        if (result.isValid())
        {
            result.setTimeSpec(Qt::UTC);
        }
    }
    return result;
}

bool remoteJobFromJson(const QJsonObject& object, JobRecord& record)
{
    if (!object.value("job_id").isString() ||
        !object.value("status").isString() ||
        !object.value("job_details").isObject())
    {
        return false;
    }
    const QString status = object.value("status").toString();
    if (status != "active" && status != "delivered" && status != "cancelled" &&
        status != "unknown")
    {
        return false;
    }
    record.status = jobStatusFromString(status.toStdString());
    record.id = object.value("job_id").toString().toStdString();
    record.localJobId = object.value("local_job_id").toString().toStdString();
    record.serverJobId = record.id;
    record.clientInstanceId = object.value("client_instance_id").toString().toStdString();
    record.syncStatus = "synced";
    record.startedAt = syncDateTime(object.value("started_at_utc"));
    record.completedAt = syncDateTime(object.value("completed_at_utc"));
    if ((record.status == JobStatus::Active && !record.startedAt.isValid()) ||
        ((record.status == JobStatus::Delivered || record.status == JobStatus::Cancelled) &&
         !record.completedAt.isValid()))
    {
        return false;
    }

    const QJsonObject details = object.value("job_details").toObject();
    record.snapshot.gameId = details.value("game_id").toString().toStdString();
    record.snapshot.gameName = details.value("game").toString().toStdString();
    record.snapshot.gameApiVersion = details.value("api_version").toString().toStdString();
    const QJsonObject values = details.value("values").toObject();
    for (auto it = values.constBegin(); it != values.constEnd(); ++it)
    {
        if (!it.value().isObject())
        {
            return false;
        }
        const QJsonObject field = it.value().toObject();
        if (!field.value("label").isString() ||
            !field.value("value").isString() ||
            !field.value("unit").isString() ||
            !field.value("available").isBool())
        {
            return false;
        }
        record.snapshot.values[it.key().toStdString()] = {
            field.value("label").toString().toStdString(),
            field.value("value").toString().toStdString(),
            field.value("unit").toString().toStdString(),
            field.value("available").toBool()
        };
    }
    const auto addFieldIfMissing = [&](const char* key, const char* label, const QString& value)
    {
        if (value.trimmed().isEmpty())
        {
            return;
        }
        const std::string fieldKey(key);
        if (record.snapshot.values.find(fieldKey) == record.snapshot.values.end())
        {
            record.snapshot.values[fieldKey] = {
                label, value.toStdString(), "", true
            };
        }
    };
    addFieldIfMissing("job.source_city", "From", object.value("source_city").toString());
    addFieldIfMissing("job.source_company", "From Company", object.value("source_company").toString());
    addFieldIfMissing("job.destination_city", "To", object.value("destination_city").toString());
    addFieldIfMissing("job.destination_company", "To Company", object.value("destination_company").toString());
    addFieldIfMissing("job.cargo", "Cargo", object.value("cargo_name").toString());
    record.snapshot.values["job.id"] = {
        "Job ID", record.id, "", true
    };
    record.snapshot.values["job.status"] = {
        "Status", jobStatusToString(record.status), "", true
    };
    record.snapshot.status = ConnectionState::Connected;
    return true;
}

} // namespace

GameProcessState detectGameProcesses()
{
#ifdef Q_OS_WIN
    const HANDLE processSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (processSnapshot == INVALID_HANDLE_VALUE)
    {
        return GameProcessState::DetectionError;
    }

    bool ets2Running = false;
    bool atsRunning = false;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!Process32FirstW(processSnapshot, &entry))
    {
        const DWORD error = GetLastError();
        CloseHandle(processSnapshot);
        return error == ERROR_NO_MORE_FILES
            ? GameProcessState::NotRunning
            : GameProcessState::DetectionError;
    }

    do
    {
        if (_wcsicmp(entry.szExeFile, L"eurotrucks2.exe") == 0)
        {
            ets2Running = true;
        }
        else if (_wcsicmp(entry.szExeFile, L"amtrucks.exe") == 0)
        {
            atsRunning = true;
        }
    } while (Process32NextW(processSnapshot, &entry));

    const DWORD enumerationError = GetLastError();
    CloseHandle(processSnapshot);
    if (enumerationError != ERROR_NO_MORE_FILES)
    {
        return GameProcessState::DetectionError;
    }
    return gameProcessStateFromFlags(ets2Running, atsRunning);
#else
    return GameProcessState::DetectionError;
#endif
}

bool snapshotMatchesProcess(const TelemetrySnapshot& snapshot, GameProcessState state)
{
    if (state == GameProcessState::Ets2Running)
    {
        return snapshot.gameId == "eut2";
    }
    if (state == GameProcessState::AtsRunning)
    {
        return snapshot.gameId == "ats";
    }
    return false;
}

} // namespace nlsi

namespace nlsi {

DashboardMainWindow::DashboardMainWindow(QWidget* parent, const QString& jobHistoryDirectory,
                                         const QString& profileFilePath)
    : QMainWindow(parent),
      jobHistory_(jobHistoryDirectory),
      profileStore_(profileFilePath)
{
    QString profileError;
    const bool profileLoaded = profileStore_.load(preferences_, &profileError);
    const bool apiSettingsLoaded = profileStore_.loadApiSettings(apiSettings_, &apiSettingsLoadError_);
    networkManager_ = new QNetworkAccessManager(this);
    setWindowTitle("NLSI Exclusive Logbook 1.8.8 Beta");
    const QString iconPath = QFileInfo(QDir::currentPath(), "resources/nlsi.ico").exists()
        ? QDir::currentPath() + "/resources/nlsi.ico"
        : QCoreApplication::applicationDirPath() + "/resources/nlsi.ico";
    setWindowIcon(QIcon(iconPath));
    resize(900, 600);
    setMinimumSize(900, 600);

    setStyleSheet(QStringLiteral(
        "QWidget { color: #28232A; background: #FFFFFF; }"
        "QLabel { font-weight: 700; }"
        "QWidget#rootWidget { background: #FFFFFF; }"
        "QWidget#sidebar { background: #FFF8FB; border-right: 1px solid #FFD1E6; }"
        "QLabel#sidebarTitle { font: 700 %1px 'Segoe UI'; color: #F896B9; }"
        "QLabel#sidebarSubtitle { font: 700 %2px 'Segoe UI'; color: #7E6672; }"
        "QLabel#sidebarGroup { font: 700 10px 'Segoe UI'; color: #A15E7D; letter-spacing: 1px; padding: 2px 4px; }"
        "QPushButton[sidebarNavButton=\"true\"] { font: 700 13px 'Segoe UI'; color: #665B62; background: transparent; border: 1px solid transparent; border-radius: 5px; padding: 9px 12px; text-align: left; }"
        "QPushButton[sidebarNavButton=\"true\"]:hover { color: #4D3440; background: #FDEBF3; border-color: #F9D6E6; }"
        "QPushButton[sidebarNavButton=\"true\"]:focus { border-color: #F896B9; }"
        "QPushButton[sidebarNavButton=\"true\"]:pressed { color: #4D3440; background: #F5C6DC; border-color: #E9A9C4; }"
        "QPushButton[sidebarNavButton=\"true\"][active=\"true\"] { font-weight: 700; color: #FFFFFF; background: #D94F8C; border: 1px solid #C83E7B; border-left: 4px solid #9F285D; }"
        "QPushButton[sidebarNavButton=\"true\"][active=\"true\"]:hover { color: #FFFFFF; background: #C83E7B; border-color: #B8306D; }"
        "QPushButton[sidebarNavButton=\"true\"][active=\"true\"]:pressed { color: #FFFFFF; background: #A92E67; border-color: #922357; }"
        "QPushButton[settingsTab=\"true\"] { font: 700 12px 'Segoe UI'; color: #665B62; background: #FFF8FB; border: 1px solid #FFD1E6; border-radius: 5px; padding: 8px 16px; }"
        "QPushButton[settingsTab=\"true\"][active=\"true\"] { color: #FFFFFF; background: #D94F8C; border-color: #C83E7B; }"
        "QPushButton#savePreferencesButton { font: 700 12px 'Segoe UI'; color: #FFFFFF; background: #D94F8C; border: 1px solid #C83E7B; border-radius: 5px; padding: 9px 16px; }"
        "QPushButton#savePreferencesButton:hover { background: #C83E7B; }"
        "QPushButton#completedJobCard { text-align: left; background: #FFFFFF; border: 1px solid #FFD1E6; border-radius: 5px; padding: 10px; }"
        "QPushButton#completedJobCard QLabel { font-weight: 700; }"
        "QPushButton#completedJobCard:hover { background: #FFF7FA; }"
        "QLabel#headerTitle { font: 700 %3px 'Segoe UI'; color: #28232A; }"
        "QLabel#headerMode { font: 700 10px 'Segoe UI'; color: #A15E7D; }"
        "QLabel#headerMeta { font: 700 10px 'Segoe UI'; color: #665B62; }"
        "QLabel#processState, QLabel#connectionState { font: 700 10px 'Segoe UI'; color: #665B62; }"
        "QLabel#profileAvatar { font: 700 24px 'Segoe UI'; color: #FFFFFF; background: #D94F8C; border-radius: 12px; }"
        "QLabel#profileHeaderName { font: 700 17px 'Segoe UI'; color: #28232A; }"
        "QLabel#profileHeaderUsername { font: 400 12px 'Segoe UI'; color: #665B62; }"
        "QLabel#profileHeaderStatus, QLabel#profileHeaderRole { font: 700 10px 'Segoe UI'; border-radius: 9px; padding: 3px 9px; }"
        "QLabel#profileHeaderStatus { color: #1C8059; background: #E4F6EC; }"
        "QLabel#profileHeaderRole { color: #B03C73; background: #FDEBF3; }"
        "QLabel#profileDetailLabel { font: 400 12px 'Segoe UI'; color: #665B62; }"
        "QLabel#profileDetailValue { font: 400 12px 'Segoe UI'; color: #28232A; }"
        "QPushButton[profileAction=\"true\"] { font: 400 11px 'Segoe UI'; color: #C83E7B; background: #FFFFFF; border: 1px solid #FFD1E6; border-radius: 5px; padding: 3px 10px; }"
        "QPushButton[profileAction=\"true\"]:hover { background: #FFF7FA; }"
        "QPushButton[profileAction=\"true\"]:disabled { color: #AFA4AA; border-color: #F0E7EB; }"
        "QProgressBar#profileTaskProgress, QProgressBar#profileRankProgress { height: 11px; color: #665B62; background: #FCEAF2; border: 1px solid #FFD1E6; border-radius: 5px; text-align: center; font: 400 9px 'Segoe UI'; }"
        "QProgressBar#profileTaskProgress::chunk, QProgressBar#profileRankProgress::chunk { background: #D94F8C; border-radius: 4px; }"
        "QProgressBar#profileTaskProgress:disabled { color: #A15E7D; background: #FFF7FA; }"
        "QLabel#profileRankMeta { font: 400 10px 'Segoe UI'; color: #A15E7D; }"
        "QLabel#profileTaskStatus, QLabel#profileTaskHistoryTitle, QLabel#profileTaskHistory { font: 400 10px 'Segoe UI'; color: #665B62; }"
        "QLabel#profileTaskHistoryTitle { font-weight: 700; color: #A15E7D; padding-top: 2px; }"
        "QLabel#profileSyncTitle { font: 700 12px 'Segoe UI'; color: #8A5A00; }"
        "QLabel#profileStatus { font: 400 10px 'Segoe UI'; color: #8A5A00; }"
        "QLabel#telemetryLabel { font: 700 %4px 'Segoe UI'; color: #A15E7D; letter-spacing: 0.35px; padding-top: 2px; }"
        "QLabel#telemetryValue { font: 700 %4px 'Segoe UI'; color: #28232A; min-height: 18px; background: #FFFFFF; }"
        "QLabel#gameApiVersionLabel { font: 700 %4px 'Segoe UI'; color: #A15E7D; }"
        "QLabel#gameApiVersionValue { font: 700 %4px 'Segoe UI'; color: #28232A; background: #FFFFFF; }"
        "QGroupBox { font: 700 %5px 'Segoe UI'; color: #28232A; background: #FFFFFF; border: 1px solid #FFD1E6; border-radius: 5px; margin-top: 7px; padding-top: 8px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 9px; padding: 0 3px; }"
        "QScrollArea { background: #FFFFFF; border: none; }"
        "QScrollBar:vertical { width: 10px; background: #FFF7FA; border: 1px solid #FFD1E6; border-radius: 5px; margin: 0; }"
        "QScrollBar:horizontal { height: 10px; background: #FFF7FA; border: 1px solid #FFD1E6; border-radius: 5px; margin: 0; }"
        "QScrollBar::handle:vertical { min-height: 28px; background: #E9A9C4; border: 1px solid #D982AA; border-radius: 4px; margin: 1px; }"
        "QScrollBar::handle:horizontal { min-width: 28px; background: #E9A9C4; border: 1px solid #D982AA; border-radius: 4px; margin: 1px; }"
        "QScrollBar::handle:vertical:hover, QScrollBar::handle:horizontal:hover { background: #D982AA; border-color: #C85F91; }"
        "QScrollBar::handle:vertical:pressed, QScrollBar::handle:horizontal:pressed { background: #C85F91; border-color: #A93B70; }"
        "QScrollBar::handle:vertical:disabled, QScrollBar::handle:horizontal:disabled { background: #F0DCE5; border-color: #E7CCD8; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; border: none; background: transparent; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; border: none; background: transparent; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical, QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }"
        "QAbstractScrollArea::corner { background: #FFF7FA; border: 1px solid #FFD1E6; }"
        "QWidget#contentWidget, QScrollArea > QWidget, QAbstractScrollArea { background: #FFFFFF; }"
    ).arg(typography::BrandName)
        .arg(typography::BrandSubtitle)
        .arg(typography::HeaderTitle)
        .arg(typography::Field)
        .arg(typography::CardTitle));

    auto* root = new QWidget(this);
    root->setObjectName("rootWidget");
    auto* outerLayout = new QHBoxLayout(root);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    sidebar_ = new QWidget(root);
    sidebar_->setObjectName("sidebar");
    sidebar_->setFixedWidth(280);
    buildSidebar();
    outerLayout->addWidget(sidebar_);

    content_ = new QWidget(root);
    buildContent();
    outerLayout->addWidget(content_, 1);
    setCentralWidget(root);

    auto* statusBar = new QStatusBar(this);
    statusBar->setSizeGripEnabled(false);
    statusBar->setStyleSheet(
        "QStatusBar { background: #FFF7FA; border-top: 1px solid #FFD1E6; color: #665B62; font: 700 10px 'Segoe UI'; padding: 0 8px; }"
    );
    statusBar->showMessage("LOCAL TELEMETRY • VERSION 1.8.8 BETA");
    setStatusBar(statusBar);
    if (!jobHistory_.errorString().isEmpty())
    {
        statusBar->showMessage(uppercaseDisplayText("JOB HISTORY WARNING: " + jobHistory_.errorString()));
    }
    if (!profileLoaded)
    {
        statusBar->showMessage(uppercaseDisplayText("PREFERENCES WARNING: " + profileError));
    }
    if (!apiSettingsLoaded)
    {
        statusBar->showMessage(uppercaseDisplayText(
            "API SETTINGS WARNING: " + apiSettingsLoadError_));
    }
    else if (!apiSettings_.baseUrl.trimmed().isEmpty() &&
             !apiSettings_.apiKey.trimmed().isEmpty())
    {
        QString identityError;
        if (!profileStore_.saveApiSettings(apiSettings_, &identityError))
        {
            apiSettingsLoadError_ = identityError;
            statusBar->showMessage(uppercaseDisplayText(
                "API SETTINGS WARNING: " + identityError));
        }
    }
    refreshCompletedJobs();
    updateApiConnectionState(ApiConnectionState::LocalMode,
        "Not connected. The API must be reachable and the configured driver key must authenticate.");
    const bool apiConfigured = !apiSettings_.baseUrl.trimmed().isEmpty() &&
        !apiSettings_.apiKey.trimmed().isEmpty();
    if (!apiConfigured)
    {
        setProfileUnavailable(apiSettingsLoadError_.isEmpty()
            ? "Not connected. Configure your API URL and key in Settings to connect."
            : "Saved API settings could not be loaded. Check the encrypted profile.");
    }
    else
    {
        profileStatus_->setText("Waiting for the configured account to authenticate.");
    }

    startTelemetryListener();

    processTimer_ = new QTimer(this);
    processTimer_->setInterval(1000);
    connect(processTimer_, &QTimer::timeout, this, &DashboardMainWindow::updateProcessDetection);
    processTimer_->start();
    updateProcessDetection();

    telemetryTimer_ = new QTimer(this);
    telemetryTimer_->setInterval(500);
    connect(telemetryTimer_, &QTimer::timeout, this, &DashboardMainWindow::refreshDisplayedSnapshot);
    telemetryTimer_->start();
    refreshDisplayedSnapshot();

    jobSyncTimer_ = new QTimer(this);
    jobSyncTimer_->setObjectName("jobSyncTimer");
    jobSyncTimer_->setInterval(syncRetryDelayMs_);
    connect(jobSyncTimer_, &QTimer::timeout, this, [this]()
    {
        if (!apiOnline_ || syncRequestInFlight_ || profileRequestInFlight_ ||
            connectionRequestInFlight_)
        {
            return;
        }
        if (authenticatedDriverId_ > 0)
        {
            synchronizeAccountJobs(authenticatedDriverId_);
        }
    });
    jobSyncTimer_->start();

    connectionCheckTimer_ = new QTimer(this);
    connectionCheckTimer_->setObjectName("profileConnectionCheckTimer");
    connectionCheckTimer_->setInterval(connectionRetryDelayMs_);
    connect(connectionCheckTimer_, &QTimer::timeout, this, [this]()
    {
        if (apiSettings_.baseUrl.trimmed().isEmpty() ||
            apiSettings_.apiKey.trimmed().isEmpty() ||
            profileRequestInFlight_ || connectionRequestInFlight_ || syncRequestInFlight_)
        {
            return;
        }
        requestAccountProfile(false, false, true);
    });
    connectionCheckTimer_->start();

    QTimer::singleShot(0, this, &DashboardMainWindow::requestCurrencyExchangeRate);
    if (apiSettingsLoaded && apiSettingsLoadError_.isEmpty() &&
        !apiSettings_.baseUrl.trimmed().isEmpty() && !apiSettings_.apiKey.trimmed().isEmpty())
    {
        QTimer::singleShot(0, this, [this]()
        {
            requestAccountProfile(false, true, true);
        });
    }
}

DashboardMainWindow::~DashboardMainWindow() = default;

void DashboardMainWindow::updateProcessDetection()
{
    if (processScanInFlight_)
    {
        return;
    }
    processScanInFlight_ = true;
    auto* watcher = new QFutureWatcher<GameProcessState>(this);
    connect(watcher, &QFutureWatcher<GameProcessState>::finished, this, [this, watcher]()
    {
        processScanInFlight_ = false;
        setProcessState(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run(detectGameProcesses));
}

void DashboardMainWindow::buildSidebar()
{
    auto* layout = new QVBoxLayout(sidebar_);
    layout->setContentsMargins(12, 16, 12, 12);
    layout->setSpacing(9);

    auto* brandRow = new QWidget(sidebar_);
    auto* brandLayout = new QHBoxLayout(brandRow);
    brandLayout->setContentsMargins(0, 0, 0, 0);
    brandLayout->setSpacing(10);
    const QString logoPath = QFileInfo(QDir::currentPath(), "resources/nlsi.ico").exists()
        ? QDir::currentPath() + "/resources/nlsi.ico"
        : QCoreApplication::applicationDirPath() + "/resources/nlsi.ico";
    auto* logoLabel = new QLabel(brandRow);
    logoLabel->setPixmap(QPixmap(logoPath).scaled(38, 38, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logoLabel->setAlignment(Qt::AlignCenter);
    auto* brandText = new QWidget(brandRow);
    auto* brandTextLayout = new QVBoxLayout(brandText);
    brandTextLayout->setContentsMargins(0, 0, 0, 0);
    brandTextLayout->setSpacing(0);
    auto* brandName = new QLabel("NABSKI", brandText);
    brandName->setObjectName("sidebarTitle");
    auto* brandSubtitle = new QLabel("LOGISTICS SOLUTIONS INC.", brandText);
    brandSubtitle->setObjectName("sidebarSubtitle");
    brandSubtitle->setWordWrap(false);
    brandSubtitle->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    brandTextLayout->addWidget(brandName);
    brandTextLayout->addWidget(brandSubtitle);
    brandLayout->addWidget(logoLabel);
    brandLayout->addWidget(brandText, 1);
    layout->addWidget(brandRow);
    auto* divider = new QFrame(sidebar_);
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Plain);
    divider->setStyleSheet("QFrame { color: #FFD1E6; background: #FFD1E6; max-height: 1px; }");
    layout->addWidget(divider);
    layout->addSpacing(14);

    auto* workspace = new QLabel("WORKSPACE", sidebar_);
    workspace->setObjectName("sidebarGroup");
    layout->addWidget(workspace);
    dashboardNavButton_ = new QPushButton("DASHBOARD", sidebar_);
    dashboardNavButton_->setObjectName("dashboardNavButton");
    dashboardNavButton_->setProperty("sidebarNavButton", true);
    dashboardNavButton_->setProperty("active", true);
    dashboardNavButton_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    dashboardNavButton_->setFixedHeight(38);
    connect(dashboardNavButton_, &QPushButton::clicked, this, &DashboardMainWindow::showDashboard);
    layout->addWidget(dashboardNavButton_);
    completedJobsNavButton_ = new QPushButton("COMPLETED JOBS", sidebar_);
    completedJobsNavButton_->setObjectName("completedJobsNavButton");
    completedJobsNavButton_->setProperty("sidebarNavButton", true);
    completedJobsNavButton_->setProperty("active", false);
    completedJobsNavButton_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    completedJobsNavButton_->setFixedHeight(38);
    connect(completedJobsNavButton_, &QPushButton::clicked, this, &DashboardMainWindow::showCompletedJobs);
    layout->addWidget(completedJobsNavButton_);
    settingsNavButton_ = new QPushButton("SETTINGS", sidebar_);
    settingsNavButton_->setObjectName("settingsNavButton");
    settingsNavButton_->setProperty("sidebarNavButton", true);
    settingsNavButton_->setProperty("active", false);
    settingsNavButton_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    settingsNavButton_->setFixedHeight(38);
    connect(settingsNavButton_, &QPushButton::clicked, this, &DashboardMainWindow::showSettings);
    layout->addWidget(settingsNavButton_);
    layout->addStretch();
    auto* footerDivider = new QFrame(sidebar_);
    footerDivider->setFrameShape(QFrame::HLine);
    footerDivider->setFrameShadow(QFrame::Plain);
    footerDivider->setStyleSheet("QFrame { color: #FFD1E6; background: #FFD1E6; max-height: 1px; }");
    layout->addWidget(footerDivider);
    auto* version = new QLabel("1.8.8 BETA", sidebar_);
    version->setObjectName("sidebarSubtitle");
    layout->addWidget(version);
}

void DashboardMainWindow::buildContent()
{
    auto* rootLayout = new QVBoxLayout(content_);
    rootLayout->setContentsMargins(12, 10, 12, 8);
    rootLayout->setSpacing(8);

    auto* header = new QWidget(content_);
    header->setStyleSheet("background: #FFFFFF; border: 1px solid #FFD1E6; border-radius: 5px;");
    auto* headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(9, 7, 9, 7);
    headerLayout->setSpacing(5);
    auto* titleRow = new QHBoxLayout();
    titleRow->setSpacing(8);
    headerTitle_ = new QLabel("DASHBOARD", header);
    headerTitle_->setObjectName("headerTitle");
    connectionModeLabel_ = new QLabel("LOCAL MODE", header);
    connectionModeLabel_->setObjectName("headerMode");
    dateLabel_ = new QLabel(header);
    dateLabel_->setObjectName("headerMeta");
    dateLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    titleRow->addWidget(headerTitle_);
    titleRow->addWidget(connectionModeLabel_);
    titleRow->addStretch();
    titleRow->addWidget(dateLabel_);
    headerLayout->addLayout(titleRow);

    processLabel_ = new QLabel(header);
    processLabel_->setObjectName("processState");
    processLabel_->hide();
    headerLayout->addWidget(processLabel_);
    connectionLabel_ = new QLabel(header);
    connectionLabel_->setObjectName("connectionState");
    connectionLabel_->hide();
    headerLayout->addWidget(connectionLabel_);
    rootLayout->addWidget(header);

    auto* scroll = new QScrollArea(content_);
    scroll->setObjectName("dashboardScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    auto* scrollContent = new QWidget(scroll);
    auto* sections = new QVBoxLayout(scrollContent);
    sections->setContentsMargins(0, 0, 0, 2);
    sections->setSpacing(8);

    auto addField = [](QGridLayout* grid, QWidget* parent, const QString& name, int row, int column)
    {
        auto* cell = new QWidget(parent);
        auto* cellLayout = new QVBoxLayout(cell);
        cellLayout->setContentsMargins(0, 0, 0, 0);
        cellLayout->setSpacing(1);
        auto* label = new QLabel(name, cell);
        label->setObjectName("telemetryLabel");
        label->setWordWrap(true);
        cellLayout->addWidget(label);
        auto* value = makeValue(cell);
        cellLayout->addWidget(value);
        grid->addWidget(cell, row, column, 1, 2);
        return value;
    };

    auto configureGrid = [](QGridLayout* grid)
    {
        grid->setContentsMargins(9, 13, 9, 8);
        grid->setHorizontalSpacing(14);
        grid->setVerticalSpacing(5);
        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 1);
        grid->setColumnStretch(2, 1);
        grid->setColumnStretch(3, 1);
    };

    auto* job = new QGroupBox("CURRENT JOB", scrollContent);
    auto* jobGrid = new QGridLayout(job);
    configureGrid(jobGrid);
    currentJobValue_ = addField(jobGrid, job, "JOB ID", 0, 0);
    currentJobValue_->setObjectName("currentJobValue");
    statusValue_ = addField(jobGrid, job, "STATUS", 0, 2);
    statusValue_->setObjectName("statusValue");
    cargoValue_ = addField(jobGrid, job, "CARGO", 1, 0);
    incomeValue_ = addField(jobGrid, job, "INCOME", 1, 2);
    incomeValue_->setObjectName("incomeValue");
    fromValue_ = addField(jobGrid, job, "FROM", 2, 0);
    fromValue_->setObjectName("fromValue");
    toValue_ = addField(jobGrid, job, "TO", 2, 2);
    toValue_->setObjectName("toValue");
    plannedDistanceValue_ = addField(jobGrid, job, "PLANNED DISTANCE", 3, 0);
    plannedDistanceValue_->setObjectName("plannedDistanceValue");
    remainingDistanceValue_ = addField(jobGrid, job, "REMAINING DISTANCE", 3, 2);
    remainingDistanceValue_->setObjectName("remainingDistanceValue");
    auto* progressCell = new QWidget(job);
    auto* progressCellLayout = new QVBoxLayout(progressCell);
    progressCellLayout->setContentsMargins(0, 0, 0, 0);
    progressCellLayout->setSpacing(1);
    auto* progressLabel = new QLabel("PROGRESS", progressCell);
    progressLabel->setObjectName("telemetryLabel");
    progressCellLayout->addWidget(progressLabel);
    progressValue_ = makeValue(progressCell);
    progressValue_->setText(unavailable);
    progressCellLayout->addWidget(progressValue_);
    jobGrid->addWidget(progressCell, 4, 0, 1, 2);
    arrivalValue_ = addField(jobGrid, job, "ARRIVAL", 4, 2);
    sections->addWidget(job);

    auto* driving = new QGroupBox("DRIVING", scrollContent);
    auto* drivingGrid = new QGridLayout(driving);
    configureGrid(drivingGrid);
    fuelValue_ = addField(drivingGrid, driving, "FUEL", 0, 0);
    rpmGearValue_ = addField(drivingGrid, driving, "RPM / GEAR", 0, 2);
    throttleValue_ = addField(drivingGrid, driving, "THROTTLE", 1, 0);
    brakeValue_ = addField(drivingGrid, driving, "BRAKE", 1, 2);
    cruiseValue_ = addField(drivingGrid, driving, "CRUISE CONTROL", 2, 0);
    retarderValue_ = addField(drivingGrid, driving, "RETARDER", 2, 2);
    fuelValue_->setObjectName("fuelValue");
    rpmGearValue_->setObjectName("rpmGearValue");
    throttleValue_->setObjectName("throttleValue");
    brakeValue_->setObjectName("brakeValue");
    cruiseValue_->setObjectName("cruiseValue");
    retarderValue_->setObjectName("retarderValue");
    arrivalValue_->setObjectName("arrivalValue");
    sections->addWidget(driving);

    gameConfigPanel_ = new QGroupBox("GAME CONFIG | —", scrollContent);
    auto* configLayout = new QVBoxLayout(gameConfigPanel_);
    configLayout->setContentsMargins(9, 13, 9, 8);
    configLayout->setSpacing(5);
    auto* apiVersionRow = new QHBoxLayout();
    apiVersionRow->setContentsMargins(0, 0, 0, 0);
    auto* apiVersionLabel = new QLabel("GAME API VERSION", gameConfigPanel_);
    apiVersionLabel->setObjectName("gameApiVersionLabel");
    gameVersionValue_ = makeValue(gameConfigPanel_);
    gameVersionValue_->setObjectName("gameApiVersionValue");
    apiVersionRow->addWidget(apiVersionLabel);
    apiVersionRow->addWidget(gameVersionValue_, 1);
    configLayout->addLayout(apiVersionRow);
    auto* configGrid = new QGridLayout();
    configureGrid(configGrid);
    vehicleValue_ = addField(configGrid, gameConfigPanel_, "VEHICLE", 0, 0);
    vehiclePlateValue_ = addField(configGrid, gameConfigPanel_, "VEHICLE PLATE", 0, 2);
    trailerValue_ = addField(configGrid, gameConfigPanel_, "TRAILER", 1, 0);
    trailerPlateValue_ = addField(configGrid, gameConfigPanel_, "TRAILER PLATE", 1, 2);
    configLayout->addLayout(configGrid);
    sections->addWidget(gameConfigPanel_);

    auto* summary = new QGroupBox("SUMMARY", scrollContent);
    auto* summaryLayout = new QVBoxLayout(summary);
    summaryLayout->setContentsMargins(9, 13, 9, 8);
    summaryValue_ = makeValue(summary);
    summaryValue_->setText("NO EVENTS AVAILABLE.");
    summaryValue_->setStyleSheet("color: #665B62;");
    summaryLayout->addWidget(summaryValue_);
    sections->addWidget(summary);
    sections->addStretch();

    scroll->setWidget(scrollContent);
    pageStack_ = new QStackedWidget(content_);
    pageStack_->setObjectName("mainPages");
    pageStack_->addWidget(scroll);

    auto* completedPage = new QWidget(pageStack_);
    auto* completedPageLayout = new QVBoxLayout(completedPage);
    completedPageLayout->setContentsMargins(0, 0, 0, 0);
    auto* completedScroll = new QScrollArea(completedPage);
    completedScroll->setObjectName("completedJobsScroll");
    completedScroll->setWidgetResizable(true);
    completedScroll->setFrameShape(QFrame::NoFrame);
    auto* completedContent = new QWidget(completedScroll);
    completedJobsLayout_ = new QVBoxLayout(completedContent);
    completedJobsLayout_->setContentsMargins(0, 0, 0, 2);
    completedJobsLayout_->setSpacing(8);
    completedEmptyLabel_ = new QLabel("NO COMPLETED OR CANCELLED JOBS YET.", completedContent);
    completedEmptyLabel_->setObjectName("completedJobsEmpty");
    completedEmptyLabel_->setWordWrap(true);
    completedEmptyLabel_->setAlignment(Qt::AlignCenter);
    completedEmptyLabel_->setStyleSheet("color: #665B62; padding: 24px;");
    completedJobsLayout_->addWidget(completedEmptyLabel_);
    completedJobsLayout_->addStretch();
    completedScroll->setWidget(completedContent);
    completedPageLayout->addWidget(completedScroll);
    pageStack_->addWidget(completedPage);

    auto* settingsPage = new QWidget(pageStack_);
    auto* settingsLayout = new QVBoxLayout(settingsPage);
    settingsLayout->setContentsMargins(0, 0, 0, 0);
    settingsLayout->setSpacing(8);
    auto* tabRow = new QHBoxLayout();
    tabRow->setSpacing(6);
    preferenceTabButton_ = new QPushButton("PREFERENCE", settingsPage);
    preferenceTabButton_->setObjectName("preferenceTabButton");
    preferenceTabButton_->setProperty("settingsTab", true);
    preferenceTabButton_->setProperty("active", true);
    configTabButton_ = new QPushButton("CONFIG", settingsPage);
    configTabButton_->setObjectName("configTabButton");
    configTabButton_->setProperty("settingsTab", true);
    configTabButton_->setProperty("active", false);
    profileTabButton_ = new QPushButton("PROFILE", settingsPage);
    profileTabButton_->setObjectName("profileTabButton");
    profileTabButton_->setProperty("settingsTab", true);
    profileTabButton_->setProperty("active", false);
    tabRow->addWidget(preferenceTabButton_);
    tabRow->addWidget(configTabButton_);
    tabRow->addWidget(profileTabButton_);
    tabRow->addStretch();
    settingsLayout->addLayout(tabRow);

    settingsTabs_ = new QStackedWidget(settingsPage);
    settingsTabs_->setObjectName("settingsTabs");
    auto* preferencePage = new QWidget(settingsTabs_);
    auto* preferencePageLayout = new QVBoxLayout(preferencePage);
    preferencePageLayout->setContentsMargins(0, 0, 0, 0);
    auto* preferencePanel = new QGroupBox("MEASUREMENT PREFERENCES", preferencePage);
    auto* preferenceForm = new QFormLayout(preferencePanel);
    preferenceForm->setContentsMargins(14, 16, 14, 12);
    preferenceForm->setHorizontalSpacing(18);
    preferenceForm->setVerticalSpacing(12);

    auto makePreference = [&](const QString& label, const QString& name)
    {
        auto* combo = new QComboBox(preferencePanel);
        combo->setObjectName(name);
        combo->setMinimumWidth(220);
        auto* formLabel = new QLabel(label, preferencePanel);
        formLabel->setObjectName("telemetryLabel");
        preferenceForm->addRow(formLabel, combo);
        return combo;
    };
    speedPreference_ = makePreference("SPEED", "speedPreference");
    speedPreference_->addItem("Kilometers per hour (km/h)",
                              static_cast<int>(SpeedUnit::KilometresPerHour));
    speedPreference_->addItem("Miles per hour (mph)",
                              static_cast<int>(SpeedUnit::MilesPerHour));
    volumePreference_ = makePreference("VOLUME", "volumePreference");
    volumePreference_->addItem("Liters (L)", static_cast<int>(VolumeUnit::Liters));
    volumePreference_->addItem("US gallons (US gal)", static_cast<int>(VolumeUnit::UsGallons));
    volumePreference_->addItem("Imperial gallons (Imp gal)",
                               static_cast<int>(VolumeUnit::ImperialGallons));
    currencyPreference_ = makePreference("CURRENCY", "currencyPreference");
    currencyPreference_->addItem("EUR (€)", static_cast<int>(CurrencyUnit::Euro));
    currencyPreference_->addItem("USD ($)", static_cast<int>(CurrencyUnit::UsDollar));
    distancePreference_ = makePreference("DISTANCE", "distancePreference");
    distancePreference_->addItem("Kilometers (km)", static_cast<int>(DistanceUnit::Kilometres));
    distancePreference_->addItem("Miles (mi)", static_cast<int>(DistanceUnit::Miles));

    auto* preferenceActions = new QHBoxLayout();
    preferenceActions->addStretch();
    savePreferencesButton_ = new QPushButton("SAVE PREFERENCES", preferencePage);
    savePreferencesButton_->setObjectName("savePreferencesButton");
    preferenceActions->addWidget(savePreferencesButton_);
    preferencePageLayout->addWidget(preferencePanel);
    preferencePageLayout->addLayout(preferenceActions);
    preferencePageLayout->addStretch();
    settingsTabs_->addWidget(preferencePage);

    auto* configPage = new QWidget(settingsTabs_);
    auto* apiConfigLayout = new QVBoxLayout(configPage);
    apiConfigLayout->setContentsMargins(0, 0, 0, 0);
    auto* configPanel = new QGroupBox("API CONNECTION", configPage);
    auto* configForm = new QFormLayout(configPanel);
    configForm->setContentsMargins(14, 16, 14, 12);
    configForm->setHorizontalSpacing(18);
    configForm->setVerticalSpacing(12);
    apiUrlInput_ = new QLineEdit(apiSettings_.baseUrl, configPanel);
    apiUrlInput_->setObjectName("apiUrlInput");
    apiUrlInput_->setMinimumWidth(320);
    apiUrlInput_->setPlaceholderText("Enter your HTTPS API URL");
    configForm->addRow("API LINK", apiUrlInput_);
    apiKeyInput_ = new QLineEdit(apiSettings_.apiKey, configPanel);
    apiKeyInput_->setObjectName("apiKeyInput");
    apiKeyInput_->setEchoMode(QLineEdit::Password);
    apiKeyInput_->setPlaceholderText("Enter your driver API key");
    configForm->addRow("API KEY", apiKeyInput_);
    apiConfigStatus_ = new QLabel(
        !apiSettingsLoadError_.isEmpty()
            ? "Saved API settings could not be loaded. Check the encrypted profile."
            : apiSettings_.baseUrl.trimmed().isEmpty() || apiSettings_.apiKey.trimmed().isEmpty()
                ? "Configure both your API URL and driver API key before connecting."
                : "Saved API settings are ready to authenticate.",
        configPanel);
    apiConfigStatus_->setObjectName("apiConfigStatus");
    apiConfigStatus_->setWordWrap(true);
    apiConfigStatus_->setStyleSheet("color: #665B62; padding-top: 6px;");
    configForm->addRow("STATUS", apiConfigStatus_);
    auto* configActions = new QHBoxLayout();
    configActions->addStretch();
    saveApiSettingsButton_ = new QPushButton("SAVE CONFIG", configPage);
    saveApiSettingsButton_->setObjectName("saveApiSettingsButton");
    testApiConnectionButton_ = new QPushButton("TEST CONNECTION", configPage);
    testApiConnectionButton_->setObjectName("testApiConnectionButton");
    removeApiConfigurationButton_ = new QPushButton("REMOVE API CONFIGURATION", configPage);
    removeApiConfigurationButton_->setObjectName("removeApiConfigurationButton");
    configActions->addWidget(saveApiSettingsButton_);
    configActions->addWidget(testApiConnectionButton_);
    configActions->addWidget(removeApiConfigurationButton_);
    apiConfigLayout->addWidget(configPanel);
    apiConfigLayout->addLayout(configActions);
    apiConfigLayout->addStretch();
    settingsTabs_->addWidget(configPage);

    auto* profilePage = new QWidget(settingsTabs_);
    auto* profileLayout = new QVBoxLayout(profilePage);
    profileLayout->setContentsMargins(0, 0, 0, 0);
    auto* profileScroll = new QScrollArea(profilePage);
    profileScroll->setObjectName("profileScroll");
    profileScroll->setWidgetResizable(true);
    profileScroll->setFrameShape(QFrame::NoFrame);
    profileScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* profileContent = new QWidget(profileScroll);
    auto* profileContentLayout = new QVBoxLayout(profileContent);
    profileContentLayout->setContentsMargins(0, 0, 0, 2);
    profileContentLayout->setSpacing(8);

    auto* accountCard = new QFrame(profileContent);
    accountCard->setObjectName("profileHeaderCard");
    accountCard->setStyleSheet(
        "QFrame#profileHeaderCard { background: #FFFFFF; border: 1px solid #FFD1E6; border-radius: 9px; }");
    auto* accountLayout = new QHBoxLayout(accountCard);
    accountLayout->setContentsMargins(16, 12, 16, 12);
    accountLayout->setSpacing(14);
    profileAvatar_ = new QLabel(profileUnavailable, accountCard);
    profileAvatar_->setObjectName("profileAvatar");
    profileAvatar_->setFixedSize(62, 62);
    profileAvatar_->setAlignment(Qt::AlignCenter);
    profileAvatar_->setWordWrap(true);
    accountLayout->addWidget(profileAvatar_);
    auto* accountText = new QVBoxLayout();
    accountText->setContentsMargins(0, 0, 0, 0);
    accountText->setSpacing(2);
    profileHeaderName_ = new QLabel(profileUnavailable, accountCard);
    profileHeaderName_->setObjectName("profileHeaderName");
    profileHeaderUsername_ = new QLabel(profileUnavailable, accountCard);
    profileHeaderUsername_->setObjectName("profileHeaderUsername");
    auto* accountBadges = new QHBoxLayout();
    accountBadges->setContentsMargins(0, 2, 0, 0);
    accountBadges->setSpacing(6);
    profileHeaderStatus_ = new QLabel(profileUnavailable, accountCard);
    profileHeaderStatus_->setObjectName("profileHeaderStatus");
    profileHeaderRole_ = new QLabel(profileUnavailable, accountCard);
    profileHeaderRole_->setObjectName("profileHeaderRole");
    accountBadges->addWidget(profileHeaderStatus_);
    accountBadges->addWidget(profileHeaderRole_);
    accountBadges->addStretch();
    accountText->addWidget(profileHeaderName_);
    accountText->addWidget(profileHeaderUsername_);
    accountText->addLayout(accountBadges);
    accountLayout->addLayout(accountText, 1);
    profileContentLayout->addWidget(accountCard);

    auto* taskCard = new QGroupBox(profileContent);
    taskCard->setObjectName("profileTaskCard");
    auto* taskLayout = new QVBoxLayout(taskCard);
    taskLayout->setContentsMargins(12, 12, 12, 10);
    taskLayout->setSpacing(7);
    auto* taskTitle = new QLabel("TASK:", taskCard);
    taskTitle->setObjectName("profileTaskTitle");
    taskTitle->setStyleSheet("font: 700 13px 'Segoe UI'; color: #28232A;");
    profileTaskProgress_ = new QProgressBar(taskCard);
    profileTaskProgress_->setObjectName("profileTaskProgress");
    profileTaskProgress_->setRange(0, 100);
    profileTaskProgress_->setValue(0);
    profileTaskProgress_->setFormat("TASK DATA NOT LOADED");
    profileTaskProgress_->setTextVisible(true);
    profileTaskProgress_->setEnabled(false);
    profileTaskStatus_ = new QLabel(
        "Connect an account to load its monthly task records.", taskCard);
    profileTaskStatus_->setObjectName("profileTaskStatus");
    profileTaskStatus_->setWordWrap(true);
    profileTaskHistoryTitle_ = new QLabel("TASK HISTORY", taskCard);
    profileTaskHistoryTitle_->setObjectName("profileTaskHistoryTitle");
    profileTaskHistoryTitle_->hide();
    profileTaskHistory_ = new QLabel(taskCard);
    profileTaskHistory_->setObjectName("profileTaskHistory");
    profileTaskHistory_->setWordWrap(true);
    profileTaskHistory_->hide();
    taskLayout->addWidget(taskTitle);
    taskLayout->addWidget(profileTaskProgress_);
    taskLayout->addWidget(profileTaskStatus_);
    taskLayout->addWidget(profileTaskHistoryTitle_);
    taskLayout->addWidget(profileTaskHistory_);
    profileContentLayout->addWidget(taskCard);

    auto* rankCard = new QGroupBox(profileContent);
    rankCard->setObjectName("profileRankCard");
    auto* rankLayout = new QVBoxLayout(rankCard);
    rankLayout->setContentsMargins(12, 12, 12, 10);
    rankLayout->setSpacing(7);
    auto* rankHeading = new QHBoxLayout();
    auto* rankTitle = new QLabel("RANK:", rankCard);
    rankTitle->setObjectName("profileDetailValue");
    profileRankValue_ = new QLabel(profileUnavailable, rankCard);
    profileRankValue_->setObjectName("profileRankValue");
    profileRankValue_->setStyleSheet("font: 700 13px 'Segoe UI';");
    profileDistanceValue_ = new QLabel(profileUnavailable, rankCard);
    profileDistanceValue_->setObjectName("profileDistanceValue");
    profileDistanceValue_->setStyleSheet("font: 700 18px 'Segoe UI';");
    auto* distanceBlock = new QVBoxLayout();
    distanceBlock->setContentsMargins(0, 0, 0, 0);
    distanceBlock->setSpacing(1);
    auto* distanceLabel = new QLabel("Qualifying distance", rankCard);
    distanceLabel->setObjectName("profileRankMeta");
    distanceLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    profileDistanceValue_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    distanceBlock->addWidget(distanceLabel);
    distanceBlock->addWidget(profileDistanceValue_);
    rankHeading->addWidget(rankTitle);
    rankHeading->addWidget(profileRankValue_);
    rankHeading->addStretch();
    rankHeading->addLayout(distanceBlock);
    rankLayout->addLayout(rankHeading);
    profileRankProgress_ = new QProgressBar(rankCard);
    profileRankProgress_->setObjectName("profileRankProgress");
    profileRankProgress_->setRange(0, 100);
    profileRankProgress_->setValue(0);
    profileRankProgress_->setFormat(profileUnavailable);
    profileRankProgress_->setTextVisible(true);
    rankLayout->addWidget(profileRankProgress_);
    auto* rankMeta = new QHBoxLayout();
    profileRemainingValue_ = new QLabel(profileUnavailable, rankCard);
    profileRemainingValue_->setObjectName("profileRankMeta");
    profileThresholdValue_ = new QLabel("Next rank at " + profileUnavailable, rankCard);
    profileThresholdValue_->setObjectName("profileRankMeta");
    rankMeta->addWidget(profileRemainingValue_);
    rankMeta->addStretch();
    rankMeta->addWidget(profileThresholdValue_);
    rankLayout->addLayout(rankMeta);
    profileContentLayout->addWidget(rankCard);

    auto* detailsCard = new QFrame(profileContent);
    detailsCard->setObjectName("profileDetailsCard");
    detailsCard->setStyleSheet(
        "QFrame#profileDetailsCard { background: #FFFFFF; border: 1px solid #FFD1E6; border-radius: 9px; }");
    auto* detailsLayout = new QVBoxLayout(detailsCard);
    detailsLayout->setContentsMargins(16, 7, 16, 7);
    detailsLayout->setSpacing(0);
    auto makeProfileRow = [detailsCard, detailsLayout](const QString& labelText,
                                                       const QString& valueName)
    {
        auto* row = new QWidget(detailsCard);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 6, 0, 6);
        rowLayout->setSpacing(8);
        auto* label = new QLabel(labelText, row);
        label->setObjectName("profileDetailLabel");
        label->setFixedWidth(125);
        auto* value = new QLabel(profileUnavailable, row);
        value->setObjectName(valueName);
        value->setProperty("profileValue", true);
        value->setStyleSheet("font: 400 12px 'Segoe UI'; color: #28232A;");
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        value->setWordWrap(true);
        rowLayout->addWidget(label);
        rowLayout->addWidget(value, 1);
        detailsLayout->addWidget(row);
        return value;
    };
    auto makeProfileAction = [](const QString& text, const QString& name, QWidget* parent)
    {
        auto* button = new QPushButton(text, parent);
        button->setObjectName(name);
        button->setProperty("profileAction", true);
        button->setFixedHeight(25);
        return button;
    };
    auto addProfileSeparator = [detailsCard, detailsLayout]()
    {
        auto* separator = new QFrame(detailsCard);
        separator->setFrameShape(QFrame::HLine);
        separator->setStyleSheet("QFrame { color: #FCEAF2; background: #FCEAF2; max-height: 1px; }");
        detailsLayout->addWidget(separator);
    };
    profileUsernameValue_ = makeProfileRow("Username", "profileUsernameValue");
    auto* usernameRow = profileUsernameValue_->parentWidget();
    profileUsernameCopyButton_ = makeProfileAction("Copy", "profileUsernameCopyButton", usernameRow);
    usernameRow->layout()->addWidget(profileUsernameCopyButton_);
    addProfileSeparator();
    profileDisplayNameValue_ = makeProfileRow("Display name", "profileDisplayNameValue");
    addProfileSeparator();
    profileEmailValue_ = makeProfileRow("Email", "profileEmailValue");
    auto* emailRow = profileEmailValue_->parentWidget();
    profileEmailVisibilityButton_ = makeProfileAction("Show", "profileEmailVisibilityButton", emailRow);
    profileEmailCopyButton_ = makeProfileAction("Copy", "profileEmailCopyButton", emailRow);
    emailRow->layout()->addWidget(profileEmailVisibilityButton_);
    emailRow->layout()->addWidget(profileEmailCopyButton_);
    addProfileSeparator();
    profileDriverIdValue_ = makeProfileRow("Driver ID", "profileDriverIdValue");
    auto* driverIdRow = profileDriverIdValue_->parentWidget();
    profileDriverIdCopyButton_ = makeProfileAction("Copy", "profileDriverIdCopyButton", driverIdRow);
    driverIdRow->layout()->addWidget(profileDriverIdCopyButton_);
    addProfileSeparator();
    profileDriverStatusValue_ = makeProfileRow("Account status", "profileDriverStatusValue");
    addProfileSeparator();
    profileRoleValue_ = makeProfileRow("Account role", "profileRoleValue");
    profileContentLayout->addWidget(detailsCard);

    auto* syncCard = new QWidget(profileContent);
    syncCard->setObjectName("profileSyncCard");
    syncCard->setStyleSheet(
        "QWidget#profileSyncCard { background: #FFF4D8; border-radius: 9px; }");
    auto* syncLayout = new QHBoxLayout(syncCard);
    syncLayout->setContentsMargins(12, 8, 12, 8);
    syncLayout->setSpacing(12);
    auto* syncText = new QVBoxLayout();
    syncText->setContentsMargins(0, 0, 0, 0);
    syncText->setSpacing(1);
    auto* syncTitle = new QLabel("Server synchronization", syncCard);
    syncTitle->setObjectName("profileSyncTitle");
    profileStatus_ = new QLabel(
        "Configure your API URL and key in Settings to connect.", syncCard);
    profileStatus_->setObjectName("profileStatus");
    profileStatus_->setWordWrap(true);
    syncText->addWidget(syncTitle);
    syncText->addWidget(profileStatus_);
    syncLayout->addLayout(syncText, 1);
    refreshProfileButton_ = new QPushButton("Refresh profile", syncCard);
    refreshProfileButton_->setObjectName("refreshProfileButton");
    refreshProfileButton_->setStyleSheet(
        "QPushButton { font: 700 12px 'Segoe UI'; color: #FFFFFF; background: #D94F8C; "
        "border: 1px solid #C83E7B; border-radius: 7px; padding: 8px 15px; }"
        "QPushButton:hover { background: #C83E7B; }"
        "QPushButton:disabled { background: #D99AB7; }");
    syncLayout->addWidget(refreshProfileButton_);
    profileContentLayout->addWidget(syncCard);
    profileContentLayout->addStretch();
    profileScroll->setWidget(profileContent);
    profileLayout->addWidget(profileScroll);
    settingsTabs_->addWidget(profilePage);
    settingsLayout->addWidget(settingsTabs_, 1);
    pageStack_->addWidget(settingsPage);

    connect(preferenceTabButton_, &QPushButton::clicked, this, [this]()
    {
        settingsTabs_->setCurrentIndex(0);
        preferenceTabButton_->setProperty("active", true);
        configTabButton_->setProperty("active", false);
        profileTabButton_->setProperty("active", false);
        preferenceTabButton_->style()->unpolish(preferenceTabButton_);
        preferenceTabButton_->style()->polish(preferenceTabButton_);
        configTabButton_->style()->unpolish(configTabButton_);
        configTabButton_->style()->polish(configTabButton_);
        profileTabButton_->style()->unpolish(profileTabButton_);
        profileTabButton_->style()->polish(profileTabButton_);
    });
    connect(profileTabButton_, &QPushButton::clicked, this, [this]()
    {
        settingsTabs_->setCurrentIndex(2);
        configTabButton_->setProperty("active", false);
        preferenceTabButton_->setProperty("active", false);
        profileTabButton_->setProperty("active", true);
        configTabButton_->style()->unpolish(configTabButton_);
        configTabButton_->style()->polish(configTabButton_);
        preferenceTabButton_->style()->unpolish(preferenceTabButton_);
        preferenceTabButton_->style()->polish(preferenceTabButton_);
        profileTabButton_->style()->unpolish(profileTabButton_);
        profileTabButton_->style()->polish(profileTabButton_);
        refreshAccountProfile();
    });
    connect(configTabButton_, &QPushButton::clicked, this, [this]()
    {
        settingsTabs_->setCurrentIndex(1);
        preferenceTabButton_->setProperty("active", false);
        configTabButton_->setProperty("active", true);
        profileTabButton_->setProperty("active", false);
        preferenceTabButton_->style()->unpolish(preferenceTabButton_);
        preferenceTabButton_->style()->polish(preferenceTabButton_);
        configTabButton_->style()->unpolish(configTabButton_);
        configTabButton_->style()->polish(configTabButton_);
        profileTabButton_->style()->unpolish(profileTabButton_);
        profileTabButton_->style()->polish(profileTabButton_);
    });
    connect(saveApiSettingsButton_, &QPushButton::clicked,
            this, [this]() { saveApiSettingsFromForm(); });
    connect(testApiConnectionButton_, &QPushButton::clicked,
            this, &DashboardMainWindow::testApiConnection);
    connect(removeApiConfigurationButton_, &QPushButton::clicked,
            this, &DashboardMainWindow::removeApiConfiguration);
    connect(refreshProfileButton_, &QPushButton::clicked,
            this, &DashboardMainWindow::refreshAccountProfile);
    connect(profileUsernameCopyButton_, &QPushButton::clicked, this, [this]()
    {
        if (profileDataAvailable_)
        {
            QApplication::clipboard()->setText(profileUsernameValue_->text());
        }
    });
    connect(profileEmailCopyButton_, &QPushButton::clicked, this, [this]()
    {
        if (profileDataAvailable_)
        {
            QApplication::clipboard()->setText(profileEmail_);
        }
    });
    connect(profileDriverIdCopyButton_, &QPushButton::clicked, this, [this]()
    {
        if (profileDataAvailable_)
        {
            QApplication::clipboard()->setText(profileDriverIdValue_->text());
        }
    });
    connect(profileEmailVisibilityButton_, &QPushButton::clicked, this, [this]()
    {
        if (!profileDataAvailable_)
        {
            return;
        }
        profileEmailVisible_ = !profileEmailVisible_;
        profileEmailValue_->setText(profileEmailVisible_ ? profileEmail_ : maskedEmail(profileEmail_));
        profileEmailVisibilityButton_->setText(profileEmailVisible_ ? "Hide" : "Show");
    });
    connect(savePreferencesButton_, &QPushButton::clicked,
            this, &DashboardMainWindow::savePreferences);
    rootLayout->addWidget(pageStack_, 1);
}

void DashboardMainWindow::showDashboard()
{
    pageStack_->setCurrentIndex(0);
    headerTitle_->setText("DASHBOARD");
    updateNavigationState(dashboardNavButton_);
}

void DashboardMainWindow::showCompletedJobs()
{
    pageStack_->setCurrentIndex(1);
    headerTitle_->setText("COMPLETED JOBS");
    updateNavigationState(completedJobsNavButton_);
    refreshCompletedJobs();
}

void DashboardMainWindow::showSettings()
{
    pageStack_->setCurrentIndex(2);
    headerTitle_->setText("SETTINGS");
    updateNavigationState(settingsNavButton_);
    speedPreference_->setCurrentIndex(speedPreference_->findData(static_cast<int>(preferences_.speed)));
    volumePreference_->setCurrentIndex(volumePreference_->findData(static_cast<int>(preferences_.volume)));
    currencyPreference_->setCurrentIndex(currencyPreference_->findData(static_cast<int>(preferences_.currency)));
    distancePreference_->setCurrentIndex(distancePreference_->findData(static_cast<int>(preferences_.distance)));
}

void DashboardMainWindow::updateNavigationState(QPushButton* activeButton)
{
    const QList<QPushButton*> buttons = {
        dashboardNavButton_, completedJobsNavButton_, settingsNavButton_
    };
    for (QPushButton* button : buttons)
    {
        if (!button)
        {
            continue;
        }
        button->setProperty("active", button == activeButton);
        button->style()->unpolish(button);
        button->style()->polish(button);
    }
}

void DashboardMainWindow::savePreferences()
{
    UserPreferences pending;
    pending.speed = static_cast<SpeedUnit>(speedPreference_->currentData().toInt());
    pending.volume = static_cast<VolumeUnit>(volumePreference_->currentData().toInt());
    pending.currency = static_cast<CurrencyUnit>(currencyPreference_->currentData().toInt());
    pending.distance = static_cast<DistanceUnit>(distancePreference_->currentData().toInt());

    QString error;
    if (!isValidPreferences(pending) || !profileStore_.save(pending, &error))
    {
        const QString message = error.isEmpty()
            ? "Preferences contain an unsupported setting value."
            : error;
        statusBar()->showMessage(uppercaseDisplayText("PREFERENCES ERROR: " + message));
        return;
    }
    preferences_ = pending;
    refreshDisplayedSnapshot();
    refreshCompletedJobs();
    if (profileQualifyingDistanceKm_)
    {
        updateProfileRank(authenticatedRank_, *profileQualifyingDistanceKm_);
    }
    statusBar()->showMessage("PREFERENCES SAVED.");
}

bool DashboardMainWindow::saveApiSettingsFromForm()
{
    ApiSettings pending;
    pending.baseUrl = apiUrlInput_->text();
    pending.apiKey = apiKeyInput_->text().trimmed();
    pending.clientInstanceId = apiSettings_.clientInstanceId;
    QString normalizedUrl;
    QString error;
    if (!normalizeApiBaseUrl(pending.baseUrl, normalizedUrl, &error))
    {
        apiConfigStatus_->setText(error);
        statusBar()->showMessage("API CONFIGURATION ERROR: " + error);
        return false;
    }
    pending.baseUrl = normalizedUrl;
    const bool credentialsChanged = pending.baseUrl != apiSettings_.baseUrl ||
        pending.apiKey != apiSettings_.apiKey;
    if (!profileStore_.saveApiSettings(pending, &error))
    {
        apiConfigStatus_->setText("API settings could not be saved securely.");
        statusBar()->showMessage("API CONFIGURATION ERROR: " + error);
        return false;
    }

    apiSettings_ = pending;
    apiSettingsLoadError_.clear();
    if (credentialsChanged)
    {
        const QString message =
            "Credentials changed. Refresh the profile to authenticate this account.";
        clearAuthenticatedAccount(message, message);
    }
    apiUrlInput_->setText(apiSettings_.baseUrl);
    apiKeyInput_->setText(apiSettings_.apiKey);
    apiConfigStatus_->setText(apiSettings_.apiKey.isEmpty()
        ? "API URL saved securely. Add your driver API key to connect."
        : "API URL and key saved securely. Authenticate to refresh your account and jobs.");
    statusBar()->showMessage("API CONFIGURATION SAVED SECURELY.");
    return true;
}

void DashboardMainWindow::removeApiConfiguration()
{
    const QMessageBox::StandardButton answer = QMessageBox::warning(
        this,
        "Remove API Configuration",
        "This removes the saved API URL and key from this computer only. "
        "It does not revoke or delete the server-side key. "
        "Your local jobs, event history, preferences, and profile data will be kept. "
        "Remove the local API configuration?",
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (answer != QMessageBox::Yes)
    {
        return;
    }

    QString error;
    if (!profileStore_.removeApiCredentials(&error))
    {
        const QString message = "The local API configuration could not be removed: " + error;
        apiConfigStatus_->setText(message);
        statusBar()->showMessage(uppercaseDisplayText(message));
        QMessageBox::critical(this, "Could Not Remove API Configuration", message);
        return;
    }

    apiSettings_.baseUrl.clear();
    apiSettings_.apiKey.clear();
    apiSettingsLoadError_.clear();
    apiUrlInput_->clear();
    apiKeyInput_->clear();
    const QString message = "Not connected. Local API configuration removed; "
        "the server-side key has not been revoked.";
    clearAuthenticatedAccount(message, message);
    apiConfigStatus_->setText(message);
    statusBar()->showMessage(
        "LOCAL API CONFIGURATION REMOVED. SERVER-SIDE KEY IS UNCHANGED.");
}

void DashboardMainWindow::testApiConnection()
{
    if (apiUrlInput_->text().trimmed().isEmpty() || apiKeyInput_->text().trimmed().isEmpty())
    {
        const QString message =
            "Not connected. Configure both the API URL and driver API key before connecting.";
        clearAuthenticatedAccount(message, message);
        apiConfigStatus_->setText(message);
        statusBar()->showMessage("API URL AND DRIVER KEY ARE REQUIRED.");
        return;
    }
    if (!saveApiSettingsFromForm())
    {
        updateApiConnectionState(ApiConnectionState::LocalMode,
                                 "The configured API URL is invalid or unavailable.");
        return;
    }

    testApiConnectionButton_->setEnabled(false);
    requestAccountProfile(true, false);
}

void DashboardMainWindow::refreshAccountProfile()
{
    requestAccountProfile(false);
}

void DashboardMainWindow::updateApiConnectionState(ApiConnectionState state,
                                                   const QString& explanation,
                                                   bool transportReachable)
{
    apiOnline_ = state == ApiConnectionState::Online;
    apiTransportReachable_ = apiOnline_ || transportReachable;
    if (connectionModeLabel_)
    {
        QString text;
        QString style;
        switch (state)
        {
        case ApiConnectionState::Online:
            text = "ONLINE";
            style = "color: #1C8059; background: #E4F6EC;";
            break;
        case ApiConnectionState::Authenticating:
            text = "AUTHENTICATING";
            style = "color: #805F16; background: #FFF4D6;";
            break;
        case ApiConnectionState::Offline:
            text = "OFFLINE";
            style = "color: #A13D43; background: #FDEBEC;";
            break;
        case ApiConnectionState::LocalMode:
            text = "LOCAL MODE";
            style = "color: #A15E7D; background: #FDEBF3;";
            break;
        }
        const QString transport = apiTransportReachable_
            ? "API transport reachable."
            : "API transport unavailable or not verified.";
        const QString authentication = authenticatedDriverId_ > 0 && profileDataAvailable_
            ? "A server-verified account profile is active."
            : "No server-verified account is active.";
        connectionModeLabel_->setText(text);
        connectionModeLabel_->setToolTip(
            explanation + " " + transport + " " + authentication);
        connectionModeLabel_->setStyleSheet(
            "font: 700 10px 'Segoe UI'; " + style +
            " border-radius: 8px; padding: 3px 8px;");
    }
    if (connectionCheckTimer_)
    {
        if (state == ApiConnectionState::Online)
        {
            connectionRetryDelayMs_ = 5 * 60 * 1000;
        }
        else if (state == ApiConnectionState::Offline ||
                 state == ApiConnectionState::LocalMode)
        {
            connectionRetryDelayMs_ = (std::min)(connectionRetryDelayMs_ * 2,
                                                  15 * 60 * 1000);
        }
        connectionCheckTimer_->setInterval(connectionRetryDelayMs_);
    }
}

void DashboardMainWindow::clearAuthenticatedAccount(const QString& explanation,
                                                    const QString& profileMessage,
                                                    ApiConnectionState state,
                                                    bool transportReachable)
{
    ++accountSessionGeneration_;
    authenticatedDriverId_ = 0;
    authenticatedRole_.clear();
    authenticatedRank_.clear();
    syncDriverId_ = 0;
    connectionRequestInFlight_ = false;
    profileRequestInFlight_ = false;
    profileTaskRequestInFlight_ = false;
    profileTaskRequestAccountId_ = 0;
    ++profileTaskRequestToken_;
    syncRequestInFlight_ = false;
    syncAgainRequested_ = false;
    remoteCompletedJobs_.clear();
    remoteActiveJobs_.clear();
    fetchedServerJobs_.clear();
    pendingSyncJobs_.clear();
    if (networkManager_)
    {
        const QList<QNetworkReply*> replies = networkManager_->findChildren<QNetworkReply*>();
        for (QNetworkReply* reply : replies)
        {
            if (reply->property("nlsiAuthenticatedRequest").toBool())
            {
                reply->abort();
            }
        }
    }
    updateApiConnectionState(state, explanation, transportReachable);
    if (testApiConnectionButton_)
    {
        testApiConnectionButton_->setEnabled(true);
    }
    if (refreshProfileButton_)
    {
        refreshProfileButton_->setEnabled(true);
    }
    if (profileStatus_)
    {
        setProfileUnavailable(profileMessage);
    }
    if (statusBar())
    {
        statusBar()->showMessage(uppercaseDisplayText(profileMessage));
    }
    refreshCompletedJobs();
}

void DashboardMainWindow::setProfileUnavailable(const QString& message)
{
    profileDataAvailable_ = false;
    profileEmailVisible_ = false;
    profileEmail_.clear();
    profileQualifyingDistanceKm_.reset();
    profileAvatar_->setText("?");
    profileHeaderName_->setText(profileUnavailable);
    profileHeaderUsername_->setText(profileUnavailable);
    profileHeaderStatus_->setText(profileUnavailable);
    profileHeaderRole_->setText(profileUnavailable);
    profileUsernameValue_->setText(profileUnavailable);
    profileDisplayNameValue_->setText(profileUnavailable);
    profileEmailValue_->setText(profileUnavailable);
    profileDriverIdValue_->setText(profileUnavailable);
    profileDriverStatusValue_->setText(profileUnavailable);
    profileRoleValue_->setText(profileUnavailable);
    profileRankValue_->setText(profileUnavailable);
    profileDistanceValue_->setText(profileUnavailable);
    profileRankProgress_->setValue(0);
    profileRankProgress_->setFormat(profileUnavailable);
    setProfileTaskUnavailable(message);
    profileRemainingValue_->setText(profileUnavailable);
    profileThresholdValue_->setText("Next rank at " + profileUnavailable);
    profileUsernameCopyButton_->setEnabled(false);
    profileEmailCopyButton_->setEnabled(false);
    profileEmailVisibilityButton_->setEnabled(false);
    profileEmailVisibilityButton_->setText("Show");
    profileDriverIdCopyButton_->setEnabled(false);
    profileStatus_->setText(message);
}

void DashboardMainWindow::updateProfileRank(const QString& serverRank,
                                            double qualifyingDistanceKm)
{
    profileRankValue_->setText(serverRank);
    profileDistanceValue_->setText(formatDistance(qualifyingDistanceKm, "km", preferences_));
    const double nextThreshold = nextRankThreshold(serverRank);
    if (nextThreshold <= 0.0)
    {
        profileRankProgress_->setValue(100);
        profileRankProgress_->setFormat("MAX RANK");
        profileRemainingValue_->setText("Top rank reached");
        profileThresholdValue_->setText("Highest rank");
        return;
    }

    const double currentThreshold = rankLowerThreshold(serverRank);
    const double progress = (qualifyingDistanceKm - currentThreshold) /
        (nextThreshold - currentThreshold) * 100.0;
    profileRankProgress_->setValue(static_cast<int>(std::round(
        (std::max)(0.0, (std::min)(progress, 100.0)))));
    profileRankProgress_->setFormat("%p%");
    profileRemainingValue_->setText(
        formatDistance(nextThreshold - qualifyingDistanceKm, "km", preferences_) +
        " to next rank");
    profileThresholdValue_->setText(
        "Next rank at " + formatDistance(nextThreshold, "km", preferences_));
}

void DashboardMainWindow::setProfileTaskUnavailable(
    const QString& explanation,
    const QString& progressFormat)
{
    if (profileTaskProgress_)
    {
        profileTaskProgress_->setValue(0);
        profileTaskProgress_->setFormat(progressFormat);
        profileTaskProgress_->setEnabled(false);
    }
    if (profileTaskStatus_)
    {
        profileTaskStatus_->setText(explanation);
    }
    if (profileTaskHistory_)
    {
        profileTaskHistory_->clear();
        profileTaskHistory_->hide();
    }
    if (profileTaskHistoryTitle_)
    {
        profileTaskHistoryTitle_->hide();
    }
}

bool DashboardMainWindow::updateProfileTask(const QJsonObject& payload)
{
    const auto validTask = [](const QJsonObject& task)
    {
        const QDate month = QDate::fromString(task.value("task_month").toString(),
                                              Qt::ISODate);
        const double target = task.value("target_distance_km").toDouble(
            std::numeric_limits<double>::quiet_NaN());
        const double completed = task.value("completed_distance_km").toDouble(
            std::numeric_limits<double>::quiet_NaN());
        const double progress = task.value("progress_percentage").toDouble(
            std::numeric_limits<double>::quiet_NaN());
        const QString status = task.value("status").toString();
        const QJsonValue completedAt = task.value("completed_at_utc");
        const QJsonValue finalizedAt = task.value("period_finalized_at_utc");
        return month.isValid() &&
            task.value("month_label").isString() &&
            std::isfinite(target) && target > 0.0 &&
            std::isfinite(completed) && completed >= 0.0 &&
            std::isfinite(progress) && progress >= 0.0 && progress <= 100.0 &&
            (status == "in_progress" || status == "completed" || status == "period_ended") &&
            task.value("is_complete").isBool() &&
            task.value("is_complete").toBool() == (status == "completed") &&
            (completedAt.isNull() || completedAt.isString()) &&
            (finalizedAt.isNull() || finalizedAt.isString());
    };

    const QJsonObject currentPeriod = payload.value("current_period").toObject();
    const QString monthStart = currentPeriod.value("month_start").toString();
    const QString monthLabel = currentPeriod.value("month_label").toString();
    if (!QDate::fromString(monthStart, Qt::ISODate).isValid() ||
        monthLabel.isEmpty() || !payload.value("history").isArray() ||
        !payload.contains("task"))
    {
        return false;
    }

    const QJsonValue currentTaskValue = payload.value("task");
    QJsonObject currentTask;
    if (!currentTaskValue.isNull())
    {
        if (!currentTaskValue.isObject())
        {
            return false;
        }
        currentTask = currentTaskValue.toObject();
        if (!validTask(currentTask) ||
            currentTask.value("task_month").toString() != monthStart)
        {
            return false;
        }
    }

    QStringList historyLines;
    const QJsonArray history = payload.value("history").toArray();
    for (const QJsonValue& value : history)
    {
        if (!value.isObject() || !validTask(value.toObject()))
        {
            return false;
        }
        const QJsonObject task = value.toObject();
        if (task.value("task_month").toString() == monthStart)
        {
            continue;
        }
        const QString status = task.value("status").toString();
        const QString statusText = status == "completed"
            ? "Completed"
            : (status == "period_ended" ? "Period ended" : "In progress");
        historyLines.append(
            QString("%1 | %2 / %3 | %4")
                .arg(task.value("month_label").toString(),
                     formatDistance(task.value("completed_distance_km").toDouble(),
                                    "km", preferences_),
                     formatDistance(task.value("target_distance_km").toDouble(),
                                    "km", preferences_),
                     statusText));
    }

    if (currentTask.isEmpty())
    {
        profileTaskProgress_->setValue(0);
        profileTaskProgress_->setFormat("NO TASK RECORD");
        profileTaskProgress_->setEnabled(false);
        profileTaskStatus_->setText(
            "No task record is initialized for " + monthLabel +
            (historyLines.isEmpty()
                 ? ". No earlier monthly task records are available."
                 : ". Earlier monthly task records are listed below."));
    }
    else
    {
        const double completed = currentTask.value("completed_distance_km").toDouble();
        const double target = currentTask.value("target_distance_km").toDouble();
        const int percentage = static_cast<int>(std::round(
            currentTask.value("progress_percentage").toDouble()));
        const QString status = currentTask.value("status").toString();
        const QString statusText = status == "completed"
            ? "Completed"
            : (status == "period_ended" ? "Period ended" : "In progress");
        profileTaskProgress_->setValue((std::max)(0, (std::min)(percentage, 100)));
        profileTaskProgress_->setFormat(
            QString("TASK %p% | %1 / %2")
                .arg(formatDistance(completed, "km", preferences_),
                     formatDistance(target, "km", preferences_)));
        profileTaskProgress_->setEnabled(true);
        QString statusMessage = monthLabel + " | " + statusText + " | " +
            formatDistance(completed, "km", preferences_) + " completed of " +
            formatDistance(target, "km", preferences_);
        const QString completedAt = currentTask.value("completed_at_utc").toString();
        if (status == "completed" && !completedAt.isEmpty())
        {
            statusMessage += " | Completed at " + completedAt + " UTC";
        }
        profileTaskStatus_->setText(statusMessage);
    }

    profileTaskHistory_->setText(historyLines.join('\n'));
    profileTaskHistory_->setVisible(!historyLines.isEmpty());
    profileTaskHistoryTitle_->setVisible(!historyLines.isEmpty());
    return true;
}

void DashboardMainWindow::requestAccountMonthlyTask(qint64 accountId)
{
    if (accountId <= 0 || accountId != authenticatedDriverId_ ||
        (profileTaskRequestInFlight_ &&
         profileTaskRequestAccountId_ == accountId) || !networkManager_)
    {
        return;
    }
    const QUrl url = apiEndpointUrl(apiSettings_.baseUrl, "monthly-task.php");
    if (!url.isValid())
    {
        setProfileTaskUnavailable("The configured task API URL is invalid.");
        return;
    }

    profileTaskRequestInFlight_ = true;
    profileTaskRequestAccountId_ = accountId;
    setProfileTaskUnavailable("Loading monthly task data...", "LOADING TASK DATA");
    const quint64 requestGeneration = accountSessionGeneration_;
    const quint64 requestToken = ++profileTaskRequestToken_;
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization",
        "Bearer " + apiSettings_.apiKey.trimmed().toUtf8());
    addNgrokWarningBypassHeader(request);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    QNetworkReply* reply = networkManager_->get(request);
    reply->setProperty("nlsiAuthenticatedRequest", true);
    QTimer::singleShot(15000, reply, [reply]()
    {
        if (reply->isRunning())
        {
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, accountId, requestGeneration, requestToken]()
    {
        if (requestGeneration != accountSessionGeneration_ ||
            accountId != authenticatedDriverId_)
        {
            if (profileTaskRequestToken_ == requestToken)
            {
                profileTaskRequestInFlight_ = false;
                profileTaskRequestAccountId_ = 0;
            }
            reply->deleteLater();
            return;
        }
        if (profileTaskRequestToken_ != requestToken)
        {
            reply->deleteLater();
            return;
        }
        profileTaskRequestInFlight_ = false;
        profileTaskRequestAccountId_ = 0;
        const int statusCode =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool jsonContentType = hasJsonContentType(reply);
        const QByteArray responseBytes = reply->readAll();
        QJsonParseError parseError;
        const QJsonDocument response = QJsonDocument::fromJson(responseBytes, &parseError);
        const QJsonObject payload = response.isObject() ? response.object() : QJsonObject();
        const double responseDriverId = payload.value("driver_id").toDouble(-1.0);
        const bool belongsToAccount = std::isfinite(responseDriverId) &&
            responseDriverId == static_cast<double>(accountId);
        const bool responseValid = reply->error() == QNetworkReply::NoError &&
            statusCode == 200 && jsonContentType &&
            parseError.error == QJsonParseError::NoError && response.isObject() &&
            payload.value("success").toBool(false) && belongsToAccount;

        if (!responseValid || !updateProfileTask(payload))
        {
            if (statusCode == 401 || statusCode == 403)
            {
                setProfileTaskUnavailable(
                    "The task API did not authorize this account. Refresh the profile to revalidate it.");
            }
            else if (reply->error() != QNetworkReply::NoError || statusCode >= 500)
            {
                setProfileTaskUnavailable(
                    "Monthly task data is currently unavailable. No offline task progress is cached.");
            }
            else
            {
                setProfileTaskUnavailable(
                    "The API returned an invalid monthly task response for this account.");
            }
        }
        reply->deleteLater();
    });
}

void DashboardMainWindow::requestCurrencyExchangeRate()
{
    if (exchangeRateRequestInFlight_)
    {
        return;
    }
    exchangeRateRequestInFlight_ = true;

    QUrl url("https://api.frankfurter.dev/v1/latest");
    QUrlQuery query;
    query.addQueryItem("base", "EUR");
    query.addQueryItem("symbols", "USD");
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    QNetworkReply* reply = networkManager_->get(request);
    QTimer::singleShot(10000, reply, [reply]()
    {
        if (reply->isRunning())
        {
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
        const QJsonObject payload = document.isObject() ? document.object() : QJsonObject();
        const QDate rateDate = QDate::fromString(payload.value("date").toString(), Qt::ISODate);
        const QJsonObject rates = payload.value("rates").toObject();
        const double rate = rates.value("USD").toDouble(std::numeric_limits<double>::quiet_NaN());
        const qint64 ageDays = rateDate.daysTo(QDate::currentDate());
        const bool valid = reply->error() == QNetworkReply::NoError &&
            statusCode == 200 && parseError.error == QJsonParseError::NoError &&
            payload.value("base").toString() == "EUR" && rateDate.isValid() &&
            ageDays >= 0 && ageDays <= 10 && std::isfinite(rate) && rate > 0.0;

        if (valid)
        {
            eurUsdRate_ = rate;
        }
        else
        {
            eurUsdRate_.reset();
            qWarning().noquote() << "NLSI EUR/USD exchange rate unavailable:"
                                 << statusCode << reply->errorString();
        }
        exchangeRateRequestInFlight_ = false;
        refreshDisplayedSnapshot();
        refreshCompletedJobs();
        if (statusBar())
        {
            statusBar()->showMessage(valid
                ? QString("EUR/USD rate updated from Frankfurter (%1).").arg(rateDate.toString(Qt::ISODate))
                : "EUR/USD rate unavailable; foreign-currency income is not converted.");
        }
        reply->deleteLater();
        QTimer::singleShot(6 * 60 * 60 * 1000, this,
                           &DashboardMainWindow::requestCurrencyExchangeRate);
    });
}

void DashboardMainWindow::requestAccountProfile(bool connectionTest, bool synchronizeJobs,
                                                bool automaticCheck)
{
    if (connectionRequestInFlight_ || profileRequestInFlight_ || syncRequestInFlight_)
    {
        return;
    }
    if (apiSettings_.baseUrl.trimmed().isEmpty() || apiSettings_.apiKey.trimmed().isEmpty())
    {
        const QString message = "Not connected. Configure your API URL and key in Settings to connect.";
        clearAuthenticatedAccount(message, message);
        if (connectionTest)
        {
            apiConfigStatus_->setText(message);
        }
        return;
    }

    QString normalizedBaseUrl;
    QString urlError;
    if (!normalizeApiBaseUrl(apiSettings_.baseUrl, normalizedBaseUrl, &urlError))
    {
        clearAuthenticatedAccount("The configured API URL is invalid.",
            "The saved API URL is invalid. Update it in Settings > Config.");
        if (connectionTest)
        {
            apiConfigStatus_->setText(urlError);
        }
        return;
    }
    const QUrl url = apiEndpointUrl(apiSettings_.baseUrl, "driver-key.php");
    if (!url.isValid())
    {
        clearAuthenticatedAccount("The configured API endpoint is invalid.",
            "Connection error. Check the API URL in Settings > Config.");
        if (connectionTest)
        {
            apiConfigStatus_->setText("The saved API link is invalid. Update it in Settings > Config.");
        }
        return;
    }

    updateApiConnectionState(ApiConnectionState::Authenticating,
        "Contacting the API and validating the server-issued account profile.");
    if (profileStatus_)
    {
        profileStatus_->setText(profileDataAvailable_
            ? "Revalidating the server-confirmed account..."
            : "Checking the configured account...");
    }
    const quint64 requestGeneration = accountSessionGeneration_;
    if (connectionTest)
    {
        connectionRequestInFlight_ = true;
        apiConfigStatus_->setText("Checking API and driver key...");
    }
    else
    {
        profileRequestInFlight_ = true;
        if (!automaticCheck)
        {
            profileStatus_->setText("Checking the configured account...");
            refreshProfileButton_->setEnabled(false);
        }
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json");
    addNgrokWarningBypassHeader(request);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    const QByteArray body = QJsonDocument(QJsonObject{
        {QStringLiteral("key"), apiSettings_.apiKey.trimmed()}
    }).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = networkManager_->post(request, body);
    reply->setProperty("nlsiAuthenticatedRequest", true);
    QTimer::singleShot(15000, reply, [reply]()
    {
        if (reply->isRunning())
        {
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, connectionTest, synchronizeJobs, automaticCheck, requestGeneration]()
    {
        if (requestGeneration != accountSessionGeneration_)
        {
            reply->deleteLater();
            return;
        }
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool jsonContentType = hasJsonContentType(reply);
        const QByteArray responseBytes = reply->readAll();
        QJsonParseError parseError;
        const QJsonDocument response = QJsonDocument::fromJson(responseBytes, &parseError);
        const QJsonObject payload = response.isObject() ? response.object() : QJsonObject();
        const QJsonObject driver = payload.value("driver").toObject();
        const QString role = driver.value("role").toString();
        const QString rank = driver.value("rank").toString();
        const QString username = driver.value("username").toString().trimmed();
        const QString displayName = driver.value("display_name").toString().trimmed();
        const QJsonValue emailValue = driver.value("email");
        const bool emailValid = emailValue.isString();
        const QString email = emailValue.toString().trimmed();
        const QJsonValue qualifyingDistance = driver.value("qualifying_distance_km");
        const double qualifyingDistanceKm = qualifyingDistance.toDouble(
            std::numeric_limits<double>::quiet_NaN());
        const double driverIdNumber = driver.value("driver_id").toDouble(-1);
        const bool rankValid = qualifyingDistance.isDouble() &&
            std::isfinite(qualifyingDistanceKm) && qualifyingDistanceKm >= 0.0 &&
            driver.value("rank").isString() && isSupportedDriverRank(rank);
        const bool authenticated = reply->error() == QNetworkReply::NoError &&
            statusCode == 200 &&
            jsonContentType &&
            parseError.error == QJsonParseError::NoError &&
            response.isObject() && payload.value("success").toBool(false) &&
            payload.value("valid").toBool(false) &&
            driver.value("username").isString() && !username.isEmpty() &&
            emailValid &&
            !email.isEmpty() && email.contains('@') &&
            driver.value("display_name").isString() && !displayName.isEmpty() &&
            isSupportedAccountRole(role) &&
            driver.value("status").toString() == "active" &&
            driver.value("driver_id").isDouble() &&
            std::isfinite(driverIdNumber) && driverIdNumber > 0 &&
            driverIdNumber < static_cast<double>((std::numeric_limits<qint64>::max)()) &&
            std::floor(driverIdNumber) == driverIdNumber && rankValid;

        QString message;
        if (authenticated)
        {
            const qint64 accountId = static_cast<qint64>(driverIdNumber);
            if (authenticatedDriverId_ != accountId)
            {
                remoteCompletedJobs_.clear();
                remoteActiveJobs_.clear();
                refreshCompletedJobs();
            }
            authenticatedDriverId_ = accountId;
            authenticatedRole_ = role;
            authenticatedRank_ = rank;
            profileDataAvailable_ = true;
            profileQualifyingDistanceKm_ = qualifyingDistanceKm;
            profileUsernameValue_->setText(username);
            profileDisplayNameValue_->setText(displayName);
            profileEmail_ = email;
            profileEmailVisible_ = false;
            profileEmailValue_->setText(email.isEmpty() ? unavailable : maskedEmail(email));
            profileDriverIdValue_->setText(QString::number(accountId));
            profileDriverStatusValue_->setText("active");
            profileRoleValue_->setText(role);
            profileAvatar_->setText(displayName.left(1).toUpper());
            profileHeaderName_->setText(displayName);
            profileHeaderUsername_->setText("@" + username);
            profileHeaderStatus_->setText("Active");
            profileHeaderRole_->setText(role);
            profileUsernameCopyButton_->setEnabled(true);
            profileEmailCopyButton_->setEnabled(!email.isEmpty());
            profileEmailVisibilityButton_->setEnabled(!email.isEmpty());
            profileDriverIdCopyButton_->setEnabled(true);
            updateProfileRank(rank, qualifyingDistanceKm);
            requestAccountMonthlyTask(accountId);
            updateApiConnectionState(ApiConnectionState::Online,
                "The configured API is reachable and the driver key authenticated successfully.");
            profileStatus_->setText("Profile confirmed by the server.");
            if (connectionTest)
            {
                message = "API connection verified and driver key accepted.";
                apiConfigStatus_->setText(message);
                statusBar()->showMessage("API CONNECTION VERIFIED.");
                testApiConnectionButton_->setEnabled(true);
            }
            else
            {
                profileStatus_->setText(automaticCheck
                    ? "Online. Profile and rank are confirmed by the server."
                    : "Profile refreshed from the authenticated account.");
            }
            if (synchronizeJobs)
            {
                synchronizeAccountJobs(accountId);
            }
        }
        else if (statusCode == 401 || statusCode == 400)
        {
            message = "API key is invalid or revoked. Update your credentials in Settings > Config.";
        }
        else if (statusCode == 403)
        {
            message = "This driver account is inactive. Contact the NLSI VTC administrator.";
        }
        else if (statusCode == 429)
        {
            message = "Too many requests. Wait a moment, then try again.";
        }
        else if (statusCode >= 500)
        {
            message = "The API is temporarily unavailable. Local telemetry remains available.";
        }
        else
        {
            if (reply->error() != QNetworkReply::NoError)
            {
                message = "Could not reach the configured API. Local telemetry and jobs remain available.";
            }
            else if (statusCode == 200 && parseError.error != QJsonParseError::NoError)
            {
                message = "The API returned a malformed profile response. Local data remains available.";
            }
            else if (statusCode == 200 && !jsonContentType)
            {
                message = "The API returned an unexpected content type. Local data remains available.";
            }
            else if (statusCode == 200)
            {
                message = "The API did not return a valid authenticated profile and rank. "
                    "Local data remains available.";
            }
            else
            {
                message = "Could not confirm the configured API account. Check your network and API URL.";
            }
        }

        if (!authenticated)
        {
            logApiResponseFailure(reply, responseBytes, apiSettings_.baseUrl,
                apiSettings_.apiKey, "HTTP 200 application/json object "
                "{success:true, valid:true, driver:{driver_id, username, email, "
                "display_name, status, role, rank, qualifying_distance_km}}");
            const bool transportReachable = statusCode > 0;
            const ApiConnectionState failedState =
                statusCode == 0 || statusCode >= 500
                    ? ApiConnectionState::Offline
                    : ApiConnectionState::LocalMode;
            const bool keepPreviouslyVerifiedProfile =
                authenticatedDriverId_ > 0 && profileDataAvailable_ &&
                (statusCode == 0 || statusCode == 429 || statusCode >= 500);
            if (keepPreviouslyVerifiedProfile)
            {
                updateApiConnectionState(failedState, message, transportReachable);
                profileStatus_->setText(
                    "The server-confirmed profile is retained; revalidation is temporarily unavailable.");
                if (statusBar())
                {
                    statusBar()->showMessage(
                        "ACCOUNT PROFILE LAST VERIFIED; API REVALIDATION UNAVAILABLE.");
                }
            }
            else
            {
                clearAuthenticatedAccount(message, message, failedState, transportReachable);
            }
            if (connectionTest)
            {
                apiConfigStatus_->setText(message);
            }
        }
        if (connectionTest)
        {
            connectionRequestInFlight_ = false;
        }
        else
        {
            profileRequestInFlight_ = false;
            refreshProfileButton_->setEnabled(true);
        }
        reply->deleteLater();
    });
}

void DashboardMainWindow::synchronizeAccountJobs(qint64 accountId)
{
    if (!apiOnline_ || accountId <= 0 || accountId != authenticatedDriverId_ ||
        apiSettings_.baseUrl.trimmed().isEmpty() ||
        apiSettings_.apiKey.trimmed().isEmpty() ||
        apiSettings_.clientInstanceId.isEmpty())
    {
        return;
    }
    if (syncRequestInFlight_)
    {
        syncAgainRequested_ = true;
        return;
    }
    syncAgainRequested_ = false;
    syncDriverId_ = accountId;
    syncRequestInFlight_ = true;
    syncUploadHadFailure_ = false;
    syncUploadOffset_ = 0;
    pendingSyncJobs_.clear();
    fetchedServerJobs_.clear();
    fetchServerJobs(accountId, 0, false);
}

void DashboardMainWindow::fetchServerJobs(qint64 accountId, int offset, bool afterUpload)
{
    if (!apiOnline_ || accountId != authenticatedDriverId_ || accountId != syncDriverId_)
    {
        return;
    }
    QUrl url = apiEndpointUrl(apiSettings_.baseUrl, "job-sync.php");
    if (!url.isValid())
    {
        const QString message =
            "LOCAL MODE. The server job endpoint is invalid; local jobs remain available.";
        clearAuthenticatedAccount(message, message);
        return;
    }
    QUrlQuery query;
    query.addQueryItem("limit", "100");
    query.addQueryItem("offset", QString::number(offset));
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");
    addNgrokWarningBypassHeader(request);
    request.setRawHeader("Authorization", "Bearer " + apiSettings_.apiKey.trimmed().toUtf8());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    QNetworkReply* reply = networkManager_->get(request);
    reply->setProperty("nlsiAuthenticatedRequest", true);
    QTimer::singleShot(20000, reply, [reply]()
    {
        if (reply->isRunning())
        {
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, accountId, offset, afterUpload]()
    {
        const QByteArray responseBytes = reply->readAll();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool jsonContentType = hasJsonContentType(reply);
        if (!apiOnline_ || accountId != authenticatedDriverId_ || accountId != syncDriverId_)
        {
            reply->deleteLater();
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(responseBytes, &parseError);
        if (reply->error() != QNetworkReply::NoError ||
            statusCode != 200 || !jsonContentType ||
            parseError.error != QJsonParseError::NoError ||
            !document.isObject() || !document.object().value("success").toBool() ||
            !document.object().value("driver_id").isDouble() ||
            document.object().value("driver_id").toDouble(-1.0) !=
                static_cast<double>(accountId) ||
            !document.object().value("records").isArray() ||
            !document.object().value("pagination").isObject())
        {
            logApiResponseFailure(reply, responseBytes, apiSettings_.baseUrl,
                apiSettings_.apiKey,
                "HTTP 200 application/json object {success:true, driver_id:number, "
                "records:array, pagination:{limit, offset, total, next_offset}}");
            const QString failure = reply->error() != QNetworkReply::NoError
                ? "network or timeout error"
                : statusCode != 200
                    ? QString("HTTP %1").arg(statusCode)
                    : !jsonContentType
                        ? "unexpected response content type"
                    : parseError.error != QJsonParseError::NoError
                        ? "invalid JSON response"
                        : "server query or response validation failed";
            reply->deleteLater();
            const QString message = "LOCAL MODE. The server could not confirm this account "
                "while refreshing jobs (" + failure + "). Local jobs remain available.";
            const ApiConnectionState failedState =
                statusCode == 0 || statusCode >= 500
                    ? ApiConnectionState::Offline
                    : ApiConnectionState::LocalMode;
            handleJobSyncFailure(accountId, message, failedState, statusCode > 0,
                statusCode == 401 || statusCode == 403);
            return;
        }

        const QJsonObject response = document.object();
        const QJsonArray records = response.value("records").toArray();
        const QJsonObject pagination = response.value("pagination").toObject();
        const QJsonValue totalValue = pagination.value("total");
        const QJsonValue limitValue = pagination.value("limit");
        const QJsonValue offsetValue = pagination.value("offset");
        const double total = totalValue.toDouble(-1.0);
        const qint64 pageEnd = static_cast<qint64>(offset) + records.size();
        if (!totalValue.isDouble() || !std::isfinite(total) ||
            total < 0.0 || std::floor(total) != total ||
            !limitValue.isDouble() || limitValue.toInt(-1) != 100 ||
            !offsetValue.isDouble() || offsetValue.toInt(-1) != offset ||
            total < static_cast<double>(pageEnd))
        {
            reply->deleteLater();
            const QString message =
                "LOCAL MODE. The server returned invalid job pagination data. "
                "Local jobs remain available.";
            handleJobSyncFailure(accountId, message, ApiConnectionState::LocalMode, true, false);
            return;
        }
        for (const QJsonValue& value : records)
        {
            JobRecord record;
            if (!value.isObject() || !remoteJobFromJson(value.toObject(), record))
            {
                reply->deleteLater();
                const QString message =
                    "LOCAL MODE. The server returned an invalid job record. "
                    "Local jobs remain available.";
                handleJobSyncFailure(accountId, message, ApiConnectionState::LocalMode, true, false);
                return;
            }
            record.accountId = accountId;
            fetchedServerJobs_.push_back(std::move(record));
        }
        const QJsonValue nextOffset = pagination.value("next_offset");
        if (!nextOffset.isNull() && !nextOffset.isUndefined())
        {
            if (!nextOffset.isDouble() ||
                nextOffset.toInt(-1) != pageEnd ||
                nextOffset.toInt(-1) >= total)
            {
                reply->deleteLater();
                const QString message =
                    "LOCAL MODE. The server returned invalid job pagination data. "
                    "Local jobs remain available.";
                handleJobSyncFailure(accountId, message, ApiConnectionState::LocalMode, true, false);
                return;
            }
            const int next = nextOffset.toInt();
            reply->deleteLater();
            QTimer::singleShot(4100, this, [this, accountId, next, afterUpload]()
            {
                fetchServerJobs(accountId, next, afterUpload);
            });
            return;
        }
        if (static_cast<double>(pageEnd) < total)
        {
            reply->deleteLater();
            const QString message =
                "LOCAL MODE. The server returned an incomplete job list. "
                "Local jobs remain available.";
            handleJobSyncFailure(accountId, message, ApiConnectionState::LocalMode, true, false);
            return;
        }

        remoteCompletedJobs_.clear();
        remoteActiveJobs_.clear();
        for (const JobRecord& serverRecord : fetchedServerJobs_)
        {
            if (!serverRecord.localJobId.empty() &&
                serverRecord.clientInstanceId == apiSettings_.clientInstanceId.toStdString())
            {
                QString reconcileError;
                if (!jobHistory_.reconcileServerRecord(serverRecord, &reconcileError))
                {
                    syncUploadHadFailure_ = true;
                    qWarning().noquote() << "NLSI server job reconciliation failed:"
                                         << reconcileError;
                }
            }
            if (serverRecord.status == JobStatus::Active)
            {
                remoteActiveJobs_.push_back(serverRecord);
            }
            else
            {
                remoteCompletedJobs_.push_back(serverRecord);
            }
        }
        fetchedServerJobs_.clear();
        refreshCompletedJobs();
        refreshDisplayedSnapshot();
        reply->deleteLater();
        if (afterUpload)
        {
            const QString message = syncUploadHadFailure_
                    ? "Job upload failed for some records; failed jobs remain saved locally "
                        "for retry and the server list was refreshed."
                    : "Server jobs refreshed; acknowledged local jobs are synchronized.";
            finishJobSynchronization(accountId, message,
                                     !syncUploadHadFailure_);
        }
        else
        {
            uploadLocalJobs(accountId);
        }
    });
}

void DashboardMainWindow::uploadLocalJobs(qint64 accountId)
{
    if (!apiOnline_ ||
        !canSynchronizeAccountJobs(authenticatedRole_, authenticatedDriverId_, accountId) ||
        accountId != syncDriverId_)
    {
        return;
    }
    QString loadError;
    std::vector<JobRecord> records = jobHistory_.completedJobs(&loadError);
    if (!loadError.isEmpty())
    {
        qWarning().noquote() << "NLSI local jobs could not all be loaded:" << loadError;
        syncUploadHadFailure_ = true;
    }
    if (jobHistory_.activeJob() &&
        jobHistory_.activeJob()->status == JobStatus::Active)
    {
        records.push_back(*jobHistory_.activeJob());
    }

    pendingSyncJobs_.clear();
    for (JobRecord record : records)
    {
        if (record.status != JobStatus::Delivered && record.status != JobStatus::Cancelled)
        {
            continue;
        }
        QString stateError;
        const std::optional<JobSyncState> state = jobHistory_.syncState(record.id, &stateError);
        if (!stateError.isEmpty())
        {
            qWarning().noquote() << "NLSI job synchronization state:" << stateError;
            syncUploadHadFailure_ = true;
            continue;
        }
        if (!shouldUploadLocalJob(accountId, state))
        {
            continue;
        }
        if (!jobHistory_.claimForAccount(record.id, accountId, &stateError))
        {
            if (stateError.contains("another account"))
            {
                continue;
            }
            qWarning().noquote() << "NLSI job ownership state:" << stateError;
            syncUploadHadFailure_ = true;
            continue;
        }
        if (state && state->status == "synced" && record.status != JobStatus::Active)
        {
            continue;
        }
        JobSyncState pending;
        pending.accountId = accountId;
        pending.status = "pending";
        pending.serverJobId = state ? state->serverJobId : QString();
        if (!jobHistory_.setSyncState(record.id, pending, &stateError))
        {
            qWarning().noquote() << "NLSI job synchronization state:" << stateError;
            syncUploadHadFailure_ = true;
            continue;
        }
        record.serverJobId = pending.serverJobId.toStdString();
        pendingSyncJobs_.push_back(std::move(record));
    }
    syncUploadOffset_ = 0;
    if (pendingSyncJobs_.empty())
    {
        if (syncUploadHadFailure_)
        {
            finishJobSynchronization(accountId,
                "Job upload could not start because local job state could not be read; "
                "affected jobs remain saved for retry.",
                false);
        }
        else
        {
            finishJobSynchronization(accountId, "Server jobs refreshed; no local jobs are pending.");
        }
        return;
    }
    uploadNextJobBatch(accountId);
}

void DashboardMainWindow::uploadNextJobBatch(qint64 accountId)
{
    if (!apiOnline_ || accountId != authenticatedDriverId_ || accountId != syncDriverId_)
    {
        return;
    }
    if (syncUploadOffset_ >= pendingSyncJobs_.size())
    {
        fetchedServerJobs_.clear();
        fetchServerJobs(accountId, 0, true);
        return;
    }

    const std::size_t end = (std::min)(syncUploadOffset_ + 100, pendingSyncJobs_.size());
    QJsonArray outgoing;
    std::vector<JobRecord> batch;
    for (std::size_t index = syncUploadOffset_; index < end; ++index)
    {
        batch.push_back(pendingSyncJobs_[index]);
        outgoing.push_back(syncRecordToJson(pendingSyncJobs_[index]));
    }
    const QJsonObject body{
        {"client_instance_id", apiSettings_.clientInstanceId},
        {"records", outgoing}
    };
    const QUrl url = apiEndpointUrl(apiSettings_.baseUrl, "job-sync.php");
    if (!url.isValid())
    {
        const QString message =
            "LOCAL MODE. The server job endpoint is invalid; local jobs remain available.";
        clearAuthenticatedAccount(message, message);
        return;
    }
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json");
    addNgrokWarningBypassHeader(request);
    request.setRawHeader("Authorization", "Bearer " + apiSettings_.apiKey.trimmed().toUtf8());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    QNetworkReply* reply = networkManager_->post(
        request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    reply->setProperty("nlsiAuthenticatedRequest", true);
    QTimer::singleShot(20000, reply, [reply]()
    {
        if (reply->isRunning())
        {
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, accountId, end, batch]()
    {
        const QByteArray responseBytes = reply->readAll();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool jsonContentType = hasJsonContentType(reply);
        if (!apiOnline_ || accountId != authenticatedDriverId_ || accountId != syncDriverId_)
        {
            reply->deleteLater();
            return;
        }
        const auto serverIds = validateJobSyncResponse(
            statusCode, reply->error() == QNetworkReply::NoError, jsonContentType,
            responseBytes, accountId, batch);
        const bool validAcknowledgement = serverIds.has_value();
        for (const JobRecord& record : batch)
        {
            const QString failureCode = statusCode == 401 || statusCode == 403
                ? "authentication_failed"
                : statusCode == 429 ? "rate_limited" : "upload_failed";
            const QString acknowledgedServerId = validAcknowledgement
                ? serverIds->at(record.id)
                : QString();
            const JobSyncState state = makeJobSyncStateAfterUpload(
                accountId, QString::fromStdString(record.serverJobId),
                acknowledgedServerId, validAcknowledgement ? QString() : failureCode);
            if (!validAcknowledgement)
            {
                syncUploadHadFailure_ = true;
            }
            QString stateError;
            if (!jobHistory_.setSyncState(record.id, state, &stateError))
            {
                qWarning().noquote() << "NLSI job synchronization state:" << stateError;
                syncUploadHadFailure_ = true;
            }
        }
        if (!validAcknowledgement)
        {
            logApiResponseFailure(reply, responseBytes, apiSettings_.baseUrl,
                apiSettings_.apiKey,
                "HTTP 200 application/json object {success:true, committed:true, "
                "driver_id:number, acknowledgements:[{local_job_id, job_id, acknowledged:true}]}");
            qWarning().noquote() << "NLSI job synchronization request failed:"
                                 << statusCode << reply->errorString();
            const QString message =
                "LOCAL MODE. The server did not confirm successful job persistence. "
                "Local jobs remain available for retry.";
            const ApiConnectionState failedState =
                statusCode == 0 || statusCode >= 500
                    ? ApiConnectionState::Offline
                    : ApiConnectionState::LocalMode;
            handleJobSyncFailure(accountId, message, failedState, statusCode > 0,
                statusCode == 401 || statusCode == 403);
            reply->deleteLater();
            return;
        }
        syncUploadOffset_ = end;
        reply->deleteLater();
        QTimer::singleShot(4100, this, [this, accountId]()
        {
            uploadNextJobBatch(accountId);
        });
    });
}

void DashboardMainWindow::handleJobSyncFailure(qint64 accountId, const QString& message,
                                               ApiConnectionState state,
                                               bool transportReachable,
                                               bool authenticationFailure)
{
    if (accountId != authenticatedDriverId_ || accountId != syncDriverId_)
    {
        return;
    }
    if (authenticationFailure)
    {
        clearAuthenticatedAccount(message, message, state, transportReachable);
        return;
    }

    syncRequestInFlight_ = false;
    syncDriverId_ = 0;
    syncAgainRequested_ = false;
    fetchedServerJobs_.clear();
    pendingSyncJobs_.clear();
    updateApiConnectionState(state, message, transportReachable);
    if (profileDataAvailable_)
    {
        profileStatus_->setText(
            "Account profile remains server-verified; job synchronization is unavailable.");
    }
    if (statusBar())
    {
        statusBar()->showMessage(uppercaseDisplayText(
            "ACCOUNT AUTHENTICATED; JOB SYNCHRONIZATION UNAVAILABLE: " + message));
    }
    refreshCompletedJobs();
}

void DashboardMainWindow::finishJobSynchronization(qint64 accountId, const QString& message,
                                                    bool successful)
{
    if (accountId != syncDriverId_)
    {
        return;
    }
    syncRequestInFlight_ = false;
    if (successful)
    {
        syncRetryDelayMs_ = 60000;
    }
    else
    {
        syncRetryDelayMs_ = (std::min)(syncRetryDelayMs_ * 2, 10 * 60 * 1000);
    }
    if (jobSyncTimer_)
    {
        jobSyncTimer_->setInterval(syncRetryDelayMs_);
    }
    profileStatus_->setText(message);
    if (statusBar())
    {
        statusBar()->showMessage(uppercaseDisplayText(message));
    }
    refreshCompletedJobs();
    const bool rerun = syncAgainRequested_;
    syncAgainRequested_ = false;
    if (rerun && authenticatedDriverId_ > 0)
    {
        QTimer::singleShot(0, this, [this]()
        {
            synchronizeAccountJobs(authenticatedDriverId_);
        });
    }
}

void DashboardMainWindow::refreshCompletedJobs()
{
    while (completedJobsLayout_ && completedJobsLayout_->count() > 0)
    {
        QLayoutItem* item = completedJobsLayout_->takeAt(0);
        if (item->widget())
        {
            delete item->widget();
        }
        delete item;
    }

    QString loadError;
    std::vector<JobRecord> records = jobHistory_.completedJobs(&loadError);
    if (!loadError.isEmpty())
    {
        qWarning().noquote() << "NLSI job history:" << loadError;
        if (statusBar())
        {
            statusBar()->showMessage(uppercaseDisplayText("JOB HISTORY WARNING: " + loadError));
        }
    }

    for (JobRecord& record : records)
    {
        record.localJobId = record.id;
        QString stateError;
        const std::optional<JobSyncState> state = jobHistory_.syncState(record.id, &stateError);
        if (!stateError.isEmpty())
        {
            record.syncStatus = "FAILED (LOCAL STATE ERROR)";
            qWarning().noquote() << "NLSI job synchronization state:" << stateError;
        }
        else if (state && authenticatedDriverId_ > 0 &&
                 state->accountId != authenticatedDriverId_)
        {
            record.syncStatus = "PENDING (ANOTHER ACCOUNT)";
        }
        else if (state)
        {
            record.accountId = state->accountId;
            record.serverJobId = state->serverJobId.toStdString();
            record.syncStatus = state->status.toUpper();
            if (!state->error.isEmpty() && state->status == "failed")
            {
                record.syncStatus += ": " + state->error;
            }
        }
        else
        {
            record.syncStatus = "PENDING";
        }
    }
    for (const JobRecord& remote : remoteCompletedJobs_)
    {
        if (remote.accountId <= 0 || remote.accountId != authenticatedDriverId_)
        {
            continue;
        }
        const bool duplicate = std::any_of(records.cbegin(), records.cend(),
            [&](const JobRecord& local)
            {
                return local.id == remote.id ||
                    (!remote.localJobId.empty() &&
                     remote.localJobId == local.id &&
                     remote.clientInstanceId == apiSettings_.clientInstanceId.toStdString());
            });
        if (!duplicate)
        {
            records.push_back(remote);
        }
    }
    std::sort(records.begin(), records.end(), [](const JobRecord& left, const JobRecord& right)
    {
        return left.completedAt > right.completedAt;
    });

    if (records.empty())
    {
        completedEmptyLabel_ = new QLabel("NO COMPLETED OR CANCELLED JOBS YET.", pageStack_);
        completedEmptyLabel_->setObjectName("completedJobsEmpty");
        completedEmptyLabel_->setWordWrap(true);
        completedEmptyLabel_->setAlignment(Qt::AlignCenter);
        completedEmptyLabel_->setStyleSheet("color: #665B62; padding: 24px;");
        completedJobsLayout_->addWidget(completedEmptyLabel_);
        completedJobsLayout_->addStretch();
        return;
    }

    completedEmptyLabel_ = nullptr;
    for (const JobRecord& record : records)
    {
        auto* card = new QPushButton();
        card->setObjectName("completedJobCard");
        card->setProperty("jobId", QString::fromStdString(record.id));
        card->setCursor(Qt::PointingHandCursor);
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(8, 7, 8, 7);
        cardLayout->setSpacing(3);
        auto* heading = new QHBoxLayout();
        auto* id = new QLabel(uppercaseDisplayText(QString::fromStdString(record.id)), card);
        id->setAttribute(Qt::WA_TransparentForMouseEvents);
        id->setStyleSheet("font: 700 18px 'Segoe UI'; color: #28232A;");
        const QString statusLabel = jobStatusLabel(record.status);
        auto* state = new QLabel(statusLabel, card);
        state->setAttribute(Qt::WA_TransparentForMouseEvents);
        state->setStyleSheet(record.status == JobStatus::Cancelled
            ? "font: 700 10px 'Segoe UI'; color: #B3261E;"
            : record.status == JobStatus::Unknown
                ? "font: 700 10px 'Segoe UI'; color: #8A6D1D;"
                : "font: 700 10px 'Segoe UI'; color: #287A45;");
        heading->addWidget(id);
        heading->addStretch();
        heading->addWidget(state);
        cardLayout->addLayout(heading);

        auto* cargo = new QLabel("CARGO  " + recordText(record, "job.cargo"), card);
        cargo->setAttribute(Qt::WA_TransparentForMouseEvents);
        cargo->setWordWrap(true);
        cardLayout->addWidget(cargo);
        const QString origin = recordLocation(record, "job.source_city", "job.source_country");
        const QString destination = recordLocation(record, "job.destination_city", "job.destination_country");
        auto* route = new QLabel("FROM  " + origin + "     TO  " + destination, card);
        route->setAttribute(Qt::WA_TransparentForMouseEvents);
        route->setWordWrap(true);
        cardLayout->addWidget(route);
        auto* footer = new QLabel(
            "INCOME  " + recordCurrency(record, preferences_, eurUsdRate_) + "     COMPLETED  " +
                (record.completedAt.isValid()
                    ? record.completedAt.toLocalTime().toString("yyyy-MM-dd  hh:mm")
                    : unavailable) + "     SYNC  " +
                (record.syncStatus.isEmpty() ? "PENDING" : record.syncStatus),
            card);
        footer->setAttribute(Qt::WA_TransparentForMouseEvents);
        footer->setWordWrap(true);
        footer->setStyleSheet("color: #665B62; font-size: 10px; font-weight: 700;");
        cardLayout->addWidget(footer);

        connect(card, &QPushButton::clicked, this, [this, record]()
        {
            showJobDetails(record);
        });
        completedJobsLayout_->addWidget(card);
    }
    completedJobsLayout_->addStretch();
}

void DashboardMainWindow::showJobDetails(const JobRecord& record)
{
    QDialog dialog(this);
    dialog.setWindowTitle(QString("%1 — %2")
        .arg(QString::fromStdString(record.id),
             jobStatusLabel(record.status)));
    dialog.setMinimumSize(720, 500);
    auto* dialogLayout = new QVBoxLayout(&dialog);
    auto* scroll = new QScrollArea(&dialog);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget(scroll);
    auto* sections = new QVBoxLayout(content);
    sections->setContentsMargins(3, 3, 3, 3);
    sections->setSpacing(8);

    QString reportHtml =
        "<html><head><style>"
        "body{font-family:'Segoe UI',sans-serif;color:#28232A;}"
        "h1{color:#D94F8C;font-size:18pt;}h2{font-size:12pt;margin-top:16px;}"
        "table{width:100%;border-collapse:collapse;}td{padding:5px;border-bottom:1px solid #FFD1E6;}"
        "td:first-child{font-weight:bold;color:#A15E7D;width:38%;}"
        "</style></head><body><h1>NLSI EXCLUSIVE LOGBOOK</h1><p>" +
        QString::fromStdString(record.id).toHtmlEscaped() + " — " +
        jobStatusLabel(record.status).toHtmlEscaped() +
        "</p>";
    const auto addSection = [&](const QString& title, const std::vector<std::pair<QString, QString>>& fields)
    {
        reportHtml += "<h2>" + title.toHtmlEscaped() + "</h2><table>";
        for (const auto& field : fields)
        {
            reportHtml += "<tr><td>" + field.first.toHtmlEscaped() + "</td><td>" +
                field.second.toHtmlEscaped() + "</td></tr>";
        }
        reportHtml += "</table>";
        auto* group = new QGroupBox(title, content);
        auto* grid = new QGridLayout(group);
        grid->setContentsMargins(9, 13, 9, 8);
        grid->setHorizontalSpacing(16);
        grid->setVerticalSpacing(6);
        for (std::size_t index = 0; index < fields.size(); index += 2)
        {
            for (std::size_t column = 0; column < 2 && index + column < fields.size(); ++column)
            {
                auto* cell = new QWidget(group);
                auto* cellLayout = new QVBoxLayout(cell);
                cellLayout->setContentsMargins(0, 0, 0, 0);
                cellLayout->setSpacing(2);
                auto* label = new QLabel(fields[index + column].first, cell);
                label->setObjectName("telemetryLabel");
                label->setWordWrap(true);
                auto* value = makeValue(cell);
                value->setText(uppercaseDisplayText(fields[index + column].second));
                cellLayout->addWidget(label);
                cellLayout->addWidget(value);
                grid->addWidget(cell, static_cast<int>(index / 2), static_cast<int>(column));
            }
        }
        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 1);
        sections->addWidget(group);
    };

    addSection("JOB DETAILS", {
        {"CARGO", recordText(record, "job.cargo")},
        {"INCOME", recordCurrency(record, preferences_, eurUsdRate_)},
        {"FROM: CITY, COUNTRY", recordLocation(record, "job.source_city", "job.source_country")},
        {"TO: CITY, COUNTRY", recordLocation(record, "job.destination_city", "job.destination_country")},
        {"FROM: COMPANY", recordText(record, "job.source_company")},
        {"TO: COMPANY", recordText(record, "job.destination_company")},
        {"PLANNED DISTANCE", recordDistance(record, "job.planned_distance_km", preferences_)},
        {"DRIVEN DISTANCE", recordDistance(record, "job.driven_distance_km", preferences_)},
        {"XP", recordNumber(record, "job.earned_xp", preferences_, "XP")},
        {"DAMAGE", recordDamage(record)},
        {"TIME TAKEN (REAL)", recordDuration(record, "job.real_duration_seconds")},
        {"TIME TAKEN (IN-GAME)", recordDuration(record, "job.ingame_duration_minutes", 60.0)}
    });
    addSection("VEHICLE & TRIP DETAILS", {
        {"TRUCK USED", recordText(record, "vehicle.make_model")},
        {"TRUCK PLATE", recordText(record, "vehicle.license_plate")},
        {"TRAILER", recordText(record, "trailer.details")},
        {"TRAILER PLATE", recordText(record, "trailer.license_plate")},
        {"FUEL COST", unavailable},
        {"REFUELED", recordNumber(record, "job.refueled", preferences_)},
        {"AVERAGE FUEL CONSUMPTION", recordAverageFuelConsumption(record, preferences_)},
        {"TOTAL FUEL CONSUMED", recordNumber(record, "job.total_fuel_consumed", preferences_)},
        {"MAX SPEED", recordNumber(record, "job.max_speed_kmh", preferences_)},
        {"AVERAGE SPEED", recordNumber(record, "job.average_speed_kmh", preferences_)}
    });
    const QString specialCargo = recordSpecialCargo(record);
    const QString status = jobStatusLabel(record.status);
    addSection("SPECIAL CARGO SUPPORT / STATUS", {
        {"SPECIAL CARGO", specialCargo},
        {"JOB STATUS", status},
        {"CANCELLATION PENALTY", recordNumber(
            record, "job.cancel_penalty", preferences_, QString(), eurUsdRate_)}
    });
    addSection("ONLINE SYNCHRONIZATION", {
        {"SYNC STATUS", record.syncStatus.isEmpty() ? "SERVER RECORD" : record.syncStatus},
        {"SERVER JOB ID", !record.serverJobId.empty()
            ? QString::fromStdString(record.serverJobId)
            : unavailable}
    });
    reportHtml += "</body></html>";
    scroll->setWidget(content);
    dialogLayout->addWidget(scroll);
    auto* exportButton = new QPushButton("EXPORT PDF", &dialog);
    exportButton->setObjectName("exportJobPdfButton");
    exportButton->setCursor(Qt::PointingHandCursor);
    connect(exportButton, &QPushButton::clicked, &dialog, [this, &dialog, reportHtml]()
    {
        const QString filePath = QFileDialog::getSaveFileName(
            &dialog, "Export completed job", "completed-job.pdf", "PDF files (*.pdf)");
        if (filePath.isEmpty())
        {
            return;
        }
        QString error;
        if (!writePdfReport(filePath, reportHtml, &error))
        {
            QMessageBox::critical(&dialog, "PDF export failed", error);
            return;
        }
        statusBar()->showMessage("COMPLETED JOB PDF EXPORTED.");
    });
    dialogLayout->addWidget(exportButton, 0, Qt::AlignRight);
    dialog.exec();
}

void DashboardMainWindow::startTelemetryListener()
{
    localServer_ = new QLocalServer(this);
    connect(localServer_, &QLocalServer::newConnection, this, [this]()
    {
        while (localServer_->hasPendingConnections())
        {
            QLocalSocket* socket = localServer_->nextPendingConnection();
            if (!socket)
            {
                continue;
            }
            receiveBuffers_.insert(socket, {});
            TelemetrySnapshot connecting;
            connecting.status = ConnectionState::Connecting;
            clientSnapshots_.insert(socket, connecting);
            lastFrameTimes_.insert(socket, QDateTime::currentMSecsSinceEpoch());
            connect(socket, &QLocalSocket::readyRead, this, [this, socket]()
            {
                handleSocketData(socket);
            });
            connect(socket, &QLocalSocket::disconnected, this, [this, socket]()
            {
                receiveBuffers_.remove(socket);
                clientSnapshots_.remove(socket);
                lastFrameTimes_.remove(socket);
                if (selectedTelemetrySocket_ == socket)
                {
                    selectedTelemetrySocket_ = nullptr;
                }
                socket->deleteLater();
                refreshDisplayedSnapshot();
            });
            refreshDisplayedSnapshot();
        }
    });

    if (!localServer_->listen(QStringLiteral("nlsi_scs_telemetry")))
    {
        ipcError_ = true;
        connectionLabel_->setText(
            uppercaseDisplayText(QString("TELEMETRY CONNECTION: ERROR — %1").arg(localServer_->errorString())));
    }
}

void DashboardMainWindow::handleSocketData(QLocalSocket* socket)
{
    if (!receiveBuffers_.contains(socket))
    {
        return;
    }
    QByteArray& buffer = receiveBuffers_[socket];
    buffer.append(socket->readAll());
    if (buffer.size() > 1024 * 1024)
    {
        ipcError_ = true;
        socket->disconnectFromServer();
        refreshDisplayedSnapshot();
        return;
    }

    for (;;)
    {
        const int frameEnd = buffer.indexOf("\n\n");
        if (frameEnd < 0)
        {
            break;
        }
        const QByteArray frame = buffer.left(frameEnd + 1);
        buffer.remove(0, frameEnd + 2);
        const TelemetrySnapshot snapshot = decodeTelemetryFrame(frame.toStdString());
        lastFrameTimes_[socket] = QDateTime::currentMSecsSinceEpoch();
        if (snapshot.status != ConnectionState::Connected || snapshot.gameId.empty())
        {
            ipcError_ = true;
            clientSnapshots_.remove(socket);
            continue;
        }
        ipcError_ = false;
        clientSnapshots_[socket] = snapshot;
    }
    refreshDisplayedSnapshot();
}

void DashboardMainWindow::setProcessState(GameProcessState state)
{
    if (state != processState_)
    {
        processState_ = state;
        clientSnapshots_.clear();
        selectedTelemetrySocket_ = nullptr;
    }
    processLabel_->setText("GAME DETECTION: " + processStateText(processState_));
    refreshDisplayedSnapshot();
}

void DashboardMainWindow::refreshDisplayedSnapshot()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = lastFrameTimes_.cbegin(); it != lastFrameTimes_.cend(); ++it)
    {
        if (it.value() > 0 && now - it.value() > 2500)
        {
            clientSnapshots_.remove(it.key());
        }
    }
    const QString dateText = QDateTime::currentDateTime().toString("ddd d MMM yyyy  hh:mm:ss").toUpper();
    if (dateLabel_->text() != dateText)
    {
        dateLabel_->setText(dateText);
    }

    TelemetrySnapshot snapshot;
    bool connecting = false;
    if (processState_ == GameProcessState::BothRunning ||
        processState_ == GameProcessState::DetectionError)
    {
        snapshot.status = ConnectionState::Error;
    }
    else if (processState_ == GameProcessState::Ets2Running ||
             processState_ == GameProcessState::AtsRunning)
    {
        if (selectedTelemetrySocket_ &&
            (!clientSnapshots_.contains(selectedTelemetrySocket_) ||
             !snapshotMatchesProcess(clientSnapshots_.value(selectedTelemetrySocket_), processState_) ||
             now - lastFrameTimes_.value(selectedTelemetrySocket_, 0) > 2500))
        {
            selectedTelemetrySocket_ = nullptr;
        }
        if (!selectedTelemetrySocket_)
        {
            qint64 newestFrame = -1;
            for (auto it = clientSnapshots_.cbegin(); it != clientSnapshots_.cend(); ++it)
            {
                const qint64 frameTime = lastFrameTimes_.value(it.key(), 0);
                if (snapshotMatchesProcess(it.value(), processState_) &&
                    frameTime > newestFrame)
                {
                    newestFrame = frameTime;
                    selectedTelemetrySocket_ = it.key();
                }
            }
        }
        if (selectedTelemetrySocket_)
        {
            snapshot = clientSnapshots_.value(selectedTelemetrySocket_);
        }
        else
        {
            for (auto it = lastFrameTimes_.cbegin(); it != lastFrameTimes_.cend(); ++it)
            {
                if (now - it.value() <= 2500)
                {
                    connecting = true;
                    break;
                }
            }
            snapshot.status = ipcError_ ? ConnectionState::Error
                                        : (connecting ? ConnectionState::Connecting : ConnectionState::Disconnected);
        }
    }
    else
    {
        snapshot.status = ConnectionState::Disconnected;
    }
    if (ipcError_ && snapshot.status != ConnectionState::Connected)
    {
        snapshot.status = ConnectionState::Error;
    }
    applySnapshot(snapshot);
}

void DashboardMainWindow::applySnapshot(const TelemetrySnapshot& snapshot)
{
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(this, [this, snapshot]() { applySnapshot(snapshot); },
            Qt::QueuedConnection);
        return;
    }

    TelemetrySnapshot viewSnapshot = snapshot;
    const std::optional<JobRecord> previousActiveJob = jobHistory_.activeJob();
    const bool startsJob = [&]()
    {
        const auto status = snapshot.values.find("job.status");
        return status != snapshot.values.end() && status->second.available &&
            jobStatusFromString(status->second.value) == JobStatus::Active;
    }();
    const auto incomingJobStatus = snapshot.values.find("job.status");
    const bool deliveredEvent = incomingJobStatus != snapshot.values.end() &&
        incomingJobStatus->second.available &&
        jobStatusFromString(incomingJobStatus->second.value) == JobStatus::Delivered;
    const bool historyUpdated = jobHistory_.processSnapshot(snapshot, QDateTime::currentDateTimeUtc());

    const TelemetrySnapshot* retainedJobSnapshot = nullptr;
    if (jobHistory_.activeJob())
    {
        retainedJobSnapshot = &jobHistory_.activeJob()->snapshot;
    }
    else if (previousActiveJob)
    {
        retainedJobSnapshot = &previousActiveJob->snapshot;
    }
    else if (!startsJob)
    {
        retainedJobSnapshot = &displayedSnapshot_;
    }
    if (retainedJobSnapshot)
    {
        for (const auto& field : retainedJobSnapshot->values)
        {
            if (field.first.compare(0, 4, "job.") != 0)
            {
                continue;
            }
            const auto current = viewSnapshot.values.find(field.first);
            if (current == viewSnapshot.values.end() || !current->second.available ||
                trimString(current->second.value).empty())
            {
                viewSnapshot.values[field.first] = field.second;
            }
        }
    }
    if (jobHistory_.activeJob())
    {
        viewSnapshot.values["job.id"] = {
            "Job ID", jobHistory_.activeJob()->id, "", true
        };
    }
    else if (!startsJob && !remoteActiveJobs_.empty())
    {
        for (const auto& field : remoteActiveJobs_.front().snapshot.values)
        {
            if (field.first.compare(0, 4, "job.") == 0)
            {
                viewSnapshot.values[field.first] = field.second;
            }
        }
    }
    if (historyUpdated && !jobHistory_.activeJob())
    {
        refreshCompletedJobs();
        if (statusBar())
        {
            if (!jobHistory_.errorString().isEmpty())
            {
                statusBar()->showMessage(
                    uppercaseDisplayText("JOB HISTORY WARNING: " + jobHistory_.errorString()));
            }
            else
            {
                statusBar()->showMessage(uppercaseDisplayText("JOB RECORD SAVED: " +
                    QString::fromStdString(viewSnapshot.values["job.id"].value)));
            }
        }
        if (deliveredEvent && authenticatedDriverId_ > 0)
        {
            synchronizeAccountJobs(authenticatedDriverId_);
        }
    }
    else if (!jobHistory_.errorString().isEmpty() && statusBar())
    {
        statusBar()->showMessage(uppercaseDisplayText("JOB HISTORY ERROR: " + jobHistory_.errorString()));
    }
    displayedSnapshot_ = viewSnapshot;
    const QString connectionText =
        "TELEMETRY CONNECTION: " +
        QString::fromStdString(connectionStateToString(viewSnapshot.status)).toUpper();
    if (connectionLabel_->text() != connectionText)
    {
        connectionLabel_->setText(connectionText);
    }

    const auto set = [](QLabel* label, const QString& text)
    {
        if (!label)
        {
            return;
        }
        const QString normalized = uppercaseDisplayText(text.isEmpty() ? unavailable : text);
        if (label->text() != normalized)
        {
            label->setText(normalized);
        }
    };

    const auto findField = [&](const std::string& key) -> const FieldValue* {
        const auto it = viewSnapshot.values.find(key);
        if (it == viewSnapshot.values.end())
        {
            return nullptr;
        }
        return &it->second;
    };

    const auto fieldText = [&](const std::string& key) -> QString {
        const auto* field = findField(key);
        if (!field || !field->available || field->value.empty())
        {
            return unavailable;
        }
        QString value = QString::fromStdString(field->value);
        const QString unit = QString::fromStdString(field->unit).trimmed();
        if (!unit.isEmpty())
        {
            value += " " + unit;
        }
        return value;
    };
    const auto rawValue = [&](const std::string& key) -> std::pair<QString, QString> {
        const auto* field = findField(key);
        if (!field || !field->available || field->value.empty())
        {
            return {unavailable, QString()};
        }
        return {QString::fromStdString(field->value), QString::fromStdString(field->unit).trimmed()};
    };

    set(currentJobValue_, fieldText("job.id"));
    set(statusValue_, fieldText("job.status"));
    set(cargoValue_, fieldText("job.cargo"));
    {
        const auto* incomeField = findField("job.income");
        if (incomeField && incomeField->available && !incomeField->value.empty())
        {
            double value = 0.0;
            if (parseDoubleValue(QString::fromStdString(incomeField->value), value))
            {
                set(incomeValue_, formatCurrencyAmount(
                    value,
                    gameCurrencyDenomination(
                        QString::fromStdString(viewSnapshot.gameId),
                        QString::fromStdString(incomeField->unit)),
                    preferences_,
                    eurUsdRate_));
            }
            else
            {
                set(incomeValue_, unavailable);
            }
        }
        else
        {
            set(incomeValue_, unavailable);
        }
    }
    set(fromValue_, currentJobCompanyCity(viewSnapshot, "job.source_company", "job.source_city",
                                        "job.source_country"));
    set(toValue_, currentJobCompanyCity(viewSnapshot, "job.destination_company", "job.destination_city",
                                      "job.destination_country"));

    {
        const auto* planned = findField("job.planned_distance_km");
        const auto* remaining = findField("truck.navigation.distance");
        double plannedValue = 0.0;
        double remainingValue = 0.0;
        bool plannedOk = false;
        bool remainingOk = false;
        if (planned && planned->available && !planned->value.empty())
        {
            plannedOk = parseDoubleValue(QString::fromStdString(planned->value), plannedValue);
        }
        if (remaining && remaining->available && !remaining->value.empty())
        {
            remainingOk = parseDoubleValue(QString::fromStdString(remaining->value), remainingValue);
        }
        if (plannedOk)
        {
            const QString plannedUnit = QString::fromStdString(planned->unit).trimmed();
            set(plannedDistanceValue_, formatDistance(plannedValue, plannedUnit, preferences_));
        }
        else
        {
            set(plannedDistanceValue_, unavailable);
        }
        if (remainingOk)
        {
            const QString remainingUnit = QString::fromStdString(remaining->unit).trimmed();
            set(remainingDistanceValue_, formatDistance(remainingValue, remainingUnit, preferences_));
        }
        else
        {
            set(remainingDistanceValue_, unavailable);
        }
        if (plannedOk && remainingOk)
        {
            double plannedKm = plannedValue;
            double remainingKm = remainingValue;
            const QString plannedUnit = QString::fromStdString(planned->unit).trimmed().toLower();
            const QString remainingUnit = QString::fromStdString(remaining->unit).trimmed().toLower();
            if (plannedUnit == "m" || plannedUnit == "meters" || plannedUnit == "metres")
            {
                plannedKm = metresToKilometres(plannedValue);
            }
            if (remainingUnit == "m" || remainingUnit == "meters" || remainingUnit == "metres")
            {
                remainingKm = metresToKilometres(remainingValue);
            }
            const QString progressText = formatProgressPercent(plannedKm, remainingKm);
            const QString progressValue = progressText == unavailable ? unavailable : progressText;
            set(progressValue_, progressValue);
        }
        else
        {
            set(progressValue_, unavailable);
        }
    }

    {
        const auto* timeField = findField("truck.navigation.time");
        if (timeField && timeField->available && !timeField->value.empty())
        {
            double gameSeconds = 0.0;
            set(arrivalValue_, parseDoubleValue(QString::fromStdString(timeField->value), gameSeconds)
                ? formatArrivalEstimate(gameSeconds)
                : unavailable);
        }
        else
        {
            set(arrivalValue_, unavailable);
        }
    }

    {
        const auto fuel = rawValue("truck.fuel.amount");
        const auto capacity = rawValue("truck.fuel.capacity");
        double fuelAmount = std::numeric_limits<double>::quiet_NaN();
        double fuelCapacity = std::numeric_limits<double>::quiet_NaN();
        const bool fuelAvailable = fuel.first != unavailable && parseDoubleValue(fuel.first, fuelAmount);
        const bool capacityAvailable = capacity.first != unavailable && parseDoubleValue(capacity.first, fuelCapacity);
        set(fuelValue_, formatFuelDisplay(
            fuelAvailable ? fuelAmount : std::numeric_limits<double>::quiet_NaN(),
            capacityAvailable ? fuelCapacity : std::numeric_limits<double>::quiet_NaN(),
            preferences_));
        fuelValue_->setStyleSheet(fuelAvailable && capacityAvailable &&
            isFuelAtOrBelowWarningThreshold(fuelAmount, fuelCapacity)
                ? "color: #C62828;"
                : QString());
    }

    {
        const auto rpm = rawValue("truck.engine.rpm");
        const QString gear = fieldText("truck.engine.gear");
        const QString formattedRpm = formatFixedNumber(rpm.first, "RPM");
        set(rpmGearValue_, QString("%1 / %2").arg(formattedRpm, gear));
    }
    const auto cruise = rawValue("truck.cruise_control");
    double cruiseSpeed = 0.0;
    const bool cruiseActive = cruise.first != unavailable &&
        parseDoubleValue(cruise.first, cruiseSpeed) && cruiseSpeed > 0.0;
    {
        const auto throttle = rawValue("truck.effective.throttle");
        const QString throttlePercent = throttle.first == unavailable
            ? unavailable
            : formatNormalizedPercent(throttle.first, throttle.second);
        set(throttleValue_, throttlePercent == unavailable || !cruiseActive
            ? throttlePercent
            : throttlePercent + " - A");
    }
    {
        const auto brake = rawValue("truck.effective.brake");
        double brakeAmount = 0.0;
        if (brake.first == unavailable || !parseDoubleValue(brake.first, brakeAmount))
        {
            set(brakeValue_, unavailable);
        }
        else
        {
            const QString brakeText = formatNormalizedPercent(brake.first, brake.second);
            set(brakeValue_, cruiseActive && brakeText != unavailable
                ? brakeText + " - A"
                : brakeText);
        }
    }
    {
        if (cruise.first == unavailable || !parseDoubleValue(cruise.first, cruiseSpeed))
        {
            set(cruiseValue_, unavailable);
        }
        else
        {
            const QString speed = formatCruiseSpeed(cruise.first, cruise.second, preferences_);
            set(cruiseValue_, speed == unavailable
                ? unavailable
                : cruiseSpeed > 0.0 ? speed + " - A" : speed);
        }
    }
    {
        const auto retarder = rawValue("truck.brake.retarder");
        double retarderLevel = 0.0;
        if (retarder.first == unavailable || !parseDoubleValue(retarder.first, retarderLevel))
        {
            set(retarderValue_, unavailable);
        }
        else
        {
            const QString retarderText = fieldText("truck.brake.retarder");
            set(retarderValue_, cruiseActive && retarderText != unavailable
                ? retarderText + " - A"
                : retarderText);
        }
    }

    if (viewSnapshot.gameName.empty())
    {
        set(gameVersionValue_, unavailable);
        gameConfigPanel_->setTitle("GAME CONFIG | —");
    }
    else
    {
        const QString game = gameDisplayName(QString::fromStdString(viewSnapshot.gameId));
        const QString version = viewSnapshot.gameApiVersion.empty()
            ? unavailable
            : QString::fromStdString(viewSnapshot.gameApiVersion);
        gameConfigPanel_->setTitle(QString("GAME CONFIG | %1").arg(game));
        set(gameVersionValue_, version);
    }
    set(vehicleValue_, fieldText("vehicle.make_model"));
    set(vehiclePlateValue_, fieldText("vehicle.license_plate"));
    set(trailerValue_, fieldText("trailer.details"));
    set(trailerPlateValue_, fieldText("trailer.license_plate"));
    const QString latestEvent = fieldText("telemetry.event.latest");
    const QString summaryText = latestEvent == unavailable
        ? "NO EVENTS AVAILABLE."
        : uppercaseDisplayText("LATEST SDK EVENT: " + latestEvent);
    if (summaryValue_->text() != summaryText)
    {
        summaryValue_->setText(summaryText);
    }
}

} // namespace nlsi
