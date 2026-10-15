#include "DisplayFormatting.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QDateTime>
#include <QLocale>

namespace nlsi {
namespace {

const QString unavailable = QString::fromUtf8("—");

QString canonicalValueText(const QString& text)
{
    if (text.isEmpty())
    {
        return unavailable;
    }
    const QString trimmed = text.trimmed();
    return trimmed.isEmpty() ? unavailable : trimmed;
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

} // namespace

QString uppercaseDisplayText(const QString& text)
{
    const QString trimmed = canonicalValueText(text);
    return trimmed == unavailable ? unavailable : trimmed.toUpper();
}

QString formatDisplayNumber(double value, int decimals)
{
    if (!std::isfinite(value) || decimals < 0)
    {
        return unavailable;
    }
    return QLocale::c().toString(value, 'f', decimals);
}

QString formatFixedNumber(const QString& valueText, const QString& unit, int decimals)
{
    double value = 0.0;
    if (!parseDoubleValue(valueText, value))
    {
        return unavailable;
    }
    const QString formatted = formatDisplayNumber(value, decimals);
    const QString normalizedUnit = unit.trimmed();
    return normalizedUnit.isEmpty() ? formatted : QString("%1 %2").arg(formatted, normalizedUnit);
}

QString formatCurrencyEuro(double value)
{
    if (!std::isfinite(value))
    {
        return unavailable;
    }
    QString amount = QLocale::c().toString(value, 'f', 2);
    const bool isNegative = amount.startsWith('-');
    if (isNegative)
    {
        amount.remove(0, 1);
    }
    const int decimalIndex = amount.indexOf('.');
    QString integerPart = decimalIndex >= 0 ? amount.left(decimalIndex) : amount;
    const QString fractionPart = decimalIndex >= 0 ? amount.mid(decimalIndex + 1) : QString();
    QString grouped;
    int pos = 0;
    for (int index = integerPart.size() - 1; index >= 0; --index)
    {
        if (pos > 0 && pos % 3 == 0)
        {
            grouped.prepend(",");
        }
        grouped.prepend(integerPart.at(index));
        ++pos;
    }
    if (fractionPart.isEmpty())
    {
        return QString("€%1%2").arg(isNegative ? "-" : "").arg(grouped);
    }
    return QString("€%1%2.%3").arg(isNegative ? "-" : "").arg(grouped).arg(fractionPart);
}

QString formatDistanceKilometres(double value, const QString& unit)
{
    if (!std::isfinite(value))
    {
        return unavailable;
    }
    const QString normalizedUnit = unit.trimmed().toLower();
    double kilometres = value;
    if (normalizedUnit == "m" || normalizedUnit == "meters" || normalizedUnit == "metres")
    {
        kilometres = value / 1000.0;
    }
    return QString("%1 KM").arg(formatDisplayNumber(kilometres, 2));
}

QString formatProgressPercent(double plannedDistance, double remainingDistance)
{
    if (!std::isfinite(plannedDistance) || !std::isfinite(remainingDistance) || plannedDistance <= 0.0)
    {
        return unavailable;
    }
    double percent = ((plannedDistance - remainingDistance) / plannedDistance) * 100.0;
    if (percent < 0.0)
    {
        percent = 0.0;
    }
    else if (percent > 100.0)
    {
        percent = 100.0;
    }
    return QString("%1%").arg(formatDisplayNumber(percent, 2));
}

QString formatDurationHhmmss(double seconds)
{
    if (!std::isfinite(seconds) || seconds < 0.0 ||
        seconds >= static_cast<double>(std::numeric_limits<qint64>::max()))
    {
        return unavailable;
    }
    const auto total = static_cast<qint64>(std::llround(seconds));
    const qint64 hours = total / 3600;
    const qint64 minutes = (total % 3600) / 60;
    const qint64 secs = total % 60;
    return QString("%1:%2:%3")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(secs, 2, 10, QLatin1Char('0'));
}

QString formatArrivalClock(double remainingRealSeconds, const QDateTime& now)
{
    if (!std::isfinite(remainingRealSeconds) || remainingRealSeconds < 0.0 ||
        remainingRealSeconds >= static_cast<double>(std::numeric_limits<qint64>::max()) ||
        !now.isValid())
    {
        return unavailable;
    }
    const QDateTime eta = now.addSecs(static_cast<qint64>(std::llround(remainingRealSeconds)));
    if (!eta.isValid())
    {
        return unavailable;
    }
    return eta.toString("HH:mm:ss");
}

QString formatArrivalEstimate(double remainingGameSeconds, const QDateTime& now)
{
    if (!std::isfinite(remainingGameSeconds) || remainingGameSeconds < 0.0)
    {
        return unavailable;
    }
    const double remainingRealSeconds = remainingGameSeconds / 15.0;
    const QString duration = formatDurationHhmmss(remainingRealSeconds);
    const QString arrival = formatArrivalClock(remainingRealSeconds, now);
    return duration == unavailable || arrival == unavailable
        ? unavailable
        : QString("%1 - %2").arg(duration, arrival);
}

QString formatFuelRemainingPercent(double fuelAmount, double fuelCapacity)
{
    if (!std::isfinite(fuelAmount) || !std::isfinite(fuelCapacity) ||
        fuelAmount < 0.0 || fuelCapacity <= 0.0)
    {
        return unavailable;
    }
    return QString("%1%").arg(formatDisplayNumber((fuelAmount / fuelCapacity) * 100.0, 2));
}

QString formatFuelDisplay(double fuelAmount, double fuelCapacity)
{
    const QString amount = std::isfinite(fuelAmount) && fuelAmount >= 0.0
        ? QString("%1 L").arg(formatDisplayNumber(fuelAmount, 2))
        : unavailable;
    const QString capacity = std::isfinite(fuelCapacity) && fuelCapacity > 0.0
        ? QString("%1 L").arg(formatDisplayNumber(fuelCapacity, 2))
        : unavailable;
    return QString("%1 / %2 (%3)")
        .arg(amount, capacity, formatFuelRemainingPercent(fuelAmount, fuelCapacity));
}

bool isFuelAtOrBelowWarningThreshold(double fuelAmount, double fuelCapacity)
{
    if (!std::isfinite(fuelAmount) || !std::isfinite(fuelCapacity) ||
        fuelAmount < 0.0 || fuelCapacity <= 0.0)
    {
        return false;
    }
    return (fuelAmount / fuelCapacity) <= 0.20;
}

QString formatNormalizedPercent(const QString& rawValue, const QString& unit)
{
    double value = 0.0;
    if (!parseDoubleValue(rawValue, value))
    {
        return unavailable;
    }
    const QString normalizedUnit = unit.trimmed().toLower();
    if (!normalizedUnit.isEmpty() && normalizedUnit != "normalized" && normalizedUnit != "normalized [0,1]" && normalizedUnit != "0-1" && normalizedUnit != "0..1")
    {
        return uppercaseDisplayText(rawValue);
    }
    const double percent = std::clamp(value * 100.0, 0.0, 100.0);
    return QString("%1%").arg(formatDisplayNumber(percent, 2));
}

QString formatCruiseKmH(const QString& rawValue, const QString& unit)
{
    double value = 0.0;
    if (!parseDoubleValue(rawValue, value))
    {
        return unavailable;
    }
    const QString normalizedUnit = unit.trimmed().toLower();
    double kmh = value;
    if (normalizedUnit == "m/s" || normalizedUnit == "mps" || normalizedUnit == "metres per second" || normalizedUnit == "meters per second")
    {
        kmh = value * 3.6;
    }
    return QString("%1 KM/H").arg(formatDisplayNumber(kmh, 2));
}

} // namespace nlsi
