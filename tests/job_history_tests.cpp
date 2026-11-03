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
    snapshot.values["job.current"] = {"Current Job", "true", "", true};
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
    snapshot.values["job.current"] = {"Current Job", "false", "", true};
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
        auto paused = activeSnapshot();
        paused.values["job.status"] = {"Status", "PAUSED", "", true};
        ok = check(history.processSnapshot(paused, start.addSecs(3)) &&
                   history.activeJob() &&
                   history.activeJob()->status == nlsi::JobStatus::Paused &&
                   history.activeJob()->id == "NLSI-0001",
                   "pausing persists the existing job as non-terminal without allocating a new ID") && ok;
        nlsi::JobHistory pausedRecovery(directory);
        ok = check(pausedRecovery.activeJob() &&
                   pausedRecovery.activeJob()->status == nlsi::JobStatus::Paused &&
                   pausedRecovery.activeJob()->id == "NLSI-0001",
                   "paused lifecycle state survives a local-record reload") && ok;
        auto resumed = activeSnapshot();
        ok = check(history.processSnapshot(resumed, start.addSecs(4)) &&
                   history.activeJob() &&
                   history.activeJob()->status == nlsi::JobStatus::Active &&
                   history.activeJob()->id == "NLSI-0001",
                   "resuming returns the same active job to ACTIVE") && ok;
    }

    {
        nlsi::JobHistory history(directory);
        ok = check(history.activeJob() && history.activeJob()->id == "NLSI-0001",
                   "active job ID survives an application restart") && ok;
        ok = check(history.activeJob()->status == nlsi::JobStatus::Active,
                   "resumed job lifecycle state survives an application restart") && ok;
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
        QTemporaryDir trailerDirectory;
        const QString historyDirectory = QDir(trailerDirectory.path()).filePath("jobs");
        nlsi::JobHistory history(historyDirectory);
        auto preJob = activeSnapshot();
        preJob.values["trailer.details"] = {"Trailers", "Owned Flatbed", "", true};
        preJob.values["trailer.license_plate"] = {"Trailer Plate", "OWNED-1", "", true};
        auto assignedTrailerJob = activeSnapshot();
        assignedTrailerJob.values["job.market"] = {"Job Market", "freight_market", "", true};
        assignedTrailerJob.values["trailer.connected"] = {"Trailer Connected", "true", "", true};
        assignedTrailerJob.values["trailer.details"] = {"Trailers", "Freight Box Trailer", "", true};
        assignedTrailerJob.values["trailer.id"] = {"Trailer ID", "freight.box", "", true};
        assignedTrailerJob.values["trailer.license_plate"] =
            {"Trailer Plate", "JOB-9", "", true};

        ok = check(history.processSnapshot(assignedTrailerJob, start, &preJob) &&
                   history.activeJob() &&
                   history.activeJob()->snapshot.values.at("job.market").value == "freight_market" &&
                   history.activeJob()->snapshot.values.at("trailer.details").value ==
                       "Freight Box Trailer" &&
                   history.activeJob()->snapshot.values.at("trailer.id").value == "freight.box" &&
                   history.activeJob()->snapshot.values.at("trailer.license_plate").value == "JOB-9",
                   "Freight Market initialization captures current SDK trailer details") && ok;

        auto changedTrailerJob = assignedTrailerJob;
        changedTrailerJob.values["trailer.details"] =
            {"Trailers", "Freight Box Trailer Updated", "", true};
        changedTrailerJob.values["trailer.id"] = {"Trailer ID", "freight.box.updated", "", true};
        changedTrailerJob.values["trailer.license_plate"] =
            {"Trailer Plate", "JOB-10", "", true};
        ok = check(history.processSnapshot(changedTrailerJob, start.addSecs(1)) &&
                   history.activeJob()->snapshot.values.at("trailer.details").value ==
                       "Freight Box Trailer Updated" &&
                   history.activeJob()->snapshot.values.at("trailer.license_plate").value == "JOB-10",
                   "later valid trailer configuration replaces the initial trailer details") && ok;

        auto delivered = terminalSnapshot("DELIVERED");
        ok = check(history.processSnapshot(delivered, finish), "captured-trailer job completes") && ok;
        const auto completed = history.completedJobs();
        ok = check(completed.size() == 1 &&
                   completed.front().snapshot.values.at("trailer.details").value ==
                       "Freight Box Trailer Updated" &&
                   completed.front().snapshot.values.at("trailer.id").value == "freight.box.updated" &&
                   completed.front().snapshot.values.at("trailer.license_plate").value == "JOB-10",
                   "completed .nlsi record retains the latest trailer configuration") && ok;
    }

    {
        QTemporaryDir trailerDirectory;
        nlsi::JobHistory history(QDir(trailerDirectory.path()).filePath("jobs"));
        nlsi::TelemetrySnapshot noPreJobTrailer;
        auto active = activeSnapshot();
        active.values["trailer.connected"] = {"Trailer Connected", "true", "", true};
        active.values["trailer.details"] = {"Trailers", "Market Trailer", "", true};
        active.values["trailer.license_plate"] = {"Trailer Plate", "JOB-1", "", true};
        ok = check(history.processSnapshot(active, start, &noPreJobTrailer) &&
                   history.activeJob() &&
                   history.activeJob()->snapshot.values.at("trailer.details").value == "Market Trailer" &&
                   history.activeJob()->snapshot.values.at("trailer.license_plate").value == "JOB-1",
                   "active job stores SDK trailer details even without a pre-job snapshot") && ok;
        auto partial = active;
        partial.values["trailer.details"].available = false;
        partial.values["trailer.details"].value.clear();
        partial.values["trailer.license_plate"].available = false;
        partial.values["trailer.license_plate"].value.clear();
        ok = check(history.processSnapshot(partial, start.addSecs(1)) &&
                   history.activeJob()->snapshot.values.at("trailer.details").value == "Market Trailer" &&
                   history.activeJob()->snapshot.values.at("trailer.license_plate").value == "JOB-1",
                   "missing trailer data in a later snapshot does not erase valid values") && ok;
        ok = check(history.activeJob() &&
                   history.activeJob()->snapshot.values.at("trailer.details").available,
                   "trailer details remain available during transient channel gaps") && ok;
        nlsi::JobRecord serverRecord;
        serverRecord.localJobId = history.activeJob()->id;
        serverRecord.status = nlsi::JobStatus::Active;
        serverRecord.snapshot.values["trailer.details"] =
            {"Trailers", "Server Job Trailer", "", true};
        serverRecord.snapshot.values["trailer.license_plate"] =
            {"Trailer Plate", "SERVER-1", "", true};
        ok = check(history.reconcileServerRecord(serverRecord) &&
                   history.activeJob()->snapshot.values.at("trailer.details").value == "Market Trailer" &&
                   history.activeJob()->snapshot.values.at("trailer.license_plate").value == "JOB-1",
                   "API synchronization does not replace local telemetry trailer details") && ok;

        auto detached = active;
        detached.values["trailer.connected"] = {"Trailer Connected", "false", "", true};
        detached.values["trailer.details"] = {"Trailers", "", "", false};
        detached.values["trailer.license_plate"] = {"Trailer Plate", "", "", false};
        ok = check(history.processSnapshot(detached, start.addSecs(2)) &&
                   !history.activeJob()->snapshot.values.at("trailer.details").available &&
                   !history.activeJob()->snapshot.values.at("trailer.license_plate").available,
                   "an explicit SDK detached state clears trailer details") && ok;
        nlsi::JobHistory reloaded(history.directory());
        ok = check(reloaded.activeJob() &&
                   !reloaded.activeJob()->snapshot.values.at("trailer.details").available &&
                   !reloaded.activeJob()->snapshot.values.at("trailer.license_plate").available,
                   "the detached trailer state survives active-record persistence") && ok;

        auto delivered = terminalSnapshot("DELIVERED");
        ok = check(history.processSnapshot(delivered, finish), "detached-trailer job completes") && ok;
        auto nextJob = activeSnapshot();
        nextJob.values["trailer.connected"] = {"Trailer Connected", "false", "", true};
        ok = check(history.processSnapshot(nextJob, finish.addSecs(60)) &&
                   history.activeJob() &&
                   !history.activeJob()->snapshot.values.at("trailer.details").available &&
                   !history.activeJob()->snapshot.values.at("trailer.license_plate").available,
                   "new job initialization cannot inherit a detached trailer from the prior job") && ok;
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

    {
        const QString syncDirectory = QDir(temporary.path()).filePath("sync-state-jobs");
        nlsi::JobHistory history(syncDirectory);
        auto active = activeSnapshot();
        ok = check(history.processSnapshot(active, start), "sync-state job starts locally") && ok;
        auto delivered = terminalSnapshot("DELIVERED");
        ok = check(history.processSnapshot(delivered, finish), "sync-state job is completed locally") && ok;
        QString syncError;
        ok = check(history.claimForAccount("NLSI-0001", 7, &syncError) && syncError.isEmpty(),
                   "an unassigned local job can be claimed by the authenticated account") && ok;
        nlsi::JobSyncState synced;
        synced.accountId = 7;
        synced.status = "synced";
        synced.serverJobId = "NLSI-9001";
        ok = check(history.setSyncState("NLSI-0001", synced, &syncError),
                   "server acknowledgment is persisted separately from the local job") && ok;
        nlsi::JobHistory restarted(syncDirectory);
        const auto persisted = restarted.syncState("NLSI-0001", &syncError);
        ok = check(persisted && persisted->accountId == 7 &&
                   persisted->status == "synced" &&
                   persisted->serverJobId == "NLSI-9001" &&
                   restarted.completedJobs().size() == 1,
                   "job synchronization state survives restart without replacing the local record") && ok;
        ok = check(!restarted.claimForAccount("NLSI-0001", 8, &syncError) &&
                   syncError.contains("another account"),
                   "a job assigned to one account cannot be claimed by another account") && ok;
    }

    {
        const QString reconcileDirectory = QDir(temporary.path()).filePath("reconciled-jobs");
        nlsi::JobHistory history(reconcileDirectory);
        ok = check(history.processSnapshot(activeSnapshot(), start),
                   "reconciliation fixture starts a local job") && ok;
        auto delivered = terminalSnapshot("DELIVERED");
        delivered.values.erase("job.earned_xp");
        ok = check(history.processSnapshot(delivered, finish),
                   "reconciliation fixture saves the local delivery") && ok;

        nlsi::JobRecord server;
        server.id = "NLSI-9001";
        server.localJobId = "NLSI-0001";
        server.status = nlsi::JobStatus::Cancelled;
        server.startedAt = start;
        server.completedAt = finish.addSecs(15);
        server.snapshot.values["job.source_city"] = {"From", "", "", false};
        server.snapshot.values["job.earned_xp"] = {"XP", "75", "", true};
        QString reconcileError;
        ok = check(history.reconcileServerRecord(server, &reconcileError) &&
                   reconcileError.isEmpty(),
                   "authenticated server state reconciles into the stable local record") && ok;
        const auto reconciled = history.completedJobs();
        ok = check(reconciled.size() == 1 &&
                   reconciled.front().status == nlsi::JobStatus::Cancelled &&
                   reconciled.front().snapshot.values.at("job.source_city").value == "Berlin" &&
                   reconciled.front().snapshot.values.at("job.income").value == "1200" &&
                   reconciled.front().snapshot.values.at("job.earned_xp").value == "75",
                   "server reconciliation updates status and fills missing fields without clearing populated local data") && ok;
    }

    {
        const QString activeReconcileDirectory = QDir(temporary.path()).filePath("active-reconciled-jobs");
        nlsi::JobHistory history(activeReconcileDirectory);
        ok = check(history.processSnapshot(activeSnapshot(), start),
                   "active reconciliation fixture persists locally") && ok;
        nlsi::JobRecord server;
        server.id = "NLSI-9002";
        server.localJobId = "NLSI-0001";
        server.status = nlsi::JobStatus::Delivered;
        server.startedAt = start;
        server.completedAt = finish;
        server.snapshot.values["job.income"] = {"Income", "", "game currency", false};
        QString reconcileError;
        ok = check(history.reconcileServerRecord(server, &reconcileError) &&
                   !history.activeJob() && history.completedJobs().size() == 1,
                   "a server-confirmed terminal status finalizes a matching local active record") && ok;
        ok = check(history.completedJobs().front().snapshot.values.at("job.income").value == "1200",
                   "active reconciliation preserves existing local income when the server value is unavailable") && ok;
    }

    if (!ok)
    {
        return 1;
    }
    std::cout << "All NLSI job history tests passed." << std::endl;
    return 0;
}
