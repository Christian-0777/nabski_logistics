#pragma once

#include <optional>

#include <QHash>
#include <QMainWindow>
#include <QByteArray>
#include <QString>

#include "DisplayFormatting.h"
#include "GameProcessDetection.h"
#include "JobHistory.h"
#include "TelemetryIPC.h"
#include "UserPreferences.h"

class QLabel;
class QGroupBox;
class QComboBox;
class QLocalServer;
class QLocalSocket;
class QStackedWidget;
class QVBoxLayout;
class QPushButton;
class QTimer;
class QLineEdit;
class QNetworkAccessManager;

namespace nlsi {

class DashboardMainWindow : public QMainWindow
{
public:
    explicit DashboardMainWindow(QWidget* parent = nullptr,
                                 const QString& jobHistoryDirectory = QString(),
                                 const QString& profileFilePath = QString());
    ~DashboardMainWindow() override;

    void applySnapshot(const TelemetrySnapshot& snapshot);

private:
    void buildSidebar();
    void buildContent();
    void showSettings();
    void savePreferences();
    bool saveApiSettingsFromForm();
    void testApiConnection();
    void refreshAccountProfile();
    void requestAccountProfile(bool connectionTest, bool synchronizeJobs = true);
    void requestCurrencyExchangeRate();
    void updateNavigationState(QPushButton* activeButton);
    void startTelemetryListener();
    void handleSocketData(QLocalSocket* socket);
    void updateProcessDetection();
    void setProcessState(GameProcessState state);
    void refreshDisplayedSnapshot();
    void showDashboard();
    void showCompletedJobs();
    void refreshCompletedJobs();
    void showJobDetails(const JobRecord& record);
    void synchronizeAccountJobs(qint64 accountId);
    void fetchServerJobs(qint64 accountId, int offset, bool afterUpload);
    void uploadLocalJobs(qint64 accountId);
    void uploadNextJobBatch(qint64 accountId);
    void finishJobSynchronization(qint64 accountId, const QString& message,
                                  bool successful = true);

    QWidget* sidebar_ = nullptr;
    QWidget* content_ = nullptr;
    QStackedWidget* pageStack_ = nullptr;
    QVBoxLayout* completedJobsLayout_ = nullptr;
    QLabel* completedEmptyLabel_ = nullptr;
    QLabel* headerTitle_ = nullptr;
    QPushButton* dashboardNavButton_ = nullptr;
    QPushButton* completedJobsNavButton_ = nullptr;
    QPushButton* settingsNavButton_ = nullptr;
    QPushButton* preferenceTabButton_ = nullptr;
    QPushButton* configTabButton_ = nullptr;
    QPushButton* profileTabButton_ = nullptr;
    QStackedWidget* settingsTabs_ = nullptr;
    QComboBox* speedPreference_ = nullptr;
    QComboBox* volumePreference_ = nullptr;
    QComboBox* currencyPreference_ = nullptr;
    QComboBox* distancePreference_ = nullptr;
    QPushButton* savePreferencesButton_ = nullptr;
    QLineEdit* apiUrlInput_ = nullptr;
    QLineEdit* apiKeyInput_ = nullptr;
    QLabel* apiConfigStatus_ = nullptr;
    QPushButton* saveApiSettingsButton_ = nullptr;
    QPushButton* testApiConnectionButton_ = nullptr;
    QLabel* profileStatus_ = nullptr;
    QLabel* profileUsernameValue_ = nullptr;
    QLabel* profileDisplayNameValue_ = nullptr;
    QLabel* profileEmailValue_ = nullptr;
    QLabel* profileDriverIdValue_ = nullptr;
    QLabel* profileDriverStatusValue_ = nullptr;
    QLabel* profileRoleValue_ = nullptr;
    QLabel* profileRankValue_ = nullptr;
    QLabel* profileDistanceValue_ = nullptr;
    QPushButton* refreshProfileButton_ = nullptr;
    QNetworkAccessManager* networkManager_ = nullptr;
    QLocalServer* localServer_ = nullptr;
    QLocalSocket* selectedTelemetrySocket_ = nullptr;
    QTimer* telemetryTimer_ = nullptr;
    QTimer* processTimer_ = nullptr;
    QTimer* jobSyncTimer_ = nullptr;
    bool processScanInFlight_ = false;
    GameProcessState processState_ = GameProcessState::NotRunning;
    QHash<QLocalSocket*, QByteArray> receiveBuffers_;
    QHash<QLocalSocket*, TelemetrySnapshot> clientSnapshots_;
    QHash<QLocalSocket*, qint64> lastFrameTimes_;
    TelemetrySnapshot displayedSnapshot_;
    JobHistory jobHistory_;
    ProfileStore profileStore_;
    UserPreferences preferences_;
    ApiSettings apiSettings_;
    QString apiSettingsLoadError_;
    std::optional<double> eurUsdRate_;
    bool connectionRequestInFlight_ = false;
    bool profileRequestInFlight_ = false;
    bool syncRequestInFlight_ = false;
    bool exchangeRateRequestInFlight_ = false;
    bool syncAgainRequested_ = false;
    int unauthenticatedSyncTicks_ = 0;
    int syncRetryDelayMs_ = 60000;
    qint64 authenticatedDriverId_ = 0;
    qint64 syncDriverId_ = 0;
    std::vector<JobRecord> remoteCompletedJobs_;
    std::vector<JobRecord> remoteActiveJobs_;
    std::vector<JobRecord> fetchedServerJobs_;
    std::vector<JobRecord> pendingSyncJobs_;
    std::size_t syncUploadOffset_ = 0;
    bool syncUploadHadFailure_ = false;
    bool syncFetchHadFailure_ = false;
    bool ipcError_ = false;

    QLabel* processLabel_ = nullptr;
    QLabel* connectionLabel_ = nullptr;
    QLabel* dateLabel_ = nullptr;
    QLabel* currentJobValue_ = nullptr;
    QLabel* statusValue_ = nullptr;
    QLabel* cargoValue_ = nullptr;
    QLabel* incomeValue_ = nullptr;
    QLabel* fromValue_ = nullptr;
    QLabel* toValue_ = nullptr;
    QLabel* plannedDistanceValue_ = nullptr;
    QLabel* remainingDistanceValue_ = nullptr;
    QLabel* progressValue_ = nullptr;
    QLabel* arrivalValue_ = nullptr;
    QLabel* fuelValue_ = nullptr;
    QLabel* rpmGearValue_ = nullptr;
    QLabel* throttleValue_ = nullptr;
    QLabel* brakeValue_ = nullptr;
    QLabel* cruiseValue_ = nullptr;
    QLabel* retarderValue_ = nullptr;
    QLabel* gameVersionValue_ = nullptr;
    QGroupBox* gameConfigPanel_ = nullptr;
    QLabel* vehicleValue_ = nullptr;
    QLabel* vehiclePlateValue_ = nullptr;
    QLabel* trailerValue_ = nullptr;
    QLabel* trailerPlateValue_ = nullptr;
    QLabel* summaryValue_ = nullptr;
};

} // namespace nlsi
