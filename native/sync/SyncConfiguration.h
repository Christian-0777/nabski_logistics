#pragma once

#include <QString>

namespace nlsi::sync {

struct SyncConfiguration {
    QString api_base_url;
    QString device_api_key;
    QString last_successful_sync;
};

struct DriverKeyConfiguration {
    QString website_api_base_url;
    QString driver_key;
};

class SyncConfigurationStore final {
public:
    static bool Load(
        const QString& user_data_directory,
        SyncConfiguration* configuration,
        QString* error = nullptr);
    static bool Save(
        const QString& user_data_directory,
        const SyncConfiguration& configuration,
        QString* error = nullptr);
    static bool IsValidApiBaseUrl(const QString& api_base_url);
    static bool IsValidDeviceApiKey(const QString& device_api_key);
    static bool LoadDriverKey(
        const QString& user_data_directory,
        DriverKeyConfiguration* configuration,
        QString* error = nullptr);
    static bool SaveDriverKey(
        const QString& user_data_directory,
        const DriverKeyConfiguration& configuration,
        QString* error = nullptr);
    static bool ClearDriverKey(const QString& user_data_directory, QString* error = nullptr);
    static bool IsValidDriverKey(const QString& driver_key);
};

} // namespace nlsi::sync
