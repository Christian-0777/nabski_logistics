#include "JobSyncProtocol.h"

#include <set>
#include <utility>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

#include "UserPreferences.h"

namespace nlsi {

bool canSynchronizeAccountJobs(const QString& role, qint64 authenticatedAccountId,
                               qint64 requestedAccountId)
{
    return authenticatedAccountId > 0 && requestedAccountId == authenticatedAccountId &&
        isSupportedAccountRole(role);
}

bool shouldUploadLocalJob(qint64 accountId, const std::optional<JobSyncState>& state)
{
    return accountId > 0 &&
        (!state || (state->accountId == accountId && state->status != "synced"));
}

std::optional<std::map<std::string, QString>> validateJobSyncResponse(
    int statusCode, bool requestSucceeded, bool jsonContentType,
    const QByteArray& responseBytes, qint64 accountId,
    const std::vector<JobRecord>& expectedJobs)
{
    if (accountId <= 0 || expectedJobs.empty() || statusCode != 200 ||
        !requestSucceeded || !jsonContentType)
    {
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(responseBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        return std::nullopt;
    }

    const QJsonObject response = document.object();
    const QJsonValue driverId = response.value("driver_id");
    if (!response.value("success").isBool() || !response.value("success").toBool() ||
        !response.value("committed").isBool() || !response.value("committed").toBool() ||
        !driverId.isDouble() ||
        driverId.toDouble(-1.0) != static_cast<double>(accountId))
    {
        return std::nullopt;
    }

    const QJsonValue acknowledgementsValue = response.value("acknowledgements");
    if (!acknowledgementsValue.isArray())
    {
        return std::nullopt;
    }
    const QJsonArray acknowledgements = acknowledgementsValue.toArray();
    if (acknowledgements.size() != static_cast<int>(expectedJobs.size()))
    {
        return std::nullopt;
    }

    std::map<std::string, QString> expectedServerIds;
    for (const JobRecord& record : expectedJobs)
    {
        if (record.id.empty() ||
            !expectedServerIds.emplace(record.id, QString::fromStdString(record.serverJobId)).second)
        {
            return std::nullopt;
        }
    }

    std::map<std::string, QString> serverIds;
    std::set<QString> uniqueServerIds;
    for (const QJsonValue& value : acknowledgements)
    {
        if (!value.isObject())
        {
            return std::nullopt;
        }
        const QJsonObject acknowledgement = value.toObject();
        const QJsonValue localJobId = acknowledgement.value("local_job_id");
        const QJsonValue serverJobId = acknowledgement.value("job_id");
        const QJsonValue acknowledged = acknowledgement.value("acknowledged");
        if (!localJobId.isString() || !serverJobId.isString() ||
            !acknowledged.isBool() || !acknowledged.toBool())
        {
            return std::nullopt;
        }
        const std::string localId = localJobId.toString().toStdString();
        const QString serverId = serverJobId.toString().trimmed();
        const auto expected = expectedServerIds.find(localId);
        if (expected == expectedServerIds.end() || serverId.isEmpty() ||
            (!expected->second.isEmpty() && expected->second != serverId) ||
            !serverIds.emplace(localId, serverId).second ||
            !uniqueServerIds.insert(serverId).second)
        {
            return std::nullopt;
        }
    }

    return serverIds.size() == expectedJobs.size()
        ? std::optional<std::map<std::string, QString>>(std::move(serverIds))
        : std::nullopt;
}

JobSyncState makeJobSyncStateAfterUpload(qint64 accountId,
                                         const QString& priorServerJobId,
                                         const QString& acknowledgedServerJobId,
                                         const QString& error)
{
    JobSyncState state;
    state.accountId = accountId;
    if (!acknowledgedServerJobId.isEmpty())
    {
        state.status = "synced";
        state.serverJobId = acknowledgedServerJobId;
    }
    else
    {
        state.status = "pending";
        state.serverJobId = priorServerJobId;
        state.error = error;
    }
    return state;
}

} // namespace nlsi
