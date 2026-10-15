#pragma once

#include <map>
#include <string>
#include <vector>

namespace nlsi {

enum class ConnectionState
{
    Disconnected,
    Connecting,
    Connected,
    Error
};

enum class JobStatus
{
    Unknown,
    Active,
    Delivered,
    Cancelled
};

struct FieldValue
{
    std::string label;
    std::string value;
    std::string unit;
    bool available = false;
};

struct TelemetrySnapshot
{
    ConnectionState status = ConnectionState::Disconnected;
    std::string gameId;
    std::string gameName;
    std::string gameApiVersion;
    std::map<std::string, FieldValue> values;
};

constexpr char kTelemetryPipeName[] = "\\\\.\\pipe\\nlsi_scs_telemetry";

std::string connectionStateToString(ConnectionState status);
ConnectionState connectionStateFromString(const std::string& text);

std::string jobStatusToString(JobStatus status);
JobStatus jobStatusFromString(const std::string& text);
std::string formatJobId(std::uint64_t sequence);

std::string encodeTelemetryFrame(const TelemetrySnapshot& snapshot);
TelemetrySnapshot decodeTelemetryFrame(const std::string& payload);

std::vector<std::string> splitString(const std::string& text, char delimiter);
std::string trimString(const std::string& text);

} // namespace nlsi
