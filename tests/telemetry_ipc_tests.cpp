#include <iostream>
#include <limits>
#include <string>

#include <QDateTime>

#include "DisplayFormatting.h"
#include "TelemetryIPC.h"
#include "GameProcessDetection.h"

namespace {

bool check(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        return false;
    }
    return true;
}

} // namespace

int main()
{
    bool ok = true;

    const std::string payload =
        "status|Connected\n"
        "game_id|eut2\n"
        "game|ETS2\n"
        "api_version|1.01\n"
        "field|truck.fuel.amount|Fuel|42.7|L|1\n"
        "field|truck.engine.rpm|Engine RPM|1600|RPM|1\n"
        "field|job.income|Income|Unavailable|€|0\n";

    const nlsi::TelemetrySnapshot snapshot = nlsi::decodeTelemetryFrame(payload);
    ok = check(snapshot.status == nlsi::ConnectionState::Connected, "status decoded") && ok;
    ok = check(snapshot.gameName == "ETS2", "game name decoded") && ok;
    ok = check(snapshot.gameId == "eut2", "game id decoded") && ok;
    ok = check(snapshot.gameApiVersion == "1.01", "game API version decoded") && ok;
    ok = check(snapshot.values.at("truck.fuel.amount").value == "42.7", "fuel value decoded") && ok;
    ok = check(snapshot.values.at("truck.engine.rpm").available, "rpm availability decoded") && ok;
    ok = check(snapshot.values.at("job.income").available == false, "unavailable value handled") && ok;

    const nlsi::TelemetrySnapshot encodedSnapshot = snapshot;
    const std::string encoded = nlsi::encodeTelemetryFrame(encodedSnapshot);
    ok = check(encoded.find("status|Connected") != std::string::npos, "encoded snapshot keeps status") && ok;
    ok = check(encoded.find("game_id|eut2") != std::string::npos, "encoded snapshot keeps game id") && ok;
    ok = check(encoded.find("api_version|1.01") != std::string::npos, "encoded snapshot keeps API version") && ok;
    ok = check(encoded.find("field|truck.fuel.amount|Fuel|42.7|L|1") != std::string::npos, "encoded snapshot keeps fields") && ok;

    const nlsi::TelemetrySnapshot disconnected = nlsi::decodeTelemetryFrame("");
    ok = check(disconnected.status == nlsi::ConnectionState::Disconnected, "empty input remains disconnected") && ok;
    ok = check(disconnected.values.empty(), "empty input does not fabricate telemetry") && ok;

    const nlsi::TelemetrySnapshot malformed = nlsi::decodeTelemetryFrame(
        "status|Connected\nfield|incomplete|Field\nnot-a-telemetry-record\n");
    ok = check(malformed.values.empty(), "malformed field records are ignored") && ok;
    ok = check(malformed.status == nlsi::ConnectionState::Error, "malformed frames report an error") && ok;

    nlsi::TelemetrySnapshot escaped;
    escaped.status = nlsi::ConnectionState::Connected;
    escaped.values["job.cargo"] = {"Cargo", "Steel | beams\nheavy", "tons", true};
    const nlsi::TelemetrySnapshot escapedDecoded =
        nlsi::decodeTelemetryFrame(nlsi::encodeTelemetryFrame(escaped));
    ok = check(escapedDecoded.values.at("job.cargo").value == "Steel | beams\nheavy",
               "escaped field delimiters round-trip") && ok;

    ok = check(nlsi::gameProcessStateFromFlags(false, false) == nlsi::GameProcessState::NotRunning,
               "neither game process detected") && ok;
    ok = check(nlsi::gameProcessStateFromFlags(true, false) == nlsi::GameProcessState::Ets2Running,
               "ETS2 process detected") && ok;
    ok = check(nlsi::gameProcessStateFromFlags(false, true) == nlsi::GameProcessState::AtsRunning,
               "ATS process detected") && ok;
    ok = check(nlsi::gameProcessStateFromFlags(true, true) == nlsi::GameProcessState::BothRunning,
               "both game processes are explicit") && ok;
    ok = check(nlsi::connectionStateFromString("Disconnected") == nlsi::ConnectionState::Disconnected &&
               nlsi::connectionStateFromString("Connecting") == nlsi::ConnectionState::Connecting &&
               nlsi::connectionStateFromString("Connected") == nlsi::ConnectionState::Connected &&
               nlsi::connectionStateFromString("Error") == nlsi::ConnectionState::Error,
               "all telemetry connection states decode") && ok;

    ok = check(nlsi::uppercaseDisplayText("Euro Truck Simulator 2") == "EURO TRUCK SIMULATOR 2",
               "display labels are uppercased") && ok;
    ok = check(nlsi::formatDisplayNumber(12.3456, 2) == "12.35",
               "decimal display formatting uses two decimals") && ok;
    ok = check(nlsi::formatFixedNumber("1171.96", "RPM") == "1171.96 RPM",
               "RPM formatting preserves exactly two decimals") && ok;
    ok = check(nlsi::formatFixedNumber("invalid", "RPM") == "—",
               "invalid fixed-point values remain unavailable") && ok;
    ok = check(nlsi::formatDistanceKilometres(150000.0, "m") == "150.00 KM",
               "distance conversion uses metres to kilometres") && ok;
    ok = check(nlsi::formatProgressPercent(100.0, 50.0) == "50.00%",
               "progress percentage is calculated correctly") && ok;
    ok = check(nlsi::formatProgressPercent(0.0, 50.0) == "—",
               "missing planned distance shows an em dash") && ok;
    ok = check(nlsi::formatNormalizedPercent("0.5", "normalized [0,1]") == "50.00%",
               "normalized throttle and brake values convert to percentages") && ok;
    ok = check(nlsi::formatCruiseKmH("25", "m/s") == "90.00 KM/H",
               "cruise-control speed is converted to km/h") && ok;
    ok = check(nlsi::formatDurationHhmmss(5432.0) == "01:30:32",
               "duration formatting preserves hours and minutes") && ok;
    ok = check(nlsi::formatDurationHhmmss(15.0 * 60.0) == "00:15:00",
               "duration formatter can represent converted real time") && ok;
    const QDateTime beforeMidnight = QDateTime::fromString("2025-01-01T23:59:00", Qt::ISODate);
    ok = check(nlsi::formatArrivalEstimate(15.0 * 60.0, beforeMidnight) ==
                   "00:01:00 - 00:00:00",
               "15 game minutes convert to one real minute and roll over midnight") && ok;
    ok = check(nlsi::formatArrivalEstimate(3600.0, beforeMidnight) ==
                   "00:04:00 - 00:03:00",
               "one game hour converts to four real minutes") && ok;
    ok = check(nlsi::formatArrivalEstimate(9.0 * 3600.0, beforeMidnight) ==
                   "00:36:00 - 00:35:00",
               "nine game hours convert to 36 real minutes") && ok;
    ok = check(nlsi::formatArrivalEstimate(24.0 * 3600.0, beforeMidnight) ==
                   "01:36:00 - 01:35:00",
               "24 game hours convert to 96 real minutes across a day boundary") && ok;
    ok = check(nlsi::formatArrivalEstimate(-1.0, beforeMidnight) == "—" &&
               nlsi::formatArrivalEstimate(std::numeric_limits<double>::infinity(), beforeMidnight) == "—",
               "invalid navigation ETAs remain unavailable") && ok;
    const QDateTime arrival = QDateTime::currentDateTime().addSecs(3600);
    ok = check(nlsi::formatArrivalClock(3600.0) == arrival.toString("HH:mm:ss"),
               "arrival clock time is computed from current time") && ok;
    ok = check(nlsi::formatFuelRemainingPercent(20.0, 100.0) == "20.00%" &&
               nlsi::isFuelAtOrBelowWarningThreshold(20.0, 100.0),
               "fuel at the 20 percent threshold is warned") && ok;
    ok = check(nlsi::formatFuelRemainingPercent(20.01, 100.0) == "20.01%" &&
               !nlsi::isFuelAtOrBelowWarningThreshold(20.01, 100.0),
               "fuel above the 20 percent threshold is not warned") && ok;
    ok = check(nlsi::formatFuelDisplay(23.11, 100.0) == "23.11 L / 100.00 L (23.11%)",
               "fuel amount, capacity, and percentage use two decimals") && ok;
    ok = check(nlsi::formatFuelDisplay(23.11, 0.0) == "23.11 L / — (—)",
               "fuel display shows unavailable capacity and percentage without hiding current fuel") && ok;
    ok = check(nlsi::formatFuelRemainingPercent(10.0, 0.0) == "—" &&
               nlsi::formatFuelRemainingPercent(10.0, -1.0) == "—" &&
               nlsi::formatFuelRemainingPercent(std::numeric_limits<double>::quiet_NaN(), 100.0) == "—" &&
               !nlsi::isFuelAtOrBelowWarningThreshold(10.0, 0.0),
               "zero, invalid, and unavailable fuel capacities are handled safely") && ok;
    ok = check(nlsi::formatCurrencyEuro(5780.0) == "€5,780.00",
               "euro currency formatting includes the euro symbol and two decimals") && ok;
    ok = check(nlsi::jobStatusToString(nlsi::JobStatus::Active) == "ACTIVE" &&
               nlsi::jobStatusToString(nlsi::JobStatus::Delivered) == "DELIVERED" &&
               nlsi::jobStatusToString(nlsi::JobStatus::Cancelled) == "CANCELLED" &&
               nlsi::jobStatusFromString("completed") == nlsi::JobStatus::Delivered &&
               nlsi::jobStatusFromString("cancelled") == nlsi::JobStatus::Cancelled,
               "job status values are normalized across the telemetry pipeline") && ok;
    ok = check(nlsi::formatJobId(1) == "NLSI-0001" &&
               nlsi::formatJobId(9999) == "NLSI-9999" &&
               nlsi::formatJobId(10000) == "NLSI-10000",
               "job IDs use a four-digit minimum and grow naturally beyond 9999") && ok;
    const nlsi::TelemetrySnapshot jobStatusSnapshot = nlsi::decodeTelemetryFrame(
        "status|Connected\nfield|job.status|Status|ACTIVE||1\nfield|job.id|Job ID|NLSI-0001||1\n");
    ok = check(jobStatusSnapshot.values.at("job.status").value == "ACTIVE" &&
               jobStatusSnapshot.values.at("job.id").value == "NLSI-0001",
               "job status and ID values survive the IPC round trip") && ok;

    if (!ok)
    {
        return 1;
    }

    std::cout << "All NLSI telemetry IPC tests passed." << std::endl;
    return 0;
}
