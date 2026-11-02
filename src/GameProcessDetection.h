#pragma once

#include <string>
#include <vector>

namespace nlsi {

enum class GameProcessState
{
    NotRunning,
    Ets2Running,
    AtsRunning,
    BothRunning,
    DetectionError
};

struct GameProcessDetectionResult
{
    GameProcessState state = GameProcessState::DetectionError;
    unsigned long windowsErrorCode = 0;

    GameProcessDetectionResult() = default;
    GameProcessDetectionResult(GameProcessState detectionState,
                               unsigned long errorCode = 0)
        : state(detectionState), windowsErrorCode(errorCode)
    {
    }
};

struct ProcessEnumerationResult
{
    bool succeeded = false;
    std::vector<std::wstring> executableNames;
    unsigned long windowsErrorCode = 0;
};

GameProcessDetectionResult detectGameProcesses();
GameProcessDetectionResult classifyGameProcesses(
    const ProcessEnumerationResult& enumeration);

inline bool gameProcessStateBlocksOperation(GameProcessState state)
{
    return state != GameProcessState::NotRunning;
}

inline bool gameProcessStateAllowsStartup(GameProcessState state)
{
    return state == GameProcessState::NotRunning;
}

inline GameProcessState gameProcessStateFromFlags(bool ets2Running, bool atsRunning)
{
    if (ets2Running && atsRunning) return GameProcessState::BothRunning;
    if (ets2Running) return GameProcessState::Ets2Running;
    if (atsRunning) return GameProcessState::AtsRunning;
    return GameProcessState::NotRunning;
}

} // namespace nlsi
