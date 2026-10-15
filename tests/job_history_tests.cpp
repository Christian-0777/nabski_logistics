#include <iostream>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "JobHistory.h"

namespace {

bool check(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

nlsi::TelemetrySnapshot activeSnapshot()
{
    nlsi::TelemetrySnapshot snapshot;
    snapshot.status = nlsi::ConnectionState::Connected;
    snapshot.gameId = "eut2";
    snapshot.gameName = "Euro Truck Simulator 2";
    snapshot.values["job.status"] = {"Status", "ACTIVE", "", true};
    snapshot.values["job.cargo"] = {"Cargo", "Steel", "", true};
    snapshot.values["job.source_city"] = {"From", "Berlin", "", true};
    snapshot.values["job.destination_city"] = {"To", "Paris", "", true};
    snapshot.values["job.income"] = {"Income", "1200", "game currency", true};
    return snapshot;
}

nlsi::TelemetrySnapshot terminalSnapshot(const char* status)
{
    auto snapshot = activeSnapshot();
    snapshot.values["job.status"] = {"Status", status, "", true};
    snapshot.values["job.earned_xp"] = {"XP", "50", "", true};
    snapshot.values["job.driven_distance_km"] = {"Driven Distance", "780.5", "km", true};
    return snapshot;
}

} // namespace

int main()
{
    bool ok = true;
    QTemporaryDir temporary;
    ok = check(temporary.isValid(), "temporary job-record directory is created") && ok;
    const QString directory = QDir(temporary.path()).filePath("jobs");
    const QDateTime start = QDateTime::fromString("2026-10-15T01:00:00.000Z", Qt::ISODateWithMs);
    const QDateTime finish = start.addSecs(3600);

    {
        nlsi::JobHistory history(directory);
        ok = check(history.isValid(), "job history store initializes") && ok;
        auto active = activeSnapshot();
        ok = check(history.processSnapshot(active, start), "active job receives a persistent ID") && ok;
        ok = check(history.activeJob() && history.activeJob()->id == "NLSI-0001",
                   "first active job uses NLSI-0001") && ok;
        ok = check(history.completedJobs().empty(), "active job is excluded from completed history") && ok;

        auto partial = activeSnapshot();
        partial.values["job.source_city"].available = false;
        partial.values["job.source_city"].value.clear();
        partial.values["job.destination_city"].value.clear();
        ok = check(history.processSnapshot(partial, start.addSecs(1)) &&
                   history.activeJob() && history.activeJob()->id == "NLSI-0001" &&
                   history.activeJob()->snapshot.values.at("job.source_city").value == "Berlin" &&
                   history.activeJob()->snapshot.values.at("job.destination_city").value == "Paris",
                   "transiently missing or empty job fields do not replace a persisted active route") && ok;
        partial.values.erase("job.status");
        ok = check(!history.processSnapshot(partial, start.addSecs(2)) &&
                   history.activeJob() && history.activeJob()->id == "NLSI-0001",
                   "a transiently missing status does not finalize or replace an active job") && ok;
    }

    {
        nlsi::JobHistory history(directory);
        ok = check(history.activeJob() && history.activeJob()->id == "NLSI-0001",
                   "active job ID survives an application restart") && ok;
        auto delivered = terminalSnapshot("DELIVERED");
        ok = check(history.processSnapshot(delivered, finish), "confirmed delivery finalizes the job") && ok;
        ok = check(!history.activeJob(), "finalized job is no longer marked active") && ok;
        auto records = history.completedJobs();
        ok = check(records.size() == 1 && records.front().status == nlsi::JobStatus::Delivered,
                   "delivered job is loaded from its .nlsi record") && ok;
        ok = check(records.front().snapshot.values.at("job.earned_xp").value == "50" &&
                   records.front().snapshot.values.at("job.driven_distance_km").value == "780.5",
                   "final event telemetry is persisted") && ok;
        ok = check(!history.processSnapshot(delivered, finish.addSecs(1)),
                   "repeated terminal event does not create a duplicate") && ok;
        ok = check(history.completedJobs().size() == 1, "duplicate terminal event leaves one card record") && ok;

        auto next = activeSnapshot();
        next.values["job.cargo"].value = "Lumber";
        ok = check(history.processSnapshot(next, finish.addSecs(60)), "next job starts") && ok;
        auto cancelled = terminalSnapshot("CANCELLED");
        cancelled.values["job.cargo"].value = "Lumber";
        ok = check(history.processSnapshot(cancelled, finish.addSecs(120)),
                   "confirmed cancellation finalizes the active job") && ok;
        records = history.completedJobs();
        ok = check(records.size() == 2 && records.front().status == nlsi::JobStatus::Cancelled,
                   "completed records are sorted newest first and cancelled jobs remain visible") && ok;
    }

    {
        QFile sequence(QDir(directory).filePath(".sequence"));
        ok = check(sequence.open(QIODevice::WriteOnly | QIODevice::Truncate),
                   "sequence test fixture can be written") && ok;
        sequence.write("10000");
        sequence.close();
        nlsi::JobHistory history(directory);
        ok = check(history.allocateJobId() == "NLSI-10000",
                   "job ID sequence expands beyond four digits") && ok;
        QFile malformed(QDir(directory).filePath("NLSI-9999.nlsi"));
        ok = check(malformed.open(QIODevice::WriteOnly | QIODevice::Truncate),
                   "malformed record fixture can be written") && ok;
        malformed.write("{broken");
        malformed.close();
        QString errors;
        ok = check(history.completedJobs(&errors).size() == 2 && !errors.isEmpty(),
                   "malformed record is reported and skipped without hiding valid records") && ok;
    }

    {
        nlsi::JobHistory history(directory);
        auto active = activeSnapshot();
        active.values["job.cargo"].value = "Unfinished";
        ok = check(history.processSnapshot(active, finish.addSecs(300)), "unfinished job is persisted as active") && ok;
        auto disconnected = active;
        disconnected.values.erase("job.status");
        ok = check(!history.processSnapshot(disconnected, finish.addSecs(600)),
                   "missing or disconnected status does not finalize an active job") && ok;
        ok = check(history.completedJobs().size() == 2, "interrupted active job remains out of completed history") && ok;
    }

    {
        const QString routeDirectory = QDir(temporary.path()).filePath("route-change-jobs");
        nlsi::JobHistory history(routeDirectory);
        auto first = activeSnapshot();
        ok = check(history.processSnapshot(first, start), "route-change test starts an active job") && ok;
        auto changedRoute = first;
        changedRoute.values["job.source_city"].value = "Prague";
        ok = check(history.processSnapshot(changedRoute, start.addSecs(1)) &&
                   history.activeJob() && history.activeJob()->id == "NLSI-0002" &&
                   history.activeJob()->snapshot.values.at("job.source_city").value == "Prague",
                   "a valid changed job route still starts a new job record") && ok;
    }

    if (!ok)
    {
        return 1;
    }
    std::cout << "All NLSI job history tests passed." << std::endl;
    return 0;
}
