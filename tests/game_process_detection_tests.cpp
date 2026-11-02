#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "GameProcessDetection.h"

#ifdef _WIN32
#include <Windows.h>
#endif

namespace {

bool check(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        return false;
    }
    return true;
}

nlsi::ProcessEnumerationResult successfulEnumeration(
    std::vector<std::wstring> executableNames)
{
    return {true, std::move(executableNames), 0};
}

} // namespace

int main()
{
    bool ok = true;

    const auto neitherRunning = nlsi::classifyGameProcesses(successfulEnumeration({}));
    ok = check(neitherRunning.state == nlsi::GameProcessState::NotRunning &&
               nlsi::gameProcessStateAllowsStartup(neitherRunning.state) &&
               !nlsi::gameProcessStateBlocksOperation(neitherRunning.state),
               "no supported game allows startup and shutdown") && ok;

    const auto ets2Running = nlsi::classifyGameProcesses(
        successfulEnumeration({L"EUROTRUCKS2.EXE"}));
    ok = check(ets2Running.state == nlsi::GameProcessState::Ets2Running &&
               !nlsi::gameProcessStateAllowsStartup(ets2Running.state) &&
               nlsi::gameProcessStateBlocksOperation(ets2Running.state),
               "ETS2 blocks startup and shutdown") && ok;

    const auto atsRunning = nlsi::classifyGameProcesses(
        successfulEnumeration({L"AmTrUcKs.ExE"}));
    ok = check(atsRunning.state == nlsi::GameProcessState::AtsRunning &&
               !nlsi::gameProcessStateAllowsStartup(atsRunning.state) &&
               nlsi::gameProcessStateBlocksOperation(atsRunning.state),
               "ATS blocks startup and shutdown independently") && ok;

    const auto bothRunning = nlsi::classifyGameProcesses(
        successfulEnumeration({L"eurotrucks2.exe", L"amtrucks.exe"}));
    ok = check(bothRunning.state == nlsi::GameProcessState::BothRunning &&
               !nlsi::gameProcessStateAllowsStartup(bothRunning.state) &&
               nlsi::gameProcessStateBlocksOperation(bothRunning.state),
               "both supported games are independently detected and block operation") && ok;

    const auto unrelatedProcesses = nlsi::classifyGameProcesses(
        successfulEnumeration({L"not-eurotrucks2.exe", L"amtrucks.exe.bak"}));
    ok = check(unrelatedProcesses.state == nlsi::GameProcessState::NotRunning,
               "matching requires an exact executable filename") && ok;

    const auto enumerationError = nlsi::classifyGameProcesses(
        {false, {}, 5});
    ok = check(enumerationError.state == nlsi::GameProcessState::DetectionError &&
               enumerationError.windowsErrorCode == 5 &&
               !nlsi::gameProcessStateAllowsStartup(enumerationError.state) &&
               nlsi::gameProcessStateBlocksOperation(enumerationError.state),
               "enumeration errors are distinguished and preserve the Windows error code") && ok;

#ifdef _WIN32
    DWORD handlesBefore = 0;
    DWORD handlesAfter = 0;
    ok = check(GetProcessHandleCount(GetCurrentProcess(), &handlesBefore) != FALSE,
               "process handle count is available before repeated scans") && ok;

    for (int attempt = 0; attempt < 500; ++attempt)
    {
        const nlsi::GameProcessDetectionResult result = nlsi::detectGameProcesses();
        if (!check(result.state != nlsi::GameProcessState::DetectionError,
                   "Tool Help process enumeration succeeds during repeated scans"))
        {
            ok = false;
            break;
        }
    }

    ok = check(GetProcessHandleCount(GetCurrentProcess(), &handlesAfter) != FALSE,
               "process handle count is available after repeated scans") && ok;
    ok = check(handlesAfter <= handlesBefore + 1,
               "repeated process scans do not leak snapshot handles") && ok;
#endif

    return ok ? 0 : 1;
}
