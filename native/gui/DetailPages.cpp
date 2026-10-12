#include "CurrentJobPage.h"
#include "TelemetryPage.h"
#include "ProvidersPage.h"
#include "SettingsPage.h"
#include "AboutPage.h"
#include "ActiveModsPage.h"

#include <cmath>

#include <QCoreApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSet>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QUrl>
#include <QTabWidget>
#include <QVBoxLayout>

#include "telemetry/TelemetryCore.h"
#include "updater/GitHubUpdater.h"
#include "MeasurementUnits.h"

namespace nlsi::gui {

class UpdateControls final : public QWidget {
public:
    explicit UpdateControls(QWidget* parent = nullptr)
        : QWidget(parent) {
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(14);
        button = new QPushButton(QStringLiteral("Check for Updates"), this);
        button->setObjectName(QStringLiteral("checkForUpdatesButton"));
        button->setAccessibleName(QStringLiteral("Check for Updates"));
        status = new QLabel(QStringLiteral("Checking for published releases…"), this);
        status->setObjectName(QStringLiteral("updateCheckStatus"));
        status->setWordWrap(true);
        status->setMinimumWidth(0);
        status->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        layout->addWidget(button);
        layout->addWidget(status, 1);
    }

    QPushButton* button = nullptr;
    QLabel* status = nullptr;
};

namespace {

QString ReleaseChannelText() {
    const QString version = QCoreApplication::applicationVersion();
    const qsizetype separator = version.lastIndexOf(QLatin1Char('-'));
    if (separator < 0 || separator == version.size() - 1) {
        return QStringLiteral("Unavailable");
    }
    QString channel = version.mid(separator + 1);
    channel[0] = channel[0].toUpper();
    return channel;
}

QString StatusText(telemetry::ProviderState state) {
    return QString::fromStdWString(telemetry::FormatStatus(state));
}

QString JobRoute(
    const telemetry::TelemetryField<std::wstring>& company,
    const telemetry::TelemetryField<std::wstring>& city) {
    const QString company_name = FieldText(company);
    const QString city_name = FieldText(city);
    if (company_name == QStringLiteral("--")) {
        return city_name;
    }
    if (city_name == QStringLiteral("--") || city_name == company_name) {
        return company_name;
    }
    return company_name + QStringLiteral(" · ") + city_name;
}

class ApplicationSettingsPage final : public DetailPage {
public:
    explicit ApplicationSettingsPage(QWidget* parent = nullptr)
        : DetailPage(parent) {
        AddField(QStringLiteral("version"), QStringLiteral("Application version"));
        AddField(QStringLiteral("channel"), QStringLiteral("Release channel"));
        AddField(QStringLiteral("refresh"), QStringLiteral("UI refresh rate"));
        AddField(QStringLiteral("transport"), QStringLiteral("Telemetry transport"));
        AddField(QStringLiteral("display"), QStringLiteral("Unavailable values"));
    }

    void UpdateState(const telemetry::TelemetryUiState&) override {
        SetValue(QStringLiteral("version"), QCoreApplication::applicationVersion());
        SetValue(QStringLiteral("channel"), ReleaseChannelText());
        SetValue(QStringLiteral("refresh"), QStringLiteral("4 Hz (250 ms)"));
        SetValue(QStringLiteral("transport"), QStringLiteral("TruckSim GPS shared memory"));
        SetValue(QStringLiteral("display"), QStringLiteral("Hidden when not supplied"));
    }
};

class ConfigPage final : public DetailPage {
public:
    explicit ConfigPage(QWidget* parent = nullptr)
        : DetailPage(parent) {
        if (QFrame* summary = findChild<QFrame*>(QStringLiteral("contentCard"))) {
            summary->hide();
        }

        auto* form = new QWidget(this);
        auto* form_layout = new QGridLayout(form);
        form_layout->setContentsMargins(0, 8, 0, 8);
        form_layout->setHorizontalSpacing(16);
        form_layout->setVerticalSpacing(10);

        const auto make_combo = [this](const QString& object_name) {
            auto* combo = new QComboBox(this);
            combo->setObjectName(object_name);
            combo->setMinimumWidth(190);
            combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            return combo;
        };

        int row = 0;
        game_profile_ = make_combo(QStringLiteral("configGameProfile"));
        game_profile_->addItem(QStringLiteral("ETS2"), QStringLiteral("ets2"));
        game_profile_->addItem(QStringLiteral("ATS"), QStringLiteral("ats"));
        form_layout->addWidget(new QLabel(QStringLiteral("GAME PROFILE"), this), row, 0);
        form_layout->addWidget(game_profile_, row, 1);
        ++row;

        unit_preset_ = make_combo(QStringLiteral("configUnitPreset"));
        unit_preset_->addItem(QStringLiteral("Metric"), QStringLiteral("metric"));
        unit_preset_->addItem(QStringLiteral("Imperial"), QStringLiteral("imperial"));
        unit_preset_->addItem(QStringLiteral("Custom"), QStringLiteral("custom"));
        form_layout->addWidget(new QLabel(QStringLiteral("UNIT PRESET"), this), row, 0);
        form_layout->addWidget(unit_preset_, row, 1);
        ++row;

        currency_ = make_combo(QStringLiteral("configCurrency"));
        currency_->addItem(QStringLiteral("EUR (€)"), QStringLiteral("EUR"));
        currency_->addItem(QStringLiteral("USD ($)"), QStringLiteral("USD"));
        form_layout->addWidget(new QLabel(QStringLiteral("CURRENCY"), this), row, 0);
        form_layout->addWidget(currency_, row, 1);
        ++row;

        distance_ = make_combo(QStringLiteral("configDistance"));
        distance_->addItem(QStringLiteral("Kilometers (km)"), QStringLiteral("km"));
        distance_->addItem(QStringLiteral("Miles (mi)"), QStringLiteral("mi"));
        form_layout->addWidget(new QLabel(QStringLiteral("DISTANCE"), this), row, 0);
        form_layout->addWidget(distance_, row, 1);
        ++row;

        weight_ = make_combo(QStringLiteral("configWeight"));
        weight_->addItem(QStringLiteral("Kilograms (kg)"), QStringLiteral("kg"));
        weight_->addItem(QStringLiteral("Pounds (lb)"), QStringLiteral("lb"));
        form_layout->addWidget(new QLabel(QStringLiteral("WEIGHT"), this), row, 0);
        form_layout->addWidget(weight_, row, 1);
        ++row;

        cargo_weight_ = make_combo(QStringLiteral("configCargoWeight"));
        cargo_weight_->addItem(QStringLiteral("Metric tonnes (t)"), QStringLiteral("metric_tonne"));
        cargo_weight_->addItem(QStringLiteral("Short tons"), QStringLiteral("short_ton"));
        cargo_weight_->addItem(QStringLiteral("Long tons"), QStringLiteral("long_ton"));
        cargo_weight_->addItem(QStringLiteral("Pounds (lb)"), QStringLiteral("lb"));
        form_layout->addWidget(new QLabel(QStringLiteral("CARGO WEIGHT"), this), row, 0);
        form_layout->addWidget(cargo_weight_, row, 1);
        ++row;

        volume_ = make_combo(QStringLiteral("configVolume"));
        volume_->addItem(QStringLiteral("Liters (L)"), QStringLiteral("L"));
        volume_->addItem(QStringLiteral("US gallons (US gal)"), QStringLiteral("us_gal"));
        volume_->addItem(QStringLiteral("Imperial gallons (Imp gal)"), QStringLiteral("imp_gal"));
        form_layout->addWidget(new QLabel(QStringLiteral("VOLUME"), this), row, 0);
        form_layout->addWidget(volume_, row, 1);
        ++row;

        speed_ = make_combo(QStringLiteral("configSpeed"));
        speed_->addItem(QStringLiteral("Kilometers/hour (km/h)"), QStringLiteral("km/h"));
        speed_->addItem(QStringLiteral("Miles/hour (mph)"), QStringLiteral("mph"));
        form_layout->addWidget(new QLabel(QStringLiteral("SPEED"), this), row, 0);
        form_layout->addWidget(speed_, row, 1);
        ++row;

        number_format_ = make_combo(QStringLiteral("configNumberFormat"));
        number_format_->addItem(QStringLiteral("Locale default"), QStringLiteral("locale_default"));
        number_format_->addItem(QStringLiteral("1,234.56"), QStringLiteral("decimal_dot"));
        number_format_->addItem(QStringLiteral("1 234,56"), QStringLiteral("decimal_comma"));
        form_layout->addWidget(new QLabel(QStringLiteral("NUMBER FORMAT"), this), row, 0);
        form_layout->addWidget(number_format_, row, 1);
        ++row;

        date_format_ = make_combo(QStringLiteral("configDateFormat"));
        date_format_->addItem(QStringLiteral("Locale default"), QStringLiteral("locale_default"));
        date_format_->addItem(QStringLiteral("YYYY-MM-DD"), QStringLiteral("iso_8601"));
        date_format_->addItem(QStringLiteral("MM/DD/YYYY"), QStringLiteral("us_short"));
        date_format_->addItem(QStringLiteral("DD/MM/YYYY"), QStringLiteral("eu_short"));
        form_layout->addWidget(new QLabel(QStringLiteral("DATE FORMAT"), this), row, 0);
        form_layout->addWidget(date_format_, row, 1);
        ++row;

        AddContentWidget(form);

        auto* actions = new QHBoxLayout();
        actions->setSpacing(12);
        save_button_ = new QPushButton(QStringLiteral("SAVE"), this);
        save_button_->setObjectName(QStringLiteral("configSaveButton"));
        reset_button_ = new QPushButton(QStringLiteral("RESET"), this);
        reset_button_->setObjectName(QStringLiteral("configResetButton"));
        actions->addStretch(1);
        actions->addWidget(save_button_);
        actions->addWidget(reset_button_);
        auto* actions_widget = new QWidget(this);
        actions_widget->setLayout(actions);
        AddContentWidget(actions_widget);

        auto* status_row = new QWidget(this);
        auto* status_layout = new QVBoxLayout(status_row);
        status_layout->setContentsMargins(0, 12, 0, 0);
        status_layout->setSpacing(4);
        status_label_ = new QLabel(QStringLiteral("Configuration Status: Saved"), status_row);
        status_label_->setObjectName(QStringLiteral("configStatusLabel"));
        updated_label_ = new QLabel(QStringLiteral("Last Updated: --"), status_row);
        updated_label_->setObjectName(QStringLiteral("configUpdatedLabel"));
        status_layout->addWidget(status_label_);
        status_layout->addWidget(updated_label_);
        AddContentWidget(status_row);

        displayed_profile_ = CurrentGameProfile();
        ApplyRegionalSettings(LoadRegionalSettings(displayed_profile_));
        persisted_profile_ = displayed_profile_;
        UpdateTimestamp(LoadRegionalSettings(displayed_profile_));

        connect(game_profile_, &QComboBox::currentIndexChanged, this, [this](int) {
            const GameProfile previous_profile = displayed_profile_;
            if (dirty_profiles_.contains(GameProfileKey(previous_profile))) {
                drafts_.insert(GameProfileKey(previous_profile),
                    ReadCurrentSelection(previous_profile));
            }
            displayed_profile_ = game_profile_->currentData().toString()
                .compare(QStringLiteral("ats"), Qt::CaseInsensitive) == 0
                ? GameProfile::ATS : GameProfile::ETS2;
            const QString profile_key = GameProfileKey(displayed_profile_);
            const RegionalSettings settings = drafts_.contains(profile_key)
                ? drafts_.value(profile_key)
                : LoadRegionalSettings(displayed_profile_);
            ApplyRegionalSettings(settings);
            UpdatePendingStatus();
        });
        connect(unit_preset_, &QComboBox::currentIndexChanged, this, [this](int index) {
            const auto preset = static_cast<MeasurementPreset>(
                index == 1 ? 1 : index == 2 ? 2 : 0);
            if (preset == MeasurementPreset::Custom) {
                MarkCurrentProfilePending();
                return;
            }
            ApplyPreset(preset);
            MarkCurrentProfilePending();
        });

        const auto bind_measurement_change = [this](QComboBox* combo) {
            connect(combo, &QComboBox::currentIndexChanged, this, [this]() {
                if (unit_preset_ && unit_preset_->currentData().toString() != QStringLiteral("custom")) {
                    unit_preset_->setCurrentIndex(2);
                }
                MarkCurrentProfilePending();
            });
        };
        bind_measurement_change(currency_);
        bind_measurement_change(distance_);
        bind_measurement_change(weight_);
        bind_measurement_change(cargo_weight_);
        bind_measurement_change(volume_);
        bind_measurement_change(speed_);
        const auto bind_format_change = [this](QComboBox* combo) {
            connect(combo, &QComboBox::currentIndexChanged, this, [this]() {
                MarkCurrentProfilePending();
            });
        };
        bind_format_change(number_format_);
        bind_format_change(date_format_);
        connect(save_button_, &QPushButton::clicked, this, [this]() {
            SavePendingSettings();
        });
        connect(reset_button_, &QPushButton::clicked, this, [this]() {
            ResetCurrentProfile();
        });
    }

    void UpdateState(const telemetry::TelemetryUiState&) override {}

private:
    RegionalSettings ReadCurrentSelection(GameProfile profile) const {
        RegionalSettings settings = LoadRegionalSettings(profile);
        settings.game_profile = profile;
        settings.preset = ParsePreset(unit_preset_->currentData().toString());
        settings.currency = currency_->currentData().toString() == QStringLiteral("USD")
            ? CurrencyUnit::USD : CurrencyUnit::EUR;
        settings.distance = distance_->currentData().toString() == QStringLiteral("mi")
            ? DistanceUnit::Miles : DistanceUnit::Kilometers;
        settings.weight = weight_->currentData().toString() == QStringLiteral("lb")
            ? WeightUnit::Pounds : WeightUnit::Kilograms;
        const QString cargo_value = cargo_weight_->currentData().toString();
        settings.cargo_weight = cargo_value == QStringLiteral("short_ton")
            ? CargoWeightUnit::ShortTons
            : cargo_value == QStringLiteral("long_ton")
                ? CargoWeightUnit::LongTons
                : cargo_value == QStringLiteral("lb")
                    ? CargoWeightUnit::Pounds
                    : CargoWeightUnit::MetricTonnes;
        const QString volume_value = volume_->currentData().toString();
        settings.volume = volume_value == QStringLiteral("us_gal")
            ? VolumeUnit::USGallons
            : volume_value == QStringLiteral("imp_gal")
                ? VolumeUnit::ImperialGallons
                : VolumeUnit::Liters;
        settings.speed = speed_->currentData().toString() == QStringLiteral("mph")
            ? SpeedUnit::MilesPerHour : SpeedUnit::KilometersPerHour;
        settings.number_format = number_format_->currentData().toString();
        settings.date_format = date_format_->currentData().toString();
        return settings;
    }

    void ApplyPreset(MeasurementPreset preset) {
        RegionalSettings settings = ReadCurrentSelection(displayed_profile_);
        settings.preset = preset;
        if (preset == MeasurementPreset::Imperial) {
            settings.currency = CurrencyUnit::USD;
            settings.distance = DistanceUnit::Miles;
            settings.weight = WeightUnit::Pounds;
            settings.cargo_weight = CargoWeightUnit::ShortTons;
            settings.volume = VolumeUnit::USGallons;
            settings.speed = SpeedUnit::MilesPerHour;
        } else if (preset == MeasurementPreset::Metric) {
            settings.currency = CurrencyUnit::EUR;
            settings.distance = DistanceUnit::Kilometers;
            settings.weight = WeightUnit::Kilograms;
            settings.cargo_weight = CargoWeightUnit::MetricTonnes;
            settings.volume = VolumeUnit::Liters;
            settings.speed = SpeedUnit::KilometersPerHour;
        }
        ApplyRegionalSettings(settings);
    }

    void MarkCurrentProfilePending() {
        dirty_profiles_.insert(GameProfileKey(displayed_profile_));
        drafts_.insert(GameProfileKey(displayed_profile_),
            ReadCurrentSelection(displayed_profile_));
        UpdatePendingStatus();
    }

    void UpdatePendingStatus() {
        const bool has_pending_changes = !dirty_profiles_.isEmpty()
            || displayed_profile_ != persisted_profile_;
        status_label_->setText(has_pending_changes
            ? QStringLiteral("Configuration Status: Unsaved changes")
            : QStringLiteral("Configuration Status: Saved"));
    }

    void UpdateTimestamp(const RegionalSettings& settings) {
        const QString updated = settings.updated_at_utc.isValid()
            ? settings.updated_at_utc.toUTC().toString(
                QStringLiteral("yyyy-MM-dd HH:mm:ss UTC"))
            : QStringLiteral("--");
        updated_label_->setText(QStringLiteral("Last Updated: %1").arg(updated));
    }

    void SavePendingSettings() {
        QString error;
        if (!ValidateForm(&error)) {
            status_label_->setText(
                QStringLiteral("Configuration Status: Save failed: %1").arg(error));
            return;
        }
        QList<RegionalSettings> configurations;
        for (const QString& profile_key : dirty_profiles_) {
            const RegionalSettings settings = profile_key == GameProfileKey(displayed_profile_)
                ? ReadCurrentSelection(displayed_profile_)
                : drafts_.value(profile_key);
            configurations.append(settings);
        }
        const QString active_profile_key = GameProfileKey(displayed_profile_);
        if (!dirty_profiles_.contains(active_profile_key)) {
            configurations.append(ReadCurrentSelection(displayed_profile_));
        }
        for (RegionalSettings& settings : configurations) {
            settings.updated_at_utc = QDateTime::currentDateTimeUtc();
        }
        if (!SaveRegionalSettings(configurations, displayed_profile_, &error)) {
            status_label_->setText(
                QStringLiteral("Configuration Status: Save failed: %1").arg(error));
            return;
        }
        dirty_profiles_.clear();
        drafts_.clear();
        persisted_profile_ = displayed_profile_;
        UpdateTimestamp(LoadRegionalSettings(displayed_profile_));
        status_label_->setText(QStringLiteral("Configuration Status: Saved successfully"));
    }

    void ResetCurrentProfile() {
        RegionalSettings defaults = DefaultRegionalSettings(displayed_profile_);
        defaults.updated_at_utc = QDateTime::currentDateTimeUtc();
        QString error;
        if (!SaveRegionalSettings(defaults, &error)) {
            status_label_->setText(
                QStringLiteral("Configuration Status: Reset failed: %1").arg(error));
            return;
        }
        const QString reset_profile_key = GameProfileKey(displayed_profile_);
        dirty_profiles_.remove(reset_profile_key);
        drafts_.remove(reset_profile_key);
        ApplyRegionalSettings(defaults);
        persisted_profile_ = displayed_profile_;
        UpdateTimestamp(defaults);
        status_label_->setText(dirty_profiles_.isEmpty()
            ? QStringLiteral("Configuration Status: Reset successfully")
            : QStringLiteral(
                "Configuration Status: Reset successful; other profile edits remain unsaved"));
    }

    bool ValidateForm(QString* error) const {
        const QList<QPair<QComboBox*, QString>> controls = {
            {game_profile_, QStringLiteral("Game Profile")},
            {unit_preset_, QStringLiteral("Unit Preset")},
            {currency_, QStringLiteral("Currency")},
            {distance_, QStringLiteral("Distance")},
            {weight_, QStringLiteral("Weight")},
            {cargo_weight_, QStringLiteral("Cargo Weight")},
            {volume_, QStringLiteral("Volume")},
            {speed_, QStringLiteral("Speed")},
            {number_format_, QStringLiteral("Number Format")},
            {date_format_, QStringLiteral("Date Format")},
        };
        for (const auto& [combo, label] : controls) {
            if (!combo || combo->currentIndex() < 0
                || !combo->currentData().isValid()
                || combo->findData(combo->currentData()) != combo->currentIndex()) {
                if (error) {
                    *error = QStringLiteral("Select a valid %1 value.").arg(label);
                }
                return false;
            }
        }
        return true;
    }

    void ApplyRegionalSettings(const RegionalSettings& settings) {
        const auto select_value = [](QComboBox* combo, const QString& value) {
            if (!combo) {
                return;
            }
            const int index = combo->findData(value);
            if (index >= 0) {
                combo->setCurrentIndex(index);
            }
        };

        const QSignalBlocker game_profile_blocker(game_profile_);
        const QSignalBlocker preset_blocker(unit_preset_);
        const QSignalBlocker currency_blocker(currency_);
        const QSignalBlocker distance_blocker(distance_);
        const QSignalBlocker weight_blocker(weight_);
        const QSignalBlocker cargo_weight_blocker(cargo_weight_);
        const QSignalBlocker volume_blocker(volume_);
        const QSignalBlocker speed_blocker(speed_);
        const QSignalBlocker number_format_blocker(number_format_);
        const QSignalBlocker date_format_blocker(date_format_);
        game_profile_->setCurrentIndex(
            settings.game_profile == GameProfile::ATS ? 1 : 0);
        unit_preset_->setCurrentIndex(settings.preset == MeasurementPreset::Imperial
            ? 1 : settings.preset == MeasurementPreset::Custom ? 2 : 0);
        select_value(currency_, settings.currency == CurrencyUnit::USD ? QStringLiteral("USD") : QStringLiteral("EUR"));
        select_value(distance_, settings.distance == DistanceUnit::Miles ? QStringLiteral("mi") : QStringLiteral("km"));
        select_value(weight_, settings.weight == WeightUnit::Pounds ? QStringLiteral("lb") : QStringLiteral("kg"));
        select_value(cargo_weight_, settings.cargo_weight == CargoWeightUnit::MetricTonnes
            ? QStringLiteral("metric_tonne")
            : settings.cargo_weight == CargoWeightUnit::ShortTons
                ? QStringLiteral("short_ton")
                : settings.cargo_weight == CargoWeightUnit::LongTons
                    ? QStringLiteral("long_ton")
                    : QStringLiteral("lb"));
        select_value(volume_, settings.volume == VolumeUnit::USGallons
            ? QStringLiteral("us_gal")
            : settings.volume == VolumeUnit::ImperialGallons
                ? QStringLiteral("imp_gal")
                : QStringLiteral("L"));
        select_value(speed_, settings.speed == SpeedUnit::MilesPerHour ? QStringLiteral("mph") : QStringLiteral("km/h"));
        select_value(number_format_, settings.number_format);
        select_value(date_format_, settings.date_format);
    }

    QComboBox* game_profile_ = nullptr;
    QComboBox* unit_preset_ = nullptr;
    QComboBox* currency_ = nullptr;
    QComboBox* distance_ = nullptr;
    QComboBox* weight_ = nullptr;
    QComboBox* cargo_weight_ = nullptr;
    QComboBox* volume_ = nullptr;
    QComboBox* speed_ = nullptr;
    QComboBox* number_format_ = nullptr;
    QComboBox* date_format_ = nullptr;
    QPushButton* save_button_ = nullptr;
    QPushButton* reset_button_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* updated_label_ = nullptr;
    GameProfile displayed_profile_ = GameProfile::ETS2;
    GameProfile persisted_profile_ = GameProfile::ETS2;
    QHash<QString, RegionalSettings> drafts_;
    QSet<QString> dirty_profiles_;
};

} // namespace

CurrentJobPage::CurrentJobPage(QWidget* parent)
    : DetailPage(parent) {
    AddField(QStringLiteral("status"), QStringLiteral("Status"));
    AddField(QStringLiteral("nlsiJobId"), QStringLiteral("NLSI job ID"));
    AddField(QStringLiteral("cargo"), QStringLiteral("Cargo"));
    AddField(QStringLiteral("cargoId"), QStringLiteral("Cargo ID"));
    AddField(QStringLiteral("source"), QStringLiteral("Source"));
    AddField(QStringLiteral("destination"), QStringLiteral("Destination"));
    AddField(QStringLiteral("income"), QStringLiteral("Income"));
    AddField(QStringLiteral("planned"), QStringLiteral("Planned distance"));
    AddField(QStringLiteral("loaded"), QStringLiteral("Loaded"));
    AddField(QStringLiteral("remaining"), QStringLiteral("Remaining distance"));
    AddField(QStringLiteral("progress"), QStringLiteral("Progress"));
    AddField(QStringLiteral("eta"), QStringLiteral("ETA"));
}

void CurrentJobPage::UpdateState(const telemetry::TelemetryUiState& state) {
    const MeasurementSystem units = CurrentMeasurementSystem();
    SetValue(QStringLiteral("status"),
        QString::fromStdWString(telemetry::FormatJobStatus(state.job_status)));
    SetValue(QStringLiteral("nlsiJobId"), state.job.nlsi_job_id.empty()
        ? QStringLiteral("--")
        : QString::fromStdWString(state.job.nlsi_job_id));
    SetValue(QStringLiteral("cargo"), FieldText(state.job.cargo));
    SetValue(QStringLiteral("cargoId"), FieldText(state.job.cargo_id));
    SetValue(QStringLiteral("source"), JobRoute(state.job.source_company, state.job.source_city));
    SetValue(QStringLiteral("destination"),
        JobRoute(state.job.destination_company, state.job.destination_city));
    SetValue(QStringLiteral("income"), NumericText(FieldText(state.job.income)));
    bool planned_distance_valid = false;
    const double planned_distance = state.job.planned_distance.available
        ? QString::fromStdWString(state.job.planned_distance.value)
            .replace(QLatin1Char(','), QString())
            .toDouble(&planned_distance_valid)
        : 0.0;
    SetValue(QStringLiteral("planned"),
        planned_distance_valid && std::isfinite(planned_distance)
            ? FormatNumber(DisplayDistance(planned_distance, units), 2)
                + QLatin1Char(' ') + DistanceUnit(units)
            : QStringLiteral("--"));
    const QString loaded = !state.job.loaded.available
        ? QStringLiteral("--")
        : (state.job.loaded.value ? QStringLiteral("Yes") : QStringLiteral("No"));
    SetValue(QStringLiteral("loaded"), loaded);
    SetValue(QStringLiteral("remaining"),
        state.progress.remaining_distance_km
                && std::isfinite(*state.progress.remaining_distance_km)
            ? FormatNumber(DisplayDistance(*state.progress.remaining_distance_km, units), 2)
                + QLatin1Char(' ') + DistanceUnit(units)
            : QStringLiteral("--"));
    SetValue(QStringLiteral("progress"),
        OptionalNumberText(state.progress.progress_percent, 2, QStringLiteral("%")));
    SetValue(QStringLiteral("eta"), DurationText(state.progress.eta_seconds));
}

TelemetryPage::TelemetryPage(QWidget* parent)
    : DetailPage(parent) {
    AddField(QStringLiteral("game"), QStringLiteral("Game"));
    AddField(QStringLiteral("sample"), QStringLiteral("Last sample"));
    AddField(QStringLiteral("speed"), QStringLiteral("Speed"));
    AddField(QStringLiteral("rpm"), QStringLiteral("RPM"));
    AddField(QStringLiteral("gear"), QStringLiteral("Gear"));
    AddField(QStringLiteral("throttle"), QStringLiteral("Effective throttle"));
    AddField(QStringLiteral("brake"), QStringLiteral("Effective brake"));
    AddField(QStringLiteral("retarder"), QStringLiteral("Retarder level"));
    AddField(QStringLiteral("cruise"), QStringLiteral("Cruise control speed"));
    AddField(QStringLiteral("fuel"), QStringLiteral("Fuel"));
    AddField(QStringLiteral("parkingBrake"), QStringLiteral("Parking brake"));
    AddField(QStringLiteral("fuelRange"), QStringLiteral("Fuel range"));
    AddField(QStringLiteral("odometer"), QStringLiteral("Odometer"));
    AddField(QStringLiteral("navigation"), QStringLiteral("Navigation distance"));
    AddField(QStringLiteral("navigationTime"), QStringLiteral("Navigation time"));
    AddField(QStringLiteral("eta"), QStringLiteral("Calculated ETA"));
}

void TelemetryPage::UpdateState(const telemetry::TelemetryUiState& state) {
    const MeasurementSystem units = CurrentMeasurementSystem();
    const auto& values = state.fast.values;
    SetValue(QStringLiteral("game"), FieldText(values.game_name));
    SetValue(QStringLiteral("sample"), TimestampText(QString::fromStdWString(values.timestamp)));
    SetValue(QStringLiteral("speed"),
        values.speed_kmh.available && std::isfinite(values.speed_kmh.value)
            ? FormatNumber(DisplaySpeed(values.speed_kmh.value, units), 2)
                + QLatin1Char(' ') + SpeedUnit(units)
            : QStringLiteral("--"));
    SetValue(QStringLiteral("rpm"), NumberText(values.rpm, 0));
    SetValue(QStringLiteral("gear"), NumberText(values.gear, 0));
    const auto percentage_text = [](const telemetry::TelemetryField<double>& field) {
        if (!field.available || !std::isfinite(field.value)) {
            return QStringLiteral("--");
        }
        QString text = FormatNumber(field.value * 100.0, 2) + QLatin1Char('%');
        if (field.stale) {
            text += QStringLiteral("  · stale");
        }
        return text;
    };
    SetValue(QStringLiteral("throttle"), percentage_text(values.effective_throttle));
    SetValue(QStringLiteral("brake"), percentage_text(values.effective_brake));
    SetValue(QStringLiteral("retarder"), NumberText(values.retarder_level, 0));
    SetValue(QStringLiteral("cruise"),
        values.cruise_control_speed.available && std::isfinite(values.cruise_control_speed.value)
            ? FormatNumber(DisplaySpeed(values.cruise_control_speed.value, units), 2)
                + QLatin1Char(' ') + SpeedUnit(units)
            : QStringLiteral("--"));
    const auto fuel_value = [&units](const telemetry::TelemetryField<double>& field) {
        return field.available && !field.stale && std::isfinite(field.value)
                && field.value >= 0.0
            ? FormatNumber(DisplayFuelVolume(field.value, units), 2)
                + QLatin1Char(' ') + FuelVolumeUnit(units)
            : QStringLiteral("--");
    };
    const auto distance_value = [&units](const telemetry::TelemetryField<double>& field) {
        return field.available && !field.stale && std::isfinite(field.value)
                && field.value >= 0.0
            ? FormatNumber(DisplayDistance(field.value, units), 2)
                + QLatin1Char(' ') + DistanceUnit(units)
            : QStringLiteral("--");
    };
    SetValue(QStringLiteral("fuel"), fuel_value(values.fuel_liters));
    SetValue(QStringLiteral("parkingBrake"),
        values.parking_brake.available && !values.parking_brake.stale
            ? (values.parking_brake.value
                ? QStringLiteral("Engaged") : QStringLiteral("Released"))
            : QStringLiteral("Unknown"));
    SetValue(QStringLiteral("fuelRange"), distance_value(values.fuel_range_km));
    SetValue(QStringLiteral("odometer"), distance_value(values.odometer_km));
    SetValue(QStringLiteral("navigation"),
        distance_value(values.navigation_distance_km));
    SetValue(QStringLiteral("navigationTime"),
        values.navigation_time_s.available && !values.navigation_time_s.stale
            ? DurationText(values.navigation_time_s.value)
            : QStringLiteral("--"));
    SetValue(QStringLiteral("eta"),
        values.eta_seconds.available && !values.eta_seconds.stale
            ? DurationText(values.eta_seconds.value)
            : QStringLiteral("--"));
}

ProvidersPage::ProvidersPage(QWidget* parent)
    : DetailPage(parent) {
    AddField(QStringLiteral("trucksim"), QStringLiteral("TruckSim GPS"));
    AddField(QStringLiteral("nlsi"), QStringLiteral("NLSI SCS telemetry"));
    AddField(QStringLiteral("nlsiError"), QStringLiteral("NLSI provider message"));
    AddField(QStringLiteral("scsSdk"), QStringLiteral("SCS SDK telemetry"));
    AddField(QStringLiteral("scsSdkLastSeen"), QStringLiteral("Last SCS SDK sample"));
    AddField(QStringLiteral("scsSdkError"), QStringLiteral("SCS SDK provider message"));
    AddField(QStringLiteral("scsPosition"), QStringLiteral("SCS position provider"));
    AddField(QStringLiteral("scsPositionMapping"), QStringLiteral("Position interface"));
    AddField(QStringLiteral("scsPositionAge"), QStringLiteral("Position sample age"));
    AddField(QStringLiteral("scsPositionError"), QStringLiteral("Position provider message"));
    AddField(QStringLiteral("combined"), QStringLiteral("Combined"));
    AddField(QStringLiteral("freshness"), QStringLiteral("Telemetry sample"));
    AddField(QStringLiteral("source"), QStringLiteral("Active telemetry source"));
    AddField(QStringLiteral("mapping"), QStringLiteral("TruckSim GPS mapping"));
    AddField(QStringLiteral("mappingOpen"), QStringLiteral("Mapping handle"));
    AddField(QStringLiteral("viewMapped"), QStringLiteral("Shared-memory view"));
    AddField(QStringLiteral("layout"), QStringLiteral("Detected layout"));
    AddField(QStringLiteral("failureStage"), QStringLiteral("TruckSim GPS stage"));
    AddField(QStringLiteral("lastRead"), QStringLiteral("Last successful read"));
    AddField(QStringLiteral("dataAge"), QStringLiteral("Source data age"));
    AddField(QStringLiteral("sourceTimestamp"), QStringLiteral("Plugin timestamps"));
    AddField(QStringLiteral("win32"), QStringLiteral("Windows error"));
    AddField(QStringLiteral("storage"), QStringLiteral("Local logs and history"));
    AddField(QStringLiteral("sync"), QStringLiteral("Online synchronization"));
    AddField(QStringLiteral("error"), QStringLiteral("Last provider message"));
}

void ProvidersPage::UpdateState(const telemetry::TelemetryUiState& state) {
    SetValue(QStringLiteral("trucksim"), StatusText(state.providers.trucksim));
    SetValue(QStringLiteral("nlsi"), StatusText(state.providers.nlsi));
    SetValue(QStringLiteral("nlsiError"),
        state.providers.nlsi_error.empty()
            ? QStringLiteral("None")
            : QString::fromStdWString(state.providers.nlsi_error));
    SetValue(QStringLiteral("scsSdk"), StatusText(state.providers.scs_sdk));
    SetValue(QStringLiteral("scsSdkLastSeen"),
        state.providers.scs_sdk_last_seen.empty()
            ? QStringLiteral("Never")
            : TimestampText(QString::fromStdWString(state.providers.scs_sdk_last_seen)));
    SetValue(QStringLiteral("scsSdkError"),
        state.providers.scs_sdk_error.empty()
            ? QStringLiteral("None")
            : QString::fromStdWString(state.providers.scs_sdk_error));
    const auto& position = state.scs_position;
    const QString position_status = position.state == providers::ScsPositionState::Connected
        ? QStringLiteral("Connected")
        : position.state == providers::ScsPositionState::Stale
            ? QStringLiteral("Stale")
            : QStringLiteral("Disconnected");
    SetValue(QStringLiteral("scsPosition"), position_status);
    SetValue(QStringLiteral("scsPositionMapping"),
        QStringLiteral("Local\\NLSI.SCS.Position.v1"));
    SetValue(QStringLiteral("scsPositionAge"),
        position.available
            ? QStringLiteral("%1 ms").arg(position.age_ms)
            : QStringLiteral("Unavailable"));
    SetValue(QStringLiteral("scsPositionError"),
        position.error.empty() ? QStringLiteral("None")
            : QString::fromStdWString(position.error));
    SetValue(QStringLiteral("combined"),
        QString::fromStdWString(telemetry::FormatCombinedStatus(state.providers.combined)));
    SetValue(QStringLiteral("freshness"),
        TimestampText(QString::fromStdWString(state.providers.telemetry_freshness)));
    const auto& values = state.fast.values;
    SetValue(QStringLiteral("source"),
        values.speed_kmh.available ? QString::fromStdWString(values.speed_kmh.source) : QStringLiteral("--"));
    SetValue(QStringLiteral("mapping"),
        QString::fromStdWString(state.providers.trucksim_mapping_name));
    SetValue(QStringLiteral("mappingOpen"),
        state.providers.trucksim_mapping_open ? QStringLiteral("Open") : QStringLiteral("Closed"));
    SetValue(QStringLiteral("viewMapped"),
        state.providers.trucksim_view_mapped ? QStringLiteral("Mapped") : QStringLiteral("Not mapped"));
    SetValue(QStringLiteral("layout"),
        QString::fromStdWString(state.providers.trucksim_layout));
    SetValue(QStringLiteral("failureStage"),
        QString::fromStdWString(state.providers.trucksim_stage));
    SetValue(QStringLiteral("lastRead"),
        state.providers.trucksim_last_read.empty()
            ? QStringLiteral("Never")
            : TimestampText(QString::fromStdWString(state.providers.trucksim_last_read)));
    SetValue(QStringLiteral("dataAge"),
        QString::fromStdWString(state.providers.trucksim_data_age));
    SetValue(QStringLiteral("sourceTimestamp"),
        state.providers.trucksim_source_timestamp.empty()
            ? QStringLiteral("--")
            : TimestampText(QString::fromStdWString(state.providers.trucksim_source_timestamp)));
    SetValue(QStringLiteral("win32"),
        state.providers.trucksim_win32_error == 0
            ? QStringLiteral("None")
            : QString::number(state.providers.trucksim_win32_error));
    SetValue(QStringLiteral("storage"),
        state.providers.storage_error.empty()
            ? QStringLiteral("Ready in the current user's application data directory")
            : QString::fromStdWString(state.providers.storage_error));
    SetValue(QStringLiteral("sync"), QString::fromStdWString(state.providers.sync_state));
    SetValue(QStringLiteral("error"),
        state.providers.trucksim_error.empty()
            ? (state.providers.last_error.empty()
                ? QStringLiteral("None")
                : QString::fromStdWString(state.providers.last_error))
            : QString::fromStdWString(state.providers.trucksim_error));
}

ConnectPage::ConnectPage(
    telemetry::TelemetryCore& telemetry_core,
    QWidget* parent)
    : StatePage(parent),
      telemetry_core_(telemetry_core) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(14);

    auto* heading = new QLabel(QStringLiteral("CONNECT"), this);
    heading->setObjectName(QStringLiteral("connectHeading"));
    auto* subtitle = new QLabel(
        QStringLiteral("Configure your NLSI Logbook connection."), this);
    subtitle->setObjectName(QStringLiteral("connectDescription"));
    subtitle->setWordWrap(true);
    layout->addWidget(heading);
    layout->addWidget(subtitle);

    auto* form = new QFrame(this);
    form->setObjectName(QStringLiteral("contentCard"));
    auto* form_layout = new QVBoxLayout(form);
    form_layout->setContentsMargins(18, 16, 18, 16);
    form_layout->setSpacing(8);

    auto* url_label = new QLabel(QStringLiteral("WEBSITE BASE URL"), form);
    url_label->setObjectName(QStringLiteral("detailLabel"));
    website_base_url_ = new QLineEdit(form);
    website_base_url_->setObjectName(QStringLiteral("connectWebsiteBaseUrl"));
    website_base_url_->setPlaceholderText(
        QStringLiteral("https://your-domain.example/nlsi_vtc"));
    website_base_url_->setClearButtonEnabled(true);
    website_base_url_->setMinimumWidth(0);
    form_layout->addWidget(url_label);
    form_layout->addWidget(website_base_url_);
    form_layout->addSpacing(8);

    auto* key_label = new QLabel(QStringLiteral("DRIVER KEY"), form);
    key_label->setObjectName(QStringLiteral("detailLabel"));
    driver_key_ = new QLineEdit(form);
    driver_key_->setObjectName(QStringLiteral("connectDriverKey"));
    driver_key_->setEchoMode(QLineEdit::Password);
    driver_key_->setMaxLength(12);
    driver_key_->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[A-Za-z0-9]{0,12}")), driver_key_));
    const sync::DriverKeyStatus key_status = telemetry_core_.DriverKeyState();
    driver_key_->setPlaceholderText(key_status.configured
        ? QStringLiteral("Saved key retained; leave blank to keep it")
        : QStringLiteral("12 letters or numbers"));
    form_layout->addWidget(key_label);
    form_layout->addWidget(driver_key_);
    layout->addWidget(form);

    const QString saved_base_url = !telemetry_core_.DriverKeyApiBaseUrl().isEmpty()
        ? telemetry_core_.DriverKeyApiBaseUrl()
        : telemetry_core_.SyncApiBaseUrl();
    website_base_url_->setText(saved_base_url);

    auto* actions = new QHBoxLayout();
    actions->setSpacing(10);
    const auto make_action = [this, actions](const QString& text, const QString& name) {
        auto* button = new QPushButton(text, this);
        button->setObjectName(name);
        button->setMinimumWidth(78);
        button->setMinimumHeight(38);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        actions->addWidget(button, 1);
        return button;
    };
    save_button_ = make_action(QStringLiteral("SAVE"), QStringLiteral("connectSaveButton"));
    clear_button_ = make_action(QStringLiteral("CLEAR"), QStringLiteral("connectClearButton"));
    sync_button_ = make_action(QStringLiteral("SYNC"), QStringLiteral("connectSyncButton"));
    demo_button_ = make_action(QStringLiteral("DEMO"), QStringLiteral("connectDemoButton"));
    layout->addLayout(actions);

    auto* status_frame = new QFrame(this);
    status_frame->setObjectName(QStringLiteral("contentCard"));
    auto* status_layout = new QVBoxLayout(status_frame);
    status_layout->setContentsMargins(18, 14, 18, 14);
    status_layout->setSpacing(5);
    auto* status_title = new QLabel(QStringLiteral("CONNECTION STATUS"), status_frame);
    status_title->setObjectName(QStringLiteral("detailLabel"));
    connection_state_ = new QLabel(QStringLiteral("Not configured"), status_frame);
    connection_state_->setObjectName(QStringLiteral("connectConnectionState"));
    connection_state_->setStyleSheet(QStringLiteral("font-size: 13px; font-weight: 600;"));
    connection_message_ = new QLabel(status_frame);
    connection_message_->setObjectName(QStringLiteral("connectConnectionMessage"));
    connection_message_->setWordWrap(true);
    status_layout->addWidget(status_title);
    status_layout->addWidget(connection_state_);
    status_layout->addWidget(connection_message_);
    layout->addWidget(status_frame);

    auto* summary = new QGridLayout();
    summary->setHorizontalSpacing(12);
    summary->setVerticalSpacing(12);
    const auto make_summary_card = [this, summary](
                                       const QString& title,
                                       const QString& initial_value,
                                       const QString& object_name,
                                       int column) {
        auto* card = new QFrame(this);
        card->setObjectName(QStringLiteral("contentCard"));
        auto* card_layout = new QVBoxLayout(card);
        card_layout->setContentsMargins(16, 12, 16, 12);
        card_layout->setSpacing(4);
        auto* heading_label = new QLabel(title, card);
        heading_label->setObjectName(QStringLiteral("detailLabel"));
        auto* value_label = new QLabel(initial_value, card);
        value_label->setObjectName(object_name);
        value_label->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: 600;"));
        value_label->setWordWrap(true);
        value_label->setMinimumWidth(0);
        value_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        card_layout->addWidget(heading_label);
        card_layout->addWidget(value_label);
        summary->addWidget(card, 0, column);
        return value_label;
    };
    last_sync_value_ = make_summary_card(
        QStringLiteral("LAST SYNC"), QStringLiteral("Never"),
        QStringLiteral("connectLastSyncValue"), 0);
    pending_records_value_ = make_summary_card(
        QStringLiteral("PENDING RECORDS"), QStringLiteral("—"),
        QStringLiteral("connectPendingRecordsValue"), 1);
    summary->setColumnStretch(0, 1);
    summary->setColumnStretch(1, 1);
    layout->addLayout(summary);
    layout->addStretch(1);

    connect(save_button_, &QPushButton::clicked, this, &ConnectPage::SaveConnection);
    connect(clear_button_, &QPushButton::clicked, this, &ConnectPage::ClearForm);
    connect(sync_button_, &QPushButton::clicked, this, &ConnectPage::Synchronize);
    connect(demo_button_, &QPushButton::clicked, this, &ConnectPage::TestConnection);
    UpdateState(telemetry_core_.UiState());
}

void ConnectPage::SaveConnection() {
    const QString url = website_base_url_->text().trimmed();
    const QString key = driver_key_->text().trimmed();
    if (!sync::SyncConfigurationStore::IsValidApiBaseUrl(url)) {
        connection_message_->setText(
            QStringLiteral("Enter a valid HTTPS URL or loopback HTTP URL."));
        connection_state_->setText(QStringLiteral("Connection failed"));
        return;
    }
    if (key.isEmpty() && !telemetry_core_.DriverKeyState().configured) {
        connection_message_->setText(QStringLiteral("Enter your 12-character driver key."));
        connection_state_->setText(QStringLiteral("Not configured"));
        return;
    }
    if (save_in_progress_) {
        return;
    }

    save_in_progress_ = true;
    connection_message_->setText(QStringLiteral("Validating the driver key and saving securely…"));
    UpdateState(telemetry_core_.UiState());
    QPointer<ConnectPage> page(this);
    telemetry_core_.ValidateDriverKey(
        url,
        key,
        [page, url, key](bool valid, const QString& result) {
            if (!page) {
                return;
            }
            page->save_in_progress_ = false;
            if (!valid) {
                page->connection_message_->setText(result);
                page->UpdateState(page->telemetry_core_.UiState());
                return;
            }

            QString configuration_error;
            if (!page->telemetry_core_.ConfigureSync(url, key, &configuration_error)) {
                page->connection_message_->setText(
                    QStringLiteral("The driver key was verified, but sync settings could not be saved: %1")
                        .arg(configuration_error));
                page->UpdateState(page->telemetry_core_.UiState());
                return;
            }
            page->driver_key_->clear();
            page->driver_key_->setPlaceholderText(
                QStringLiteral("Saved key retained; leave blank to keep it"));
            page->connection_message_->setText(
                QStringLiteral("Driver key verified for %1. Connection check started.")
                    .arg(result));
            page->UpdateState(page->telemetry_core_.UiState());
        });
}

void ConnectPage::ClearForm() {
    website_base_url_->clear();
    driver_key_->clear();
    connection_message_->setText(QStringLiteral(
        "Form cleared only. Saved connection settings and local telemetry records were not changed."));
    UpdateState(telemetry_core_.UiState());
}

void ConnectPage::Synchronize() {
    if (!sync::SyncConfigurationStore::IsValidApiBaseUrl(
            website_base_url_->text().trimmed())) {
        connection_message_->setText(QStringLiteral("Enter a valid connection URL before syncing."));
        return;
    }
    if (!telemetry_core_.UiState().providers.sync_configured) {
        connection_message_->setText(QStringLiteral("Save and verify a driver key before syncing."));
        return;
    }
    if (website_base_url_->text().trimmed() != telemetry_core_.SyncApiBaseUrl()
        || !driver_key_->text().isEmpty()) {
        connection_message_->setText(
            QStringLiteral("Save the displayed connection changes before syncing."));
        return;
    }
    if (telemetry_core_.UiState().providers.sync_in_progress) {
        return;
    }
    connection_message_->setText(
        QStringLiteral("Synchronization requested. Local records remain queued until acknowledged."));
    telemetry_core_.SyncNow();
}

void ConnectPage::TestConnection() {
    if (!sync::SyncConfigurationStore::IsValidApiBaseUrl(
            website_base_url_->text().trimmed())) {
        connection_message_->setText(QStringLiteral("Enter a valid connection URL before testing."));
        connection_state_->setText(QStringLiteral("Connection failed"));
        return;
    }
    if (!telemetry_core_.UiState().providers.sync_configured) {
        connection_message_->setText(QStringLiteral(
            "Connection test unavailable. Save and verify a driver key first."));
        connection_state_->setText(QStringLiteral("Not configured"));
        return;
    }
    if (website_base_url_->text().trimmed() != telemetry_core_.SyncApiBaseUrl()
        || !driver_key_->text().isEmpty()) {
        connection_message_->setText(
            QStringLiteral("Save the displayed connection changes before testing."));
        return;
    }
    if (telemetry_core_.UiState().providers.api_connecting) {
        connection_message_->setText(
            QStringLiteral("An authenticated connection check is already in progress."));
        return;
    }
    if (connection_test_in_progress_) {
        return;
    }

    connection_test_in_progress_ = true;
    connection_message_->setText(QStringLiteral(
        "Testing the authenticated API health endpoint. No records will be uploaded."));
    UpdateState(telemetry_core_.UiState());
    QPointer<ConnectPage> page(this);
    telemetry_core_.TestSyncConnection(
        [page](bool connected, const QString& result) {
            if (!page) {
                return;
            }
            page->connection_test_in_progress_ = false;
            page->connection_message_->setText(connected
                ? QStringLiteral("Connection test succeeded: %1").arg(result)
                : QStringLiteral("Connection test failed: %1").arg(result));
            page->UpdateState(page->telemetry_core_.UiState());
        });
}

void ConnectPage::UpdateState(const telemetry::TelemetryUiState& state) {
    const telemetry::ProviderStatus& sync = state.providers;
    const QString last_sync = QString::fromStdWString(sync.last_successful_sync);
    if (!initial_sync_observed_) {
        observed_last_sync_ = last_sync;
        initial_sync_observed_ = true;
    } else if (!last_sync.isEmpty() && last_sync != observed_last_sync_) {
        observed_last_sync_ = last_sync;
        sync_completion_timer_.start();
    }
    last_sync_value_->setText(last_sync.isEmpty()
        ? QStringLiteral("Never")
        : TimestampText(last_sync));
    pending_records_value_->setText(sync.storage_error.empty()
        ? QString::number(sync.sync_pending_count)
        : QStringLiteral("Unavailable"));

    const sync::DriverKeyStatus key_status = telemetry_core_.DriverKeyState();
    QString status;
    if (save_in_progress_ || connection_test_in_progress_ || key_status.validation_pending
        || sync.api_connecting) {
        status = QStringLiteral("Connecting");
    } else if (sync.sync_in_progress) {
        status = QStringLiteral("Synchronizing");
    } else if (!sync.sync_error.empty()) {
        status = sync.api_connected
            ? QStringLiteral("Synchronization failed")
            : QStringLiteral("Connection failed");
    } else if (!sync.sync_configured && !key_status.configured) {
        status = QStringLiteral("Not configured");
    } else if (!sync.api_connected) {
        status = QStringLiteral("Ready to connect");
    } else if (sync_completion_timer_.isValid()
        && sync_completion_timer_.elapsed() < 5000) {
        status = QStringLiteral("Sync completed");
    } else {
        status = QStringLiteral("Connected");
    }
    connection_state_->setText(QStringLiteral("● ") + status);
    const QString status_color =
        status == QStringLiteral("Connected") || status == QStringLiteral("Sync completed")
        ? QStringLiteral("#26734D")
        : (status == QStringLiteral("Connection failed")
                || status == QStringLiteral("Synchronization failed")
            ? QStringLiteral("#A62E4D")
            : (status == QStringLiteral("Connecting")
                    || status == QStringLiteral("Synchronizing")
                ? QStringLiteral("#A85B13")
                : QStringLiteral("#6D6870")));
    connection_state_->setStyleSheet(
        QStringLiteral("font-size: 13px; font-weight: 600; color: %1;").arg(status_color));
    save_button_->setText(save_in_progress_
        ? QStringLiteral("SAVING…")
        : QStringLiteral("SAVE"));
    sync_button_->setText(sync.sync_in_progress
        ? QStringLiteral("SYNCING…")
        : QStringLiteral("SYNC"));
    demo_button_->setText(connection_test_in_progress_
        ? QStringLiteral("TESTING…")
        : QStringLiteral("DEMO"));
    sync_button_->setEnabled(
        sync.sync_configured && !sync.sync_in_progress && !save_in_progress_);
    demo_button_->setEnabled(
        sync.sync_configured && !sync.api_connecting
        && !connection_test_in_progress_ && !save_in_progress_);
    save_button_->setEnabled(!save_in_progress_ && !connection_test_in_progress_);
}

SettingsPage::SettingsPage(
    telemetry::TelemetryCore& telemetry_core,
    QWidget* parent)
    : StatePage(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(14);

    tabs_ = new QTabWidget(this);
    tabs_->setObjectName(QStringLiteral("contentTabs"));
    application_settings_ = new ApplicationSettingsPage(tabs_);
    providers_ = new ProvidersPage(tabs_);
    telemetry_ = new ConfigPage(tabs_);
    active_mods_ = new ActiveModsPage(tabs_);
    connect_page_ = new ConnectPage(telemetry_core, tabs_);
    tabs_->addTab(application_settings_, QStringLiteral("APPLICATION"));
    tabs_->addTab(providers_, QStringLiteral("PROVIDERS"));
    tabs_->addTab(telemetry_, QStringLiteral("CONFIG"));
    tabs_->addTab(active_mods_, QStringLiteral("ACTIVE MODS"));
    tabs_->addTab(connect_page_, QStringLiteral("CONNECT"));
    layout->addWidget(tabs_, 1);
}

void SettingsPage::UpdateState(const telemetry::TelemetryUiState& state) {
    application_settings_->UpdateState(state);
    providers_->UpdateState(state);
    telemetry_->UpdateState(state);
    active_mods_->UpdateState(state);
    connect_page_->UpdateState(state);
}

AboutPage::AboutPage(const QString& version, QWidget* parent)
    : DetailPage(parent),
      version_(version) {
    AddField(QStringLiteral("product"), QStringLiteral("Product"));
    AddField(QStringLiteral("company"), QStringLiteral("Company"));
    AddField(QStringLiteral("version"), QStringLiteral("Version"));
    AddField(QStringLiteral("channel"), QStringLiteral("Channel"));
    AddField(QStringLiteral("framework"), QStringLiteral("UI framework"));
    AddField(QStringLiteral("backend"), QStringLiteral("Telemetry backend"));
    AddField(QStringLiteral("discord"), QStringLiteral("Community"));
    AddField(QStringLiteral("ceo"), QStringLiteral("CEO"));
    AddField(QStringLiteral("developer"), QStringLiteral("Developer"));
    AddField(QStringLiteral("plugin"), QStringLiteral("Telemetry plugin attribution"));
    AddField(QStringLiteral("licenses"), QStringLiteral("Third-party notices"));
    update_controls_ = new UpdateControls(this);
    AddContentWidget(update_controls_);
    updater_ = new nlsi::updater::GitHubUpdater(
        QCoreApplication::applicationVersion(), this);
    connect(update_controls_->button, &QPushButton::clicked, this, [this] {
        update_controls_->button->setEnabled(false);
        update_controls_->status->setText(QStringLiteral("Checking GitHub releases…"));
        updater_->CheckForUpdates(true);
    });
    updater_->SetResultHandler([this](const nlsi::updater::UpdateCheckResult& result, bool) {
        update_controls_->button->setEnabled(true);
        const QString current = QCoreApplication::applicationVersion();
        if (!result.succeeded) {
            update_controls_->status->setText(
                QStringLiteral("Update check failed: %1").arg(result.error));
            update_controls_->status->setToolTip(update_controls_->status->text());
            return;
        }
        if (result.release.version.isEmpty()) {
            update_controls_->status->setText(
                QStringLiteral("No published releases are available."));
            update_controls_->status->setToolTip(update_controls_->status->text());
            return;
        }
        if (!result.update_available) {
            update_controls_->status->setText(
                QStringLiteral("Current: %1 · no newer release found.")
                    .arg(current));
            update_controls_->status->setToolTip(result.from_cache
                ? QStringLiteral("Showing the last successful cached check. Use Check for Updates "
                    "to request a fresh result.")
                : update_controls_->status->text());
            return;
        }

        const QString available = QStringLiteral("v%1 · %2")
            .arg(result.release.version, result.release.channel);
        update_controls_->status->setText(
            QStringLiteral("Update available: %1").arg(available));
        update_controls_->status->setToolTip(result.from_cache
            ? QStringLiteral("Showing cached release data; use Check for Updates to refresh.")
            : available);
        QString notes = result.release.notes.trimmed();
        constexpr qsizetype kMaximumNotesCharacters = 6000;
        if (notes.size() > kMaximumNotesCharacters) {
            notes.truncate(kMaximumNotesCharacters);
            notes += QStringLiteral("\n\n(Release notes truncated.)");
        }
        const QString release_name = result.release.name.isEmpty()
            ? available : result.release.name;
        auto* dialog = new QMessageBox(
            QMessageBox::Information,
            QStringLiteral("NLSI Exclusive Logbook update available"),
            QStringLiteral("Current version: %1\nAvailable version: %2\nChannel: %3\n\n%4")
                .arg(current, available, result.release.channel,
                    notes.isEmpty() ? release_name : release_name + QStringLiteral("\n\n") + notes),
            QMessageBox::NoButton,
            this);
        dialog->setTextFormat(Qt::PlainText);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        auto* open_release = dialog->addButton(
            QStringLiteral("Open release page"), QMessageBox::ActionRole);
        dialog->addButton(QStringLiteral("Later"), QMessageBox::RejectRole);
        connect(open_release, &QPushButton::clicked, dialog, [dialog, url = result.release.url] {
            if (!QDesktopServices::openUrl(url)) {
                QMessageBox::warning(dialog, QStringLiteral("Could not open release page"),
                    QStringLiteral("The default browser could not open %1.")
                        .arg(url.toString()));
            }
            dialog->close();
        });
        dialog->open();
    });
    updater_->CheckForUpdates(false);
}

AboutPage::~AboutPage() = default;

void AboutPage::UpdateState(const telemetry::TelemetryUiState&) {
    SetValue(QStringLiteral("product"), QStringLiteral("NLSI Exclusive Logbook"));
    SetValue(QStringLiteral("company"), QStringLiteral("Nabski Logistics and Solutions Inc."));
    const QString app_version = QCoreApplication::applicationVersion();
    SetValue(QStringLiteral("version"), app_version.isEmpty() ? version_ : app_version);
    SetValue(QStringLiteral("channel"), ReleaseChannelText());
    SetValue(QStringLiteral("framework"),
        QStringLiteral("Qt %1 Widgets").arg(QString::fromLatin1(qVersion())));
    SetValue(QStringLiteral("backend"), QStringLiteral("TruckSim GPS shared-memory telemetry"));
    SetExternalLink(QStringLiteral("discord"),
        QStringLiteral("Nabski Logistics and Solutions Inc. Community"),
        QUrl(QStringLiteral("https://discord.gg/gerAGTS6YB")));
    SetExternalLink(QStringLiteral("ceo"), QStringLiteral("Follow CEO on TikTok"),
        QUrl(QStringLiteral("https://www.tiktok.com/@nabskiplays")));
    SetExternalLink(QStringLiteral("developer"), QStringLiteral("Follow Developer on TikTok"),
        QUrl(QStringLiteral("https://www.tiktok.com/@kape_073")));
    SetExternalLink(QStringLiteral("plugin"), QStringLiteral("TruckSim GPS project (MIT license)"),
        QUrl(QStringLiteral("https://github.com/TruckSim-GPS/trucksim-gps-plugin")));
    SetValue(QStringLiteral("licenses"),
        QStringLiteral("Third-party notices, including Lucide ISC, are in the licenses folder."));
}

} // namespace nlsi::gui
