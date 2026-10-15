#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <limits>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include <windows.h>

#include "TelemetryIPC.h"
#include "amtrucks/scssdk_ats.h"
#include "common/scssdk_telemetry_common_configs.h"
#include "common/scssdk_telemetry_common_gameplay_events.h"
#include "common/scssdk_telemetry_job_common_channels.h"
#include "common/scssdk_telemetry_truck_common_channels.h"
#include "eurotrucks2/scssdk_eut2.h"
#include "scssdk_telemetry.h"
#include "scssdk_telemetry_event.h"

namespace {

std::mutex g_snapshotMutex;
std::condition_variable g_snapshotChanged;
nlsi::TelemetrySnapshot g_snapshot;
std::deque<nlsi::TelemetrySnapshot> g_pendingJobFinalSnapshots;
std::uint64_t g_snapshotRevision = 0;
std::atomic<bool> g_stopWorker{false};
std::thread g_ipcWorker;
HANDLE g_pipe = INVALID_HANDLE_VALUE;
scs_log_t g_gameLog = nullptr;
std::mutex g_trailerMutex;
std::map<std::string, std::pair<std::string, std::string>> g_trailers;
double g_lastFuelAmount = 0.0;
bool g_lastFuelAvailable = false;
double g_jobFuelConsumed = 0.0;
double g_jobRefueled = 0.0;
double g_jobMaxSpeedKmh = 0.0;
double g_jobSpeedIntegral = 0.0;
double g_jobSpeedSeconds = 0.0;
std::chrono::steady_clock::time_point g_lastSpeedSample;
bool g_jobInProgress = false;

std::string formatFloat(const scs_float_t value)
{
    char buffer[32];
    const auto result = std::to_chars(
        buffer,
        buffer + sizeof(buffer),
        value,
        std::chars_format::general,
        std::numeric_limits<scs_float_t>::max_digits10);
    if (result.ec != std::errc())
    {
        return {};
    }
    return std::string(buffer, result.ptr);
}

void logRegistrationFailure(const char* const channel, const scs_result_t result)
{
    if (!g_gameLog)
    {
        return;
    }

    const std::string message = std::string("NLSI telemetry channel registration failed: ") +
                               channel + " (result " + std::to_string(result) + ")";
    g_gameLog(SCS_LOG_TYPE_warning, message.c_str());
}

void publishField(const std::string& key, nlsi::FieldValue field)
{
    {
        std::lock_guard<std::mutex> lock(g_snapshotMutex);
        g_snapshot.status = nlsi::ConnectionState::Connected;
        g_snapshot.values[key] = std::move(field);
        ++g_snapshotRevision;
    }
    g_snapshotChanged.notify_one();
}

void publishJobStatus(const std::string& status)
{
    {
        std::lock_guard<std::mutex> lock(g_snapshotMutex);
        g_snapshot.status = nlsi::ConnectionState::Connected;
        g_snapshot.values["job.status"] = {"Status", status, "", true};
        ++g_snapshotRevision;
        if (status == nlsi::jobStatusToString(nlsi::JobStatus::Delivered) ||
            status == nlsi::jobStatusToString(nlsi::JobStatus::Cancelled))
        {
            g_pendingJobFinalSnapshots.push_back(g_snapshot);
        }
    }
    g_snapshotChanged.notify_one();
}

void ipcWorker()
{
    while (!g_stopWorker.load())
    {
        nlsi::TelemetrySnapshot snapshot;
        bool isPendingJobFinalization = false;
        {
            std::unique_lock<std::mutex> lock(g_snapshotMutex);
            g_snapshotChanged.wait_for(lock, std::chrono::milliseconds(500), [] {
                return g_stopWorker.load() || !g_pendingJobFinalSnapshots.empty();
            });
            if (g_stopWorker.load())
            {
                break;
            }
            if (!g_pendingJobFinalSnapshots.empty())
            {
                snapshot = g_pendingJobFinalSnapshots.front();
                isPendingJobFinalization = true;
            }
            else
            {
                snapshot = g_snapshot;
            }
        }

        if (g_pipe == INVALID_HANDLE_VALUE)
        {
            g_pipe = CreateFileA(
                nlsi::kTelemetryPipeName,
                GENERIC_WRITE,
                0,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);
            if (g_pipe == INVALID_HANDLE_VALUE)
            {
                continue;
            }
        }

        std::string frame = nlsi::encodeTelemetryFrame(snapshot);
        frame.push_back('\n');
        std::size_t offset = 0;
        bool frameWritten = true;
        while (offset < frame.size())
        {
            DWORD written = 0;
            const DWORD remaining = static_cast<DWORD>(frame.size() - offset);
            if (!WriteFile(g_pipe, frame.data() + offset, remaining, &written, nullptr) ||
                written == 0)
            {
                CloseHandle(g_pipe);
                g_pipe = INVALID_HANDLE_VALUE;
                frameWritten = false;
                break;
            }
            offset += written;
        }
        if (frameWritten && isPendingJobFinalization)
        {
            std::lock_guard<std::mutex> lock(g_snapshotMutex);
            if (!g_pendingJobFinalSnapshots.empty() &&
                nlsi::encodeTelemetryFrame(g_pendingJobFinalSnapshots.front()) ==
                    nlsi::encodeTelemetryFrame(snapshot))
            {
                g_pendingJobFinalSnapshots.pop_front();
            }
        }
    }

    if (g_pipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle(g_pipe);
        g_pipe = INVALID_HANDLE_VALUE;
    }
}

SCSAPI_VOID onChannelValue(
    const scs_string_t name,
    const scs_u32_t index,
    const scs_value_t* const value,
    const scs_context_t context)
{
    (void)index;
    (void)context;

    if (!name)
    {
        return;
    }

    const bool available = value != nullptr;
    if (std::strcmp(name, SCS_TELEMETRY_TRUCK_CHANNEL_fuel) == 0)
    {
        const double fuel = available ? value->value_float.value : 0.0;
        if (available)
        {
            if (g_jobInProgress && g_lastFuelAvailable)
            {
                const double change = fuel - g_lastFuelAmount;
                if (change < 0.0) g_jobFuelConsumed += -change;
                else if (change > 0.05) g_jobRefueled += change;
            }
            g_lastFuelAmount = fuel;
            g_lastFuelAvailable = true;
        }
        else
        {
            g_lastFuelAvailable = false;
        }
        publishField("truck.fuel.amount", {"Fuel", available ? formatFloat(value->value_float.value) : "", "L", available});
    }
    else if (std::strcmp(name, SCS_TELEMETRY_TRUCK_CHANNEL_speed) == 0)
    {
        const auto now = std::chrono::steady_clock::now();
        if (available)
        {
            const double speedKmh = std::fabs(static_cast<double>(value->value_float.value)) * 3.6;
            if (g_jobInProgress)
            {
                g_jobMaxSpeedKmh = (std::max)(g_jobMaxSpeedKmh, speedKmh);
                if (g_lastSpeedSample.time_since_epoch().count() != 0)
                {
                    const double seconds = std::chrono::duration<double>(now - g_lastSpeedSample).count();
                    if (seconds > 0.0 && seconds <= 5.0)
                    {
                        g_jobSpeedIntegral += speedKmh * seconds;
                        g_jobSpeedSeconds += seconds;
                    }
                }
                g_lastSpeedSample = now;
            }
            publishField("truck.speed", {"Speed", formatFloat(value->value_float.value), "m/s", true});
        }
        else
        {
            g_lastSpeedSample = std::chrono::steady_clock::time_point();
            publishField("truck.speed", {"Speed", "", "m/s", false});
        }
    }
    else if (std::strcmp(name, SCS_TELEMETRY_TRUCK_CHANNEL_odometer) == 0)
    {
        publishField("truck.odometer", {"Odometer", available ? formatFloat(value->value_float.value) : "", "km", available});
    }
    else if (std::strcmp(name, SCS_TELEMETRY_TRUCK_CHANNEL_fuel_average_consumption) == 0)
    {
        publishField("truck.fuel.average_consumption",
            {"Average Fuel Consumption", available ? formatFloat(value->value_float.value) : "", "L/km", available});
    }
    else if (std::strcmp(name, SCS_TELEMETRY_JOB_CHANNEL_cargo_damage) == 0)
    {
        publishField("job.cargo_damage",
            {"Cargo Damage", available ? formatFloat(value->value_float.value) : "", "ratio", available});
    }
    else if (std::strcmp(name, SCS_TELEMETRY_TRUCK_CHANNEL_adblue) == 0)
    {
        publishField("truck.adblue", {"AdBlue", available ? formatFloat(value->value_float.value) : "", "L", available});
    }
    else if (std::strcmp(name, SCS_TELEMETRY_TRUCK_CHANNEL_engine_rpm) == 0)
    {
        publishField("truck.engine.rpm", {"Engine RPM", available ? formatFloat(value->value_float.value) : "", "RPM", available});
    }
    else if (std::strcmp(name, SCS_TELEMETRY_TRUCK_CHANNEL_engine_gear) == 0)
    {
        publishField("truck.engine.gear", {"Gear", available ? std::to_string(value->value_s32.value) : "", "", available});
    }
    else if (std::strcmp(name, SCS_TELEMETRY_TRUCK_CHANNEL_effective_throttle) == 0)
    {
        publishField("truck.effective.throttle", {"Throttle", available ? formatFloat(value->value_float.value) : "", "normalized [0,1]", available});
    }
    else if (std::strcmp(name, SCS_TELEMETRY_TRUCK_CHANNEL_effective_brake) == 0)
    {
        publishField("truck.effective.brake", {"Brake", available ? formatFloat(value->value_float.value) : "", "normalized [0,1]", available});
    }
    else if (std::strcmp(name, SCS_TELEMETRY_TRUCK_CHANNEL_effective_clutch) == 0)
    {
        publishField("truck.effective.clutch", {"Clutch", available ? formatFloat(value->value_float.value) : "", "normalized [0,1]", available});
    }
    else if (std::strcmp(name, SCS_TELEMETRY_TRUCK_CHANNEL_effective_steering) == 0)
    {
        publishField("truck.effective.steering", {"Steering", available ? formatFloat(value->value_float.value) : "", "normalized [-1,1]", available});
    }
    else if (std::strcmp(name, SCS_TELEMETRY_TRUCK_CHANNEL_cruise_control) == 0)
    {
        publishField("truck.cruise_control", {"Cruise Control", available ? formatFloat(value->value_float.value) : "", "m/s", available});
    }
    else if (std::strcmp(name, SCS_TELEMETRY_TRUCK_CHANNEL_retarder_level) == 0)
    {
        publishField("truck.brake.retarder", {"Retarder", available ? std::to_string(value->value_u32.value) : "", "level", available});
    }
    else if (std::strcmp(name, SCS_TELEMETRY_TRUCK_CHANNEL_parking_brake) == 0)
    {
        publishField("truck.brake.parking", {"Parking Brake", available && value->value_bool.value ? "true" : "false", "", available});
    }
    else if (std::strcmp(name, SCS_TELEMETRY_TRUCK_CHANNEL_navigation_distance) == 0)
    {
        publishField("truck.navigation.distance", {"Navigation Distance", available ? formatFloat(value->value_float.value) : "", "m", available});
    }
    else if (std::strcmp(name, SCS_TELEMETRY_TRUCK_CHANNEL_navigation_time) == 0)
    {
        publishField("truck.navigation.time", {"Navigation ETA", available ? formatFloat(value->value_float.value) : "", "s", available});
    }
}

std::string stringAttribute(const scs_named_value_t* const attributes, const char* const name)
{
    for (const scs_named_value_t* attribute = attributes; attribute && attribute->name; ++attribute)
    {
        if (std::strcmp(attribute->name, name) == 0 &&
            attribute->value.type == SCS_VALUE_TYPE_string &&
            attribute->value.value_string.value)
        {
            return attribute->value.value_string.value;
        }
    }
    return {};
}

std::string numericAttribute(const scs_named_value_t* const attributes, const char* const name)
{
    for (const scs_named_value_t* attribute = attributes; attribute && attribute->name; ++attribute)
    {
        if (std::strcmp(attribute->name, name) != 0)
        {
            continue;
        }
        switch (attribute->value.type)
        {
        case SCS_VALUE_TYPE_u32:
            return std::to_string(attribute->value.value_u32.value);
        case SCS_VALUE_TYPE_u64:
            return std::to_string(attribute->value.value_u64.value);
        case SCS_VALUE_TYPE_s32:
            return std::to_string(attribute->value.value_s32.value);
        case SCS_VALUE_TYPE_s64:
            return std::to_string(attribute->value.value_s64.value);
        case SCS_VALUE_TYPE_float:
            return formatFloat(attribute->value.value_float.value);
        default:
            return {};
        }
    }
    return {};
}

std::string boolAttribute(const scs_named_value_t* const attributes, const char* const name)
{
    for (const scs_named_value_t* attribute = attributes; attribute && attribute->name; ++attribute)
    {
        if (std::strcmp(attribute->name, name) == 0 &&
            attribute->value.type == SCS_VALUE_TYPE_bool)
        {
            return attribute->value.value_bool.value ? "true" : "false";
        }
    }
    return {};
}

void publishConfigurationField(
    const std::string& key,
    const std::string& label,
    std::string value,
    const std::string& unit = {})
{
    const bool available = !value.empty();
    publishField(key, {label, std::move(value), unit, available});
}

void publishTripStatistics()
{
    publishConfigurationField("job.total_fuel_consumed", "Total Fuel Consumed",
        g_jobFuelConsumed > 0.0 ? formatFloat(static_cast<scs_float_t>(g_jobFuelConsumed)) : "", "L");
    publishConfigurationField("job.refueled", "Refueled",
        g_jobRefueled > 0.0 ? formatFloat(static_cast<scs_float_t>(g_jobRefueled)) : "", "L");
    publishConfigurationField("job.max_speed_kmh", "Maximum Speed",
        g_jobMaxSpeedKmh > 0.0 ? formatFloat(static_cast<scs_float_t>(g_jobMaxSpeedKmh)) : "", "km/h");
    publishConfigurationField("job.average_speed_kmh", "Average Speed",
        g_jobSpeedSeconds > 0.0
            ? formatFloat(static_cast<scs_float_t>(g_jobSpeedIntegral / g_jobSpeedSeconds))
            : "",
        "km/h");
}

SCSAPI_VOID onTelemetryEvent(
    const scs_event_t event,
    const void* const eventInfo,
    const scs_context_t context)
{
    (void)context;
    if (!eventInfo)
    {
        return;
    }

    if (event == SCS_TELEMETRY_EVENT_gameplay)
    {
        const auto* const gameplayEvent =
            static_cast<const scs_telemetry_gameplay_event_t*>(eventInfo);
        if (gameplayEvent->id)
        {
            const std::string eventId = gameplayEvent->id;
            publishField("telemetry.event.latest",
                {"Latest SDK Event", eventId, "", true});

            if (eventId == SCS_TELEMETRY_GAMEPLAY_EVENT_job_delivered ||
                eventId == SCS_TELEMETRY_GAMEPLAY_EVENT_car_job_delivered)
            {
                const auto* attributes = gameplayEvent->attributes;
                const std::string revenue = numericAttribute(
                    attributes, SCS_TELEMETRY_GAMEPLAY_EVENT_ATTRIBUTE_revenue);
                const std::string xp = numericAttribute(
                    attributes, SCS_TELEMETRY_GAMEPLAY_EVENT_ATTRIBUTE_earned_xp);
                const std::string cargoDamage = numericAttribute(
                    attributes, SCS_TELEMETRY_GAMEPLAY_EVENT_ATTRIBUTE_cargo_damage);
                const std::string vehicleDamage = numericAttribute(
                    attributes, SCS_TELEMETRY_GAMEPLAY_EVENT_ATTRIBUTE_vehicle_damage);
                const std::string distance = numericAttribute(
                    attributes, SCS_TELEMETRY_GAMEPLAY_EVENT_ATTRIBUTE_distance_km);
                const std::string gameMinutes = numericAttribute(
                    attributes, SCS_TELEMETRY_GAMEPLAY_EVENT_ATTRIBUTE_delivery_time);
                publishConfigurationField("job.income", "Income", revenue, "game currency");
                publishConfigurationField("job.earned_xp", "XP", xp);
                publishConfigurationField("job.cargo_damage", "Cargo Damage", cargoDamage, "ratio");
                publishConfigurationField("job.vehicle_damage", "Vehicle Damage", vehicleDamage, "ratio");
                publishConfigurationField("job.driven_distance_km", "Driven Distance", distance, "km");
                publishConfigurationField("job.ingame_duration_minutes", "In-game Duration", gameMinutes, "min");
                publishTripStatistics();
                g_jobInProgress = false;
                publishJobStatus(nlsi::jobStatusToString(nlsi::JobStatus::Delivered));
                return;
            }

            if (eventId == SCS_TELEMETRY_GAMEPLAY_EVENT_job_cancelled ||
                eventId == SCS_TELEMETRY_GAMEPLAY_EVENT_car_job_cancelled)
            {
                const std::string penalty = numericAttribute(
                    gameplayEvent->attributes, SCS_TELEMETRY_GAMEPLAY_EVENT_ATTRIBUTE_cancel_penalty);
                publishConfigurationField("job.cancel_penalty", "Cancellation Penalty",
                    penalty, "game currency");
                publishTripStatistics();
                g_jobInProgress = false;
                publishJobStatus(nlsi::jobStatusToString(nlsi::JobStatus::Cancelled));
                return;
            }
        }
        return;
    }

    if (event != SCS_TELEMETRY_EVENT_configuration)
    {
        return;
    }

    const auto* const configuration =
        static_cast<const scs_telemetry_configuration_t*>(eventInfo);
    if (!configuration->id || !configuration->attributes)
    {
        return;
    }

    const std::string id = configuration->id;
    const scs_named_value_t* const attributes = configuration->attributes;
    if (id == SCS_TELEMETRY_CONFIG_job)
    {
        g_jobInProgress = true;
        publishJobStatus(nlsi::jobStatusToString(nlsi::JobStatus::Active));
        g_jobFuelConsumed = 0.0;
        g_jobRefueled = 0.0;
        g_jobMaxSpeedKmh = 0.0;
        g_jobSpeedIntegral = 0.0;
        g_jobSpeedSeconds = 0.0;
        g_lastSpeedSample = std::chrono::steady_clock::now();
        publishConfigurationField("job.earned_xp", "XP", {});
        publishConfigurationField("job.cargo_damage", "Cargo Damage", {}, "ratio");
        publishConfigurationField("job.vehicle_damage", "Vehicle Damage", {}, "ratio");
        publishConfigurationField("job.driven_distance_km", "Driven Distance", {}, "km");
        publishConfigurationField("job.ingame_duration_minutes", "In-game Duration", {}, "min");
        publishConfigurationField("job.total_fuel_consumed", "Total Fuel Consumed", {}, "L");
        publishConfigurationField("job.refueled", "Refueled", {}, "L");
        publishConfigurationField("job.max_speed_kmh", "Maximum Speed", {}, "km/h");
        publishConfigurationField("job.average_speed_kmh", "Average Speed", {}, "km/h");
        publishConfigurationField("job.cancel_penalty", "Cancellation Penalty", {}, "game currency");
        publishConfigurationField("job.cargo", "Cargo",
            stringAttribute(attributes, SCS_TELEMETRY_CONFIG_ATTRIBUTE_cargo));
        publishConfigurationField("job.source_city", "From",
            stringAttribute(attributes, SCS_TELEMETRY_CONFIG_ATTRIBUTE_source_city));
        publishConfigurationField("job.source_company", "Origin Company",
            stringAttribute(attributes, SCS_TELEMETRY_CONFIG_ATTRIBUTE_source_company));
        publishConfigurationField("job.destination_city", "To",
            stringAttribute(attributes, SCS_TELEMETRY_CONFIG_ATTRIBUTE_destination_city));
        publishConfigurationField("job.destination_company", "Destination Company",
            stringAttribute(attributes, SCS_TELEMETRY_CONFIG_ATTRIBUTE_destination_company));
        publishConfigurationField("job.income", "Income",
            numericAttribute(attributes, SCS_TELEMETRY_CONFIG_ATTRIBUTE_income), "game currency");
        publishConfigurationField("job.planned_distance_km", "Planned Distance",
            numericAttribute(attributes, SCS_TELEMETRY_CONFIG_ATTRIBUTE_planned_distance_km), "km");
        publishConfigurationField("job.special_cargo", "Special Cargo",
            boolAttribute(attributes, SCS_TELEMETRY_CONFIG_ATTRIBUTE_special_job));
    }
    else if (id == SCS_TELEMETRY_CONFIG_truck)
    {
        const std::string brand = stringAttribute(attributes, SCS_TELEMETRY_CONFIG_ATTRIBUTE_brand);
        const std::string name = stringAttribute(attributes, SCS_TELEMETRY_CONFIG_ATTRIBUTE_name);
        std::string description = brand;
        if (!name.empty())
        {
            if (!description.empty()) description += " ";
            description += name;
        }
        publishConfigurationField("vehicle.make_model", "Vehicle", std::move(description));
        publishConfigurationField("vehicle.license_plate", "Vehicle Plate",
            stringAttribute(attributes, SCS_TELEMETRY_CONFIG_ATTRIBUTE_license_plate));
        publishConfigurationField("truck.fuel.capacity", "Fuel Capacity",
            numericAttribute(attributes, SCS_TELEMETRY_CONFIG_ATTRIBUTE_fuel_capacity), "L");
    }
    else if (id == SCS_TELEMETRY_CONFIG_trailer || id.rfind("trailer.", 0) == 0)
    {
        const std::string brand = stringAttribute(attributes, SCS_TELEMETRY_CONFIG_ATTRIBUTE_brand);
        const std::string name = stringAttribute(attributes, SCS_TELEMETRY_CONFIG_ATTRIBUTE_name);
        std::string description = brand;
        if (!name.empty())
        {
            if (!description.empty()) description += " ";
            description += name;
        }
        const std::string plate =
            stringAttribute(attributes, SCS_TELEMETRY_CONFIG_ATTRIBUTE_license_plate);
        const std::string trailerId = id == SCS_TELEMETRY_CONFIG_trailer ? "trailer.0" : id;
        std::string descriptions;
        std::string plates;
        {
            std::lock_guard<std::mutex> lock(g_trailerMutex);
            if (description.empty() && plate.empty())
            {
                g_trailers.erase(trailerId);
            }
            else
            {
                g_trailers[trailerId] = {std::move(description), plate};
            }
            for (const auto& trailer : g_trailers)
            {
                if (!trailer.second.first.empty())
                {
                    if (!descriptions.empty()) descriptions += ", ";
                    descriptions += trailer.second.first;
                }
                if (!trailer.second.second.empty())
                {
                    if (!plates.empty()) plates += ", ";
                    plates += trailer.second.second;
                }
            }
        }
        publishConfigurationField("trailer.details", "Trailers", std::move(descriptions));
        publishConfigurationField("trailer.license_plate", "Trailer Plate", std::move(plates));
    }
}

void registerChannel(
    const scs_telemetry_register_for_channel_t registerForChannel,
    const char* const channel,
    const scs_value_type_t type)
{
    const scs_result_t result = registerForChannel(
        channel,
        SCS_U32_NIL,
        type,
        SCS_TELEMETRY_CHANNEL_FLAG_no_value | SCS_TELEMETRY_CHANNEL_FLAG_each_frame,
        onChannelValue,
        nullptr);
    if (result != SCS_RESULT_ok)
    {
        logRegistrationFailure(channel, result);
    }
}

} // namespace

SCSAPI_RESULT scs_telemetry_init(
    const scs_u32_t version,
    const scs_telemetry_init_params_t* const params)
{
    if (!params)
    {
        return SCS_RESULT_invalid_parameter;
    }
    if (SCS_GET_MAJOR_VERSION(version) != SCS_GET_MAJOR_VERSION(SCS_TELEMETRY_VERSION_CURRENT))
    {
        return SCS_RESULT_unsupported;
    }

    const auto* init = reinterpret_cast<const scs_telemetry_init_params_v100_t*>(params);
    g_gameLog = init->common.log;
    if (!init->register_for_channel)
    {
        g_gameLog = nullptr;
        return SCS_RESULT_invalid_parameter;
    }
    if (!init->common.game_id || !init->common.game_name ||
        (std::strcmp(init->common.game_id, SCS_GAME_ID_EUT2) != 0 &&
         std::strcmp(init->common.game_id, SCS_GAME_ID_ATS) != 0))
    {
        g_gameLog = nullptr;
        return SCS_RESULT_unsupported;
    }

    {
        std::lock_guard<std::mutex> lock(g_snapshotMutex);
        g_snapshot = nlsi::TelemetrySnapshot{};
        g_pendingJobFinalSnapshots.clear();
        g_snapshot.status = nlsi::ConnectionState::Connected;
        g_snapshot.gameId = init->common.game_id;
        g_snapshot.gameName = init->common.game_name;
        g_snapshot.gameApiVersion =
            std::to_string(SCS_GET_MAJOR_VERSION(init->common.game_version)) + "." +
            std::to_string(SCS_GET_MINOR_VERSION(init->common.game_version));
        g_snapshotRevision = 1;
    }
    {
        std::lock_guard<std::mutex> lock(g_trailerMutex);
        g_trailers.clear();
    }
    g_lastFuelAmount = 0.0;
    g_lastFuelAvailable = false;
    g_jobFuelConsumed = 0.0;
    g_jobRefueled = 0.0;
    g_jobMaxSpeedKmh = 0.0;
    g_jobSpeedIntegral = 0.0;
    g_jobSpeedSeconds = 0.0;
    g_lastSpeedSample = std::chrono::steady_clock::time_point();
    g_stopWorker.store(false);
    g_ipcWorker = std::thread(ipcWorker);

    if (init->register_for_event)
    {
        const scs_result_t configResult = init->register_for_event(
            SCS_TELEMETRY_EVENT_configuration, onTelemetryEvent, nullptr);
        if (configResult != SCS_RESULT_ok)
        {
            logRegistrationFailure("configuration event", configResult);
        }
        const scs_result_t gameplayResult = init->register_for_event(
            SCS_TELEMETRY_EVENT_gameplay, onTelemetryEvent, nullptr);
        if (gameplayResult != SCS_RESULT_ok)
        {
            logRegistrationFailure("gameplay event", gameplayResult);
        }
    }

    registerChannel(init->register_for_channel, SCS_TELEMETRY_TRUCK_CHANNEL_fuel, SCS_VALUE_TYPE_float);
    registerChannel(init->register_for_channel, SCS_TELEMETRY_TRUCK_CHANNEL_speed, SCS_VALUE_TYPE_float);
    registerChannel(init->register_for_channel, SCS_TELEMETRY_TRUCK_CHANNEL_odometer, SCS_VALUE_TYPE_float);
    registerChannel(init->register_for_channel, SCS_TELEMETRY_TRUCK_CHANNEL_fuel_average_consumption, SCS_VALUE_TYPE_float);
    registerChannel(init->register_for_channel, SCS_TELEMETRY_JOB_CHANNEL_cargo_damage, SCS_VALUE_TYPE_float);
    if (std::strcmp(init->common.game_id, SCS_GAME_ID_EUT2) == 0)
    {
        registerChannel(init->register_for_channel, SCS_TELEMETRY_TRUCK_CHANNEL_adblue, SCS_VALUE_TYPE_float);
    }
    registerChannel(init->register_for_channel, SCS_TELEMETRY_TRUCK_CHANNEL_engine_rpm, SCS_VALUE_TYPE_float);
    registerChannel(init->register_for_channel, SCS_TELEMETRY_TRUCK_CHANNEL_engine_gear, SCS_VALUE_TYPE_s32);
    registerChannel(init->register_for_channel, SCS_TELEMETRY_TRUCK_CHANNEL_effective_throttle, SCS_VALUE_TYPE_float);
    registerChannel(init->register_for_channel, SCS_TELEMETRY_TRUCK_CHANNEL_effective_brake, SCS_VALUE_TYPE_float);
    registerChannel(init->register_for_channel, SCS_TELEMETRY_TRUCK_CHANNEL_effective_clutch, SCS_VALUE_TYPE_float);
    registerChannel(init->register_for_channel, SCS_TELEMETRY_TRUCK_CHANNEL_effective_steering, SCS_VALUE_TYPE_float);
    registerChannel(init->register_for_channel, SCS_TELEMETRY_TRUCK_CHANNEL_cruise_control, SCS_VALUE_TYPE_float);
    registerChannel(init->register_for_channel, SCS_TELEMETRY_TRUCK_CHANNEL_retarder_level, SCS_VALUE_TYPE_u32);
    registerChannel(init->register_for_channel, SCS_TELEMETRY_TRUCK_CHANNEL_parking_brake, SCS_VALUE_TYPE_bool);
    registerChannel(init->register_for_channel, SCS_TELEMETRY_TRUCK_CHANNEL_navigation_distance, SCS_VALUE_TYPE_float);
    registerChannel(init->register_for_channel, SCS_TELEMETRY_TRUCK_CHANNEL_navigation_time, SCS_VALUE_TYPE_float);

    return SCS_RESULT_ok;
}

SCSAPI_VOID scs_telemetry_shutdown(void)
{
    g_stopWorker.store(true);
    g_snapshotChanged.notify_one();
    if (g_ipcWorker.joinable())
    {
        g_ipcWorker.join();
    }
    g_gameLog = nullptr;
}
