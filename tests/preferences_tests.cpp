#include <cmath>
#include <iostream>
#include <limits>

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <WinCrypt.h>
#endif

#include "DisplayFormatting.h"
#include "JobReport.h"
#include "MeasurementFormatting.h"
#include "UserPreferences.h"

namespace {

bool check(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        return false;
    }
    return true;
}

bool approximatelyEqual(double left, double right, double tolerance = 1e-10)
{
    return std::fabs(left - right) <= tolerance;
}

#ifdef Q_OS_WIN
bool writeEncryptedProfile(const QString& path, const QJsonObject& profile)
{
    const QByteArray plain = QJsonDocument(profile).toJson(QJsonDocument::Compact);
    DATA_BLOB input{};
    input.cbData = static_cast<DWORD>(plain.size());
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plain.constData()));
    DATA_BLOB output{};
    if (!CryptProtectData(&input, nullptr, nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &output))
    {
        return false;
    }
    QByteArray bytes("NLSI-PROFILE-1\n");
    bytes.append(reinterpret_cast<const char*>(output.pbData), static_cast<int>(output.cbData));
    LocalFree(output.pbData);
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

bool readEncryptedProfile(const QString& path, QJsonObject& profile)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        return false;
    }
    const QByteArray bytes = file.readAll();
    const QByteArray header("NLSI-PROFILE-1\n");
    if (!bytes.startsWith(header))
    {
        return false;
    }
    QByteArray encrypted = bytes.mid(header.size());
    DATA_BLOB input{};
    input.cbData = static_cast<DWORD>(encrypted.size());
    input.pbData = reinterpret_cast<BYTE*>(encrypted.data());
    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &output))
    {
        return false;
    }
    const QJsonDocument document = QJsonDocument::fromJson(
        QByteArray(reinterpret_cast<const char*>(output.pbData), static_cast<int>(output.cbData)));
    LocalFree(output.pbData);
    if (!document.isObject())
    {
        return false;
    }
    profile = document.object();
    return true;
}
#endif

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;

    for (const QString& role : {"DRIVER", "MANAGER", "CEO", "ADMIN"})
    {
        ok = check(nlsi::isSupportedAccountRole(role),
                   "the supported server account roles are recognized") && ok;
    }
    ok = check(!nlsi::isSupportedAccountRole("OWNER") &&
                   !nlsi::isSupportedAccountRole("admin"),
               "unknown or noncanonical account roles are rejected") && ok;

    ok = check(approximatelyEqual(nlsi::kilometresPerHourToMilesPerHour(1.0), 0.621371192237334),
               "km/h to mph uses the specified conversion factor") && ok;
    ok = check(approximatelyEqual(nlsi::milesPerHourToKilometresPerHour(0.621371192237334), 1.0),
               "mph to km/h uses the inverse conversion") && ok;
    ok = check(approximatelyEqual(nlsi::kilometresToMiles(1.609344), 1.0) &&
               approximatelyEqual(nlsi::milesToKilometres(1.0), 1.609344),
               "kilometres and miles convert in both directions") && ok;
    ok = check(approximatelyEqual(nlsi::litersToUsGallons(3.785411784), 1.0) &&
               approximatelyEqual(nlsi::usGallonsToLiters(1.0), 3.785411784),
               "liters and US gallons convert in both directions") && ok;
    ok = check(approximatelyEqual(nlsi::litersToImperialGallons(4.54609), 1.0) &&
               approximatelyEqual(nlsi::imperialGallonsToLiters(1.0), 4.54609),
               "liters and Imperial gallons convert in both directions") && ok;

    nlsi::UserPreferences preferences;
    preferences.speed = nlsi::SpeedUnit::MilesPerHour;
    preferences.distance = nlsi::DistanceUnit::Miles;
    preferences.volume = nlsi::VolumeUnit::UsGallons;
    ok = check(nlsi::formatSpeed(100.0, "km/h", preferences) == "62.14 MPH",
               "speed conversion and fixed two-decimal rounding") && ok;
    ok = check(nlsi::formatSpeed(25.0, "m/s", preferences) == "55.92 MPH",
               "m/s telemetry is converted to selected mph units") && ok;
    ok = check(nlsi::formatDistance(1.609344, "km", preferences) == "1.00 MI",
               "distance output uses the selected miles label") && ok;
    ok = check(nlsi::formatDistance(1234.5, "m", preferences) == "0.77 MI",
               "meter boundary converts to miles without altering source values") && ok;
    ok = check(nlsi::formatVolume(3.785411784, preferences) == "1.00 US GAL",
               "volume output uses the selected US gallon label") && ok;
    ok = check(nlsi::formatFuelDisplay(3.785411784, 0.0, preferences) ==
                   "1.00 US GAL / — (—)",
               "fuel display converts capacity and amount consistently") && ok;
    ok = check(nlsi::formatSpeed(std::numeric_limits<double>::infinity(), "km/h", preferences) == "—",
               "non-finite measurements remain unavailable") && ok;
    ok = check(nlsi::formatSpeed(1.0051, "km/h", nlsi::UserPreferences()) == "1.01 KM/H",
               "display rounding is consistent at decimal boundaries") && ok;

    preferences.currency = nlsi::CurrencyUnit::Euro;
    ok = check(nlsi::formatCurrencyAmount(1234.5, "EUR", preferences) == "€1,234.50",
               "known EUR denomination keeps its correct symbol") && ok;
    preferences.currency = nlsi::CurrencyUnit::UsDollar;
    ok = check(nlsi::formatCurrencyAmount(1234.5, "USD", preferences) == "$1,234.50",
               "known USD denomination keeps its correct symbol") && ok;
    ok = check(nlsi::formatCurrencyAmount(1234.5, "EUR", preferences, 1.2) == "$1,481.40",
               "EUR income converts once to the selected USD using a valid rate") && ok;
    preferences.currency = nlsi::CurrencyUnit::Euro;
    ok = check(nlsi::formatCurrencyAmount(1234.5, "USD", preferences, 1.2) == "€1,028.75",
               "USD income converts to the selected EUR using the inverse rate") && ok;
    preferences.currency = nlsi::CurrencyUnit::UsDollar;
    ok = check(nlsi::formatCurrencyAmount(1234.5, "EUR", preferences) == "€1,234.50",
               "foreign income remains in its native currency when no exchange rate exists") && ok;
    ok = check(nlsi::formatCurrencyAmount(1234.5, "EUR", preferences, 0.0) == "€1,234.50" &&
               nlsi::formatCurrencyAmount(
                   1234.5, "EUR", preferences,
                   std::numeric_limits<double>::quiet_NaN()) == "€1,234.50",
               "invalid exchange rates are not used and native currency is retained") && ok;
    preferences.currency = nlsi::CurrencyUnit::Euro;
    ok = check(nlsi::formatCurrencyAmount(1234.5, "USD", preferences) == "$1,234.50",
               "USD income remains correctly labeled when the EUR/USD rate is unavailable") && ok;
    ok = check(nlsi::formatCurrencyAmount(1234.5, "game currency", preferences) ==
                   "1,234.50 GAME CURRENCY",
               "unknown or game-denominated amounts are never relabeled as EUR or USD") && ok;
    ok = check(nlsi::formatCurrencyAmount(
                   1.0, "EUR", preferences, std::numeric_limits<double>::quiet_NaN()) == "€1.00",
               "native currency display does not depend on an irrelevant invalid exchange rate") && ok;

    QString normalizedApiUrl;
    QString apiUrlError;
    ok = check(nlsi::normalizeApiBaseUrl(
                   "  https://example.com/xtian_tools/nlsi-api/nlsi-api///  ",
                   normalizedApiUrl, &apiUrlError) &&
                   normalizedApiUrl == "https://example.com/xtian_tools/nlsi-api",
               "API base URLs are validated and repeated trailing path segments normalized") && ok;
    ok = check(!nlsi::normalizeApiBaseUrl("http://example.com/api", normalizedApiUrl, &apiUrlError) &&
                   !apiUrlError.isEmpty(),
               "non-HTTPS API URLs are rejected") && ok;
    ok = check(!nlsi::normalizeApiBaseUrl(
                   "https://user:password@example.com/api", normalizedApiUrl, &apiUrlError),
               "API URLs containing credentials are rejected") && ok;

    QTemporaryDir temporaryDirectory;
    ok = check(temporaryDirectory.isValid(), "temporary profile directory is available") && ok;
    if (temporaryDirectory.isValid())
    {
        const QString profilePath = temporaryDirectory.filePath("profile.nlsi");
        nlsi::ProfileStore store(profilePath);
        nlsi::UserPreferences loaded;
        nlsi::ApiSettings loadedApi;
        QString error;
        ok = check(store.load(loaded, &error) && error.isEmpty() &&
                   loaded.speed == nlsi::SpeedUnit::KilometresPerHour &&
                   loaded.volume == nlsi::VolumeUnit::Liters &&
                   loaded.currency == nlsi::CurrencyUnit::Euro &&
                   loaded.distance == nlsi::DistanceUnit::Kilometres &&
                   !QFile::exists(profilePath),
                   "missing profile uses safe defaults without creating a file") && ok;
        ok = check(store.loadApiSettings(loadedApi, &error) && error.isEmpty() &&
                   loadedApi.baseUrl.isEmpty() &&
                   loadedApi.apiKey.isEmpty() && !QFile::exists(profilePath),
                   "missing API settings do not invent a URL or key and do not create a file") && ok;

        nlsi::UserPreferences saved;
        saved.speed = nlsi::SpeedUnit::MilesPerHour;
        saved.volume = nlsi::VolumeUnit::ImperialGallons;
        saved.currency = nlsi::CurrencyUnit::UsDollar;
        saved.distance = nlsi::DistanceUnit::Miles;
#ifdef Q_OS_WIN
        const auto checkApiConfiguration = [&](const QString& fileName,
                                               const QJsonObject& apiObject,
                                               const QString& expectedUrl,
                                               const QString& expectedKey)
        {
            QJsonObject fixture;
            fixture.insert("api", apiObject);
            const QString fixturePath = temporaryDirectory.filePath(fileName);
            nlsi::ApiSettings fixtureSettings;
            QString fixtureError;
            return writeEncryptedProfile(fixturePath, fixture) &&
                nlsi::ProfileStore(fixturePath).loadApiSettings(fixtureSettings, &fixtureError) &&
                fixtureError.isEmpty() && fixtureSettings.baseUrl == expectedUrl &&
                fixtureSettings.apiKey == expectedKey;
        };
        ok = check(checkApiConfiguration("missing-url.nlsi",
                   QJsonObject{{"key", "AbC123xYz789"}}, QString(), "AbC123xYz789"),
                   "an encrypted key without an API URL remains unconfigured") && ok;
        ok = check(checkApiConfiguration("missing-key.nlsi",
                   QJsonObject{{"base_url", "https://example.com/api"}},
                   "https://example.com/api", QString()),
                   "an encrypted API URL without a key remains unconfigured") && ok;
        ok = check(checkApiConfiguration("missing-both.nlsi", QJsonObject(), QString(), QString()),
                   "an encrypted empty API configuration remains unconfigured") && ok;
        ok = check(store.save(saved, &error), "all preferences save together to encrypted profile") && ok;
        QFile protectedProfile(profilePath);
        ok = check(protectedProfile.open(QIODevice::ReadOnly) &&
                   !protectedProfile.readAll().contains("imperial_gallons"),
                   "profile file does not contain plaintext preferences") && ok;
        ok = check(store.load(loaded, &error) &&
                   loaded.speed == saved.speed && loaded.volume == saved.volume &&
                   loaded.currency == saved.currency && loaded.distance == saved.distance,
                   "saved preferences reload together after restart") && ok;

        const QString preservationPath = temporaryDirectory.filePath("preserved.nlsi");
        QJsonObject customProfile;
        customProfile.insert("account", QJsonObject{{"id", "opaque-profile-field"}});
        customProfile.insert("preferences", QJsonObject{{"eur_to_usd_rate", 1.09}});
        customProfile.insert("api", QJsonObject{{"future_setting", "keep-me"}});
        ok = check(writeEncryptedProfile(preservationPath, customProfile),
                   "test profile with unrelated encrypted data is created") && ok;
        nlsi::ProfileStore preservationStore(preservationPath);
        ok = check(preservationStore.save(saved, &error),
                   "saving preferences updates an existing encrypted profile") && ok;
        QJsonObject decryptedProfile;
        ok = check(readEncryptedProfile(preservationPath, decryptedProfile) &&
                   decryptedProfile.value("account").toObject().value("id").toString() ==
                       "opaque-profile-field" &&
                   decryptedProfile.value("preferences").toObject().value("eur_to_usd_rate").toDouble() ==
                       1.09 &&
                   decryptedProfile.value("preferences").toObject().value("volume").toString() ==
                       "imperial_gallons" &&
                   decryptedProfile.value("api").toObject().value("future_setting").toString() ==
                       "keep-me",
                   "saving preferences preserves unrelated profile and preference fields") && ok;
        nlsi::ApiSettings savedApi;
        savedApi.baseUrl = "https://example.com/xtian_tools/nlsi-api/";
        savedApi.apiKey = "AbC123xYz789";
        savedApi.clientInstanceId = "e3b0c442-98fc-5c14-9afb-f4c8996fb924";
        ok = check(preservationStore.saveApiSettings(savedApi, &error),
                   "API URL and key save together in the encrypted profile") && ok;
        QFile apiProtectedProfile(preservationPath);
        ok = check(apiProtectedProfile.open(QIODevice::ReadOnly) &&
                   !apiProtectedProfile.readAll().contains(savedApi.apiKey.toUtf8()),
                   "API key is never stored in plaintext in profile.nlsi") && ok;
        apiProtectedProfile.close();
        ok = check(preservationStore.loadApiSettings(loadedApi, &error) &&
                   loadedApi.baseUrl == "https://example.com/xtian_tools/nlsi-api" &&
                   loadedApi.apiKey == savedApi.apiKey &&
                   loadedApi.clientInstanceId == savedApi.clientInstanceId,
                   "saved API settings and stable client identity reload from encrypted profile") && ok;
        ok = check(readEncryptedProfile(preservationPath, decryptedProfile) &&
                   decryptedProfile.value("api").toObject().value("future_setting").toString() ==
                       "keep-me" &&
                   decryptedProfile.value("preferences").toObject().value("eur_to_usd_rate").toDouble() ==
                       1.09,
                   "saving API settings preserves unrelated profile and preference fields") && ok;
        const QString clientInstanceId = savedApi.clientInstanceId;
        ok = check(preservationStore.removeApiCredentials(&error) && error.isEmpty(),
                   "removing API credentials updates the encrypted profile successfully") && ok;
        ok = check(preservationStore.loadApiSettings(loadedApi, &error) &&
                   loadedApi.baseUrl.isEmpty() && loadedApi.apiKey.isEmpty() &&
                   loadedApi.clientInstanceId == clientInstanceId,
                   "removed API credentials do not load while the stable local client identity remains") && ok;
        ok = check(readEncryptedProfile(preservationPath, decryptedProfile) &&
                   !decryptedProfile.value("api").toObject().contains("base_url") &&
                   !decryptedProfile.value("api").toObject().contains("key") &&
                   decryptedProfile.value("api").toObject().value("client_instance_id").toString() ==
                       clientInstanceId &&
                   decryptedProfile.value("api").toObject().value("future_setting").toString() ==
                       "keep-me" &&
                   decryptedProfile.value("account").toObject().value("id").toString() ==
                       "opaque-profile-field" &&
                   decryptedProfile.value("preferences").toObject().value("eur_to_usd_rate").toDouble() ==
                       1.09,
                   "removing credentials preserves profile, preferences, client identity, and unknown settings") && ok;

        const QString invalidPreferencesPath = temporaryDirectory.filePath("invalid-preferences.nlsi");
        QJsonObject invalidPreferences;
        invalidPreferences.insert("speed", "kmh");
        invalidPreferences.insert("volume", "barrels");
        invalidPreferences.insert("currency", "EUR");
        invalidPreferences.insert("distance", "km");
        QJsonObject invalidProfile;
        invalidProfile.insert("preferences", invalidPreferences);
        ok = check(writeEncryptedProfile(invalidPreferencesPath, invalidProfile),
                   "encrypted invalid-preference fixture is created") && ok;
        nlsi::ProfileStore invalidStore(invalidPreferencesPath);
        ok = check(!invalidStore.load(loaded, &error) &&
                   loaded.speed == nlsi::SpeedUnit::KilometresPerHour &&
                   loaded.volume == nlsi::VolumeUnit::Liters &&
                   !error.isEmpty(),
                   "invalid encrypted preference values use safe defaults and report an error") && ok;

        QFile damaged(profilePath);
        ok = check(damaged.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
                   damaged.write("corrupt") == 7,
                   "corrupt-profile fixture is written") && ok;
        damaged.close();
        const QByteArray damagedBytes("corrupt");
        ok = check(!store.load(loaded, &error) &&
                   loaded.speed == nlsi::SpeedUnit::KilometresPerHour &&
                   !error.isEmpty(),
                   "corrupt profile falls back to defaults and reports the error") && ok;
        ok = check(!store.save(saved, &error) &&
                   QFile(profilePath).size() == damagedBytes.size(),
                   "saving never overwrites an unreadable existing profile") && ok;
#else
        ok = check(!store.save(saved, &error) && !error.isEmpty(),
                   "unsupported platforms report that encrypted profile storage is unavailable") && ok;
#endif

        nlsi::UserPreferences invalid;
        invalid.speed = static_cast<nlsi::SpeedUnit>(99);
        ok = check(!nlsi::isValidPreferences(invalid), "unsupported preference values are rejected") && ok;
    }

    return ok ? 0 : 1;
}
