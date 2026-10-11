#include "DriverKeyClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <utility>

namespace nlsi::sync {

DriverKeyClient::DriverKeyClient(
    const QString& user_data_directory,
    QObject* parent)
    : QObject(parent),
      user_data_directory_(user_data_directory),
      network_(this) {
    QString error;
    if (!SyncConfigurationStore::LoadDriverKey(
            user_data_directory_, &configuration_, &error)) {
        status_.error = error;
        return;
    }
    status_.configured = !configuration_.driver_key.isEmpty();
    status_.website_api_base_url = configuration_.website_api_base_url;
}

void DriverKeyClient::Start() {
    if (!status_.configured) {
        return;
    }
    status_.validation_pending = true;
    Validate(
        configuration_.website_api_base_url,
        configuration_.driver_key,
        false,
        {});
}

void DriverKeyClient::Stop() {
    ++request_generation_;
    request_in_flight_ = false;
    status_.validation_pending = false;
    const auto replies = network_.findChildren<QNetworkReply*>();
    for (QNetworkReply* reply : replies) {
        reply->abort();
    }
}

void DriverKeyClient::ValidateAndSave(
    const QString& website_api_base_url,
    const QString& driver_key,
    Completion completion) {
    const QString endpoint = website_api_base_url.trimmed();
    const QString key = driver_key.trimmed();
    if (!SyncConfigurationStore::IsValidApiBaseUrl(endpoint)) {
        if (completion) {
            completion(false, QStringLiteral("Enter a valid HTTPS website API URL."));
        }
        return;
    }
    if (!SyncConfigurationStore::IsValidDriverKey(key)) {
        if (completion) {
            completion(false, QStringLiteral("Enter a 12-character alphanumeric driver key."));
        }
        return;
    }
    Validate(endpoint, key, true, std::move(completion));
}

void DriverKeyClient::Validate(
    const QString& website_api_base_url,
    const QString& driver_key,
    bool save_on_success,
    Completion completion) {
    if (request_in_flight_) {
        if (completion) {
            completion(false, QStringLiteral("A driver key validation request is already running."));
        }
        return;
    }

    QString base = website_api_base_url.trimmed();
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    QNetworkRequest request(QUrl(base + QStringLiteral("/api/driver-key.php")));
    request.setHeader(
        QNetworkRequest::ContentTypeHeader,
        QStringLiteral("application/json; charset=utf-8"));
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(15000);
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::ManualRedirectPolicy);
    const QByteArray body = QJsonDocument(QJsonObject{
        {QStringLiteral("key"), driver_key},
    }).toJson(QJsonDocument::Compact);

    request_in_flight_ = true;
    const std::uint64_t generation = ++request_generation_;
    status_.validation_pending = true;
    status_.error.clear();
    QNetworkReply* reply = network_.post(request, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply, website_api_base_url, driver_key,
                                                      save_on_success, generation,
                                                      completion = std::move(completion)]() mutable {
        if (generation != request_generation_) {
            reply->deleteLater();
            return;
        }
        request_in_flight_ = false;
        status_.validation_pending = false;
        const QByteArray response_body = reply->readAll();
        const int http_status = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QJsonParseError parse_error;
        const QJsonDocument response = QJsonDocument::fromJson(response_body, &parse_error);
        const QJsonObject payload = response.isObject() ? response.object() : QJsonObject{};
        const QJsonObject driver = payload.value(QStringLiteral("driver")).toObject();
        const int driver_id = driver.value(QStringLiteral("driver_id")).toInt();
        const QString display_name = driver.value(QStringLiteral("display_name")).toString();
        const bool valid = reply->error() == QNetworkReply::NoError
            && http_status == 200
            && parse_error.error == QJsonParseError::NoError
            && payload.value(QStringLiteral("valid")).toBool()
            && driver_id > 0
            && !display_name.trimmed().isEmpty()
            && display_name.size() <= 100;

        if (!valid) {
            const QString message = http_status == 401
                ? QStringLiteral("This driver key is invalid or has been revoked.")
                : QStringLiteral(
                    "Website validation is unavailable or returned an invalid response. "
                    "Local telemetry remains available.");
            status_.verified = false;
            status_.driver_id = 0;
            status_.display_name.clear();
            status_.error = message;
            if (completion) {
                completion(false, message);
            }
            reply->deleteLater();
            return;
        }

        if (save_on_success) {
            DriverKeyConfiguration candidate{website_api_base_url.trimmed(), driver_key};
            QString save_error;
            if (!SyncConfigurationStore::SaveDriverKey(
                    user_data_directory_, candidate, &save_error)) {
                status_.verified = false;
                status_.driver_id = 0;
                status_.display_name.clear();
                status_.error = save_error;
                if (completion) {
                    completion(false, save_error);
                }
                reply->deleteLater();
                return;
            }
            configuration_ = std::move(candidate);
        }

        status_.configured = true;
        status_.verified = true;
        status_.driver_id = driver_id;
        status_.display_name = display_name;
        status_.website_api_base_url = website_api_base_url.trimmed();
        status_.error.clear();
        if (completion) {
            completion(true, display_name);
        }
        reply->deleteLater();
    });
}

bool DriverKeyClient::Clear(QString* error) {
    Stop();
    if (!SyncConfigurationStore::ClearDriverKey(user_data_directory_, error)) {
        return false;
    }
    configuration_ = {};
    status_ = {};
    return true;
}

DriverKeyStatus DriverKeyClient::Status() const {
    return status_;
}

QString DriverKeyClient::ApiBaseUrl() const {
    return configuration_.website_api_base_url;
}

} // namespace nlsi::sync
