#include "UserPreferences.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <WinCrypt.h>
#endif

namespace nlsi {
namespace {

const QByteArray profileHeader("NLSI-PROFILE-1\n");

QString speedToString(SpeedUnit value)
{
    switch (value)
    {
    case SpeedUnit::KilometresPerHour: return "kmh";
    case SpeedUnit::MilesPerHour: return "mph";
    }
    return {};
}

QString volumeToString(VolumeUnit value)
{
    switch (value)
    {
    case VolumeUnit::Liters: return "liters";
    case VolumeUnit::UsGallons: return "us_gallons";
    case VolumeUnit::ImperialGallons: return "imperial_gallons";
    }
    return {};
}

QString currencyToString(CurrencyUnit value)
{
    switch (value)
    {
    case CurrencyUnit::Euro: return "EUR";
    case CurrencyUnit::UsDollar: return "USD";
    }
    return {};
}

QString distanceToString(DistanceUnit value)
{
    switch (value)
    {
    case DistanceUnit::Kilometres: return "km";
    case DistanceUnit::Miles: return "mi";
    }
    return {};
}

bool preferencesFromJson(const QJsonObject& object, UserPreferences& preferences)
{
    const QString speed = object.value("speed").toString();
    const QString volume = object.value("volume").toString();
    const QString currency = object.value("currency").toString();
    const QString distance = object.value("distance").toString();
    if (speed == "kmh")
    {
        preferences.speed = SpeedUnit::KilometresPerHour;
    }
    else if (speed == "mph")
    {
        preferences.speed = SpeedUnit::MilesPerHour;
    }
    else
    {
        return false;
    }

    if (volume == "liters")
    {
        preferences.volume = VolumeUnit::Liters;
    }
    else if (volume == "us_gallons")
    {
        preferences.volume = VolumeUnit::UsGallons;
    }
    else if (volume == "imperial_gallons")
    {
        preferences.volume = VolumeUnit::ImperialGallons;
    }
    else
    {
        return false;
    }

    if (currency == "EUR")
    {
        preferences.currency = CurrencyUnit::Euro;
    }
    else if (currency == "USD")
    {
        preferences.currency = CurrencyUnit::UsDollar;
    }
    else
    {
        return false;
    }

    if (distance == "km")
    {
        preferences.distance = DistanceUnit::Kilometres;
    }
    else if (distance == "mi")
    {
        preferences.distance = DistanceUnit::Miles;
    }
    else
    {
        return false;
    }
    return true;
}

QJsonObject preferencesToJson(const UserPreferences& preferences)
{
    QJsonObject result;
    result.insert("speed", speedToString(preferences.speed));
    result.insert("volume", volumeToString(preferences.volume));
    result.insert("currency", currencyToString(preferences.currency));
    result.insert("distance", distanceToString(preferences.distance));
    return result;
}

bool protect(const QByteArray& plainText, QByteArray& encrypted, QString& error)
{
#ifdef Q_OS_WIN
    DATA_BLOB input{};
    input.cbData = static_cast<DWORD>(plainText.size());
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plainText.constData()));
    DATA_BLOB output{};
    if (!CryptProtectData(&input, nullptr, nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &output))
    {
        error = QString("Windows could not encrypt the profile (error %1).")
            .arg(GetLastError());
        return false;
    }
    encrypted = QByteArray(reinterpret_cast<const char*>(output.pbData),
                           static_cast<int>(output.cbData));
    LocalFree(output.pbData);
    return true;
#else
    Q_UNUSED(plainText);
    Q_UNUSED(encrypted);
    error = "Encrypted profile storage is supported only on Windows.";
    return false;
#endif
}

bool unprotect(const QByteArray& encrypted, QByteArray& plainText, QString& error)
{
#ifdef Q_OS_WIN
    DATA_BLOB input{};
    input.cbData = static_cast<DWORD>(encrypted.size());
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(encrypted.constData()));
    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &output))
    {
        error = QString("Windows could not decrypt the profile (error %1).")
            .arg(GetLastError());
        return false;
    }
    plainText = QByteArray(reinterpret_cast<const char*>(output.pbData),
                           static_cast<int>(output.cbData));
    LocalFree(output.pbData);
    return true;
#else
    Q_UNUSED(encrypted);
    Q_UNUSED(plainText);
    error = "Encrypted profile storage is supported only on Windows.";
    return false;
#endif
}

bool readProfile(const QString& filePath, QJsonObject& profile, bool& exists, QString& error)
{
    QFile file(filePath);
    exists = file.exists();
    if (!exists)
    {
        profile = QJsonObject();
        return true;
    }
    if (!file.open(QIODevice::ReadOnly))
    {
        error = QString("Cannot read encrypted profile %1: %2")
            .arg(filePath, file.errorString());
        return false;
    }
    const QByteArray bytes = file.readAll();
    if (!bytes.startsWith(profileHeader))
    {
        error = QString("Encrypted profile has an unsupported or invalid format: %1")
            .arg(filePath);
        return false;
    }

    QByteArray plainText;
    if (!unprotect(bytes.mid(profileHeader.size()), plainText, error))
    {
        error = QString("%1 File: %2").arg(error, filePath);
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(plainText, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        error = QString("Decrypted profile data is invalid: %1").arg(filePath);
        return false;
    }
    profile = document.object();
    return true;
}

void setError(QString* target, const QString& error)
{
    if (target)
    {
        *target = error;
    }
}

} // namespace

bool isValidPreferences(const UserPreferences& preferences)
{
    return !speedToString(preferences.speed).isEmpty() &&
        !volumeToString(preferences.volume).isEmpty() &&
        !currencyToString(preferences.currency).isEmpty() &&
        !distanceToString(preferences.distance).isEmpty();
}

ProfileStore::ProfileStore(QString filePath)
{
    if (filePath.isEmpty())
    {
        filePath = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
            .filePath("profile.nlsi");
    }
    filePath_ = QDir::cleanPath(filePath);
}

QString ProfileStore::filePath() const
{
    return filePath_;
}

bool ProfileStore::load(UserPreferences& preferences, QString* error) const
{
    preferences = UserPreferences();
    setError(error, QString());
    QString readError;
    QJsonObject profile;
    bool exists = false;
    if (!readProfile(filePath_, profile, exists, readError))
    {
        setError(error, readError);
        return false;
    }
    if (!exists || !profile.contains("preferences"))
    {
        return true;
    }
    const QJsonValue value = profile.value("preferences");
    UserPreferences loaded;
    if (!value.isObject() || !preferencesFromJson(value.toObject(), loaded))
    {
        setError(error, "Saved preferences are invalid; safe defaults are active.");
        return false;
    }
    preferences = loaded;
    return true;
}

bool ProfileStore::save(const UserPreferences& preferences, QString* error) const
{
    setError(error, QString());
    if (!isValidPreferences(preferences))
    {
        setError(error, "Preferences contain an unsupported setting value.");
        return false;
    }

    const QFileInfo destination(filePath_);
    if (!QDir().mkpath(destination.absolutePath()))
    {
        setError(error, QString("Cannot create profile directory: %1")
            .arg(destination.absolutePath()));
        return false;
    }
    QLockFile lock(filePath_ + ".lock");
    lock.setStaleLockTime(30000);
    if (!lock.tryLock(5000))
    {
        setError(error, QString("Cannot lock encrypted profile %1: %2")
            .arg(filePath_, lock.error() == QLockFile::LockFailedError
                ? "another process is saving it"
                : "lock file could not be created"));
        return false;
    }

    QJsonObject profile;
    bool exists = false;
    QString readError;
    if (!readProfile(filePath_, profile, exists, readError))
    {
        setError(error, readError);
        return false;
    }
    profile.insert("preferences", preferencesToJson(preferences));

    QByteArray encrypted;
    if (!protect(QJsonDocument(profile).toJson(QJsonDocument::Compact), encrypted, readError))
    {
        setError(error, readError);
        return false;
    }

    QSaveFile file(filePath_);
    if (!file.open(QIODevice::WriteOnly))
    {
        setError(error, QString("Cannot write encrypted profile %1: %2")
            .arg(filePath_, file.errorString()));
        return false;
    }
    if (file.write(profileHeader) != profileHeader.size() ||
        file.write(encrypted) != encrypted.size())
    {
        setError(error, QString("Cannot write complete encrypted profile %1: %2")
            .arg(filePath_, file.errorString()));
        file.cancelWriting();
        return false;
    }
    if (!file.commit())
    {
        setError(error, QString("Cannot atomically replace encrypted profile %1: %2")
            .arg(filePath_, file.errorString()));
        return false;
    }
    return true;
}

} // namespace nlsi
