#pragma once

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>

#include <cstddef>
#include <cstdint>
#include <functional>

#include "SyncConfiguration.h"

namespace nlsi::logging {
class TelemetryRecorder;
}

namespace nlsi::sync {

struct SyncStatus {
    bool configured = false;
    bool database_connected = false;
    bool credential_rejected = false;
    bool connecting = false;
    bool syncing = false;
    std::uint64_t pending_records = 0;
    QString last_successful_check;
    QString last_successful_sync;
    QString last_sync_attempt;
    QString error;
};

class SyncClient final : public QObject {
public:
    using ConnectionTestCompletion = std::function<void(bool, const QString&)>;

    explicit SyncClient(
        logging::TelemetryRecorder& recorder,
        const QString& user_data_directory,
        QObject* parent = nullptr);

    void Start();
    void Stop();
    void SyncNow();
    void TestConnection(ConnectionTestCompletion completion);
    bool Configure(const QString& api_base_url, const QString& device_api_key, QString* error);
    QString ApiBaseUrl() const;
    SyncStatus Status() const;

private:
    void CheckDatabase();
    void UploadNextBatch();
    void SetError(const QString& error, bool disconnected);
    void ScheduleRetry();
    QNetworkRequest Request(const QString& endpoint) const;

    logging::TelemetryRecorder& recorder_;
    QString user_data_directory_;
    QNetworkAccessManager network_;
    QTimer heartbeat_timer_;
    QTimer sync_timer_;
    QTimer retry_timer_;
    QUrl base_url_;
    QString token_;
    SyncStatus status_;
    std::size_t retry_delay_seconds_ = 30;
    bool heartbeat_in_flight_ = false;
    bool credential_valid_ = false;
};

} // namespace nlsi::sync
