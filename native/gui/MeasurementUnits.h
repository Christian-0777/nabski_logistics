#pragma once

#include <cmath>
#include <optional>

#include <QDateTime>
#include <QList>
#include <QSettings>
#include <QString>

namespace nlsi::gui {

enum class MeasurementSystem {
    Metric,
    USCustomary
};

enum class MeasurementPreset {
    Metric,
    Imperial,
    Custom
};

enum class GameProfile {
    ETS2,
    ATS
};

enum class CurrencyUnit {
    EUR,
    USD
};

enum class DistanceUnit {
    Kilometers,
    Miles
};

enum class WeightUnit {
    Kilograms,
    Pounds
};

enum class CargoWeightUnit {
    MetricTonnes,
    ShortTons,
    LongTons,
    Pounds
};

enum class VolumeUnit {
    Liters,
    USGallons,
    ImperialGallons
};

enum class SpeedUnit {
    KilometersPerHour,
    MilesPerHour
};

struct RegionalSettings {
    GameProfile game_profile = GameProfile::ETS2;
    MeasurementPreset preset = MeasurementPreset::Metric;
    CurrencyUnit currency = CurrencyUnit::EUR;
    DistanceUnit distance = DistanceUnit::Kilometers;
    WeightUnit weight = WeightUnit::Kilograms;
    CargoWeightUnit cargo_weight = CargoWeightUnit::MetricTonnes;
    VolumeUnit volume = VolumeUnit::Liters;
    SpeedUnit speed = SpeedUnit::KilometersPerHour;
    QString number_format = QStringLiteral("locale_default");
    QString date_format = QStringLiteral("locale_default");
    QDateTime updated_at_utc = QDateTime::currentDateTimeUtc();
};

inline QString GameProfileKey(GameProfile profile) {
    return profile == GameProfile::ATS ? QStringLiteral("ats") : QStringLiteral("ets2");
}

inline GameProfile CurrentGameProfile() {
    QSettings settings(QStringLiteral("NLSI"), QStringLiteral("Exclusive Logbook"));
    const QString profile = settings.value(QStringLiteral("measurement/current_game"), QStringLiteral("ets2"))
        .toString();
    return profile.compare(QStringLiteral("ats"), Qt::CaseInsensitive) == 0
        ? GameProfile::ATS : GameProfile::ETS2;
}

inline void SetCurrentGameProfile(GameProfile profile) {
    QSettings settings(QStringLiteral("NLSI"), QStringLiteral("Exclusive Logbook"));
    settings.setValue(QStringLiteral("measurement/current_game"), GameProfileKey(profile));
}

inline QString ToString(CurrencyUnit currency) {
    return currency == CurrencyUnit::USD ? QStringLiteral("USD") : QStringLiteral("EUR");
}

inline QString ToString(DistanceUnit unit) {
    return unit == DistanceUnit::Miles ? QStringLiteral("mi") : QStringLiteral("km");
}

inline QString ToString(WeightUnit unit) {
    return unit == WeightUnit::Pounds ? QStringLiteral("lb") : QStringLiteral("kg");
}

inline QString ToString(CargoWeightUnit unit) {
    switch (unit) {
    case CargoWeightUnit::MetricTonnes:
        return QStringLiteral("metric_tonne");
    case CargoWeightUnit::ShortTons:
        return QStringLiteral("short_ton");
    case CargoWeightUnit::LongTons:
        return QStringLiteral("long_ton");
    case CargoWeightUnit::Pounds:
        return QStringLiteral("lb");
    }
    return QStringLiteral("metric_tonne");
}

inline QString ToString(VolumeUnit unit) {
    switch (unit) {
    case VolumeUnit::USGallons:
        return QStringLiteral("us_gal");
    case VolumeUnit::ImperialGallons:
        return QStringLiteral("imp_gal");
    case VolumeUnit::Liters:
    default:
        return QStringLiteral("L");
    }
}

inline QString ToString(SpeedUnit unit) {
    return unit == SpeedUnit::MilesPerHour ? QStringLiteral("mph") : QStringLiteral("km/h");
}

inline RegionalSettings DefaultRegionalSettings(GameProfile profile) {
    RegionalSettings defaults;
    defaults.game_profile = profile;
    defaults.number_format = QStringLiteral("locale_default");
    defaults.date_format = QStringLiteral("locale_default");
    defaults.updated_at_utc = QDateTime::currentDateTimeUtc();
    if (profile == GameProfile::ATS) {
        defaults.preset = MeasurementPreset::Imperial;
        defaults.currency = CurrencyUnit::USD;
        defaults.distance = DistanceUnit::Miles;
        defaults.weight = WeightUnit::Pounds;
        defaults.cargo_weight = CargoWeightUnit::ShortTons;
        defaults.volume = VolumeUnit::USGallons;
        defaults.speed = SpeedUnit::MilesPerHour;
        return defaults;
    }
    defaults.preset = MeasurementPreset::Metric;
    defaults.currency = CurrencyUnit::EUR;
    defaults.distance = DistanceUnit::Kilometers;
    defaults.weight = WeightUnit::Kilograms;
    defaults.cargo_weight = CargoWeightUnit::MetricTonnes;
    defaults.volume = VolumeUnit::Liters;
    defaults.speed = SpeedUnit::KilometersPerHour;
    return defaults;
}

inline QString MeasurementPresetKey(MeasurementPreset preset) {
    switch (preset) {
    case MeasurementPreset::Imperial:
        return QStringLiteral("imperial");
    case MeasurementPreset::Custom:
        return QStringLiteral("custom");
    case MeasurementPreset::Metric:
    default:
        return QStringLiteral("metric");
    }
}

inline MeasurementPreset ParsePreset(const QString& value) {
    if (value.compare(QStringLiteral("imperial"), Qt::CaseInsensitive) == 0) {
        return MeasurementPreset::Imperial;
    }
    if (value.compare(QStringLiteral("custom"), Qt::CaseInsensitive) == 0) {
        return MeasurementPreset::Custom;
    }
    return MeasurementPreset::Metric;
}

inline RegionalSettings LoadRegionalSettings(GameProfile profile = CurrentGameProfile()) {
    QSettings settings(QStringLiteral("NLSI"), QStringLiteral("Exclusive Logbook"));
    settings.beginGroup(QStringLiteral("measurement"));
    settings.beginGroup(GameProfileKey(profile));

    RegionalSettings loaded;
    loaded.game_profile = profile;
    loaded.preset = ParsePreset(settings.value(QStringLiteral("preset"), MeasurementPresetKey(DefaultRegionalSettings(profile).preset)).toString());
    loaded.currency = settings.value(QStringLiteral("currency"), ToString(DefaultRegionalSettings(profile).currency)).toString().compare(QStringLiteral("USD"), Qt::CaseInsensitive) == 0
        ? CurrencyUnit::USD : CurrencyUnit::EUR;
    loaded.distance = settings.value(QStringLiteral("distance"), ToString(DefaultRegionalSettings(profile).distance)).toString().compare(QStringLiteral("mi"), Qt::CaseInsensitive) == 0
        ? DistanceUnit::Miles : DistanceUnit::Kilometers;
    loaded.weight = settings.value(QStringLiteral("weight"), ToString(DefaultRegionalSettings(profile).weight)).toString().compare(QStringLiteral("lb"), Qt::CaseInsensitive) == 0
        ? WeightUnit::Pounds : WeightUnit::Kilograms;
    const QString cargo_raw = settings.value(QStringLiteral("cargo_weight"), ToString(DefaultRegionalSettings(profile).cargo_weight)).toString();
    if (cargo_raw.compare(QStringLiteral("short_ton"), Qt::CaseInsensitive) == 0) {
        loaded.cargo_weight = CargoWeightUnit::ShortTons;
    } else if (cargo_raw.compare(QStringLiteral("long_ton"), Qt::CaseInsensitive) == 0) {
        loaded.cargo_weight = CargoWeightUnit::LongTons;
    } else if (cargo_raw.compare(QStringLiteral("lb"), Qt::CaseInsensitive) == 0) {
        loaded.cargo_weight = CargoWeightUnit::Pounds;
    } else {
        loaded.cargo_weight = CargoWeightUnit::MetricTonnes;
    }
    const QString volume_raw = settings.value(QStringLiteral("volume"), ToString(DefaultRegionalSettings(profile).volume)).toString();
    if (volume_raw.compare(QStringLiteral("us_gal"), Qt::CaseInsensitive) == 0) {
        loaded.volume = VolumeUnit::USGallons;
    } else if (volume_raw.compare(QStringLiteral("imp_gal"), Qt::CaseInsensitive) == 0) {
        loaded.volume = VolumeUnit::ImperialGallons;
    } else {
        loaded.volume = VolumeUnit::Liters;
    }
    const QString speed_raw = settings.value(QStringLiteral("speed"), ToString(DefaultRegionalSettings(profile).speed)).toString();
    loaded.speed = speed_raw.compare(QStringLiteral("mph"), Qt::CaseInsensitive) == 0
        ? SpeedUnit::MilesPerHour : SpeedUnit::KilometersPerHour;
    loaded.number_format = settings.value(QStringLiteral("number_format"), QStringLiteral("locale_default")).toString();
    loaded.date_format = settings.value(QStringLiteral("date_format"), QStringLiteral("locale_default")).toString();
    loaded.updated_at_utc = QDateTime::fromString(
        settings.value(QStringLiteral("updated_at_utc")).toString(), Qt::ISODateWithMs);
    if (!loaded.updated_at_utc.isValid()) {
        loaded.updated_at_utc = QDateTime::currentDateTimeUtc();
    }
    return loaded;
}

inline bool IsValidRegionalSettings(
    const RegionalSettings& settings,
    QString* error = nullptr) {
    const bool valid_profile = settings.game_profile == GameProfile::ETS2
        || settings.game_profile == GameProfile::ATS;
    const bool valid_preset = settings.preset == MeasurementPreset::Metric
        || settings.preset == MeasurementPreset::Imperial
        || settings.preset == MeasurementPreset::Custom;
    const bool valid_currency = settings.currency == CurrencyUnit::EUR
        || settings.currency == CurrencyUnit::USD;
    const bool valid_distance = settings.distance == DistanceUnit::Kilometers
        || settings.distance == DistanceUnit::Miles;
    const bool valid_weight = settings.weight == WeightUnit::Kilograms
        || settings.weight == WeightUnit::Pounds;
    const bool valid_cargo_weight = settings.cargo_weight == CargoWeightUnit::MetricTonnes
        || settings.cargo_weight == CargoWeightUnit::ShortTons
        || settings.cargo_weight == CargoWeightUnit::LongTons
        || settings.cargo_weight == CargoWeightUnit::Pounds;
    const bool valid_volume = settings.volume == VolumeUnit::Liters
        || settings.volume == VolumeUnit::USGallons
        || settings.volume == VolumeUnit::ImperialGallons;
    const bool valid_speed = settings.speed == SpeedUnit::KilometersPerHour
        || settings.speed == SpeedUnit::MilesPerHour;
    const bool valid_number_format = settings.number_format == QStringLiteral("locale_default")
        || settings.number_format == QStringLiteral("decimal_dot")
        || settings.number_format == QStringLiteral("decimal_comma");
    const bool valid_date_format = settings.date_format == QStringLiteral("locale_default")
        || settings.date_format == QStringLiteral("iso_8601")
        || settings.date_format == QStringLiteral("us_short")
        || settings.date_format == QStringLiteral("eu_short");
    if (!valid_profile || !valid_preset || !valid_currency || !valid_distance
        || !valid_weight || !valid_cargo_weight || !valid_volume || !valid_speed
        || !valid_number_format || !valid_date_format) {
        if (error) {
            *error = QStringLiteral("One or more configuration values are invalid.");
        }
        return false;
    }
    if (!settings.updated_at_utc.isValid()) {
        if (error) {
            *error = QStringLiteral("The configuration update time is invalid.");
        }
        return false;
    }
    return true;
}

inline bool SaveRegionalSettings(
    const QList<RegionalSettings>& configurations,
    GameProfile active_profile,
    QString* error = nullptr) {
    if (active_profile != GameProfile::ETS2 && active_profile != GameProfile::ATS) {
        if (error) {
            *error = QStringLiteral("The selected game profile is invalid.");
        }
        return false;
    }
    if (configurations.isEmpty()) {
        if (error) {
            *error = QStringLiteral("There are no configuration values to save.");
        }
        return false;
    }
    for (const RegionalSettings& settings : configurations) {
        if (!IsValidRegionalSettings(settings, error)) {
            return false;
        }
    }

    QSettings qsettings(QStringLiteral("NLSI"), QStringLiteral("Exclusive Logbook"));
    for (const RegionalSettings& settings : configurations) {
        qsettings.beginGroup(QStringLiteral("measurement"));
        qsettings.beginGroup(GameProfileKey(settings.game_profile));
        qsettings.setValue(QStringLiteral("preset"), MeasurementPresetKey(settings.preset));
        qsettings.setValue(QStringLiteral("currency"), ToString(settings.currency));
        qsettings.setValue(QStringLiteral("distance"), ToString(settings.distance));
        qsettings.setValue(QStringLiteral("weight"), ToString(settings.weight));
        qsettings.setValue(QStringLiteral("cargo_weight"), ToString(settings.cargo_weight));
        qsettings.setValue(QStringLiteral("volume"), ToString(settings.volume));
        qsettings.setValue(QStringLiteral("speed"), ToString(settings.speed));
        qsettings.setValue(QStringLiteral("number_format"), settings.number_format);
        qsettings.setValue(QStringLiteral("date_format"), settings.date_format);
        qsettings.setValue(QStringLiteral("updated_at_utc"),
            settings.updated_at_utc.toUTC().toString(Qt::ISODateWithMs));
        qsettings.endGroup();
        qsettings.endGroup();
    }
    qsettings.setValue(QStringLiteral("measurement/current_game"), GameProfileKey(active_profile));
    qsettings.sync();
    if (qsettings.status() != QSettings::NoError) {
        if (error) {
            *error = qsettings.status() == QSettings::AccessError
                ? QStringLiteral("Windows denied access to the settings store.")
                : qsettings.status() == QSettings::FormatError
                    ? QStringLiteral("The settings store contains invalid data.")
                    : QStringLiteral("The settings store ran out of space while saving.");
        }
        return false;
    }
    return true;
}

inline bool SaveRegionalSettings(
    const RegionalSettings& settings,
    QString* error = nullptr) {
    return SaveRegionalSettings(
        {settings}, settings.game_profile, error);
}

inline MeasurementSystem CurrentMeasurementSystem() {
    const RegionalSettings settings = LoadRegionalSettings();
    return settings.preset == MeasurementPreset::Imperial
        || settings.distance == DistanceUnit::Miles
        || settings.weight == WeightUnit::Pounds
        || settings.speed == SpeedUnit::MilesPerHour
        || settings.volume == VolumeUnit::USGallons
        || settings.volume == VolumeUnit::ImperialGallons
        ? MeasurementSystem::USCustomary
        : MeasurementSystem::Metric;
}

inline void SetMeasurementSystem(MeasurementSystem system) {
    RegionalSettings settings = LoadRegionalSettings();
    settings.preset = system == MeasurementSystem::USCustomary
        ? MeasurementPreset::Imperial : MeasurementPreset::Metric;
    if (system == MeasurementSystem::USCustomary) {
        settings.distance = DistanceUnit::Miles;
        settings.weight = WeightUnit::Pounds;
        settings.cargo_weight = CargoWeightUnit::ShortTons;
        settings.volume = VolumeUnit::USGallons;
        settings.speed = SpeedUnit::MilesPerHour;
        settings.currency = CurrencyUnit::USD;
    } else {
        settings.distance = DistanceUnit::Kilometers;
        settings.weight = WeightUnit::Kilograms;
        settings.cargo_weight = CargoWeightUnit::MetricTonnes;
        settings.volume = VolumeUnit::Liters;
        settings.speed = SpeedUnit::KilometersPerHour;
        settings.currency = CurrencyUnit::EUR;
    }
    settings.updated_at_utc = QDateTime::currentDateTimeUtc();
    SaveRegionalSettings(settings);
}

inline double DisplayDistance(double kilometers, MeasurementSystem system) {
    return system == MeasurementSystem::USCustomary
        ? kilometers * 0.621371192237334
        : kilometers;
}

inline double DisplayDistance(double kilometers, const RegionalSettings& settings) {
    return settings.distance == DistanceUnit::Miles
        ? kilometers * 0.621371192237334
        : kilometers;
}

inline double DisplaySpeed(double kilometers_per_hour, MeasurementSystem system) {
    return DisplayDistance(kilometers_per_hour, system);
}

inline double DisplaySpeed(double kilometers_per_hour, const RegionalSettings& settings) {
    return settings.speed == SpeedUnit::MilesPerHour
        ? kilometers_per_hour * 0.621371192237334
        : kilometers_per_hour;
}

inline double DisplayFuelVolume(double liters, MeasurementSystem system) {
    return system == MeasurementSystem::USCustomary
        ? liters / 3.785411784
        : liters;
}

inline double DisplayFuelVolume(double liters, const RegionalSettings& settings) {
    switch (settings.volume) {
    case VolumeUnit::USGallons:
        return liters / 3.785411784;
    case VolumeUnit::ImperialGallons:
        return liters / 4.54609;
    case VolumeUnit::Liters:
    default:
        return liters;
    }
}

inline std::optional<double> DisplayFuelEconomy(
    double liters_per_100_kilometers,
    MeasurementSystem system) {
    if (system == MeasurementSystem::Metric) {
        return liters_per_100_kilometers;
    }
    if (!std::isfinite(liters_per_100_kilometers)
        || liters_per_100_kilometers <= 0.0) {
        return std::nullopt;
    }
    return 235.214583 / liters_per_100_kilometers;
}

inline std::optional<double> DisplayFuelEconomy(
    double liters_per_100_kilometers,
    const RegionalSettings& settings) {
    if (settings.volume == VolumeUnit::Liters) {
        return liters_per_100_kilometers;
    }
    if (!std::isfinite(liters_per_100_kilometers)
        || liters_per_100_kilometers <= 0.0) {
        return std::nullopt;
    }
    return 235.214583 / liters_per_100_kilometers;
}

inline double DisplayMassKilograms(double kilograms, MeasurementSystem system) {
    return system == MeasurementSystem::USCustomary
        ? kilograms * 2.20462262185
        : kilograms;
}

inline double DisplayMassKilograms(double kilograms, const RegionalSettings& settings) {
    switch (settings.weight) {
    case WeightUnit::Pounds:
        return kilograms * 2.20462262185;
    case WeightUnit::Kilograms:
    default:
        return kilograms;
    }
}

inline double DisplayCargoWeight(double kilograms, const RegionalSettings& settings) {
    switch (settings.cargo_weight) {
    case CargoWeightUnit::MetricTonnes:
        return kilograms / 1000.0;
    case CargoWeightUnit::ShortTons:
        return kilograms / 907.18474;
    case CargoWeightUnit::LongTons:
        return kilograms / 1016.0469088;
    case CargoWeightUnit::Pounds:
        return kilograms * 2.20462262185;
    }
    return kilograms;
}

inline QString DistanceUnit(MeasurementSystem system) {
    return system == MeasurementSystem::USCustomary
        ? QStringLiteral("mi") : QStringLiteral("km");
}

inline QString DistanceUnit(const RegionalSettings& settings) {
    return settings.distance == DistanceUnit::Miles ? QStringLiteral("mi") : QStringLiteral("km");
}

inline QString SpeedUnit(MeasurementSystem system) {
    return system == MeasurementSystem::USCustomary
        ? QStringLiteral("mph") : QStringLiteral("km/h");
}

inline QString SpeedUnit(const RegionalSettings& settings) {
    return settings.speed == SpeedUnit::MilesPerHour ? QStringLiteral("mph") : QStringLiteral("km/h");
}

inline QString FuelVolumeUnit(MeasurementSystem system) {
    return system == MeasurementSystem::USCustomary
        ? QStringLiteral("US gal") : QStringLiteral("L");
}

inline QString FuelVolumeUnit(const RegionalSettings& settings) {
    switch (settings.volume) {
    case VolumeUnit::USGallons:
        return QStringLiteral("US gal");
    case VolumeUnit::ImperialGallons:
        return QStringLiteral("Imp gal");
    case VolumeUnit::Liters:
    default:
        return QStringLiteral("L");
    }
}

inline QString FuelEconomyUnit(MeasurementSystem system) {
    return system == MeasurementSystem::USCustomary
        ? QStringLiteral("US mpg") : QStringLiteral("L/100 km");
}

inline QString FuelEconomyUnit(const RegionalSettings& settings) {
    return settings.volume == VolumeUnit::Liters
        ? QStringLiteral("L/100 km") : QStringLiteral("US mpg");
}

inline QString MassUnit(MeasurementSystem system) {
    return system == MeasurementSystem::USCustomary
        ? QStringLiteral("lb") : QStringLiteral("kg");
}

inline QString MassUnit(const RegionalSettings& settings) {
    return settings.weight == WeightUnit::Pounds ? QStringLiteral("lb") : QStringLiteral("kg");
}

inline QString CargoMassUnit(const RegionalSettings& settings) {
    switch (settings.cargo_weight) {
    case CargoWeightUnit::MetricTonnes:
        return QStringLiteral("t");
    case CargoWeightUnit::ShortTons:
        return QStringLiteral("st");
    case CargoWeightUnit::LongTons:
        return QStringLiteral("lt");
    case CargoWeightUnit::Pounds:
        return QStringLiteral("lb");
    }
    return QStringLiteral("t");
}

} // namespace nlsi::gui
