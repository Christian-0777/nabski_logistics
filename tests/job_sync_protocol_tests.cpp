#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include "JobSyncProtocol.h"

namespace {

bool check(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
    }
    return condition;
}

std::vector<nlsi::JobRecord> expectedJobs()
{
    nlsi::JobRecord first;
    first.id = "local-job-1";
    first.serverJobId = "NLSI-101";
    nlsi::JobRecord second;
    second.id = "local-job-2";
    return {first, second};
}

QJsonObject validResponse()
{
    return {
        {"success", true},
        {"committed", true},
        {"driver_id", 42},
        {"acknowledgements", QJsonArray{
            QJsonObject{{"local_job_id", "local-job-1"},
                        {"job_id", "NLSI-101"},
                        {"acknowledged", true}},
            QJsonObject{{"local_job_id", "local-job-2"},
                        {"job_id", "NLSI-102"},
                        {"acknowledged", true}}
        }}
    };
}

QByteArray jsonBytes(const QJsonObject& response)
{
    return QJsonDocument(response).toJson(QJsonDocument::Compact);
}

bool responseIsRejected(const QJsonObject& response)
{
    return !nlsi::validateJobSyncResponse(
        200, true, true, jsonBytes(response), 42, expectedJobs());
}

} // namespace

int main()
{
    bool ok = true;
    const std::vector<QString> supportedRoles = {"DRIVER", "MANAGER", "CEO", "ADMIN"};
    for (const QString& role : supportedRoles)
    {
        ok = check(nlsi::canSynchronizeAccountJobs(role, 42, 42),
                   "all supported roles can sync jobs for their authenticated driver identity") && ok;
        ok = check(nlsi::shouldUploadLocalJob(42, std::nullopt),
                   "an eligible job without saved per-account sync state is uploadable") && ok;
    }
    ok = check(nlsi::canSynchronizeAccountJobs("DRIVER", 42, 42) &&
               !nlsi::canSynchronizeAccountJobs("MANAGER", 42, 43) &&
               !nlsi::canSynchronizeAccountJobs("UNSUPPORTED", 42, 42) &&
               !nlsi::canSynchronizeAccountJobs("CEO", 0, 0),
               "sync policy rejects invalid identities, mismatched ownership, and unsupported roles") && ok;

    nlsi::JobSyncState pending;
    pending.accountId = 42;
    pending.status = "pending";
    nlsi::JobSyncState failed;
    failed.accountId = 42;
    failed.status = "failed";
    nlsi::JobSyncState synced;
    synced.accountId = 42;
    synced.status = "synced";
    nlsi::JobSyncState otherAccount = pending;
    otherAccount.accountId = 43;
    ok = check(nlsi::shouldUploadLocalJob(42, pending) &&
               nlsi::shouldUploadLocalJob(42, failed) &&
               !nlsi::shouldUploadLocalJob(42, synced) &&
               !nlsi::shouldUploadLocalJob(42, otherAccount),
               "pending and legacy failed states retry while synced or other-account jobs do not") && ok;

    const auto jobs = expectedJobs();
    const QByteArray successBytes = jsonBytes(validResponse());
    const auto valid = nlsi::validateJobSyncResponse(
        200, true, true, successBytes, 42, jobs);
    ok = check(valid && valid->size() == 2 &&
               valid->at("local-job-1") == "NLSI-101" &&
               valid->at("local-job-2") == "NLSI-102",
               "committed response returns exact local-to-server job acknowledgements") && ok;

    ok = check(!nlsi::validateJobSyncResponse(500, false, true, successBytes, 42, jobs) &&
               !nlsi::validateJobSyncResponse(0, false, true, QByteArray(), 42, jobs) &&
               !nlsi::validateJobSyncResponse(200, true, false, successBytes, 42, jobs) &&
               !nlsi::validateJobSyncResponse(
                   200, true, true, QByteArray("{invalid"), 42, jobs),
               "server errors, request failures, non-JSON responses, and invalid JSON are rejected") && ok;

    QJsonObject response = validResponse();
    response.insert("committed", false);
    ok = check(responseIsRejected(response), "an uncommitted response is rejected") && ok;
    response = validResponse();
    response.insert("driver_id", 43);
    ok = check(responseIsRejected(response), "an acknowledgement for another driver is rejected") && ok;
    response = validResponse();
    response.remove("committed");
    ok = check(responseIsRejected(response), "an incomplete response is rejected") && ok;
    response = validResponse();
    QJsonArray acknowledgements = response.value("acknowledgements").toArray();
    acknowledgements.removeLast();
    response.insert("acknowledgements", acknowledgements);
    ok = check(responseIsRejected(response), "a missing acknowledgement is rejected") && ok;
    response = validResponse();
    acknowledgements = response.value("acknowledgements").toArray();
    QJsonObject acknowledgement = acknowledgements.at(0).toObject();
    acknowledgement.insert("local_job_id", "unrelated-job");
    acknowledgements.replace(0, acknowledgement);
    response.insert("acknowledgements", acknowledgements);
    ok = check(responseIsRejected(response), "an acknowledgement with a mismatched local job ID is rejected") && ok;
    response = validResponse();
    acknowledgements = response.value("acknowledgements").toArray();
    acknowledgements.replace(1, acknowledgements.at(0));
    response.insert("acknowledgements", acknowledgements);
    ok = check(responseIsRejected(response), "duplicate local-job acknowledgements are rejected") && ok;
    response = validResponse();
    acknowledgements = response.value("acknowledgements").toArray();
    acknowledgement = acknowledgements.at(0).toObject();
    acknowledgement.insert("job_id", "NLSI-999");
    acknowledgements.replace(0, acknowledgement);
    response.insert("acknowledgements", acknowledgements);
    ok = check(responseIsRejected(response), "a mismatched known server job ID is rejected") && ok;
    response = validResponse();
    acknowledgements = response.value("acknowledgements").toArray();
    acknowledgement = acknowledgements.at(0).toObject();
    acknowledgement.insert("acknowledged", false);
    acknowledgements.replace(0, acknowledgement);
    response.insert("acknowledgements", acknowledgements);
    ok = check(responseIsRejected(response), "an unconfirmed acknowledgement is rejected") && ok;
    response = validResponse();
    acknowledgements = response.value("acknowledgements").toArray();
    acknowledgement = acknowledgements.at(0).toObject();
    acknowledgement.insert("job_id", " ");
    acknowledgements.replace(0, acknowledgement);
    response.insert("acknowledgements", acknowledgements);
    ok = check(responseIsRejected(response), "an acknowledgement without a server job ID is rejected") && ok;
    response = validResponse();
    acknowledgements = response.value("acknowledgements").toArray();
    acknowledgement = acknowledgements.at(1).toObject();
    acknowledgement.insert("job_id", "NLSI-101");
    acknowledgements.replace(1, acknowledgement);
    response.insert("acknowledgements", acknowledgements);
    ok = check(responseIsRejected(response), "duplicate server job IDs are rejected") && ok;

    const nlsi::JobSyncState retryState = nlsi::makeJobSyncStateAfterUpload(
        42, "NLSI-101", QString(), "upload_failed");
    ok = check(retryState.status == "pending" &&
               retryState.serverJobId == "NLSI-101" &&
               retryState.error == "upload_failed" &&
               nlsi::shouldUploadLocalJob(42, retryState),
               "transient failure leaves a pending retry with any known server ID preserved") && ok;
    const auto retriedAcknowledgements = nlsi::validateJobSyncResponse(
        200, true, true, successBytes, 42, jobs);
    const nlsi::JobSyncState retriedState = nlsi::makeJobSyncStateAfterUpload(
        42, retryState.serverJobId,
        retriedAcknowledgements ? retriedAcknowledgements->at("local-job-1") : QString(),
        QString());
    ok = check(retriedState.status == "synced" &&
               retriedState.serverJobId == "NLSI-101" &&
               !nlsi::shouldUploadLocalJob(42, retriedState),
               "a valid retry acknowledgement marks the job synced and prevents re-upload") && ok;

    return ok ? 0 : 1;
}
