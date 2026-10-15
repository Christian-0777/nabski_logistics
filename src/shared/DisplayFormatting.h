#pragma once

#include <QDateTime>
#include <QString>

#include "UserPreferences.h"

namespace nlsi {

QString uppercaseDisplayText(const QString& text);
QString formatDisplayNumber(double value, int decimals = 2);
QString formatFixedNumber(const QString& value, const QString& unit, int decimals = 2);
QString formatCurrencyEuro(double value);
QString formatDistanceKilometres(double value, const QString& unit = QString());
QString formatProgressPercent(double plannedDistance, double remainingDistance);
QString formatDurationHhmmss(double seconds);
QString formatArrivalClock(double remainingRealSeconds, const QDateTime& now = QDateTime::currentDateTime());
QString formatArrivalEstimate(double remainingGameSeconds, const QDateTime& now = QDateTime::currentDateTime());
QString formatFuelRemainingPercent(double fuelAmount, double fuelCapacity);
QString formatFuelDisplay(double fuelAmount, double fuelCapacity);
QString formatFuelDisplay(double fuelAmount, double fuelCapacity,
                          const UserPreferences& preferences);
bool isFuelAtOrBelowWarningThreshold(double fuelAmount, double fuelCapacity);
QString formatNormalizedPercent(const QString& rawValue, const QString& unit = QString());
QString formatCruiseKmH(const QString& rawValue, const QString& unit = QString());
QString formatCruiseSpeed(const QString& rawValue, const QString& unit,
                          const UserPreferences& preferences);

} // namespace nlsi
