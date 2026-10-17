#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <QByteArray>
#include <QString>

#include "JobHistory.h"

namespace nlsi {

bool canSynchronizeAccountJobs(const QString& role, qint64 authenticatedAccountId,
                              qint64 requestedAccountId);
bool shouldUploadLocalJob(qint64 accountId, const std::optional<JobSyncState>& state);
std::optional<std::map<std::string, QString>> validateJobSyncResponse(
    int statusCode, bool requestSucceeded, bool jsonContentType,
    const QByteArray& responseBytes, qint64 accountId,
    const std::vector<JobRecord>& expectedJobs);
JobSyncState makeJobSyncStateAfterUpload(qint64 accountId,
                                         const QString& priorServerJobId,
                                         const QString& acknowledgedServerJobId,
                                         const QString& error);

} // namespace nlsi
