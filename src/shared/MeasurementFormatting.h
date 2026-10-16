#pragma once

#include <optional>

#include <QString>

#include "UserPreferences.h"

namespace nlsi {

double kilometresPerHourToMilesPerHour(double value);
double milesPerHourToKilometresPerHour(double value);
double metresToKilometres(double value);
double kilometresToMiles(double value);
double milesToKilometres(double value);
double litersToUsGallons(double value);
double usGallonsToLiters(double value);
double litersToImperialGallons(double value);
double imperialGallonsToLiters(double value);

QString speedUnitLabel(SpeedUnit unit);
QString distanceUnitLabel(DistanceUnit unit);
QString volumeUnitLabel(VolumeUnit unit);
QString currencyUnitLabel(CurrencyUnit unit);
QString formatSpeed(double value, const QString& sourceUnit, const UserPreferences& preferences);
QString formatDistance(double value, const QString& sourceUnit, const UserPreferences& preferences);
QString formatVolume(double liters, const UserPreferences& preferences, int decimals = 2);
QString formatCurrencyAmount(double value, const QString& denomination,
                             const UserPreferences& preferences,
                             std::optional<double> eurUsdRate = std::nullopt);

} // namespace nlsi
