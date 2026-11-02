#include "GameProcessDetection.h"

#include <cwctype>

#ifdef _WIN32
#include <Windows.h>
#include <TlHelp32.h>
#endif

namespace nlsi {
namespace {

bool executableNameEquals(const std::wstring& left, const wchar_t* right)
{
#ifdef _WIN32
    return _wcsicmp(left.c_str(), right) == 0;
#else
    std::wstring foldedLeft = left;
    std::wstring foldedRight = right;
    for (wchar_t& character : foldedLeft)
    {
        character = static_cast<wchar_t>(std::towlower(character));
    }
    for (wchar_t& character : foldedRight)
    {
        character = static_cast<wchar_t>(std::towlower(character));
    }
    return foldedLeft == foldedRight;
#endif
}

#ifdef _WIN32
class ProcessSnapshot
{
public:
    explicit ProcessSnapshot(HANDLE handle) : handle_(handle) {}
    ~ProcessSnapshot()
    {
        if (handle_ != INVALID_HANDLE_VALUE)
        {
            CloseHandle(handle_);
        }
    }

    ProcessSnapshot(const ProcessSnapshot&) = delete;
    ProcessSnapshot& operator=(const ProcessSnapshot&) = delete;

    HANDLE get() const { return handle_; }

private:
    HANDLE handle_;
};

ProcessEnumerationResult enumerateWindowsProcesses()
{
    const HANDLE snapshotHandle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshotHandle == INVALID_HANDLE_VALUE)
    {
        const DWORD error = GetLastError();
        return {false, {}, error == ERROR_SUCCESS ? ERROR_GEN_FAILURE : error};
    }

    const ProcessSnapshot snapshot(snapshotHandle);
    ProcessEnumerationResult result;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!Process32FirstW(snapshot.get(), &entry))
    {
        const DWORD error = GetLastError();
        if (error == ERROR_NO_MORE_FILES)
        {
            result.succeeded = true;
            return result;
        }
        result.windowsErrorCode = error == ERROR_SUCCESS ? ERROR_GEN_FAILURE : error;
        return result;
    }

    do
    {
        result.executableNames.emplace_back(entry.szExeFile);
    } while (Process32NextW(snapshot.get(), &entry));

    const DWORD error = GetLastError();
    if (error == ERROR_NO_MORE_FILES)
    {
        result.succeeded = true;
        return result;
    }

    result.windowsErrorCode = error == ERROR_SUCCESS ? ERROR_GEN_FAILURE : error;
    return result;
}
#endif

} // namespace

GameProcessDetectionResult classifyGameProcesses(
    const ProcessEnumerationResult& enumeration)
{
    if (!enumeration.succeeded)
    {
        return {GameProcessState::DetectionError, enumeration.windowsErrorCode};
    }

    bool ets2Running = false;
    bool atsRunning = false;
    for (const std::wstring& executableName : enumeration.executableNames)
    {
        if (executableNameEquals(executableName, L"eurotrucks2.exe"))
        {
            ets2Running = true;
        }
        else if (executableNameEquals(executableName, L"amtrucks.exe"))
        {
            atsRunning = true;
        }
    }

    return {gameProcessStateFromFlags(ets2Running, atsRunning)};
}

GameProcessDetectionResult detectGameProcesses()
{
#ifdef _WIN32
    return classifyGameProcesses(enumerateWindowsProcesses());
#else
    return {GameProcessState::DetectionError};
#endif
}

} // namespace nlsi
