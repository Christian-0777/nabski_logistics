#include "TelemetryIPC.h"

#include <sstream>
#include <utility>

namespace nlsi {
namespace {

std::string lowerString(std::string value)
{
    for (char& ch : value)
    {
        if (ch >= 'A' && ch <= 'Z')
        {
            ch = static_cast<char>(ch + ('a' - 'A'));
        }
    }
    return value;
}

std::string encodeValue(const std::string& value)
{
    std::string encoded;
    encoded.reserve(value.size());
    for (const char ch : value)
    {
        switch (ch)
        {
        case '%': encoded += "%25"; break;
        case '|': encoded += "%7C"; break;
        case '\n': encoded += "%0A"; break;
        case '\r': encoded += "%0D"; break;
        default: encoded += ch; break;
        }
    }
    return encoded;
}

std::string decodeValue(const std::string& value)
{
    std::string decoded;
    decoded.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index)
    {
        if (value[index] == '%' && index + 2 < value.size())
        {
            const std::string code = value.substr(index, 3);
            if (code == "%25") decoded += '%';
            else if (code == "%7C") decoded += '|';
            else if (code == "%0A") decoded += '\n';
            else if (code == "%0D") decoded += '\r';
            else
            {
                decoded += value[index];
                continue;
            }
            index += 2;
        }
        else
        {
            decoded += value[index];
        }
    }
    return decoded;
}

} // namespace

std::string trimString(const std::string& text)
{
    const std::string whitespace = " \t\r\n";
    const std::size_t begin = text.find_first_not_of(whitespace);
    if (begin == std::string::npos)
    {
        return {};
    }

    const std::size_t end = text.find_last_not_of(whitespace);
    return text.substr(begin, end - begin + 1);
}

std::vector<std::string> splitString(const std::string& text, char delimiter)
{
    std::vector<std::string> parts;
    std::stringstream stream(text);
    std::string item;
    while (std::getline(stream, item, delimiter))
    {
        parts.push_back(item);
    }
    return parts;
}

std::string connectionStateToString(ConnectionState status)
{
    switch (status)
    {
    case ConnectionState::Disconnected:
        return "Disconnected";
    case ConnectionState::Connecting:
        return "Connecting";
    case ConnectionState::Connected:
        return "Connected";
    case ConnectionState::Error:
        return "Error";
    default:
        return "Disconnected";
    }
}

std::string jobStatusToString(JobStatus status)
{
    switch (status)
    {
    case JobStatus::Active:
        return "ACTIVE";
    case JobStatus::Delivered:
        return "DELIVERED";
    case JobStatus::Cancelled:
        return "CANCELLED";
    case JobStatus::Unknown:
    default:
        return "UNKNOWN";
    }
}

JobStatus jobStatusFromString(const std::string& text)
{
    const std::string normalized = lowerString(trimString(text));
    if (normalized == "active")
    {
        return JobStatus::Active;
    }
    if (normalized == "delivered" || normalized == "completed")
    {
        return JobStatus::Delivered;
    }
    if (normalized == "cancelled" || normalized == "canceled")
    {
        return JobStatus::Cancelled;
    }
    return JobStatus::Unknown;
}

std::string formatJobId(std::uint64_t sequence)
{
    if (sequence == 0)
    {
        return "NLSI-0000";
    }

    std::string digits = std::to_string(sequence);
    const std::size_t minimumWidth = 4;
    while (digits.size() < minimumWidth)
    {
        digits.insert(digits.begin(), '0');
    }
    return "NLSI-" + digits;
}

ConnectionState connectionStateFromString(const std::string& text)
{
    const std::string normalized = lowerString(trimString(text));
    if (normalized == "connected")
    {
        return ConnectionState::Connected;
    }
    if (normalized == "connecting")
    {
        return ConnectionState::Connecting;
    }
    if (normalized == "error")
    {
        return ConnectionState::Error;
    }
    if (normalized == "disconnected")
    {
        return ConnectionState::Disconnected;
    }
    return ConnectionState::Error;
}

std::string encodeTelemetryFrame(const TelemetrySnapshot& snapshot)
{
    std::ostringstream output;
    output << "status|" << connectionStateToString(snapshot.status) << '\n';
    if (!snapshot.gameId.empty())
    {
        output << "game_id|" << encodeValue(snapshot.gameId) << '\n';
    }
    if (!snapshot.gameName.empty())
    {
        output << "game|" << encodeValue(snapshot.gameName) << '\n';
    }
    if (!snapshot.gameApiVersion.empty())
    {
        output << "api_version|" << encodeValue(snapshot.gameApiVersion) << '\n';
    }
    for (const auto& entry : snapshot.values)
    {
        const auto& field = entry.second;
        output << "field|" << encodeValue(entry.first) << '|'
               << encodeValue(field.label) << '|'
               << encodeValue(field.value) << '|'
               << encodeValue(field.unit) << '|'
               << (field.available ? "1" : "0") << '\n';
    }
    return output.str();
}

TelemetrySnapshot decodeTelemetryFrame(const std::string& payload)
{
    TelemetrySnapshot snapshot;
    const std::vector<std::string> lines = splitString(payload, '\n');

    for (const std::string& rawLine : lines)
    {
        const std::string line = trimString(rawLine);
        if (line.empty())
        {
            continue;
        }

        const std::vector<std::string> parts = splitString(line, '|');
        if (parts.empty())
        {
            continue;
        }

        const std::string prefix = trimString(parts[0]);

        if (prefix == "status" && parts.size() == 2)
        {
            snapshot.status = connectionStateFromString(parts[1]);
            continue;
        }

        if (prefix == "game_id" && parts.size() == 2)
        {
            snapshot.gameId = decodeValue(parts[1]);
            continue;
        }

        if (prefix == "game" && parts.size() == 2)
        {
            snapshot.gameName = decodeValue(parts[1]);
            continue;
        }

        if (prefix == "api_version" && parts.size() == 2)
        {
            snapshot.gameApiVersion = decodeValue(parts[1]);
            continue;
        }

        if (prefix == "field" && parts.size() == 6)
        {
            FieldValue value;
            value.label = decodeValue(parts[2]);
            value.value = decodeValue(parts[3]);
            value.unit = decodeValue(parts[4]);
            const std::string available = lowerString(trimString(parts[5]));
            if (available != "1" && available != "0" && available != "true" && available != "false")
            {
                snapshot.status = ConnectionState::Error;
                continue;
            }
            value.available = available == "1" || available == "true";
            snapshot.values[decodeValue(parts[1])] = value;
            continue;
        }

        snapshot.status = ConnectionState::Error;
    }

    return snapshot;
}

} // namespace nlsi
