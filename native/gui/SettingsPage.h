#pragma once

#include <QElapsedTimer>

#include "PageSupport.h"

class QTabWidget;
class QLabel;
class QLineEdit;
class QPushButton;
class QWidget;

namespace nlsi::telemetry {
class TelemetryCore;
}

namespace nlsi::gui {

class ConnectPage final : public StatePage {
public:
    explicit ConnectPage(telemetry::TelemetryCore& telemetry_core, QWidget* parent = nullptr);
    void UpdateState(const telemetry::TelemetryUiState& state) override;

private:
    void SaveConnection();
    void ClearForm();
    void Synchronize();
    void TestConnection();

    telemetry::TelemetryCore& telemetry_core_;
    QLineEdit* website_base_url_ = nullptr;
    QLineEdit* driver_key_ = nullptr;
    QPushButton* save_button_ = nullptr;
    QPushButton* clear_button_ = nullptr;
    QPushButton* sync_button_ = nullptr;
    QPushButton* demo_button_ = nullptr;
    QLabel* connection_state_ = nullptr;
    QLabel* connection_message_ = nullptr;
    QLabel* last_sync_value_ = nullptr;
    QLabel* pending_records_value_ = nullptr;
    QElapsedTimer sync_completion_timer_;
    QString observed_last_sync_;
    bool initial_sync_observed_ = false;
    bool save_in_progress_ = false;
    bool connection_test_in_progress_ = false;
};

class SettingsPage final : public StatePage {
public:
    explicit SettingsPage(
        telemetry::TelemetryCore& telemetry_core,
        QWidget* parent = nullptr);
    void UpdateState(const telemetry::TelemetryUiState& state) override;

private:
    QTabWidget* tabs_ = nullptr;
    StatePage* application_settings_ = nullptr;
    StatePage* providers_ = nullptr;
    StatePage* telemetry_ = nullptr;
    StatePage* active_mods_ = nullptr;
    ConnectPage* connect_page_ = nullptr;
};

} // namespace nlsi::gui
