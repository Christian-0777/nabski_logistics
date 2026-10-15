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

bool isValidPreferences(const UserPreferences& preferences);

class ProfileStore
{
public:
    explicit ProfileStore(QString filePath = QString());

    QString filePath() const;
    bool load(UserPreferences& preferences, QString* error = nullptr) const;
    bool save(const UserPreferences& preferences, QString* error = nullptr) const;

private:
    QString filePath_;
};

} // namespace nlsi
