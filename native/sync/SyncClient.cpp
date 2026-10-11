#include "SyncClient.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QStringList>
#include <QUrl>
#include <QUuid>

#include <cstdint>

#include "logging/TelemetryRecorder.h"

namespace nlsi::sync {
namespace {

constexpr std::size_t kMaximumUploadRecords = 50;
constexpr std::size_t kMaximumRetryDelaySeconds = 15 * 60;

QString UtcNow() {
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

QString SafeErrorText(const QByteArray& body, const QString& fallback) {
    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parse_error);
    if (parse_error.error == QJsonParseError::NoError && document.isObject()) {
        const QString message = document.object().value(QStringLiteral("error"))
            .toString().simplified().left(400);
        if (!message.isEmpty()) {
            return message;
        }
    }
    return fallback.simplified().left(400);
}

} // namespace

SyncClient::SyncClient(
    logging::TelemetryRecorder& recorder,
    const QString& user_data_directory,
    QObject* parent)
    : QObject(parent),
      recorder_(recorder),
      user_data_directory_(user_data_directory),
      network_(this) {
    SyncConfiguration configuration;
    QString configuration_error;
    if (SyncConfigurationStore::Load(
            user_data_directory_, &configuration, &configuration_error)) {
        base_url_ = QUrl(configuration.api_base_url);
        token_ = configuration.device_api_key;
        status_.last_successful_sync = configuration.last_successful_sync;
        status_.configured = SyncConfigurationStore::IsValidApiBaseUrl(configuration.api_base_url)
            && SyncConfigurationStore::IsValidDeviceApiKey(configuration.device_api_key);
        if (!status_.configured
            && (!configuration.api_base_url.isEmpty()
                || !configuration.device_api_key.isEmpty())) {
            status_.error = QStringLiteral(
                    "Saved sync settings are invalid. Enter Your Key in Settings > Connect to configure them again.");
        }
    } else {
        status_.error = configuration_error;
    }

    heartbeat_timer_.setInterval(60 * 1000);
    sync_timer_.setInterval(15 * 60 * 1000);
    retry_timer_.setSingleShot(true);
    connect(&heartbeat_timer_, &QTimer::timeout, this, &SyncClient::CheckDatabase);
    connect(&sync_timer_, &QTimer::timeout, this, &SyncClient::SyncNow);
    connect(&retry_timer_, &QTimer::timeout, this, &SyncClient::SyncNow);
}

void SyncClient::Start() {
    heartbeat_timer_.start();
    sync_timer_.start();
    if (status_.configured) {
        CheckDatabase();
        SyncNow();
    }
}

void SyncClient::Stop() {
    heartbeat_timer_.stop();
    sync_timer_.stop();
    retry_timer_.stop();
}

bool SyncClient::Configure(
    const QString& api_base_url,
    const QString& device_api_key,
    QString* error) {
    SyncConfiguration configuration;
    QString load_error;
    const bool loaded = SyncConfigurationStore::Load(
        user_data_directory_, &configuration, &load_error);
    const QString key = device_api_key.trimmed();
    if (key.isEmpty() && (!loaded || configuration.device_api_key.isEmpty())) {
        if (error) {
            *error = loaded
                ? QStringLiteral("Enter a valid NLSI driver key.")
                : load_error;
        }
        return false;
    }
    if (!api_base_url.trimmed().isEmpty()) {
        configuration.api_base_url = api_base_url.trimmed();
    }
    if (!key.isEmpty()) {
        configuration.device_api_key = key;
    }
    if (!SyncConfigurationStore::IsValidApiBaseUrl(configuration.api_base_url)
        || !SyncConfigurationStore::IsValidDeviceApiKey(configuration.device_api_key)) {
        if (error) {
            *error = QStringLiteral(
                "Enter an HTTPS API base URL and a valid NLSI driver key.");
        }
        return false;
    }
    if (!SyncConfigurationStore::Save(user_data_directory_, configuration, error)) {
        return false;
    }
    base_url_ = QUrl(configuration.api_base_url);
    token_ = configuration.device_api_key;
    status_.configured = true;
    status_.database_connected = false;
    status_.credential_rejected = false;
    credential_valid_ = false;
    status_.error.clear();
    CheckDatabase();
    SyncNow();
    return true;
}

QString SyncClient::ApiBaseUrl() const {
    return base_url_.toString();
}

SyncStatus SyncClient::Status() const {
    SyncStatus result = status_;
    result.pending_records = recorder_.PendingCount();
    result.syncing = status_.syncing;
    return result;
}

QNetworkRequest SyncClient::Request(const QString& endpoint) const {
    QString base = base_url_.toString(QUrl::RemoveQuery | QUrl::RemoveFragment);
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    QNetworkRequest request(QUrl(base + QStringLiteral("/api/") + endpoint));
    request.setRawHeader("Authorization", QByteArray("Bearer ") + token_.toUtf8());
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(15 * 1000);
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::ManualRedirectPolicy);
    return request;
}

void SyncClient::CheckDatabase() {
    if (!status_.configured || status_.credential_rejected || heartbeat_in_flight_) {
        return;
    }
    heartbeat_in_flight_ = true;
    QNetworkReply* reply = network_.get(Request(QStringLiteral("heartbeat.php")));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        heartbeat_in_flight_ = false;
        const QByteArray body = reply->readAll();
        QJsonParseError parse_error;
        const QJsonDocument document = QJsonDocument::fromJson(body, &parse_error);
        const QJsonObject payload = document.isObject() ? document.object() : QJsonObject{};
        const QJsonObject database = payload.value(QStringLiteral("database")).toObject();
        const QJsonObject device = payload.value(QStringLiteral("device")).toObject();
        const int http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool connected = reply->error() == QNetworkReply::NoError
            && parse_error.error == QJsonParseError::NoError
            && payload.value(QStringLiteral("success")).toBool()
            && database.value(QStringLiteral("status")).toString()
                == QStringLiteral("connected")
            && device.value(QStringLiteral("status")).toString()
                == QStringLiteral("active");
        if (connected) {
            status_.database_connected = true;
            status_.credential_rejected = false;
            credential_valid_ = true;
            status_.last_successful_check = payload.value(QStringLiteral("checked_at"))
                .toString(UtcNow());
            status_.error.clear();
            retry_delay_seconds_ = 30;
            SyncNow();
        } else if (http_status == 401 || http_status == 403) {
            status_.database_connected = false;
            credential_valid_ = false;
            status_.credential_rejected = true;
            status_.error = http_status == 403
                ? QStringLiteral("This driver key was revoked. Enter a current key in Settings > Connect.")
                : QStringLiteral("This driver key is invalid. Enter a current key in Settings > Connect.");
        } else {
            status_.database_connected = false;
            credential_valid_ = false;
            const QString fallback = reply->error() == QNetworkReply::NoError
                ? QStringLiteral("The API is temporarily unavailable; local records remain queued.")
                : reply->errorString();
            SetError(QStringLiteral("Connection unavailable: %1")
                .arg(SafeErrorText(body, fallback)), true);
            ScheduleRetry();
        }
        reply->deleteLater();
    });
}

void SyncClient::SyncNow() {
    status_.last_sync_attempt = UtcNow();
    if (!status_.configured || status_.credential_rejected) {
        return;
    }
    if (!credential_valid_) {
        CheckDatabase();
        return;
    }
    if (status_.syncing) {
        return;
    }

    std::wstring local_error;
    const std::vector<QJsonObject> records =
        recorder_.PendingSyncRecords(kMaximumUploadRecords, &local_error);
    if (!local_error.empty()) {
        SetError(QString::fromStdWString(local_error), false);
        ScheduleRetry();
        return;
    }
    if (records.empty()) {
        status_.error.clear();
        status_.pending_records = recorder_.PendingCount();
        return;
    }

    QJsonArray payload_records;
    QStringList expected_ids;
    for (const QJsonObject& record : records) {
        const QString record_id = record.value(QStringLiteral("record_id")).toString();
        if (QUuid(record_id).isNull()) {
            SetError(QStringLiteral("A pending local record has an invalid stable ID."), false);
            ScheduleRetry();
            return;
        }
        expected_ids.append(record_id);
        payload_records.append(record);
    }

    status_.syncing = true;
    QNetworkRequest request = Request(QStringLiteral("ingest.php"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QNetworkReply* reply = network_.post(
        request,
        QJsonDocument(QJsonObject{{QStringLiteral("records"), payload_records}})
            .toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, expected_ids] {
        status_.syncing = false;
        const QByteArray body = reply->readAll();
        QJsonParseError parse_error;
        const QJsonDocument document = QJsonDocument::fromJson(body, &parse_error);
        const QJsonObject payload = document.isObject() ? document.object() : QJsonObject{};
        const int http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QStringList acknowledged_ids;
        QSet<QString> acknowledged_set;
        for (const QJsonValue& value :
             payload.value(QStringLiteral("acknowledged_record_ids")).toArray()) {
            if (!value.isString()) {
                acknowledged_ids.clear();
                acknowledged_set.clear();
                break;
            }
            const QString acknowledged_id = value.toString();
            acknowledged_ids.append(acknowledged_id);
            acknowledged_set.insert(acknowledged_id);
        }
        QSet<QString> expected_set;
        for (const QString& expected_id : expected_ids) {
            expected_set.insert(expected_id);
        }
        const bool committed = reply->error() == QNetworkReply::NoError
            && parse_error.error == QJsonParseError::NoError
            && payload.value(QStringLiteral("success")).toBool()
            && payload.value(QStringLiteral("committed")).toBool()
            && acknowledged_ids.size() == expected_ids.size()
            && acknowledged_set.size() == acknowledged_ids.size()
            && acknowledged_set == expected_set;
        if (!committed) {
            if (http_status == 401 || http_status == 403) {
                status_.database_connected = false;
                credential_valid_ = false;
                status_.credential_rejected = true;
                status_.error = http_status == 403
                    ? QStringLiteral("This driver key was revoked. Enter a current key in Settings > Connect.")
                    : QStringLiteral("This driver key is invalid. Enter a current key in Settings > Connect.");
                reply->deleteLater();
                return;
            }
            const QString fallback = reply->error() == QNetworkReply::NoError
                ? QStringLiteral("The API did not confirm a committed transaction for every record.")
                : reply->errorString();
            SetError(QStringLiteral("Synchronization failed: %1")
                .arg(SafeErrorText(body, fallback)), false);
            ScheduleRetry();
            reply->deleteLater();
            return;
        }

        std::wstring queue_error;
        if (!recorder_.MarkSynchronized(acknowledged_ids, &queue_error)) {
            SetError(QStringLiteral(
                "The server committed the upload, but the local acknowledgement could not be saved: %1")
                .arg(QString::fromStdWString(queue_error)), false);
            ScheduleRetry();
            reply->deleteLater();
            return;
        }
        status_.error.clear();
        status_.last_successful_sync = UtcNow();
        SyncConfiguration configuration{
            base_url_.toString(),
            token_,
            status_.last_successful_sync,
        };
        QString config_error;
        if (!SyncConfigurationStore::Save(
                user_data_directory_, configuration, &config_error)) {
            status_.error = QStringLiteral(
                "Records were synchronized, but the last successful sync time could not be saved locally.");
        }
        retry_delay_seconds_ = 30;
        retry_timer_.stop();
        status_.pending_records = recorder_.PendingCount();
        if (status_.pending_records > 0) {
            SyncNow();
        }
        reply->deleteLater();
    });
}

void SyncClient::SetError(const QString& error, bool disconnected) {
    status_.error = error.simplified().left(512);
    if (disconnected) {
        status_.database_connected = false;
    }
}

void SyncClient::ScheduleRetry() {
    if (retry_timer_.isActive()) {
        return;
    }
    retry_timer_.start(static_cast<int>(retry_delay_seconds_ * 1000));
    retry_delay_seconds_ = qMin(
        retry_delay_seconds_ * 2, kMaximumRetryDelaySeconds);
}

} // namespace nlsi::sync
