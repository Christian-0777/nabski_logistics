#pragma once

#include <functional>
#include <cstdint>

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

#include "SyncConfiguration.h"

namespace nlsi::sync {

struct DriverKeyStatus {
    bool configured = false;
    bool validation_pending = false;
    bool verified = false;
    int driver_id = 0;
    QString display_name;
    QString website_api_base_url;
    QString error;
};

class DriverKeyClient final : public QObject {
public:
    using Completion = std::function<void(bool, const QString&)>;

    explicit DriverKeyClient(const QString& user_data_directory, QObject* parent = nullptr);

    void Start();
    void Stop();
    void ValidateAndSave(
        const QString& website_api_base_url,
        const QString& driver_key,
        Completion completion);
    bool Clear(QString* error);
    DriverKeyStatus Status() const;
    QString ApiBaseUrl() const;

private:
    void Validate(
        const QString& website_api_base_url,
        const QString& driver_key,
        bool save_on_success,
        Completion completion);
    void SetFailure(const QString& message);

    QString user_data_directory_;
    QNetworkAccessManager network_;
    DriverKeyConfiguration configuration_;
    DriverKeyStatus status_;
    bool request_in_flight_ = false;
    std::uint64_t request_generation_ = 0;
};

} // namespace nlsi::sync
