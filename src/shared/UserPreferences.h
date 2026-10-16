#pragma once

#include <QString>

namespace nlsi {

enum class SpeedUnit
{
    KilometresPerHour,
    MilesPerHour
};

enum class VolumeUnit
{
    Liters,
    UsGallons,
    ImperialGallons
};

enum class CurrencyUnit
{
    Euro,
    UsDollar
};

enum class DistanceUnit
{
    Kilometres,
    Miles
};

struct UserPreferences
{
    SpeedUnit speed = SpeedUnit::KilometresPerHour;
    VolumeUnit volume = VolumeUnit::Liters;
    CurrencyUnit currency = CurrencyUnit::Euro;
    DistanceUnit distance = DistanceUnit::Kilometres;
};

struct ApiSettings
{
    QString baseUrl;
    QString apiKey;
    QString clientInstanceId;
};

bool isValidPreferences(const UserPreferences& preferences);
bool normalizeApiBaseUrl(const QString& value, QString& normalized, QString* error = nullptr);

class ProfileStore
{
public:
    explicit ProfileStore(QString filePath = QString());

    QString filePath() const;
    bool load(UserPreferences& preferences, QString* error = nullptr) const;
    bool loadApiSettings(ApiSettings& settings, QString* error = nullptr) const;
    bool save(const UserPreferences& preferences, QString* error = nullptr) const;
    bool saveApiSettings(const ApiSettings& settings, QString* error = nullptr) const;

private:
    QString filePath_;
};

} // namespace nlsi
