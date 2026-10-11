#pragma once

#include "PageSupport.h"

class QTabWidget;
class QHBoxLayout;
class QWidget;

namespace nlsi::gui {

class SettingsPage final : public StatePage {
public:
    explicit SettingsPage(QWidget* parent = nullptr);
    void UpdateState(const telemetry::TelemetryUiState& state) override;
    void AddConnectControl(QWidget* control);

private:
    QTabWidget* tabs_ = nullptr;
    StatePage* application_settings_ = nullptr;
    StatePage* providers_ = nullptr;
    StatePage* telemetry_ = nullptr;
    StatePage* active_mods_ = nullptr;
    QWidget* connect_page_ = nullptr;
    QHBoxLayout* connect_controls_layout_ = nullptr;
};

} // namespace nlsi::gui
