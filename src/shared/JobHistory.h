#pragma once

#include <map>
#include <optional>

#include <QDateTime>
#include <QJsonObject>
#include <QString>

#include "TelemetryIPC.h"

namespace nlsi {

struct JobRecord
{
    std::string id;
    JobStatus status = JobStatus::Unknown;
    QDateTime startedAt;
    QDateTime completedAt;
    TelemetrySnapshot snapshot;
    std::string localJobId;
    std::string serverJobId;
    std::string clientInstanceId;
    QString syncStatus;
    qint64 accountId = 0;
};

struct JobSyncState
{
    qint64 accountId = 0;
    QString status = "pending";
    QString serverJobId;
    QString error;
};

class JobHistory
{
public:
    explicit JobHistory(QString directory = QString());

    bool isValid() const;
    QString errorString() const;
    QString directory() const;
    QString allocateJobId();
    bool processSnapshot(const TelemetrySnapshot& snapshot, const QDateTime& now);
    std::vector<JobRecord> completedJobs(QString* error = nullptr) const;
    std::optional<JobSyncState> syncState(const std::string& localJobId,
                                          QString* error = nullptr) const;
    bool claimForAccount(const std::string& localJobId, qint64 accountId, QString* error = nullptr);
    bool reconcileServerRecord(const JobRecord& serverRecord, QString* error = nullptr);
    bool setSyncState(const std::string& localJobId, const JobSyncState& state,
                      QString* error = nullptr);
    const std::optional<JobRecord>& activeJob() const;

private:
    bool loadSequence(std::uint64_t& nextSequence, QString& error) const;
    bool saveSequence(std::uint64_t nextSequence, QString& error) const;
    bool saveRecord(const JobRecord& record, const QString& fileName, QString& error) const;
    bool readSyncStates(QJsonObject& states, QString& error) const;
    bool writeSyncStates(const QJsonObject& states, QString& error) const;
    bool saveActive(QString& error) const;
    bool loadActive(QString& error);
    bool finalizeActive(JobStatus status, const QDateTime& now);

    QString directory_;
    QString error_;
    std::optional<JobRecord> active_;
};

} // namespace nlsi
