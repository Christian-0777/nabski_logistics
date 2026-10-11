#include "HistoryStore.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMap>
#include <QRandomGenerator>
#include <QSaveFile>

#include <algorithm>
#include <initializer_list>
#include <utility>

#include "logging/Logger.h"
#include "telemetry/TelemetryUiState.h"
#include "time/ApplicationTime.h"

namespace nlsi::session {
namespace {

QString ToQString(const std::wstring& value) {
    return QString::fromStdWString(value);
}

QString FieldText(const telemetry::TelemetryField<std::wstring>& field) {
    if (!field.available || field.stale) {
        return {};
    }
    const QString text = ToQString(field.value).trimmed();
    return text.compare(QStringLiteral("null"), Qt::CaseInsensitive) == 0
            || text.compare(QStringLiteral("undefined"), Qt::CaseInsensitive) == 0
            || text.compare(QStringLiteral("n/a"), Qt::CaseInsensitive) == 0
        ? QString() : text;
}

QJsonValue JsonValueAtPath(const QJsonObject& object, const QString& path) {
    const QJsonValue direct = object.value(path);
    if (!direct.isUndefined()) {
        return direct;
    }
    QJsonValue value = object;
    for (const QString& component : path.split(QLatin1Char('.'))) {
        if (!value.isObject()) {
            return {};
        }
        value = value.toObject().value(component);
    }
    return value;
}

bool IsUsableText(const QString& text) {
    const QString normalized = text.trimmed();
    return !normalized.isEmpty()
        && normalized.compare(QStringLiteral("null"), Qt::CaseInsensitive) != 0
        && normalized.compare(QStringLiteral("undefined"), Qt::CaseInsensitive) != 0
        && normalized.compare(QStringLiteral("n/a"), Qt::CaseInsensitive) != 0;
}

QString EventText(const QJsonObject& object, std::initializer_list<QString> keys) {
    for (const QString& key : keys) {
        const QJsonValue value = JsonValueAtPath(object, key);
        if (value.isString()) {
            const QString text = value.toString().trimmed();
            if (IsUsableText(text)) {
                return text;
            }
        } else if (value.isDouble() && std::isfinite(value.toDouble())) {
            return value.toVariant().toString();
        }
    }
    return {};
}

QString RouteText(const QString& company, const QString& city) {
    if (company.isEmpty()) {
        return city;
    }
    if (city.isEmpty() || company == city) {
        return company;
    }
    return company + QStringLiteral(" · ") + city;
}

bool HasValue(const QJsonValue& value) {
    if (value.isNull() || value.isUndefined()) {
        return false;
    }
    if (value.isString()) {
        return IsUsableText(value.toString());
    }
    if (value.isDouble()) {
        return std::isfinite(value.toDouble());
    }
    if (value.isObject()) {
        return !value.toObject().isEmpty();
    }
    if (value.isArray()) {
        return !value.toArray().isEmpty();
    }
    return true;
}

QJsonObject MetadataObject(const QJsonValue& value) {
    if (value.isObject()) {
        return value.toObject();
    }
    if (!value.isString()) {
        return {};
    }
    const QByteArray encoded = value.toString().trimmed().toUtf8();
    if (!encoded.startsWith('{')) {
        return {};
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(encoded, &error);
    return error.error == QJsonParseError::NoError && document.isObject()
        ? document.object() : QJsonObject{};
}

QVector<QJsonObject> JobDetailSources(const QJsonObject& details) {
    QVector<QJsonObject> sources{details};
    for (const QString& key : {
             QStringLiteral("job_snapshot"),
             QStringLiteral("job_configuration"),
             QStringLiteral("identity"),
             QStringLiteral("event_key")}) {
        const QJsonObject metadata = MetadataObject(details.value(key));
        if (!metadata.isEmpty()) {
            sources.push_back(metadata);
        }
    }
    return sources;
}

void SetCanonicalText(
    QJsonObject& normalized,
    const QVector<QJsonObject>& sources,
    const QString& target,
    std::initializer_list<QString> aliases) {
    if (HasValue(normalized.value(target))) {
        if (normalized.value(target).isString()) {
            normalized.insert(target, normalized.value(target).toString().trimmed());
        }
        return;
    }
    for (const QJsonObject& source : sources) {
        const QString text = EventText(source, aliases);
        if (!text.isEmpty()) {
            normalized.insert(target, text);
            return;
        }
    }
}

void SetCanonicalValue(
    QJsonObject& normalized,
    const QVector<QJsonObject>& sources,
    const QString& target,
    std::initializer_list<QString> aliases) {
    const auto usable_number = [](const QJsonValue& value) {
        if (value.isDouble()) {
            return std::isfinite(value.toDouble()) && value.toDouble() >= 0.0;
        }
        if (value.isString()) {
            bool converted = false;
            const double number = value.toString().trimmed().toDouble(&converted);
            return converted && std::isfinite(number) && number >= 0.0;
        }
        return false;
    };
    if (usable_number(normalized.value(target))) {
        return;
    }
    for (const QJsonObject& source : sources) {
        for (const QString& alias : aliases) {
            const QJsonValue value = JsonValueAtPath(source, alias);
            if (usable_number(value)) {
                normalized.insert(target, value.isDouble()
                    ? value : QJsonValue(value.toString().trimmed().toDouble()));
                return;
            }
        }
    }
}

void SetCanonicalBoolean(
    QJsonObject& normalized,
    const QVector<QJsonObject>& sources,
    const QString& target,
    std::initializer_list<QString> aliases) {
    const QJsonValue current = normalized.value(target);
    if (current.isBool()) {
        return;
    }
    for (const QJsonObject& source : sources) {
        for (const QString& alias : aliases) {
            const QJsonValue value = JsonValueAtPath(source, alias);
            if (value.isBool()) {
                normalized.insert(target, value);
                return;
            }
            if (value.isString()) {
                const QString text = value.toString().trimmed().toLower();
                if (text == QStringLiteral("true")) {
                    normalized.insert(target, true);
                    return;
                }
                if (text == QStringLiteral("false")) {
                    normalized.insert(target, false);
                    return;
                }
            }
        }
    }
}

QJsonObject NormalizeJobDetails(const QJsonObject& details) {
    QJsonObject normalized = details;
    const QVector<QJsonObject> sources = JobDetailSources(details);
    SetCanonicalText(normalized, sources, QStringLiteral("job_id"), {
        QStringLiteral("job_id"), QStringLiteral("job.id"),
        QStringLiteral("delivery_id"), QStringLiteral("delivery.id"),
        QStringLiteral("identity.job_id"), QStringLiteral("identity.id"),
        QStringLiteral("event_key.job_id"), QStringLiteral("event_key.delivery_id")});
    SetCanonicalText(normalized, sources, QStringLiteral("cargo"), {
        QStringLiteral("cargo"), QStringLiteral("cargo_name"), QStringLiteral("cargo.name")});
    SetCanonicalText(normalized, sources, QStringLiteral("cargo_id"), {
        QStringLiteral("cargo_id"), QStringLiteral("cargo.id")});
    SetCanonicalText(normalized, sources, QStringLiteral("source_city"), {
        QStringLiteral("source_city"), QStringLiteral("source.city"),
        QStringLiteral("origin_city"), QStringLiteral("origin.city"),
        QStringLiteral("source"), QStringLiteral("origin")});
    SetCanonicalText(normalized, sources, QStringLiteral("source_company"), {
        QStringLiteral("source_company"), QStringLiteral("source.company"),
        QStringLiteral("origin_company"), QStringLiteral("origin.company")});
    SetCanonicalText(normalized, sources, QStringLiteral("destination_city"), {
        QStringLiteral("destination_city"), QStringLiteral("destination.city"),
        QStringLiteral("target_city"), QStringLiteral("target.city"),
        QStringLiteral("destination"), QStringLiteral("target")});
    SetCanonicalText(normalized, sources, QStringLiteral("destination_company"), {
        QStringLiteral("destination_company"), QStringLiteral("destination.company"),
        QStringLiteral("target_company"), QStringLiteral("target.company")});
    SetCanonicalValue(normalized, sources, QStringLiteral("income"), {
        QStringLiteral("income"), QStringLiteral("revenue")});
    SetCanonicalValue(normalized, sources, QStringLiteral("driven_distance_km"), {
        QStringLiteral("driven_distance_km"), QStringLiteral("distance_driven_km"),
        QStringLiteral("distance.km")});
    SetCanonicalValue(normalized, sources, QStringLiteral("delivery_time_game_minutes"), {
        QStringLiteral("delivery_time_game_minutes"), QStringLiteral("delivery.time")});
    SetCanonicalValue(normalized, sources, QStringLiteral("xp"), {
        QStringLiteral("xp"), QStringLiteral("earned.xp")});
    SetCanonicalValue(normalized, sources, QStringLiteral("damage"), {
        QStringLiteral("damage"), QStringLiteral("cargo.damage")});
    SetCanonicalValue(normalized, sources, QStringLiteral("weight"), {
        QStringLiteral("weight"), QStringLiteral("cargo.mass")});
    SetCanonicalBoolean(normalized, sources, QStringLiteral("special_job"), {
        QStringLiteral("special_job"), QStringLiteral("is_special_job"),
        QStringLiteral("special.job")});
    return normalized;
}

QString StableJobIdentity(const QJsonObject& details) {
    return EventText(details, {
        QStringLiteral("job_id"), QStringLiteral("job.id"),
        QStringLiteral("delivery_id"), QStringLiteral("delivery.id"),
        QStringLiteral("nlsi_job_id")});
}

QString EventRecordIdentity(
    const QJsonObject& details,
    const QString& timestamp,
    const QString& provider) {
    const QString stable_identity = StableJobIdentity(details);
    if (!stable_identity.isEmpty()) {
        return stable_identity;
    }
    const QByteArray identity_material = provider.toUtf8() + '\n'
        + timestamp.toUtf8() + '\n'
        + QJsonDocument(details).toJson(QJsonDocument::Compact);
    return QStringLiteral("event-%1").arg(QString::fromLatin1(
        QCryptographicHash::hash(identity_material, QCryptographicHash::Sha256).toHex()));
}

bool HasCompleteDeliveryLabels(const QJsonObject& details) {
    return !EventText(details, {
               QStringLiteral("cargo"), QStringLiteral("cargo_name"),
               QStringLiteral("cargo.name")}).isEmpty()
        && !EventText(details, {
               QStringLiteral("source_city"), QStringLiteral("source.city")}).isEmpty()
        && !EventText(details, {
               QStringLiteral("destination_city"), QStringLiteral("destination.city")}).isEmpty();
}

bool CompleteJobLabelsMatch(const JobRecord& left, const JobRecord& right) {
    const QString left_cargo = EventText(left.details, {
        QStringLiteral("cargo"), QStringLiteral("cargo_name"), QStringLiteral("cargo.name")});
    const QString right_cargo = EventText(right.details, {
        QStringLiteral("cargo"), QStringLiteral("cargo_name"), QStringLiteral("cargo.name")});
    const QString left_source = EventText(left.details, {
        QStringLiteral("source_city"), QStringLiteral("source.city")});
    const QString right_source = EventText(right.details, {
        QStringLiteral("source_city"), QStringLiteral("source.city")});
    const QString left_destination = EventText(left.details, {
        QStringLiteral("destination_city"), QStringLiteral("destination.city")});
    const QString right_destination = EventText(right.details, {
        QStringLiteral("destination_city"), QStringLiteral("destination.city")});
    if (left_cargo.isEmpty() || right_cargo.isEmpty()
        || left_source.isEmpty() || right_source.isEmpty()
        || left_destination.isEmpty() || right_destination.isEmpty()
        || left_cargo != right_cargo || left_source != right_source
        || left_destination != right_destination) {
        return false;
    }
    for (const auto& keys : {
             std::initializer_list<QString>{QStringLiteral("source_company"), QStringLiteral("source.company")},
             std::initializer_list<QString>{QStringLiteral("destination_company"), QStringLiteral("destination.company")}}) {
        const QString left_company = EventText(left.details, keys);
        const QString right_company = EventText(right.details, keys);
        if (!left_company.isEmpty() && !right_company.isEmpty()
            && left_company != right_company) {
            return false;
        }
    }
    return true;
}

QJsonObject MergeAvailableDetails(const QJsonObject& existing, const QJsonObject& incoming) {
    QJsonObject merged = existing;
    for (auto it = incoming.constBegin(); it != incoming.constEnd(); ++it) {
        const QJsonValue old_value = merged.value(it.key());
        if (old_value.isObject() && it.value().isObject()) {
            merged.insert(it.key(), MergeAvailableDetails(old_value.toObject(), it.value().toObject()));
        } else if (!HasValue(old_value) && HasValue(it.value())) {
            merged.insert(it.key(), it.value());
        }
    }
    return merged;
}

bool TimestampsWithinFiveSeconds(const QString& left, const QString& right) {
    const QDateTime left_time = nlsi::time::ParseInstant(left);
    const QDateTime right_time = nlsi::time::ParseInstant(right);
    return left_time.isValid() && right_time.isValid()
        && std::abs(left_time.msecsTo(right_time)) <= 5000;
}

bool JobRecordsMatch(const JobRecord& left, const JobRecord& right) {
    if (left.status != right.status) {
        return false;
    }
    if (!TimestampsWithinFiveSeconds(left.timestamp, right.timestamp)) {
        return false;
    }
    const QString left_job_id = StableJobIdentity(left.details);
    const QString right_job_id = StableJobIdentity(right.details);
    if (!left_job_id.isEmpty() && !right_job_id.isEmpty()) {
        if (left_job_id != right_job_id) {
            return false;
        }
    } else {
        const bool same_provider_duplicate = !left.provider.isEmpty()
            && left.provider == right.provider && left.timestamp == right.timestamp;
        const bool cross_provider_match = !left.provider.isEmpty()
            && !right.provider.isEmpty() && left.provider != right.provider;
        if ((!same_provider_duplicate && !cross_provider_match)
            || !CompleteJobLabelsMatch(left, right)) {
            return false;
        }
    }

    for (const auto& keys : {
             std::initializer_list<QString>{QStringLiteral("cargo"), QStringLiteral("cargo_name")},
             std::initializer_list<QString>{QStringLiteral("source_city"), QStringLiteral("source.city")},
             std::initializer_list<QString>{QStringLiteral("source_company"), QStringLiteral("source.company")},
             std::initializer_list<QString>{QStringLiteral("destination_city"), QStringLiteral("destination.city")},
             std::initializer_list<QString>{QStringLiteral("destination_company"), QStringLiteral("destination.company")}}) {
        const QString left_value = EventText(left.details, keys);
        const QString right_value = EventText(right.details, keys);
        if (!left_value.isEmpty() && !right_value.isEmpty() && left_value != right_value) {
            return false;
        }
    }
    return true;
}

void MergeJobRecord(JobRecord& existing, const JobRecord& incoming) {
    if (existing.cargo.isEmpty()) existing.cargo = incoming.cargo;
    if (existing.source.isEmpty()) existing.source = incoming.source;
    if (existing.destination.isEmpty()) existing.destination = incoming.destination;
    if (existing.identity.isEmpty()) existing.identity = incoming.identity;
    if (existing.nlsi_job_id.isEmpty()) existing.nlsi_job_id = incoming.nlsi_job_id;
    existing.details = MergeAvailableDetails(existing.details, incoming.details);
    if (existing.provider.isEmpty()) existing.provider = incoming.provider;
}

QString ProviderEventKey(
    const QString& timestamp,
    const QString& source,
    const QString& type,
    const QJsonObject& details) {
    const QString source_event_id = EventText(details, {
        QStringLiteral("provider_event_id")});
    const QString event_identity = source_event_id.isEmpty()
        ? QString::fromUtf8(QJsonDocument(details).toJson(QJsonDocument::Compact))
        : source_event_id;
    return source + QLatin1Char('|') + type + QLatin1Char('|') + timestamp
        + QLatin1Char('|') + event_identity;
}

bool ParseRecord(
    const QByteArray& line,
    const QString& path,
    int line_number,
    QJsonObject& object,
    QString& error) {
    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(line.trimmed(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("Invalid JSONL record in %1 at line %2: %3")
            .arg(path)
            .arg(line_number)
            .arg(parse_error.errorString());
        return false;
    }
    object = document.object();
    return true;
}

bool TimestampNewer(const QString& left, const QString& right) {
    const QDateTime left_time = nlsi::time::ParseInstant(left);
    const QDateTime right_time = nlsi::time::ParseInstant(right);
    if (left_time.isValid() && right_time.isValid()) {
        return left_time > right_time;
    }
    if (left_time.isValid() != right_time.isValid()) {
        return left_time.isValid();
    }
    return left > right;
}

void SortNewestFirst(QVector<EventRecord>& events) {
    std::sort(events.begin(), events.end(), [](const EventRecord& left, const EventRecord& right) {
        return TimestampNewer(left.timestamp, right.timestamp);
    });
}

void SortNewestFirst(QVector<SessionRecord>& sessions) {
    std::sort(sessions.begin(), sessions.end(), [](const SessionRecord& left, const SessionRecord& right) {
        return TimestampNewer(left.started_at, right.started_at);
    });
}

void SortNewestFirst(QVector<JobRecord>& jobs) {
    std::sort(jobs.begin(), jobs.end(), [](const JobRecord& left, const JobRecord& right) {
        return TimestampNewer(left.timestamp, right.timestamp);
    });
}

} // namespace

HistoryStore::HistoryStore(
    nlsi::logging::Logger& logger,
    std::function<std::uint32_t()> job_id_number_source)
    : logger_(logger),
      job_id_number_source_(std::move(job_id_number_source)) {
}

bool HistoryStore::Initialize(
    const QString& user_data_directory,
    const QString& legacy_application_directory,
    const QString& legacy_user_data_directory) {
    std::lock_guard<std::mutex> lock(mutex_);
    root_ = user_data_directory;
    events_path_ = QDir(root_).filePath(QStringLiteral("logs/events.jsonl"));
    sessions_path_ = QDir(root_).filePath(QStringLiteral("session_logs/sessions.jsonl"));
    jobs_path_ = QDir(root_).filePath(QStringLiteral("session_logs/jobs.jsonl"));
    job_ids_path_ = QDir(root_).filePath(QStringLiteral("session_logs/job_ids.json"));
    if (!QDir().mkpath(QDir(root_).filePath(QStringLiteral("logs")))
        || !QDir().mkpath(QDir(root_).filePath(QStringLiteral("session_logs")))) {
        SetError(QStringLiteral("Could not create logs or session_logs under %1.").arg(root_));
        return false;
    }
    const auto migrate_legacy_root = [this](const QString& legacy_root) {
        if (legacy_root.isEmpty()) {
            return true;
        }
        const bool logs_migrated = MigrateLegacyDirectory(
            QDir(legacy_root).filePath(QStringLiteral("logs")),
            QDir(root_).filePath(QStringLiteral("logs")));
        const bool sessions_migrated = MigrateLegacyDirectory(
            QDir(legacy_root).filePath(QStringLiteral("session_logs")),
            QDir(root_).filePath(QStringLiteral("session_logs")));
        return logs_migrated && sessions_migrated;
    };
    if (!migrate_legacy_root(legacy_application_directory)
        || !migrate_legacy_root(legacy_user_data_directory)) {
        return false;
    }
    return Load();
}

bool HistoryStore::MigrateLegacyDirectory(
    const QString& source,
    const QString& destination) {
    if (!QFileInfo::exists(source)) {
        return true;
    }
    bool succeeded = true;
    QDirIterator files(
        source,
        QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const QString source_path = files.next();
        const QString relative_path = QDir(source).relativeFilePath(source_path);
        const QString destination_path = QDir(destination).filePath(relative_path);
        if (QFileInfo::exists(destination_path)) {
            continue;
        }
        if (!QDir().mkpath(QFileInfo(destination_path).absolutePath())) {
            SetError(QStringLiteral("Could not create legacy log destination directory for %1.")
                .arg(destination_path));
            succeeded = false;
            continue;
        }
        if (!QFile::copy(source_path, destination_path)) {
            SetError(QStringLiteral("Could not preserve legacy log file %1 at %2.")
                .arg(source_path, destination_path));
            succeeded = false;
        }
    }
    return succeeded;
}

bool HistoryStore::Load() {
    snapshot_ = {};
    recorded_provider_events_.clear();
    recorded_job_events_.clear();
    used_job_ids_.clear();
    job_ids_.clear();
    const bool events_loaded = LoadEvents();
    const bool sessions_loaded = LoadSessions();
    const bool jobs_loaded = LoadJobs();
    const bool telemetry_jobs_loaded = LoadTelemetryJobs();
    const bool job_ids_loaded = LoadJobIds();
    SortNewestFirst(snapshot_.events);
    SortNewestFirst(snapshot_.sessions);
    SortNewestFirst(snapshot_.jobs);
    return events_loaded && sessions_loaded && jobs_loaded
        && telemetry_jobs_loaded && job_ids_loaded;
}

bool HistoryStore::LoadEvents() {
    QFile file(events_path_);
    if (!file.exists()) {
        return true;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        SetError(QStringLiteral("Could not read %1: %2").arg(events_path_, file.errorString()));
        return false;
    }
    int line_number = 0;
    while (!file.atEnd()) {
        ++line_number;
        QJsonObject object;
        QString error;
        if (!ParseRecord(file.readLine(), events_path_, line_number, object, error)) {
            SetError(error);
            continue;
        }
        const QString timestamp = object.value(QStringLiteral("timestamp")).toString();
        const QString source = object.value(QStringLiteral("source")).toString();
        const QString type = object.value(QStringLiteral("type")).toString();
        const QJsonObject details = object.value(QStringLiteral("details")).toObject();
        const QString event_key = ProviderEventKey(timestamp, source, type, details);
        if (recorded_provider_events_.contains(event_key)) {
            continue;
        }
        recorded_provider_events_.insert(event_key);
        snapshot_.events.push_back({
            timestamp,
            source,
            type,
            QString::fromUtf8(QJsonDocument(details).toJson(QJsonDocument::Compact)),
        });
    }
    return true;
}

bool HistoryStore::LoadSessions() {
    QFile file(sessions_path_);
    if (!file.exists()) {
        return true;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        SetError(QStringLiteral("Could not read %1: %2").arg(sessions_path_, file.errorString()));
        return false;
    }

    QMap<QString, int> rows_by_id;
    int line_number = 0;
    while (!file.atEnd()) {
        ++line_number;
        QJsonObject object;
        QString error;
        if (!ParseRecord(file.readLine(), sessions_path_, line_number, object, error)) {
            SetError(error);
            continue;
        }
        const QString id = object.value(QStringLiteral("session_id")).toString();
        const QString kind = object.value(QStringLiteral("record_type")).toString();
        if (id.isEmpty() || (kind != QStringLiteral("started") && kind != QStringLiteral("ended"))) {
            SetError(QStringLiteral("Invalid session record in %1 at line %2.")
                .arg(sessions_path_)
                .arg(line_number));
            continue;
        }
        int row = rows_by_id.value(id, -1);
        if (kind == QStringLiteral("started")) {
            if (row < 0) {
                row = snapshot_.sessions.size();
                rows_by_id.insert(id, row);
                snapshot_.sessions.push_back({
                    id,
                    object.value(QStringLiteral("game")).toString(),
                    object.value(QStringLiteral("timestamp")).toString(),
                    {},
                    QStringLiteral("Active"),
                });
            } else {
                SetError(QStringLiteral("Duplicate session start record for %1.").arg(id));
            }
        } else if (row >= 0) {
            auto& session = snapshot_.sessions[row];
            session.ended_at = object.value(QStringLiteral("timestamp")).toString();
            session.result = object.value(QStringLiteral("reason")).toString();
        } else {
            SetError(QStringLiteral("Session end has no matching start record for %1.").arg(id));
        }
    }
    return true;
}

bool HistoryStore::LoadJobs() {
    QFile file(jobs_path_);
    if (!file.exists()) {
        return true;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        SetError(QStringLiteral("Could not read %1: %2").arg(jobs_path_, file.errorString()));
        return false;
    }
    int line_number = 0;
    while (!file.atEnd()) {
        ++line_number;
        QJsonObject object;
        QString error;
        if (!ParseRecord(file.readLine(), jobs_path_, line_number, object, error)) {
            SetError(error);
            continue;
        }
        const QString key = object.value(QStringLiteral("event_key")).toString();
        const QJsonObject details = object.value(QStringLiteral("details")).toObject();
        if (!key.isEmpty()) {
            recorded_job_events_.insert(key);
        }
        const QJsonArray source_event_keys =
            object.value(QStringLiteral("source_event_keys")).toArray();
        for (const QJsonValue& event_key : source_event_keys) {
            if (event_key.isString() && !event_key.toString().isEmpty()) {
                recorded_job_events_.insert(event_key.toString());
            }
        }
        const QString identity = object.value(QStringLiteral("identity")).toString();
        const QString nlsi_job_id = object.value(QStringLiteral("nlsi_job_id")).toString();
        if (!identity.isEmpty()) {
            used_job_ids_.insert(identity);
        }
        if (!nlsi_job_id.isEmpty()) {
            used_job_ids_.insert(nlsi_job_id);
        }
        JobRecord candidate{
            identity.isEmpty()
                ? EventRecordIdentity(NormalizeJobDetails(details),
                    object.value(QStringLiteral("timestamp")).toString(),
                    object.value(QStringLiteral("provider")).toString())
                : identity,
            IsUsableText(object.value(QStringLiteral("cargo")).toString())
                ? object.value(QStringLiteral("cargo")).toString().trimmed() : QString(),
            IsUsableText(object.value(QStringLiteral("source")).toString())
                ? object.value(QStringLiteral("source")).toString().trimmed() : QString(),
            IsUsableText(object.value(QStringLiteral("destination")).toString())
                ? object.value(QStringLiteral("destination")).toString().trimmed() : QString(),
            object.value(QStringLiteral("status")).toString(),
            object.value(QStringLiteral("timestamp")).toString(),
            NormalizeJobDetails(details),
            nlsi_job_id,
            object.value(QStringLiteral("provider")).toString(),
        };
        if (candidate.cargo.isEmpty()) {
            candidate.cargo = EventText(candidate.details, {
                QStringLiteral("cargo"), QStringLiteral("cargo_name"), QStringLiteral("cargo.name")});
        }
        if (candidate.source.isEmpty()) {
            candidate.source = RouteText(
                EventText(candidate.details, {
                    QStringLiteral("source_company"), QStringLiteral("source.company")}),
                EventText(candidate.details, {
                    QStringLiteral("source_city"), QStringLiteral("source.city")}));
        }
        if (candidate.destination.isEmpty()) {
            candidate.destination = RouteText(
                EventText(candidate.details, {
                    QStringLiteral("destination_company"), QStringLiteral("destination.company")}),
                EventText(candidate.details, {
                    QStringLiteral("destination_city"), QStringLiteral("destination.city")}));
        }
        bool merged = false;
        for (JobRecord& existing : snapshot_.jobs) {
            if (JobRecordsMatch(existing, candidate)) {
                MergeJobRecord(existing, candidate);
                merged = true;
                break;
            }
        }
        if (!merged) {
            snapshot_.jobs.push_back(std::move(candidate));
        }
    }
    return true;
}

bool HistoryStore::LoadTelemetryJobs() {
    const QString telemetry_directory =
        QDir(root_).filePath(QStringLiteral("logs/telemetry"));
    if (!QFileInfo::exists(telemetry_directory)) {
        return true;
    }
    QDirIterator files(
        telemetry_directory,
        {QStringLiteral("*.nlsi")},
        QDir::Files,
        QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const QString path = files.next();
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            SetError(QStringLiteral("Could not read completed-job telemetry in %1: %2")
                .arg(path, file.errorString()));
            return false;
        }
        int line_number = 0;
        while (!file.atEnd()) {
            ++line_number;
            QJsonObject object;
            QString error;
            if (!ParseRecord(file.readLine(), path, line_number, object, error)) {
                SetError(error);
                continue;
            }
            if (object.value(QStringLiteral("record_type")).toString()
                    != QStringLiteral("provider_event")) {
                continue;
            }
            const QString event_type = object.value(QStringLiteral("event")).toString();
            const bool delivered = event_type == QStringLiteral("job.delivered");
            const bool cancelled = event_type == QStringLiteral("job.cancelled");
            if (!delivered && !cancelled) {
                continue;
            }
            QJsonObject details = NormalizeJobDetails(
                object.value(QStringLiteral("data")).toObject());
            const QString status = delivered
                ? QStringLiteral("Delivered") : QStringLiteral("Cancelled");
            const QString timestamp = object.value(QStringLiteral("timestamp_utc")).toString();
            const QString event_key = QStringLiteral("telemetry:%1|%2|%3|%4")
                .arg(object.value(QStringLiteral("provider")).toString(),
                    EventText(details, {QStringLiteral("provider_event_id")}),
                    status, timestamp)
                + QLatin1Char('|')
                + QString::fromUtf8(QJsonDocument(details).toJson(QJsonDocument::Compact));
            if (recorded_job_events_.contains(event_key)) {
                continue;
            }
            const QString stable_identity = StableJobIdentity(details);
            if (stable_identity.isEmpty() && !HasCompleteDeliveryLabels(details)) {
                continue;
            }
            const QString identity = EventRecordIdentity(
                details, timestamp, object.value(QStringLiteral("provider")).toString());
            const QString cargo = EventText(details, {
                QStringLiteral("cargo"), QStringLiteral("cargo_name"),
                QStringLiteral("cargo.name")});
            const QString source_company = EventText(details, {
                QStringLiteral("source_company"), QStringLiteral("source.company")});
            const QString source_city = EventText(details, {
                QStringLiteral("source_city"), QStringLiteral("source.city")});
            const QString destination_company = EventText(details, {
                QStringLiteral("destination_company"),
                QStringLiteral("destination.company")});
            const QString destination_city = EventText(details, {
                QStringLiteral("destination_city"), QStringLiteral("destination.city")});
            JobRecord candidate{
                identity,
                cargo,
                RouteText(source_company, source_city),
                RouteText(destination_company, destination_city),
                status,
                timestamp,
                details,
                details.value(QStringLiteral("nlsi_job_id")).toString(),
                object.value(QStringLiteral("provider")).toString(),
            };
            bool merged = false;
            for (JobRecord& existing : snapshot_.jobs) {
                if (JobRecordsMatch(existing, candidate)) {
                    MergeJobRecord(existing, candidate);
                    merged = true;
                    break;
                }
            }
            if (!merged) {
                snapshot_.jobs.push_back(std::move(candidate));
            }
            recorded_job_events_.insert(event_key);
        }
    }
    return true;
}

bool HistoryStore::LoadJobIds() {
    QFile file(job_ids_path_);
    if (!file.exists()) {
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        SetError(QStringLiteral("Could not read %1: %2").arg(job_ids_path_, file.errorString()));
        return false;
    }
    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        SetError(QStringLiteral("Invalid NLSI job ID mapping in %1: %2")
            .arg(job_ids_path_, parse_error.errorString()));
        return false;
    }
    const QJsonObject mappings = document.object();
    for (auto it = mappings.constBegin(); it != mappings.constEnd(); ++it) {
        const QString id = it.value().toString();
        if (it.key().isEmpty() || !id.startsWith(QStringLiteral("JOB-NLSI-"))
            || job_ids_.values().contains(id)) {
            SetError(QStringLiteral("Invalid or duplicate NLSI job ID mapping in %1.")
                .arg(job_ids_path_));
            return false;
        }
        job_ids_.insert(it.key(), id);
        used_job_ids_.insert(id);
    }
    return true;
}

bool HistoryStore::SaveJobIds() {
    QJsonObject mappings;
    for (auto it = job_ids_.constBegin(); it != job_ids_.constEnd(); ++it) {
        mappings.insert(it.key(), it.value());
    }
    QSaveFile file(job_ids_path_);
    const QByteArray contents = QJsonDocument(mappings).toJson(QJsonDocument::Compact);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(contents) != contents.size()
        || !file.commit()) {
        SetError(QStringLiteral("Could not persist NLSI job ID mappings in %1: %2")
            .arg(job_ids_path_, file.errorString()));
        return false;
    }
    return true;
}

QString HistoryStore::AllocateNlsiJobId() {
    for (int attempt = 0; attempt < 10000; ++attempt) {
        const std::uint32_t number = job_id_number_source_
            ? job_id_number_source_()
            : QRandomGenerator::global()->bounded(10000U);
        const QString candidate = QStringLiteral("JOB-NLSI-%1")
            .arg(number % 10000U, 4, 10, QLatin1Char('0'));
        if (!used_job_ids_.contains(candidate)) {
            return candidate;
        }
    }
    return {};
}

QString HistoryStore::EnsureNlsiJobId(const QString& game_job_identity) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (game_job_identity.isEmpty()) {
        return {};
    }
    const auto existing = job_ids_.constFind(game_job_identity);
    if (existing != job_ids_.cend()) {
        return existing.value();
    }
    const QString nlsi_job_id = AllocateNlsiJobId();
    if (nlsi_job_id.isEmpty()) {
        SetError(QStringLiteral("Could not allocate a unique NLSI job ID; all four-digit IDs are in use."));
        return {};
    }
    job_ids_.insert(game_job_identity, nlsi_job_id);
    used_job_ids_.insert(nlsi_job_id);
    if (!SaveJobIds()) {
        job_ids_.remove(game_job_identity);
        used_job_ids_.remove(nlsi_job_id);
        return {};
    }
    ++snapshot_.revision;
    snapshot_.error.clear();
    return nlsi_job_id;
}

bool HistoryStore::RecordProviderEvent(
    const QByteArray& raw_packet,
    bool* newly_recorded) {
    if (newly_recorded) {
        *newly_recorded = false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(raw_packet, &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        SetError(QStringLiteral("Could not persist provider event: %1").arg(parse_error.errorString()));
        return false;
    }
    const QJsonObject packet = document.object();
    QString type = packet.value(QStringLiteral("event")).toString();
    if (type.isEmpty()) {
        type = packet.value(QStringLiteral("type")).toString();
    }
    if (type == QStringLiteral("lifecycle")) {
        type = packet.value(QStringLiteral("state")).toString() == QStringLiteral("paused")
            ? QStringLiteral("DRIVING_PAUSED")
            : QStringLiteral("DRIVING_RESUMED");
    }
    if (type.isEmpty()) {
        SetError(QStringLiteral("Provider event did not contain an event type."));
        return false;
    }

    QJsonObject details = packet;
    details.remove(QStringLiteral("type"));
    details.remove(QStringLiteral("timestamp"));
    const QString timestamp = packet.value(QStringLiteral("timestamp")).toString();
    const QString source = packet.value(QStringLiteral("provider")).toString(QStringLiteral("NLSI"));
    const QString event_key = ProviderEventKey(timestamp, source, type, details);
    if (recorded_provider_events_.contains(event_key)) {
        return true;
    }
    const QJsonObject record{
        {QStringLiteral("timestamp"), timestamp},
        {QStringLiteral("source"), source},
        {QStringLiteral("type"), type},
        {QStringLiteral("details"), details},
    };
    if (!AppendJsonLine(events_path_, record)) {
        return false;
    }

    recorded_provider_events_.insert(event_key);
    if (newly_recorded) {
        *newly_recorded = true;
    }
    snapshot_.events.prepend({
        timestamp,
        source,
        type,
        QString::fromUtf8(QJsonDocument(details).toJson(QJsonDocument::Compact)),
    });
    ++snapshot_.revision;
    snapshot_.error.clear();
    std::wstring log_error;
    const QString data_text = QString::fromUtf8(
        QJsonDocument(details.value(QStringLiteral("data")).toObject())
            .toJson(QJsonDocument::Compact));
    if (!logger_.Log(
            (QStringLiteral("[event] %1 %2 %3")
                .arg(timestamp, source, type)
                + (data_text == QStringLiteral("{}")
                    ? QString() : QStringLiteral(" | data=%1").arg(data_text)))
                .toStdWString(),
            &log_error)) {
        snapshot_.error = QString::fromStdWString(log_error);
    }
    return snapshot_.error.isEmpty();
}

bool HistoryStore::StartSession(
    const std::wstring& id,
    const std::wstring& game,
    const std::wstring& timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);
    const QString session_id = ToQString(id);
    for (const auto& existing : snapshot_.sessions) {
        if (existing.id == session_id) {
            return true;
        }
    }
    const QString start_time = ToQString(timestamp);
    if (!AppendJsonLine(sessions_path_, {
            {QStringLiteral("record_type"), QStringLiteral("started")},
            {QStringLiteral("session_id"), session_id},
            {QStringLiteral("game"), ToQString(game)},
            {QStringLiteral("timestamp"), start_time},
        })) {
        return false;
    }
    snapshot_.sessions.prepend({
        session_id,
        ToQString(game),
        start_time,
        {},
        QStringLiteral("Active"),
    });
    ++snapshot_.revision;
    snapshot_.error.clear();
    const QString path = QDir(root_).filePath(
        QStringLiteral("session_logs/%1.txt").arg(session_id));
    if (!AppendTextLine(path, QStringLiteral("Session started: %1 | Game: %2")
            .arg(start_time, ToQString(game)))) {
        return false;
    }
    std::wstring log_error;
    if (!logger_.Log(
            QStringLiteral("[session] started %1 game=%2")
                .arg(session_id, ToQString(game)).toStdWString(),
            &log_error)) {
        SetError(QString::fromStdWString(log_error));
        return false;
    }
    return true;
}

bool HistoryStore::EndSession(
    const std::wstring& id,
    const std::wstring& timestamp,
    const std::wstring& reason,
    double duration_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    const QString session_id = ToQString(id);
    auto session = std::find_if(snapshot_.sessions.begin(), snapshot_.sessions.end(),
        [&session_id](const SessionRecord& record) { return record.id == session_id; });
    if (session == snapshot_.sessions.end() || !session->ended_at.isEmpty()) {
        return true;
    }

    const QString end_time = ToQString(timestamp);
    const QString result = ToQString(reason);
    if (!AppendJsonLine(sessions_path_, {
            {QStringLiteral("record_type"), QStringLiteral("ended")},
            {QStringLiteral("session_id"), session_id},
            {QStringLiteral("timestamp"), end_time},
            {QStringLiteral("reason"), result},
            {QStringLiteral("duration_seconds"), duration_seconds},
        })) {
        return false;
    }
    session->ended_at = end_time;
    session->result = result;
    ++snapshot_.revision;
    snapshot_.error.clear();
    const QString path = QDir(root_).filePath(
        QStringLiteral("session_logs/%1.txt").arg(session_id));
    if (!AppendTextLine(path, QStringLiteral("Session ended: %1 | Reason: %2 | Duration: %3 seconds")
            .arg(end_time, result)
            .arg(duration_seconds, 0, 'f', 1))) {
        return false;
    }
    std::wstring log_error;
    if (!logger_.Log(
            QStringLiteral("[session] ended %1 reason=%2")
                .arg(session_id, result).toStdWString(),
            &log_error)) {
        SetError(QString::fromStdWString(log_error));
        return false;
    }
    return true;
}

bool HistoryStore::RecordJob(
    const telemetry::JobSnapshot& job,
    const QString& event_type,
    const QString& timestamp,
    const QJsonObject& event_details,
    const QString& provider) {
    std::lock_guard<std::mutex> lock(mutex_);
    const bool delivered = event_type == QStringLiteral("job.delivered");
    const bool cancelled = event_type == QStringLiteral("job.cancelled");
    if (!delivered && !cancelled) {
        return true;
    }
    const QString status = delivered ? QStringLiteral("Delivered") : QStringLiteral("Cancelled");
    const QJsonObject normalized_details = NormalizeJobDetails(event_details);
    const QString job_id = StableJobIdentity(normalized_details);
    const QString event_identity = EventText(normalized_details, {
        QStringLiteral("cargo_id"), QStringLiteral("cargo.id")});
    const QString snapshot_cargo_id = FieldText(job.cargo_id);
    const QString event_job_identity = !job_id.isEmpty() ? job_id : event_identity;
    const bool snapshot_identity_matches =
        !snapshot_cargo_id.isEmpty()
        && !event_job_identity.isEmpty() && event_job_identity == snapshot_cargo_id;
    const auto raw_snapshot_field = [snapshot_identity_matches](const auto& field) {
        return snapshot_identity_matches ? FieldText(field) : QString();
    };
    const QString event_cargo = EventText(normalized_details, {
        QStringLiteral("cargo"), QStringLiteral("cargo_name"), QStringLiteral("cargo.name")});
    const QString snapshot_cargo = raw_snapshot_field(job.cargo);
    const bool snapshot_details_compatible =
        (event_cargo.isEmpty() || snapshot_cargo.isEmpty() || event_cargo == snapshot_cargo);
    const auto matches_event_field = [](const QString& event_value, const QString& snapshot_value) {
        return event_value.isEmpty() || snapshot_value.isEmpty() || event_value == snapshot_value;
    };
    const bool snapshot_route_compatible =
        matches_event_field(
            EventText(normalized_details, {QStringLiteral("source_company"), QStringLiteral("source.company")}),
            raw_snapshot_field(job.source_company))
        && matches_event_field(
            EventText(normalized_details, {QStringLiteral("source_city"), QStringLiteral("source.city")}),
            raw_snapshot_field(job.source_city))
        && matches_event_field(
            EventText(normalized_details, {
                QStringLiteral("destination_company"), QStringLiteral("destination.company")}),
            raw_snapshot_field(job.destination_company))
        && matches_event_field(
            EventText(normalized_details, {
                QStringLiteral("destination_city"), QStringLiteral("destination.city")}),
            raw_snapshot_field(job.destination_city));
    const bool snapshot_matches_event = snapshot_identity_matches
        && snapshot_details_compatible && snapshot_route_compatible;
    if (job_id.isEmpty() && !snapshot_matches_event
        && !HasCompleteDeliveryLabels(normalized_details)) {
        return true;
    }
    const auto snapshot_field = [snapshot_matches_event](const auto& field) {
        return snapshot_matches_event ? FieldText(field) : QString();
    };
    const QString identity = !job_id.isEmpty()
        ? job_id
        : (snapshot_matches_event ? ToQString(job.identity)
            : EventRecordIdentity(normalized_details, timestamp, provider));
    const QString detail_key = QString::fromUtf8(
        QJsonDocument(normalized_details).toJson(QJsonDocument::Compact));
    const QString event_key = identity + QLatin1Char('|') + status
        + QLatin1Char('|') + timestamp + QLatin1Char('|') + detail_key;
    if (recorded_job_events_.contains(event_key)
        && (job_id.isEmpty() || std::none_of(
            snapshot_.jobs.cbegin(), snapshot_.jobs.cend(), [&job_id, &status](const JobRecord& record) {
                return record.status == status
                    && EventText(record.details, {
                        QStringLiteral("job_id"), QStringLiteral("job.id")}) == job_id;
            }))) {
        return true;
    }
    const QString cargo = event_cargo.isEmpty() ? snapshot_cargo : event_cargo;
    const QString event_source_company = EventText(normalized_details, {
        QStringLiteral("source_company"), QStringLiteral("source.company")});
    const QString event_source_city = EventText(normalized_details, {
        QStringLiteral("source_city"), QStringLiteral("source.city")});
    const QString event_destination_company = EventText(normalized_details, {
        QStringLiteral("destination_company"), QStringLiteral("destination.company")});
    const QString event_destination_city = EventText(normalized_details, {
        QStringLiteral("destination_city"), QStringLiteral("destination.city")});
    const QString source_company = event_source_company.isEmpty()
        ? snapshot_field(job.source_company) : event_source_company;
    const QString source_city = event_source_city.isEmpty()
        ? snapshot_field(job.source_city) : event_source_city;
    const QString destination_company = event_destination_company.isEmpty()
        ? snapshot_field(job.destination_company) : event_destination_company;
    const QString destination_city = event_destination_city.isEmpty()
        ? snapshot_field(job.destination_city) : event_destination_city;
    const QString source = RouteText(source_company, source_city);
    const QString destination = RouteText(destination_company, destination_city);
    QJsonObject persisted_details = normalized_details;
    const auto persist_snapshot_field = [&persisted_details, &normalized_details](
                                            const QString& name,
                                            const QString& value,
                                            std::initializer_list<QString> event_keys) {
        if (!value.isEmpty() && EventText(normalized_details, event_keys).isEmpty()) {
            persisted_details.insert(name, value);
        }
    };
    persist_snapshot_field(QStringLiteral("cargo"), snapshot_field(job.cargo), {
        QStringLiteral("cargo"), QStringLiteral("cargo_name"), QStringLiteral("cargo.id")});
    persist_snapshot_field(QStringLiteral("cargo_id"),
        snapshot_matches_event ? snapshot_cargo_id : QString(), {
        QStringLiteral("cargo_id"), QStringLiteral("cargo.id")});
    persist_snapshot_field(QStringLiteral("source_company"), source_company, {
        QStringLiteral("source_company"), QStringLiteral("source.company")});
    persist_snapshot_field(QStringLiteral("source_city"), source_city, {
        QStringLiteral("source_city"), QStringLiteral("source.city")});
    persist_snapshot_field(QStringLiteral("destination_company"),
        destination_company, {
            QStringLiteral("destination_company"), QStringLiteral("destination.company")});
    persist_snapshot_field(QStringLiteral("destination_city"),
        destination_city, {
            QStringLiteral("destination_city"), QStringLiteral("destination.city")});
    persist_snapshot_field(QStringLiteral("income"), snapshot_field(job.income), {
        QStringLiteral("income")});
    persist_snapshot_field(QStringLiteral("planned_distance"),
        snapshot_field(job.planned_distance), {
            QStringLiteral("planned_distance"), QStringLiteral("planned_distance_km")});

    JobRecord candidate{
        identity, cargo, source, destination, status, timestamp, persisted_details, {}, provider};
    auto matching_job = snapshot_.jobs.end();
    for (auto it = snapshot_.jobs.begin(); it != snapshot_.jobs.end(); ++it) {
        if (JobRecordsMatch(*it, candidate)) {
            matching_job = it;
            break;
        }
    }
    QString canonical_identity = identity;
    QString nlsi_job_id;
    QString record_timestamp = timestamp;
    bool matched_existing = matching_job != snapshot_.jobs.end();
    if (matching_job != snapshot_.jobs.end()) {
        candidate = *matching_job;
        const QJsonObject previous_details = candidate.details;
        const QString incoming_cargo = event_cargo.isEmpty() ? snapshot_cargo : event_cargo;
        const QString incoming_source = RouteText(source_company, source_city);
        const QString incoming_destination =
            RouteText(destination_company, destination_city);
        candidate.details = MergeAvailableDetails(previous_details, persisted_details);
        if (candidate.cargo.isEmpty()) candidate.cargo = incoming_cargo;
        if (candidate.source.isEmpty()) candidate.source = incoming_source;
        if (candidate.destination.isEmpty()) candidate.destination = incoming_destination;
        canonical_identity = matching_job->identity;
        nlsi_job_id = matching_job->nlsi_job_id;
        record_timestamp = matching_job->timestamp;
        candidate.identity = canonical_identity;
        candidate.timestamp = record_timestamp;
        candidate.nlsi_job_id = nlsi_job_id;
        if (candidate.provider.isEmpty()) {
            candidate.provider = provider;
        }
    }
    if (nlsi_job_id.isEmpty()) {
        nlsi_job_id = job_ids_.value(canonical_identity);
    }
    if (nlsi_job_id.isEmpty()) {
        nlsi_job_id = AllocateNlsiJobId();
        if (!nlsi_job_id.isEmpty()) {
            job_ids_.insert(canonical_identity, nlsi_job_id);
            used_job_ids_.insert(nlsi_job_id);
            if (!SaveJobIds()) {
                job_ids_.remove(canonical_identity);
                used_job_ids_.remove(nlsi_job_id);
                return false;
            }
        }
    }
    if (nlsi_job_id.isEmpty()) {
        SetError(QStringLiteral("Could not allocate a unique NLSI job ID; all four-digit IDs are in use."));
        return false;
    }
    persisted_details = candidate.details;
    persisted_details.insert(QStringLiteral("nlsi_job_id"), nlsi_job_id);
    const QJsonObject record{
        {QStringLiteral("event_key"), event_key},
        {QStringLiteral("source_event_keys"), QJsonArray{event_key}},
        {QStringLiteral("identity"), canonical_identity},
        {QStringLiteral("nlsi_job_id"), nlsi_job_id},
        {QStringLiteral("cargo"), candidate.cargo},
        {QStringLiteral("source"), candidate.source},
        {QStringLiteral("destination"), candidate.destination},
        {QStringLiteral("status"), status},
        {QStringLiteral("timestamp"), record_timestamp},
        {QStringLiteral("provider"), candidate.provider},
        {QStringLiteral("details"), persisted_details},
    };
    if (matched_existing) {
        const JobRecord previous = snapshot_.jobs[
            static_cast<qsizetype>(std::distance(snapshot_.jobs.begin(), matching_job))];
        if (!UpdateJobRecord(previous, record, {event_key})) {
            return false;
        }
    } else if (!AppendJsonLine(jobs_path_, record)) {
        return false;
    }
    recorded_job_events_.insert(event_key);
    used_job_ids_.insert(canonical_identity);
    used_job_ids_.insert(nlsi_job_id);
    if (matched_existing) {
        *matching_job = candidate;
        matching_job->details = persisted_details;
        matching_job->nlsi_job_id = nlsi_job_id;
    } else {
        snapshot_.jobs.prepend({
            canonical_identity, candidate.cargo, candidate.source, candidate.destination,
            status, record_timestamp, persisted_details, nlsi_job_id, provider});
    }
    ++snapshot_.revision;
    snapshot_.error.clear();
    std::wstring log_error;
    if (!logger_.Log(
            QStringLiteral("[job] %1 %2 cargo=%3")
                .arg(record_timestamp, status, candidate.cargo).toStdWString(),
            &log_error)) {
        SetError(QString::fromStdWString(log_error));
        return false;
    }
    return true;
}

bool HistoryStore::UpdateJobRecord(
    const JobRecord& previous,
    const QJsonObject& replacement,
    const QStringList& event_keys) {
    QFile input(jobs_path_);
    if (!input.exists()) {
        return AppendJsonLine(jobs_path_, replacement);
    }
    if (!input.open(QIODevice::ReadOnly | QIODevice::Text)) {
        SetError(QStringLiteral("Could not read %1 before updating a completed job: %2")
            .arg(jobs_path_, input.errorString()));
        return false;
    }

    QByteArray contents;
    bool replaced = false;
    while (!input.atEnd()) {
        const QByteArray line = input.readLine();
        QJsonObject object;
        QString parse_error;
        const bool valid_record = ParseRecord(
            line, jobs_path_, 0, object, parse_error);
        const bool target_record = valid_record
            && object.value(QStringLiteral("identity")).toString() == previous.identity
            && object.value(QStringLiteral("status")).toString() == previous.status
            && object.value(QStringLiteral("timestamp")).toString() == previous.timestamp;
        if (!target_record) {
            contents += line;
            continue;
        }
        if (replaced) {
            continue;
        }

        QJsonArray recorded_keys =
            object.value(QStringLiteral("source_event_keys")).toArray();
        const QString original_key = object.value(QStringLiteral("event_key")).toString();
        if (!original_key.isEmpty() && !recorded_keys.contains(original_key)) {
            recorded_keys.push_back(original_key);
        }
        for (const QString& event_key : event_keys) {
            if (!event_key.isEmpty() && !recorded_keys.contains(event_key)) {
                recorded_keys.push_back(event_key);
            }
        }
        QJsonObject updated = object;
        for (auto it = replacement.constBegin(); it != replacement.constEnd(); ++it) {
            if (it.key() != QStringLiteral("event_key")
                && it.key() != QStringLiteral("source_event_keys")) {
                updated.insert(it.key(), it.value());
            }
        }
        if (original_key.isEmpty()) {
            updated.insert(QStringLiteral("event_key"),
                replacement.value(QStringLiteral("event_key")));
        }
        updated.insert(QStringLiteral("source_event_keys"), recorded_keys);
        contents += QJsonDocument(updated).toJson(QJsonDocument::Compact) + '\n';
        replaced = true;
    }
    input.close();

    if (!replaced) {
        contents += QJsonDocument(replacement).toJson(QJsonDocument::Compact) + '\n';
    }
    QSaveFile output(jobs_path_);
    if (!output.open(QIODevice::WriteOnly)
        || output.write(contents) != contents.size()
        || !output.commit()) {
        SetError(QStringLiteral("Could not atomically update completed-job history %1: %2")
            .arg(jobs_path_, output.errorString()));
        return false;
    }
    return true;
}

HistorySnapshot HistoryStore::Snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

bool HistoryStore::AppendJsonLine(const QString& path, const QJsonObject& object) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        SetError(QStringLiteral("Could not append to %1: %2").arg(path, file.errorString()));
        return false;
    }
    const QByteArray line = QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
    if (file.write(line) != line.size() || !file.flush()) {
        SetError(QStringLiteral("Could not write %1: %2").arg(path, file.errorString()));
        return false;
    }
    return true;
}

bool HistoryStore::AppendTextLine(const QString& path, const QString& line) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        SetError(QStringLiteral("Could not append to %1: %2").arg(path, file.errorString()));
        return false;
    }
    const QByteArray bytes = line.toUtf8() + '\n';
    if (file.write(bytes) != bytes.size() || !file.flush()) {
        SetError(QStringLiteral("Could not write %1: %2").arg(path, file.errorString()));
        return false;
    }
    return true;
}

void HistoryStore::SetError(const QString& error) {
    snapshot_.error = error;
    ++snapshot_.revision;
    std::wstring log_error;
    logger_.Log((QStringLiteral("[history error] ") + error).toStdWString(), &log_error);
}

} // namespace nlsi::session
