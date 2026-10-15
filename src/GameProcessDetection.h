#pragma once

namespace nlsi {

enum class GameProcessState
{
    NotRunning,
    Ets2Running,
    AtsRunning,
    BothRunning,
    DetectionError
};

inline GameProcessState gameProcessStateFromFlags(bool ets2Running, bool atsRunning)
{
    if (ets2Running && atsRunning) return GameProcessState::BothRunning;
    if (ets2Running) return GameProcessState::Ets2Running;
    if (atsRunning) return GameProcessState::AtsRunning;
    return GameProcessState::NotRunning;
}

} // namespace nlsi
