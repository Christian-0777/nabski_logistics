#pragma once

#include <map>
#include <optional>

#include <QDateTime>
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
    const std::optional<JobRecord>& activeJob() const;

private:
    bool loadSequence(std::uint64_t& nextSequence, QString& error) const;
    bool saveSequence(std::uint64_t nextSequence, QString& error) const;
    bool saveRecord(const JobRecord& record, const QString& fileName, QString& error) const;
    bool saveActive(QString& error) const;
    bool loadActive(QString& error);
    bool finalizeActive(JobStatus status, const QDateTime& now);

    QString directory_;
    QString error_;
    std::optional<JobRecord> active_;
};

} // namespace nlsi
