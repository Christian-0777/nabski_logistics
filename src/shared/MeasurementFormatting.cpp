#include "MeasurementFormatting.h"

#include <cmath>

#include "DisplayFormatting.h"

namespace nlsi {
namespace {

const double kilometresPerMile = 1.609344;
const double milesPerKilometre = 0.621371192237334;
const double litersPerUsGallon = 3.785411784;
const double litersPerImperialGallon = 4.54609;

QString groupedNumber(double value, int decimals)
{
    if (!std::isfinite(value))
    {
        return QString::fromUtf8("—");
    }
    QString formatted = formatDisplayNumber(value, decimals);
    const bool negative = formatted.startsWith('-');
    if (negative)
    {
        formatted.remove(0, 1);
    }
    const int decimalIndex = formatted.indexOf('.');
    QString integer = decimalIndex < 0 ? formatted : formatted.left(decimalIndex);
    const QString fraction = decimalIndex < 0 ? QString() : formatted.mid(decimalIndex);
    for (int index = integer.size() - 3; index > 0; index -= 3)
    {
        integer.insert(index, ',');
    }
    return (negative ? "-" : "") + integer + fraction;
}

QString normalized(const QString& unit)
{
    return unit.trimmed().toLower();
}

} // namespace

double kilometresPerHourToMilesPerHour(double value)
{
    return value * milesPerKilometre;
}

double milesPerHourToKilometresPerHour(double value)
{
    return value / milesPerKilometre;
}

double metresToKilometres(double value)
{
    return value / 1000.0;
}

double kilometresToMiles(double value)
{
    return value / kilometresPerMile;
}

double milesToKilometres(double value)
{
    return value * kilometresPerMile;
}

double litersToUsGallons(double value)
{
    return value / litersPerUsGallon;
}

double usGallonsToLiters(double value)
{
    return value * litersPerUsGallon;
}

double litersToImperialGallons(double value)
{
    return value / litersPerImperialGallon;
}

double imperialGallonsToLiters(double value)
{
    return value * litersPerImperialGallon;
}

QString speedUnitLabel(SpeedUnit unit)
{
    return unit == SpeedUnit::MilesPerHour ? "MPH" : "KM/H";
}

QString distanceUnitLabel(DistanceUnit unit)
{
    return unit == DistanceUnit::Miles ? "MI" : "KM";
}

QString volumeUnitLabel(VolumeUnit unit)
{
    switch (unit)
    {
    case VolumeUnit::Liters: return "L";
    case VolumeUnit::UsGallons: return "US GAL";
    case VolumeUnit::ImperialGallons: return "IMP GAL";
    }
    return "L";
}

QString currencyUnitLabel(CurrencyUnit unit)
{
    return unit == CurrencyUnit::UsDollar ? "USD ($)" : "EUR (€)";
}

QString formatSpeed(double value, const QString& sourceUnit, const UserPreferences& preferences)
{
    if (!std::isfinite(value))
    {
        return QString::fromUtf8("—");
    }
    const QString source = normalized(sourceUnit);
    double kilometresPerHour = value;
    if (source == "m/s" || source == "mps" || source == "meters per second" ||
        source == "metres per second")
    {
        kilometresPerHour *= 3.6;
    }
    else if (source == "mph")
    {
        kilometresPerHour = milesPerHourToKilometresPerHour(value);
    }
    const double displayValue = preferences.speed == SpeedUnit::MilesPerHour
        ? kilometresPerHourToMilesPerHour(kilometresPerHour)
        : kilometresPerHour;
    return QString("%1 %2").arg(formatDisplayNumber(displayValue, 2), speedUnitLabel(preferences.speed));
}

QString formatDistance(double value, const QString& sourceUnit, const UserPreferences& preferences)
{
    if (!std::isfinite(value))
    {
        return QString::fromUtf8("—");
    }
    const QString source = normalized(sourceUnit);
    double kilometres = value;
    if (source == "m" || source == "meters" || source == "metres")
    {
        kilometres = metresToKilometres(value);
    }
    else if (source == "mi" || source == "mile" || source == "miles")
    {
        kilometres = milesToKilometres(value);
    }
    const double displayValue = preferences.distance == DistanceUnit::Miles
        ? kilometresToMiles(kilometres)
        : kilometres;
    return QString("%1 %2").arg(formatDisplayNumber(displayValue, 2),
                                distanceUnitLabel(preferences.distance));
}

QString formatVolume(double liters, const UserPreferences& preferences, int decimals)
{
    if (!std::isfinite(liters))
    {
        return QString::fromUtf8("—");
    }
    double value = liters;
    switch (preferences.volume)
    {
    case VolumeUnit::Liters: break;
    case VolumeUnit::UsGallons: value = litersToUsGallons(liters); break;
    case VolumeUnit::ImperialGallons: value = litersToImperialGallons(liters); break;
    }
    return QString("%1 %2").arg(formatDisplayNumber(value, decimals),
                                volumeUnitLabel(preferences.volume));
}

QString formatCurrencyAmount(double value, const QString& denomination,
                             const UserPreferences& preferences)
{
    Q_UNUSED(preferences);
    if (!std::isfinite(value))
    {
        return QString::fromUtf8("—");
    }
    const QString source = denomination.trimmed();
    const QString lower = source.toLower();
    if (lower == "eur" || source == "€")
    {
        return QString("%1%2")
            .arg(value < 0.0 ? "€-" : "€")
            .arg(groupedNumber(std::fabs(value), 2));
    }
    if (lower == "usd" || source == "$")
    {
        return QString("%1%2")
            .arg(value < 0.0 ? "$-" : "$")
            .arg(groupedNumber(std::fabs(value), 2));
    }
    const QString amount = groupedNumber(value, 2);
    return source.isEmpty() ? amount : QString("%1 %2").arg(amount, source.toUpper());
}

} // namespace nlsi
