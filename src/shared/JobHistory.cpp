#include "JobHistory.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QDir>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLockFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QRegularExpression>

namespace nlsi {
namespace {

QString snapshotPath(const QString& directory, const QString& fileName)
{
    return QDir(directory).filePath(fileName);
}

QJsonObject recordToJson(const JobRecord& record)
{
    QJsonObject values;
    for (const auto& entry : record.snapshot.values)
    {
        QJsonObject field;
        field.insert("label", QString::fromStdString(entry.second.label));
        field.insert("value", QString::fromStdString(entry.second.value));
        field.insert("unit", QString::fromStdString(entry.second.unit));
        field.insert("available", entry.second.available);
        values.insert(QString::fromStdString(entry.first), field);
    }

    QJsonObject root;
    root.insert("format", 1);
    root.insert("id", QString::fromStdString(record.id));
    root.insert("status", QString::fromStdString(jobStatusToString(record.status)));
    root.insert("started_at", record.startedAt.toUTC().toString(Qt::ISODateWithMs));
    root.insert("completed_at", record.completedAt.isValid()
        ? record.completedAt.toUTC().toString(Qt::ISODateWithMs)
        : QString());
    root.insert("game_id", QString::fromStdString(record.snapshot.gameId));
    root.insert("game", QString::fromStdString(record.snapshot.gameName));
    root.insert("api_version", QString::fromStdString(record.snapshot.gameApiVersion));
    root.insert("values", values);
    return root;
}

bool jsonToRecord(const QJsonObject& root, JobRecord& record)
{
    if (root.value("format").toInt(-1) != 1 ||
        !root.value("id").isString() ||
        !root.value("status").isString() ||
        !root.value("values").isObject())
    {
        return false;
    }
    record.id = root.value("id").toString().toStdString();
    record.status = jobStatusFromString(root.value("status").toString().toStdString());
    record.startedAt = QDateTime::fromString(root.value("started_at").toString(), Qt::ISODateWithMs);
    record.completedAt = QDateTime::fromString(root.value("completed_at").toString(), Qt::ISODateWithMs);
    record.snapshot.gameId = root.value("game_id").toString().toStdString();
    record.snapshot.gameName = root.value("game").toString().toStdString();
    record.snapshot.gameApiVersion = root.value("api_version").toString().toStdString();
    static const QRegularExpression idPattern("^NLSI-[0-9]{4,}$");
    if (!idPattern.match(QString::fromStdString(record.id)).hasMatch() ||
        record.status == JobStatus::Unknown || !record.startedAt.isValid() ||
        ((record.status == JobStatus::Delivered || record.status == JobStatus::Cancelled) &&
         !record.completedAt.isValid()))
    {
        return false;
    }

    const QJsonObject values = root.value("values").toObject();
    for (auto it = values.constBegin(); it != values.constEnd(); ++it)
    {
        if (!it.value().isObject())
        {
            return false;
        }
        const QJsonObject field = it.value().toObject();
        if (!field.value("label").isString() ||
            !field.value("value").isString() ||
            !field.value("unit").isString() ||
            !field.value("available").isBool())
        {
            return false;
        }
        record.snapshot.values[it.key().toStdString()] = {
            field.value("label").toString().toStdString(),
            field.value("value").toString().toStdString(),
            field.value("unit").toString().toStdString(),
            field.value("available").toBool()
        };
    }
    record.snapshot.status = ConnectionState::Connected;
    return true;
}

bool readRecord(const QString& path, JobRecord& record, QString& error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        error = QString("Cannot read %1: %2").arg(path, file.errorString());
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
        !jsonToRecord(document.object(), record))
    {
        error = QString("Malformed NLSI job record: %1").arg(path);
        return false;
    }
    return true;
}

bool sameSnapshot(const TelemetrySnapshot& left, const TelemetrySnapshot& right)
{
    return encodeTelemetryFrame(left) == encodeTelemetryFrame(right);
}

} // namespace

JobHistory::JobHistory(QString directory)
{
    if (directory.isEmpty())
    {
        directory = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
            .filePath("jobs");
    }
    directory_ = QDir::cleanPath(directory);
    if (!QDir().mkpath(directory_))
    {
        error_ = QString("Cannot create job-record directory: %1").arg(directory_);
        return;
    }
    if (!loadActive(error_))
    {
        qWarning().noquote() << error_;
    }
}

bool JobHistory::isValid() const
{
    return error_.isEmpty();
}

QString JobHistory::errorString() const
{
    return error_;
}

QString JobHistory::directory() const
{
    return directory_;
}

QString JobHistory::allocateJobId()
{
    QLockFile sequenceLock(snapshotPath(directory_, ".sequence.lock"));
    sequenceLock.setStaleLockTime(30000);
    if (!sequenceLock.tryLock(5000))
    {
        error_ = "Could not lock the NLSI job ID sequence; another application may be allocating an ID.";
        return {};
    }

    std::uint64_t nextSequence = 0;
    QString error;
    if (!loadSequence(nextSequence, error) ||
        nextSequence == 0 ||
        nextSequence == std::numeric_limits<std::uint64_t>::max())
    {
        error_ = error.isEmpty() ? "NLSI job ID sequence is exhausted." : error;
        return {};
    }
    if (!saveSequence(nextSequence + 1, error))
    {
        error_ = error;
        return {};
    }
    error_.clear();
    return QString::fromStdString(formatJobId(nextSequence));
}

bool JobHistory::processSnapshot(const TelemetrySnapshot& snapshot, const QDateTime& now)
{
    if (!now.isValid())
    {
        return false;
    }

    const auto statusField = snapshot.values.find("job.status");
    const JobStatus incoming = statusField != snapshot.values.end() && statusField->second.available
        ? jobStatusFromString(statusField->second.value)
        : JobStatus::Unknown;

    if (incoming == JobStatus::Active)
    {
        const auto hasJob = [](const TelemetrySnapshot& value)
        {
            const auto cargo = value.values.find("job.cargo");
            return cargo != value.values.end() && cargo->second.available &&
                !trimString(cargo->second.value).empty();
        };
        if (!hasJob(snapshot))
        {
            return false;
        }

        bool isNewJob = !active_.has_value();
        if (active_)
        {
            const auto fieldValue = [](const TelemetrySnapshot& value, const std::string& key)
            {
                const auto it = value.values.find(key);
                return it == value.values.end() || !it->second.available
                    ? std::string()
                    : trimString(it->second.value);
            };
            const auto incomingIdentityChanged = [&](const std::string& key)
            {
                const std::string incomingValue = fieldValue(snapshot, key);
                const std::string currentValue = fieldValue(active_->snapshot, key);
                return !incomingValue.empty() && !currentValue.empty() &&
                    incomingValue != currentValue;
            };
            const std::string currentGameId = trimString(active_->snapshot.gameId);
            const std::string incomingGameId = trimString(snapshot.gameId);
            const bool differs =
                (!currentGameId.empty() && !incomingGameId.empty() &&
                 currentGameId != incomingGameId) ||
                fieldValue(active_->snapshot, "job.cargo") != fieldValue(snapshot, "job.cargo") ||
                incomingIdentityChanged("job.source_city") ||
                incomingIdentityChanged("job.source_company") ||
                incomingIdentityChanged("job.destination_city") ||
                incomingIdentityChanged("job.destination_company");
            if (differs)
            {
                active_.reset();
                isNewJob = true;
            }
        }
        if (isNewJob)
        {
            const QString id = allocateJobId();
            if (id.isEmpty())
            {
                return false;
            }
            active_ = JobRecord{};
            active_->id = id.toStdString();
            active_->status = JobStatus::Active;
            active_->startedAt = now;
        }

        const TelemetrySnapshot previousSnapshot = active_->snapshot;
        const std::string stableId = active_->id;
        const QDateTime startedAt = active_->startedAt;
        for (const auto& field : snapshot.values)
        {
            if (field.second.available && !trimString(field.second.value).empty())
            {
                active_->snapshot.values[field.first] = field.second;
            }
            else if (active_->snapshot.values.find(field.first) == active_->snapshot.values.end())
            {
                active_->snapshot.values[field.first] = field.second;
            }
        }
        active_->snapshot.status = snapshot.status;
        if (!snapshot.gameId.empty()) active_->snapshot.gameId = snapshot.gameId;
        if (!snapshot.gameName.empty()) active_->snapshot.gameName = snapshot.gameName;
        if (!snapshot.gameApiVersion.empty()) active_->snapshot.gameApiVersion = snapshot.gameApiVersion;
        active_->id = stableId;
        active_->startedAt = startedAt;
        active_->status = JobStatus::Active;
        active_->completedAt = QDateTime();
        const auto idField = active_->snapshot.values.find("job.id");
        if (idField == active_->snapshot.values.end() || !idField->second.available ||
            idField->second.value != stableId)
        {
            active_->snapshot.values["job.id"] = {"Job ID", stableId, "", true};
        }
        if (!sameSnapshot(previousSnapshot, active_->snapshot) || isNewJob)
        {
            const bool saved = saveActive(error_);
            if (saved) error_.clear();
            return saved;
        }
        return true;
    }

    if (incoming == JobStatus::Delivered || incoming == JobStatus::Cancelled)
    {
        if (active_)
        {
            for (const auto& field : snapshot.values)
            {
                if (field.second.available && !trimString(field.second.value).empty())
                {
                    active_->snapshot.values[field.first] = field.second;
                }
                else if (active_->snapshot.values.find(field.first) == active_->snapshot.values.end())
                {
                    active_->snapshot.values[field.first] = field.second;
                }
            }
            active_->snapshot.status = snapshot.status;
            if (!snapshot.gameId.empty()) active_->snapshot.gameId = snapshot.gameId;
            if (!snapshot.gameName.empty()) active_->snapshot.gameName = snapshot.gameName;
            if (!snapshot.gameApiVersion.empty()) active_->snapshot.gameApiVersion = snapshot.gameApiVersion;
            active_->snapshot.values["job.id"] = {"Job ID", active_->id, "", true};
        }
        return finalizeActive(incoming, now);
    }

    return false;
}

std::vector<JobRecord> JobHistory::completedJobs(QString* error) const
{
    std::vector<JobRecord> records;
    QDir directory(directory_);
    const QStringList files = directory.entryList({"NLSI-*.nlsi"}, QDir::Files, QDir::Name);
    for (const QString& name : files)
    {
        JobRecord record;
        QString readError;
        if (!readRecord(directory.filePath(name), record, readError))
        {
            if (error)
            {
                if (!error->isEmpty()) *error += "\n";
                *error += readError;
            }
            continue;
        }
        if ((record.status == JobStatus::Delivered || record.status == JobStatus::Cancelled) &&
            name == QString::fromStdString(record.id) + ".nlsi")
        {
            records.push_back(std::move(record));
        }
    }
    std::sort(records.begin(), records.end(), [](const JobRecord& left, const JobRecord& right)
    {
        return left.completedAt > right.completedAt;
    });
    return records;
}

bool JobHistory::reconcileServerRecord(const JobRecord& serverRecord, QString* error)
{
    if (error)
    {
        error->clear();
    }
    const QString localId = QString::fromStdString(serverRecord.localJobId);
    static const QRegularExpression idPattern("^NLSI-[0-9]{4,}$");
    if (!idPattern.match(localId).hasMatch())
    {
        if (error)
        {
            *error = "The server job does not contain a valid stable local job ID.";
        }
        return false;
    }

    const auto mergeMissingFields = [](JobRecord& local, const JobRecord& remote)
    {
        for (const auto& field : remote.snapshot.values)
        {
            const auto existing = local.snapshot.values.find(field.first);
            if (existing == local.snapshot.values.end() ||
                !existing->second.available ||
                trimString(existing->second.value).empty())
            {
                if (field.second.available && !trimString(field.second.value).empty())
                {
                    local.snapshot.values[field.first] = field.second;
                }
            }
        }
        if (local.snapshot.gameId.empty()) local.snapshot.gameId = remote.snapshot.gameId;
        if (local.snapshot.gameName.empty()) local.snapshot.gameName = remote.snapshot.gameName;
        if (local.snapshot.gameApiVersion.empty())
        {
            local.snapshot.gameApiVersion = remote.snapshot.gameApiVersion;
        }
    };

    if (active_ && active_->id == serverRecord.localJobId)
    {
        mergeMissingFields(*active_, serverRecord);
        if (serverRecord.status == JobStatus::Delivered ||
            serverRecord.status == JobStatus::Cancelled)
        {
            if (!serverRecord.completedAt.isValid())
            {
                if (error)
                {
                    *error = "The server terminal job is missing its completion timestamp.";
                }
                return false;
            }
            if (!finalizeActive(serverRecord.status, serverRecord.completedAt))
            {
                if (error)
                {
                    *error = error_;
                }
                return false;
            }
            return true;
        }
        if (!saveActive(error_))
        {
            if (error)
            {
                *error = error_;
            }
            return false;
        }
        error_.clear();
        return true;
    }

    const QString fileName = localId + ".nlsi";
    const QString path = snapshotPath(directory_, fileName);
    if (!QFile::exists(path))
    {
        return true;
    }
    JobRecord local;
    QString readError;
    if (!readRecord(path, local, readError))
    {
        if (error)
        {
            *error = readError;
        }
        return false;
    }
    mergeMissingFields(local, serverRecord);
    if (serverRecord.status == JobStatus::Delivered ||
        serverRecord.status == JobStatus::Cancelled)
    {
        if (!serverRecord.completedAt.isValid())
        {
            if (error)
            {
                *error = "The server terminal job is missing its completion timestamp.";
            }
            return false;
        }
        local.status = serverRecord.status;
        local.completedAt = serverRecord.completedAt;
        local.snapshot.values["job.status"] = {
            "Status", jobStatusToString(local.status), "", true
        };
    }
    if (!saveRecord(local, fileName, readError))
    {
        if (error)
        {
            *error = readError;
        }
        return false;
    }
    return true;
}

std::optional<JobSyncState> JobHistory::syncState(const std::string& localJobId,
                                                  QString* error) const
{
    if (error)
    {
        error->clear();
    }
    QJsonObject root;
    QString readError;
    if (!readSyncStates(root, readError))
    {
        if (error)
        {
            *error = readError;
        }
        return std::nullopt;
    }
    const QJsonValue value = root.value("jobs").toObject().value(
        QString::fromStdString(localJobId));
    if (!value.isObject())
    {
        return std::nullopt;
    }
    const QJsonObject entry = value.toObject();
    bool accountIdValid = false;
    qint64 accountId = 0;
    if (entry.value("account_id").isString())
    {
        accountId = entry.value("account_id").toString().toLongLong(&accountIdValid);
    }
    else if (entry.value("account_id").isDouble())
    {
        const double numericId = entry.value("account_id").toDouble();
        if (numericId > 0 && numericId <= static_cast<double>(
                std::numeric_limits<qint64>::max()) &&
            std::floor(numericId) == numericId)
        {
            accountId = static_cast<qint64>(numericId);
            accountIdValid = true;
        }
    }
    if (!accountIdValid || !entry.value("status").isString() ||
        !entry.value("server_job_id").isString() ||
        !entry.value("error").isString())
    {
        if (error)
        {
            *error = "The local job synchronization state is malformed.";
        }
        return std::nullopt;
    }
    JobSyncState state;
    state.accountId = accountId;
    state.status = entry.value("status").toString();
    state.serverJobId = entry.value("server_job_id").toString();
    state.error = entry.value("error").toString();
    if (state.accountId <= 0 ||
        (state.status != "pending" && state.status != "synced" && state.status != "failed"))
    {
        if (error)
        {
            *error = "The local job synchronization state contains unsupported values.";
        }
        return std::nullopt;
    }
    return state;
}

bool JobHistory::claimForAccount(const std::string& localJobId, qint64 accountId, QString* error)
{
    if (error)
    {
        error->clear();
    }
    if (accountId <= 0 || localJobId.empty())
    {
        if (error)
        {
            *error = "A valid account and local job ID are required to synchronize a job.";
        }
        return false;
    }

    QLockFile lock(snapshotPath(directory_, ".sync-state.lock"));
    lock.setStaleLockTime(30000);
    if (!lock.tryLock(5000))
    {
        if (error)
        {
            *error = "Could not lock local job synchronization state.";
        }
        return false;
    }
    QJsonObject root;
    QString stateError;
    if (!readSyncStates(root, stateError))
    {
        if (error)
        {
            *error = stateError;
        }
        return false;
    }
    QJsonObject jobs = root.value("jobs").toObject();
    const QString key = QString::fromStdString(localJobId);
    if (jobs.contains(key))
    {
        const QJsonValue value = jobs.value(key);
        if (!value.isObject())
        {
            if (error)
            {
                *error = "The local job synchronization state is malformed.";
            }
            return false;
        }
        const QJsonValue storedId = value.toObject().value("account_id");
        const qint64 storedAccountId = storedId.isString()
            ? storedId.toString().toLongLong()
            : static_cast<qint64>(storedId.toDouble(-1));
        if (storedAccountId != accountId)
        {
            if (error)
            {
                *error = "This local job belongs to another account and was not uploaded.";
            }
            return false;
        }
        return true;
    }

    jobs.insert(key, QJsonObject{
        {"account_id", QString::number(accountId)},
        {"status", "pending"},
        {"server_job_id", QString()},
        {"error", QString()}
    });
    root.insert("format", 1);
    root.insert("jobs", jobs);
    if (!writeSyncStates(root, stateError))
    {
        if (error)
        {
            *error = stateError;
        }
        return false;
    }
    return true;
}

bool JobHistory::setSyncState(const std::string& localJobId, const JobSyncState& state,
                              QString* error)
{
    if (error)
    {
        error->clear();
    }
    if (state.accountId <= 0 ||
        (state.status != "pending" && state.status != "synced" && state.status != "failed"))
    {
        if (error)
        {
            *error = "The requested local job synchronization state is invalid.";
        }
        return false;
    }

    QLockFile lock(snapshotPath(directory_, ".sync-state.lock"));
    lock.setStaleLockTime(30000);
    if (!lock.tryLock(5000))
    {
        if (error)
        {
            *error = "Could not lock local job synchronization state.";
        }
        return false;
    }
    QJsonObject root;
    QString stateError;
    if (!readSyncStates(root, stateError))
    {
        if (error)
        {
            *error = stateError;
        }
        return false;
    }
    QJsonObject jobs = root.value("jobs").toObject();
    const QString key = QString::fromStdString(localJobId);
    const QJsonValue existing = jobs.value(key);
    if (!existing.isObject() ||
        (existing.toObject().value("account_id").isString()
            ? existing.toObject().value("account_id").toString().toLongLong()
            : static_cast<qint64>(existing.toObject().value("account_id").toDouble(-1))) !=
            state.accountId)
    {
        if (error)
        {
            *error = "The local job is not assigned to this authenticated account.";
        }
        return false;
    }
    jobs.insert(key, QJsonObject{
        {"account_id", QString::number(state.accountId)},
        {"status", state.status},
        {"server_job_id", state.serverJobId},
        {"error", state.error}
    });
    root.insert("format", 1);
    root.insert("jobs", jobs);
    if (!writeSyncStates(root, stateError))
    {
        if (error)
        {
            *error = stateError;
        }
        return false;
    }
    return true;
}

bool JobHistory::readSyncStates(QJsonObject& states, QString& error) const
{
    const QString path = snapshotPath(directory_, ".sync-state.nlsi");
    QFile file(path);
    if (!file.exists())
    {
        states = QJsonObject{{"format", 1}, {"jobs", QJsonObject()}};
        return true;
    }
    if (!file.open(QIODevice::ReadOnly))
    {
        error = QString("Cannot read local job synchronization state: %1").arg(file.errorString());
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
        document.object().value("format").toInt(-1) != 1 ||
        !document.object().value("jobs").isObject())
    {
        error = "The local job synchronization state file is invalid; local jobs were preserved.";
        return false;
    }
    states = document.object();
    return true;
}

bool JobHistory::writeSyncStates(const QJsonObject& states, QString& error) const
{
    QSaveFile file(snapshotPath(directory_, ".sync-state.nlsi"));
    if (!file.open(QIODevice::WriteOnly))
    {
        error = QString("Cannot write local job synchronization state: %1").arg(file.errorString());
        return false;
    }
    const QByteArray bytes = QJsonDocument(states).toJson(QJsonDocument::Compact);
    if (file.write(bytes) != bytes.size() || !file.commit())
    {
        error = QString("Cannot commit local job synchronization state: %1").arg(file.errorString());
        return false;
    }
    return true;
}

const std::optional<JobRecord>& JobHistory::activeJob() const
{
    return active_;
}

bool JobHistory::loadSequence(std::uint64_t& nextSequence, QString& error) const
{
    const QString sequencePath = snapshotPath(directory_, ".sequence");
    QFile file(sequencePath);
    nextSequence = 1;
    if (file.exists())
    {
        if (!file.open(QIODevice::ReadOnly))
        {
            error = QString("Cannot read NLSI job sequence: %1").arg(file.errorString());
            return false;
        }
        bool ok = false;
        const qulonglong stored = file.readAll().trimmed().toULongLong(&ok);
        if (!ok || stored == 0)
        {
            error = "The NLSI job sequence file is malformed.";
            return false;
        }
        nextSequence = static_cast<std::uint64_t>(stored);
    }

    QDir directory(directory_);
    const QStringList files = directory.entryList({"NLSI-*.nlsi"}, QDir::Files);
    for (const QString& name : files)
    {
        const QString digits = name.mid(5, name.size() - 10);
        bool ok = false;
        const qulonglong sequence = digits.toULongLong(&ok);
        if (ok && sequence >= nextSequence &&
            sequence < static_cast<qulonglong>(std::numeric_limits<std::uint64_t>::max()))
        {
            nextSequence = static_cast<std::uint64_t>(sequence + 1);
        }
    }
    const QString activePath = snapshotPath(directory_, ".active.nlsi");
    if (QFile::exists(activePath))
    {
        JobRecord activeRecord;
        QString activeError;
        if (readRecord(activePath, activeRecord, activeError))
        {
            const std::string prefix = "NLSI-";
            if (activeRecord.id.compare(0, prefix.size(), prefix) == 0)
            {
                bool ok = false;
                const qulonglong sequence =
                    QString::fromStdString(activeRecord.id.substr(prefix.size())).toULongLong(&ok);
                if (ok && sequence >= nextSequence &&
                    sequence < static_cast<qulonglong>(std::numeric_limits<std::uint64_t>::max()))
                {
                    nextSequence = static_cast<std::uint64_t>(sequence + 1);
                }
            }
        }
    }
    return true;
}

bool JobHistory::saveSequence(std::uint64_t nextSequence, QString& error) const
{
    QSaveFile file(snapshotPath(directory_, ".sequence"));
    if (!file.open(QIODevice::WriteOnly))
    {
        error = QString("Cannot write NLSI job sequence: %1").arg(file.errorString());
        return false;
    }
    const QByteArray bytes = QByteArray::number(static_cast<qulonglong>(nextSequence));
    if (file.write(bytes) != bytes.size() || !file.commit())
    {
        error = QString("Cannot commit NLSI job sequence: %1").arg(file.errorString());
        return false;
    }
    return true;
}

bool JobHistory::saveRecord(const JobRecord& record, const QString& fileName, QString& error) const
{
    QSaveFile file(snapshotPath(directory_, fileName));
    if (!file.open(QIODevice::WriteOnly))
    {
        error = QString("Cannot write NLSI job record %1: %2").arg(fileName, file.errorString());
        return false;
    }
    const QByteArray bytes = QJsonDocument(recordToJson(record)).toJson(QJsonDocument::Compact);
    if (file.write(bytes) != bytes.size() || !file.commit())
    {
        error = QString("Cannot commit NLSI job record %1: %2").arg(fileName, file.errorString());
        return false;
    }
    return true;
}

bool JobHistory::saveActive(QString& error) const
{
    return active_ ? saveRecord(*active_, ".active.nlsi", error) : true;
}

bool JobHistory::loadActive(QString& error)
{
    const QString activePath = snapshotPath(directory_, ".active.nlsi");
    if (!QFile::exists(activePath))
    {
        return true;
    }
    JobRecord record;
    if (!readRecord(activePath, record, error))
    {
        return false;
    }
    if (record.status != JobStatus::Active)
    {
        error = "The saved active NLSI job has a terminal status; it was not loaded as active.";
        return false;
    }
    active_ = std::move(record);
    return true;
}

bool JobHistory::finalizeActive(JobStatus status, const QDateTime& now)
{
    if (!active_ || (status != JobStatus::Delivered && status != JobStatus::Cancelled))
    {
        return false;
    }

    active_->status = status;
    active_->completedAt = now;
    const qint64 realSeconds = active_->startedAt.secsTo(now);
    if (realSeconds >= 0)
    {
        active_->snapshot.values["job.real_duration_seconds"] = {
            "Real Duration",
            std::to_string(realSeconds),
            "s",
            true
        };
    }
    active_->snapshot.values["job.status"] = {
        "Status",
        jobStatusToString(status),
        "",
        true
    };
    const QString recordName = QString::fromStdString(active_->id) + ".nlsi";
    const QString activePath = snapshotPath(directory_, ".active.nlsi");
    if (QFile::exists(snapshotPath(directory_, recordName)))
    {
        JobRecord existing;
        QString readError;
        if (readRecord(snapshotPath(directory_, recordName), existing, readError))
        {
            if (existing.status == status)
            {
                active_.reset();
                if (QFile::exists(activePath) && !QFile::remove(activePath))
                {
                    error_ = QString("Could not remove finalized active-job state: %1").arg(activePath);
                    return true;
                }
                return true;
            }
            error_ = QString("Refusing conflicting final status for job %1.").arg(recordName);
            return false;
        }
        error_ = readError;
        return false;
    }
    if (!saveRecord(*active_, recordName, error_))
    {
        return false;
    }
    active_.reset();
    if (QFile::exists(activePath) && !QFile::remove(activePath))
    {
        error_ = QString("Saved completed job, but could not remove active state: %1").arg(activePath);
        return true;
    }
    error_.clear();
    return true;
}

} // namespace nlsi
