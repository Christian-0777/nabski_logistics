#include "DashboardMainWindow.h"
#include "DisplayFormatting.h"
#include "DashboardTypography.h"
#include "JobReport.h"
#include "MeasurementFormatting.h"

#include <cmath>
#include <limits>

#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QComboBox>
#include <QFileInfo>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QFrame>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLocale>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMetaObject>
#include <QPixmap>
#include <QPushButton>
#include <QMessageBox>
#include <QSizePolicy>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QThreadPool>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <TlHelp32.h>
#endif

namespace nlsi {
namespace {

const QString unavailable = QString::fromUtf8("—");

QString canonicalValueText(const QString& text)
{
    if (text.isEmpty())
    {
        return unavailable;
    }
    const QString trimmed = text.trimmed();
    return trimmed.isEmpty() ? unavailable : trimmed;
}

bool parseDoubleValue(const QString& text, double& value)
{
    if (text.isEmpty())
    {
        return false;
    }
    bool ok = false;
    value = QLocale::c().toDouble(text, &ok);
    if (!ok)
    {
        QString normalized = text;
        normalized.replace(',', '.');
        value = normalized.toDouble(&ok);
    }
    return ok && std::isfinite(value);
}

QLabel* makeValue(QWidget* parent)
{
    auto* label = new QLabel(unavailable, parent);
    label->setObjectName("telemetryValue");
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

QString formattedField(const TelemetrySnapshot& snapshot, const std::string& key)
{
    const auto it = snapshot.values.find(key);
    if (it == snapshot.values.end() || !it->second.available || it->second.value.empty())
    {
        return unavailable;
    }

    QString result = QString::fromStdString(it->second.value);
    if (!it->second.unit.empty())
    {
        result += " " + QString::fromStdString(it->second.unit);
    }
    return result;
}

QString processStateText(GameProcessState state)
{
    switch (state)
    {
    case GameProcessState::NotRunning: return "NOT RUNNING";
    case GameProcessState::Ets2Running: return "ETS2 RUNNING";
    case GameProcessState::AtsRunning: return "ATS RUNNING";
    case GameProcessState::BothRunning: return "BOTH RUNNING — ETS2 / ATS";
    case GameProcessState::DetectionError: return "DETECTION ERROR";
    }
    return "DETECTION ERROR";
}

QString gameDisplayName(const QString& gameId)
{
    if (gameId.compare("eut2", Qt::CaseInsensitive) == 0)
    {
        return "EURO TRUCK SIMULATOR 2";
    }
    if (gameId.compare("ats", Qt::CaseInsensitive) == 0)
    {
        return "AMERICAN TRUCK SIMULATOR";
    }
    return gameId.trimmed().isEmpty() ? unavailable : gameId.toUpper();
}

QString recordValue(const JobRecord& record, const std::string& key)
{
    const auto it = record.snapshot.values.find(key);
    if (it == record.snapshot.values.end() || !it->second.available || it->second.value.empty())
    {
        return unavailable;
    }
    return QString::fromStdString(it->second.value);
}

QString recordText(const JobRecord& record, const std::string& key)
{
    const QString value = recordValue(record, key);
    return uppercaseDisplayText(value);
}

QString recordNumber(const JobRecord& record, const std::string& key,
                     const UserPreferences& preferences, const QString& unit = QString())
{
    const auto it = record.snapshot.values.find(key);
    if (it == record.snapshot.values.end() || !it->second.available)
    {
        return unavailable;
    }
    double value = 0.0;
    if (!parseDoubleValue(QString::fromStdString(it->second.value), value))
    {
        return unavailable;
    }
    const QString sourceUnit = QString::fromStdString(it->second.unit);
    if (key == "job.max_speed_kmh" || key == "job.average_speed_kmh")
    {
        return formatSpeed(value, sourceUnit, preferences);
    }
    if (key == "job.refueled" || key == "job.total_fuel_consumed")
    {
        return formatVolume(value, preferences);
    }
    if (key == "job.cancel_penalty")
    {
        return formatCurrencyAmount(value, sourceUnit, preferences);
    }
    const QString suffix = unit.isEmpty() ? sourceUnit : unit;
    return formatFixedNumber(QString::number(value, 'g', 16), suffix, 2);
}

QString recordDistance(const JobRecord& record, const std::string& key,
                       const UserPreferences& preferences)
{
    const auto it = record.snapshot.values.find(key);
    if (it == record.snapshot.values.end() || !it->second.available)
    {
        return unavailable;
    }
    double value = 0.0;
    if (!parseDoubleValue(QString::fromStdString(it->second.value), value))
    {
        return unavailable;
    }
    return formatDistance(value, QString::fromStdString(it->second.unit), preferences);
}

QString recordCurrency(const JobRecord& record, const UserPreferences& preferences)
{
    double value = 0.0;
    const auto it = record.snapshot.values.find("job.income");
    return it != record.snapshot.values.end() && it->second.available &&
            parseDoubleValue(QString::fromStdString(it->second.value), value)
        ? formatCurrencyAmount(value, QString::fromStdString(it->second.unit), preferences)
        : unavailable;
}

QString recordDamage(const JobRecord& record)
{
    QStringList parts;
    for (const auto& item : {std::pair<std::string, QString>{"job.cargo_damage", "CARGO"},
                             {"job.vehicle_damage", "VEHICLE"}})
    {
        double value = 0.0;
        const QString raw = recordValue(record, item.first);
        if (raw != unavailable && parseDoubleValue(raw, value))
        {
            parts.push_back(QString("%1 %2%").arg(item.second, formatDisplayNumber(value * 100.0, 2)));
        }
    }
    return parts.isEmpty() ? unavailable : parts.join(" / ");
}

QString recordDuration(const JobRecord& record, const std::string& key, double multiplier = 1.0)
{
    double value = 0.0;
    const QString raw = recordValue(record, key);
    return raw != unavailable && parseDoubleValue(raw, value)
        ? formatDurationHhmmss(value * multiplier)
        : unavailable;
}

QString recordLocation(const JobRecord& record, const std::string& cityKey, const std::string& countryKey)
{
    const QString city = recordValue(record, cityKey);
    const QString country = recordValue(record, countryKey);
    if (city == unavailable && country == unavailable)
    {
        return unavailable;
    }
    return uppercaseDisplayText(QString("%1, %2").arg(city, country));
}

QString currentJobCompanyCity(const TelemetrySnapshot& snapshot,
                              const std::string& companyKey,
                              const std::string& cityKey)
{
    const auto availableValue = [&](const std::string& key)
    {
        const auto it = snapshot.values.find(key);
        return it == snapshot.values.end() || !it->second.available
            ? unavailable
            : uppercaseDisplayText(QString::fromStdString(it->second.value));
    };
    const QString company = availableValue(companyKey);
    const QString city = availableValue(cityKey);
    if (company == unavailable)
    {
        return city;
    }
    if (city == unavailable)
    {
        return company;
    }
    return QString("%1 - %2").arg(company, city);
}

QString recordAverageFuelConsumption(const JobRecord& record, const UserPreferences& preferences)
{
    const QString raw = recordValue(record, "truck.fuel.average_consumption");
    double value = 0.0;
    if (raw == unavailable || !parseDoubleValue(raw, value))
    {
        return unavailable;
    }
    const QString unit = QString::fromStdString(
        record.snapshot.values.at("truck.fuel.average_consumption").unit).toLower();
    if (unit == "l/km")
    {
        value *= 100.0;
    }
    if (preferences.volume == VolumeUnit::UsGallons)
    {
        value = litersToUsGallons(value *
            (preferences.distance == DistanceUnit::Miles ? milesToKilometres(1.0) : 1.0));
    }
    else if (preferences.volume == VolumeUnit::ImperialGallons)
    {
        value = litersToImperialGallons(value *
            (preferences.distance == DistanceUnit::Miles ? milesToKilometres(1.0) : 1.0));
    }
    else if (preferences.distance == DistanceUnit::Miles)
    {
        value *= milesToKilometres(1.0);
    }
    return QString("%1 %2/100 %3")
        .arg(formatDisplayNumber(value, 2),
             volumeUnitLabel(preferences.volume),
             distanceUnitLabel(preferences.distance));
}

QString recordSpecialCargo(const JobRecord& record)
{
    const QString value = recordValue(record, "job.special_cargo");
    if (value == unavailable)
    {
        return unavailable;
    }
    return value.compare("true", Qt::CaseInsensitive) == 0 ? "SUPPORTED" : "NOT SPECIAL CARGO";
}

} // namespace

GameProcessState detectGameProcesses()
{
#ifdef Q_OS_WIN
    const HANDLE processSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (processSnapshot == INVALID_HANDLE_VALUE)
    {
        return GameProcessState::DetectionError;
    }

    bool ets2Running = false;
    bool atsRunning = false;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!Process32FirstW(processSnapshot, &entry))
    {
        const DWORD error = GetLastError();
        CloseHandle(processSnapshot);
        return error == ERROR_NO_MORE_FILES
            ? GameProcessState::NotRunning
            : GameProcessState::DetectionError;
    }

    do
    {
        if (_wcsicmp(entry.szExeFile, L"eurotrucks2.exe") == 0)
        {
            ets2Running = true;
        }
        else if (_wcsicmp(entry.szExeFile, L"amtrucks.exe") == 0)
        {
            atsRunning = true;
        }
    } while (Process32NextW(processSnapshot, &entry));

    const DWORD enumerationError = GetLastError();
    CloseHandle(processSnapshot);
    if (enumerationError != ERROR_NO_MORE_FILES)
    {
        return GameProcessState::DetectionError;
    }
    return gameProcessStateFromFlags(ets2Running, atsRunning);
#else
    return GameProcessState::DetectionError;
#endif
}

bool snapshotMatchesProcess(const TelemetrySnapshot& snapshot, GameProcessState state)
{
    if (state == GameProcessState::Ets2Running)
    {
        return snapshot.gameId == "eut2";
    }
    if (state == GameProcessState::AtsRunning)
    {
        return snapshot.gameId == "ats";
    }
    return false;
}

} // namespace nlsi

namespace nlsi {

DashboardMainWindow::DashboardMainWindow(QWidget* parent, const QString& jobHistoryDirectory,
                                         const QString& profileFilePath)
    : QMainWindow(parent),
      jobHistory_(jobHistoryDirectory),
      profileStore_(profileFilePath)
{
    QString profileError;
    const bool profileLoaded = profileStore_.load(preferences_, &profileError);
    setWindowTitle("NLSI Exclusive Logbook 1.7.4 Beta");
    const QString iconPath = QFileInfo(QDir::currentPath(), "resources/nlsi.ico").exists()
        ? QDir::currentPath() + "/resources/nlsi.ico"
        : QCoreApplication::applicationDirPath() + "/resources/nlsi.ico";
    setWindowIcon(QIcon(iconPath));
    resize(900, 600);
    setMinimumSize(900, 600);

    setStyleSheet(QStringLiteral(
        "QWidget { color: #28232A; background: #FFFFFF; }"
        "QLabel { font-weight: 700; }"
        "QWidget#rootWidget { background: #FFFFFF; }"
        "QWidget#sidebar { background: #FFF8FB; border-right: 1px solid #FFD1E6; }"
        "QLabel#sidebarTitle { font: 700 %1px 'Segoe UI'; color: #F896B9; }"
        "QLabel#sidebarSubtitle { font: 700 %2px 'Segoe UI'; color: #7E6672; }"
        "QLabel#sidebarGroup { font: 700 10px 'Segoe UI'; color: #A15E7D; letter-spacing: 1px; padding: 2px 4px; }"
        "QPushButton[sidebarNavButton=\"true\"] { font: 700 13px 'Segoe UI'; color: #665B62; background: transparent; border: 1px solid transparent; border-radius: 5px; padding: 9px 12px; text-align: left; }"
        "QPushButton[sidebarNavButton=\"true\"]:hover { color: #4D3440; background: #FDEBF3; border-color: #F9D6E6; }"
        "QPushButton[sidebarNavButton=\"true\"]:focus { border-color: #F896B9; }"
        "QPushButton[sidebarNavButton=\"true\"]:pressed { color: #4D3440; background: #F5C6DC; border-color: #E9A9C4; }"
        "QPushButton[sidebarNavButton=\"true\"][active=\"true\"] { font-weight: 700; color: #FFFFFF; background: #D94F8C; border: 1px solid #C83E7B; border-left: 4px solid #9F285D; }"
        "QPushButton[sidebarNavButton=\"true\"][active=\"true\"]:hover { color: #FFFFFF; background: #C83E7B; border-color: #B8306D; }"
        "QPushButton[sidebarNavButton=\"true\"][active=\"true\"]:pressed { color: #FFFFFF; background: #A92E67; border-color: #922357; }"
        "QPushButton[settingsTab=\"true\"] { font: 700 12px 'Segoe UI'; color: #665B62; background: #FFF8FB; border: 1px solid #FFD1E6; border-radius: 5px; padding: 8px 16px; }"
        "QPushButton[settingsTab=\"true\"][active=\"true\"] { color: #FFFFFF; background: #D94F8C; border-color: #C83E7B; }"
        "QPushButton#savePreferencesButton { font: 700 12px 'Segoe UI'; color: #FFFFFF; background: #D94F8C; border: 1px solid #C83E7B; border-radius: 5px; padding: 9px 16px; }"
        "QPushButton#savePreferencesButton:hover { background: #C83E7B; }"
        "QPushButton#completedJobCard { text-align: left; background: #FFFFFF; border: 1px solid #FFD1E6; border-radius: 5px; padding: 10px; }"
        "QPushButton#completedJobCard QLabel { font-weight: 700; }"
        "QPushButton#completedJobCard:hover { background: #FFF7FA; }"
        "QLabel#headerTitle { font: 700 %3px 'Segoe UI'; color: #28232A; }"
        "QLabel#headerMode { font: 700 10px 'Segoe UI'; color: #A15E7D; }"
        "QLabel#headerMeta { font: 700 10px 'Segoe UI'; color: #665B62; }"
        "QLabel#processState, QLabel#connectionState { font: 700 10px 'Segoe UI'; color: #665B62; }"
        "QLabel#telemetryLabel { font: 700 %4px 'Segoe UI'; color: #A15E7D; letter-spacing: 0.35px; padding-top: 2px; }"
        "QLabel#telemetryValue { font: 700 %4px 'Segoe UI'; color: #28232A; min-height: 18px; background: #FFFFFF; }"
        "QLabel#gameApiVersionLabel { font: 700 %4px 'Segoe UI'; color: #A15E7D; }"
        "QLabel#gameApiVersionValue { font: 700 %4px 'Segoe UI'; color: #28232A; background: #FFFFFF; }"
        "QGroupBox { font: 700 %5px 'Segoe UI'; color: #28232A; background: #FFFFFF; border: 1px solid #FFD1E6; border-radius: 5px; margin-top: 7px; padding-top: 8px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 9px; padding: 0 3px; }"
        "QScrollArea { background: #FFFFFF; border: none; }"
        "QScrollArea#dashboardScroll QScrollBar:vertical { width: 8px; background: #FFF7FA; border: none; margin: 0; }"
        "QScrollArea#dashboardScroll QScrollBar::handle:vertical { min-height: 28px; background: #E9A9C4; border: none; border-radius: 4px; }"
        "QScrollArea#dashboardScroll QScrollBar::handle:vertical:hover { background: #D982AA; }"
        "QScrollArea#dashboardScroll QScrollBar::handle:vertical:pressed { background: #C85F91; }"
        "QScrollArea#dashboardScroll QScrollBar::add-line:vertical, QScrollArea#dashboardScroll QScrollBar::sub-line:vertical { height: 0; border: none; background: none; }"
        "QScrollArea#dashboardScroll QScrollBar::add-page:vertical, QScrollArea#dashboardScroll QScrollBar::sub-page:vertical { background: transparent; }"
        "QWidget#contentWidget, QScrollArea > QWidget, QAbstractScrollArea { background: #FFFFFF; }"
    ).arg(typography::BrandName)
        .arg(typography::BrandSubtitle)
        .arg(typography::HeaderTitle)
        .arg(typography::Field)
        .arg(typography::CardTitle));

    auto* root = new QWidget(this);
    root->setObjectName("rootWidget");
    auto* outerLayout = new QHBoxLayout(root);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    sidebar_ = new QWidget(root);
    sidebar_->setObjectName("sidebar");
    sidebar_->setFixedWidth(280);
    buildSidebar();
    outerLayout->addWidget(sidebar_);

    content_ = new QWidget(root);
    buildContent();
    outerLayout->addWidget(content_, 1);
    setCentralWidget(root);

    auto* statusBar = new QStatusBar(this);
    statusBar->setSizeGripEnabled(false);
    statusBar->setStyleSheet(
        "QStatusBar { background: #FFF7FA; border-top: 1px solid #FFD1E6; color: #665B62; font: 700 10px 'Segoe UI'; padding: 0 8px; }"
    );
    statusBar->showMessage("READ-ONLY LOCAL TELEMETRY • VERSION 1.7.4 BETA");
    setStatusBar(statusBar);
    if (!jobHistory_.errorString().isEmpty())
    {
        statusBar->showMessage(uppercaseDisplayText("JOB HISTORY WARNING: " + jobHistory_.errorString()));
    }
    if (!profileLoaded)
    {
        statusBar->showMessage(uppercaseDisplayText("PREFERENCES WARNING: " + profileError));
    }
    refreshCompletedJobs();

    startTelemetryListener();

    processTimer_ = new QTimer(this);
    processTimer_->setInterval(1000);
    connect(processTimer_, &QTimer::timeout, this, &DashboardMainWindow::updateProcessDetection);
    processTimer_->start();
    updateProcessDetection();

    telemetryTimer_ = new QTimer(this);
    telemetryTimer_->setInterval(500);
    connect(telemetryTimer_, &QTimer::timeout, this, &DashboardMainWindow::refreshDisplayedSnapshot);
    telemetryTimer_->start();
    refreshDisplayedSnapshot();
}

DashboardMainWindow::~DashboardMainWindow() = default;

void DashboardMainWindow::updateProcessDetection()
{
    if (processScanInFlight_)
    {
        return;
    }
    processScanInFlight_ = true;
    auto* watcher = new QFutureWatcher<GameProcessState>(this);
    connect(watcher, &QFutureWatcher<GameProcessState>::finished, this, [this, watcher]()
    {
        processScanInFlight_ = false;
        setProcessState(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run(detectGameProcesses));
}

void DashboardMainWindow::buildSidebar()
{
    auto* layout = new QVBoxLayout(sidebar_);
    layout->setContentsMargins(12, 16, 12, 12);
    layout->setSpacing(9);

    auto* brandRow = new QWidget(sidebar_);
    auto* brandLayout = new QHBoxLayout(brandRow);
    brandLayout->setContentsMargins(0, 0, 0, 0);
    brandLayout->setSpacing(10);
    const QString logoPath = QFileInfo(QDir::currentPath(), "resources/nlsi.ico").exists()
        ? QDir::currentPath() + "/resources/nlsi.ico"
        : QCoreApplication::applicationDirPath() + "/resources/nlsi.ico";
    auto* logoLabel = new QLabel(brandRow);
    logoLabel->setPixmap(QPixmap(logoPath).scaled(38, 38, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logoLabel->setAlignment(Qt::AlignCenter);
    auto* brandText = new QWidget(brandRow);
    auto* brandTextLayout = new QVBoxLayout(brandText);
    brandTextLayout->setContentsMargins(0, 0, 0, 0);
    brandTextLayout->setSpacing(0);
    auto* brandName = new QLabel("NABSKI", brandText);
    brandName->setObjectName("sidebarTitle");
    auto* brandSubtitle = new QLabel("LOGISTICS SOLUTIONS INC.", brandText);
    brandSubtitle->setObjectName("sidebarSubtitle");
    brandSubtitle->setWordWrap(false);
    brandSubtitle->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    brandTextLayout->addWidget(brandName);
    brandTextLayout->addWidget(brandSubtitle);
    brandLayout->addWidget(logoLabel);
    brandLayout->addWidget(brandText, 1);
    layout->addWidget(brandRow);
    auto* divider = new QFrame(sidebar_);
    divider->setFrameShape(QFrame::HLine);
    divider->setFrameShadow(QFrame::Plain);
    divider->setStyleSheet("QFrame { color: #FFD1E6; background: #FFD1E6; max-height: 1px; }");
    layout->addWidget(divider);
    layout->addSpacing(14);

    auto* workspace = new QLabel("WORKSPACE", sidebar_);
    workspace->setObjectName("sidebarGroup");
    layout->addWidget(workspace);
    dashboardNavButton_ = new QPushButton("DASHBOARD", sidebar_);
    dashboardNavButton_->setObjectName("dashboardNavButton");
    dashboardNavButton_->setProperty("sidebarNavButton", true);
    dashboardNavButton_->setProperty("active", true);
    dashboardNavButton_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    dashboardNavButton_->setFixedHeight(38);
    connect(dashboardNavButton_, &QPushButton::clicked, this, &DashboardMainWindow::showDashboard);
    layout->addWidget(dashboardNavButton_);
    completedJobsNavButton_ = new QPushButton("COMPLETED JOBS", sidebar_);
    completedJobsNavButton_->setObjectName("completedJobsNavButton");
    completedJobsNavButton_->setProperty("sidebarNavButton", true);
    completedJobsNavButton_->setProperty("active", false);
    completedJobsNavButton_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    completedJobsNavButton_->setFixedHeight(38);
    connect(completedJobsNavButton_, &QPushButton::clicked, this, &DashboardMainWindow::showCompletedJobs);
    layout->addWidget(completedJobsNavButton_);
    settingsNavButton_ = new QPushButton("SETTINGS", sidebar_);
    settingsNavButton_->setObjectName("settingsNavButton");
    settingsNavButton_->setProperty("sidebarNavButton", true);
    settingsNavButton_->setProperty("active", false);
    settingsNavButton_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    settingsNavButton_->setFixedHeight(38);
    connect(settingsNavButton_, &QPushButton::clicked, this, &DashboardMainWindow::showSettings);
    layout->addWidget(settingsNavButton_);
    layout->addStretch();
    auto* footerDivider = new QFrame(sidebar_);
    footerDivider->setFrameShape(QFrame::HLine);
    footerDivider->setFrameShadow(QFrame::Plain);
    footerDivider->setStyleSheet("QFrame { color: #FFD1E6; background: #FFD1E6; max-height: 1px; }");
    layout->addWidget(footerDivider);
    auto* version = new QLabel("1.7.4 BETA", sidebar_);
    version->setObjectName("sidebarSubtitle");
    layout->addWidget(version);
}

void DashboardMainWindow::buildContent()
{
    auto* rootLayout = new QVBoxLayout(content_);
    rootLayout->setContentsMargins(12, 10, 12, 8);
    rootLayout->setSpacing(8);

    auto* header = new QWidget(content_);
    header->setStyleSheet("background: #FFFFFF; border: 1px solid #FFD1E6; border-radius: 5px;");
    auto* headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(9, 7, 9, 7);
    headerLayout->setSpacing(5);
    auto* titleRow = new QHBoxLayout();
    titleRow->setSpacing(8);
    headerTitle_ = new QLabel("DASHBOARD", header);
    headerTitle_->setObjectName("headerTitle");
    auto* mode = new QLabel("LOCAL MODE", header);
    mode->setObjectName("headerMode");
    dateLabel_ = new QLabel(header);
    dateLabel_->setObjectName("headerMeta");
    dateLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    titleRow->addWidget(headerTitle_);
    titleRow->addWidget(mode);
    titleRow->addStretch();
    titleRow->addWidget(dateLabel_);
    headerLayout->addLayout(titleRow);

    processLabel_ = new QLabel(header);
    processLabel_->setObjectName("processState");
    processLabel_->hide();
    headerLayout->addWidget(processLabel_);
    connectionLabel_ = new QLabel(header);
    connectionLabel_->setObjectName("connectionState");
    connectionLabel_->hide();
    headerLayout->addWidget(connectionLabel_);
    rootLayout->addWidget(header);

    auto* scroll = new QScrollArea(content_);
    scroll->setObjectName("dashboardScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    auto* scrollContent = new QWidget(scroll);
    auto* sections = new QVBoxLayout(scrollContent);
    sections->setContentsMargins(0, 0, 0, 2);
    sections->setSpacing(8);

    auto addField = [](QGridLayout* grid, QWidget* parent, const QString& name, int row, int column)
    {
        auto* cell = new QWidget(parent);
        auto* cellLayout = new QVBoxLayout(cell);
        cellLayout->setContentsMargins(0, 0, 0, 0);
        cellLayout->setSpacing(1);
        auto* label = new QLabel(name, cell);
        label->setObjectName("telemetryLabel");
        label->setWordWrap(true);
        cellLayout->addWidget(label);
        auto* value = makeValue(cell);
        cellLayout->addWidget(value);
        grid->addWidget(cell, row, column, 1, 2);
        return value;
    };

    auto configureGrid = [](QGridLayout* grid)
    {
        grid->setContentsMargins(9, 13, 9, 8);
        grid->setHorizontalSpacing(14);
        grid->setVerticalSpacing(5);
        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 1);
        grid->setColumnStretch(2, 1);
        grid->setColumnStretch(3, 1);
    };

    auto* job = new QGroupBox("CURRENT JOB", scrollContent);
    auto* jobGrid = new QGridLayout(job);
    configureGrid(jobGrid);
    currentJobValue_ = addField(jobGrid, job, "JOB ID", 0, 0);
    currentJobValue_->setObjectName("currentJobValue");
    statusValue_ = addField(jobGrid, job, "STATUS", 0, 2);
    statusValue_->setObjectName("statusValue");
    cargoValue_ = addField(jobGrid, job, "CARGO", 1, 0);
    incomeValue_ = addField(jobGrid, job, "INCOME", 1, 2);
    incomeValue_->setObjectName("incomeValue");
    fromValue_ = addField(jobGrid, job, "FROM", 2, 0);
    fromValue_->setObjectName("fromValue");
    toValue_ = addField(jobGrid, job, "TO", 2, 2);
    toValue_->setObjectName("toValue");
    plannedDistanceValue_ = addField(jobGrid, job, "PLANNED DISTANCE", 3, 0);
    plannedDistanceValue_->setObjectName("plannedDistanceValue");
    remainingDistanceValue_ = addField(jobGrid, job, "REMAINING DISTANCE", 3, 2);
    remainingDistanceValue_->setObjectName("remainingDistanceValue");
    auto* progressCell = new QWidget(job);
    auto* progressCellLayout = new QVBoxLayout(progressCell);
    progressCellLayout->setContentsMargins(0, 0, 0, 0);
    progressCellLayout->setSpacing(1);
    auto* progressLabel = new QLabel("PROGRESS", progressCell);
    progressLabel->setObjectName("telemetryLabel");
    progressCellLayout->addWidget(progressLabel);
    progressValue_ = makeValue(progressCell);
    progressValue_->setText(unavailable);
    progressCellLayout->addWidget(progressValue_);
    jobGrid->addWidget(progressCell, 4, 0, 1, 2);
    arrivalValue_ = addField(jobGrid, job, "ARRIVAL", 4, 2);
    sections->addWidget(job);

    auto* driving = new QGroupBox("DRIVING", scrollContent);
    auto* drivingGrid = new QGridLayout(driving);
    configureGrid(drivingGrid);
    fuelValue_ = addField(drivingGrid, driving, "FUEL", 0, 0);
    rpmGearValue_ = addField(drivingGrid, driving, "RPM / GEAR", 0, 2);
    throttleValue_ = addField(drivingGrid, driving, "THROTTLE", 1, 0);
    brakeValue_ = addField(drivingGrid, driving, "BRAKE", 1, 2);
    cruiseValue_ = addField(drivingGrid, driving, "CRUISE CONTROL", 2, 0);
    retarderValue_ = addField(drivingGrid, driving, "RETARDER", 2, 2);
    fuelValue_->setObjectName("fuelValue");
    rpmGearValue_->setObjectName("rpmGearValue");
    throttleValue_->setObjectName("throttleValue");
    brakeValue_->setObjectName("brakeValue");
    cruiseValue_->setObjectName("cruiseValue");
    retarderValue_->setObjectName("retarderValue");
    arrivalValue_->setObjectName("arrivalValue");
    sections->addWidget(driving);

    gameConfigPanel_ = new QGroupBox("GAME CONFIG | —", scrollContent);
    auto* configLayout = new QVBoxLayout(gameConfigPanel_);
    configLayout->setContentsMargins(9, 13, 9, 8);
    configLayout->setSpacing(5);
    auto* apiVersionRow = new QHBoxLayout();
    apiVersionRow->setContentsMargins(0, 0, 0, 0);
    auto* apiVersionLabel = new QLabel("GAME API VERSION", gameConfigPanel_);
    apiVersionLabel->setObjectName("gameApiVersionLabel");
    gameVersionValue_ = makeValue(gameConfigPanel_);
    gameVersionValue_->setObjectName("gameApiVersionValue");
    apiVersionRow->addWidget(apiVersionLabel);
    apiVersionRow->addWidget(gameVersionValue_, 1);
    configLayout->addLayout(apiVersionRow);
    auto* configGrid = new QGridLayout();
    configureGrid(configGrid);
    vehicleValue_ = addField(configGrid, gameConfigPanel_, "VEHICLE", 0, 0);
    vehiclePlateValue_ = addField(configGrid, gameConfigPanel_, "VEHICLE PLATE", 0, 2);
    trailerValue_ = addField(configGrid, gameConfigPanel_, "TRAILER", 1, 0);
    trailerPlateValue_ = addField(configGrid, gameConfigPanel_, "TRAILER PLATE", 1, 2);
    configLayout->addLayout(configGrid);
    sections->addWidget(gameConfigPanel_);

    auto* summary = new QGroupBox("SUMMARY", scrollContent);
    auto* summaryLayout = new QVBoxLayout(summary);
    summaryLayout->setContentsMargins(9, 13, 9, 8);
    summaryValue_ = makeValue(summary);
    summaryValue_->setText("NO EVENTS AVAILABLE.");
    summaryValue_->setStyleSheet("color: #665B62;");
    summaryLayout->addWidget(summaryValue_);
    sections->addWidget(summary);
    sections->addStretch();

    scroll->setWidget(scrollContent);
    pageStack_ = new QStackedWidget(content_);
    pageStack_->setObjectName("mainPages");
    pageStack_->addWidget(scroll);

    auto* completedPage = new QWidget(pageStack_);
    auto* completedPageLayout = new QVBoxLayout(completedPage);
    completedPageLayout->setContentsMargins(0, 0, 0, 0);
    auto* completedScroll = new QScrollArea(completedPage);
    completedScroll->setObjectName("completedJobsScroll");
    completedScroll->setWidgetResizable(true);
    completedScroll->setFrameShape(QFrame::NoFrame);
    auto* completedContent = new QWidget(completedScroll);
    completedJobsLayout_ = new QVBoxLayout(completedContent);
    completedJobsLayout_->setContentsMargins(0, 0, 0, 2);
    completedJobsLayout_->setSpacing(8);
    completedEmptyLabel_ = new QLabel("NO COMPLETED OR CANCELLED JOBS YET.", completedContent);
    completedEmptyLabel_->setObjectName("completedJobsEmpty");
    completedEmptyLabel_->setWordWrap(true);
    completedEmptyLabel_->setAlignment(Qt::AlignCenter);
    completedEmptyLabel_->setStyleSheet("color: #665B62; padding: 24px;");
    completedJobsLayout_->addWidget(completedEmptyLabel_);
    completedJobsLayout_->addStretch();
    completedScroll->setWidget(completedContent);
    completedPageLayout->addWidget(completedScroll);
    pageStack_->addWidget(completedPage);

    auto* settingsPage = new QWidget(pageStack_);
    auto* settingsLayout = new QVBoxLayout(settingsPage);
    settingsLayout->setContentsMargins(0, 0, 0, 0);
    settingsLayout->setSpacing(8);
    auto* tabRow = new QHBoxLayout();
    tabRow->setSpacing(6);
    preferenceTabButton_ = new QPushButton("PREFERENCE", settingsPage);
    preferenceTabButton_->setObjectName("preferenceTabButton");
    preferenceTabButton_->setProperty("settingsTab", true);
    preferenceTabButton_->setProperty("active", true);
    profileTabButton_ = new QPushButton("PROFILE", settingsPage);
    profileTabButton_->setObjectName("profileTabButton");
    profileTabButton_->setProperty("settingsTab", true);
    profileTabButton_->setProperty("active", false);
    tabRow->addWidget(preferenceTabButton_);
    tabRow->addWidget(profileTabButton_);
    tabRow->addStretch();
    settingsLayout->addLayout(tabRow);

    settingsTabs_ = new QStackedWidget(settingsPage);
    settingsTabs_->setObjectName("settingsTabs");
    auto* preferencePage = new QWidget(settingsTabs_);
    auto* preferencePageLayout = new QVBoxLayout(preferencePage);
    preferencePageLayout->setContentsMargins(0, 0, 0, 0);
    auto* preferencePanel = new QGroupBox("MEASUREMENT PREFERENCES", preferencePage);
    auto* preferenceForm = new QFormLayout(preferencePanel);
    preferenceForm->setContentsMargins(14, 16, 14, 12);
    preferenceForm->setHorizontalSpacing(18);
    preferenceForm->setVerticalSpacing(12);

    auto makePreference = [&](const QString& label, const QString& name)
    {
        auto* combo = new QComboBox(preferencePanel);
        combo->setObjectName(name);
        combo->setMinimumWidth(220);
        auto* formLabel = new QLabel(label, preferencePanel);
        formLabel->setObjectName("telemetryLabel");
        preferenceForm->addRow(formLabel, combo);
        return combo;
    };
    speedPreference_ = makePreference("SPEED", "speedPreference");
    speedPreference_->addItem("Kilometers per hour (km/h)",
                              static_cast<int>(SpeedUnit::KilometresPerHour));
    speedPreference_->addItem("Miles per hour (mph)",
                              static_cast<int>(SpeedUnit::MilesPerHour));
    volumePreference_ = makePreference("VOLUME", "volumePreference");
    volumePreference_->addItem("Liters (L)", static_cast<int>(VolumeUnit::Liters));
    volumePreference_->addItem("US gallons (US gal)", static_cast<int>(VolumeUnit::UsGallons));
    volumePreference_->addItem("Imperial gallons (Imp gal)",
                               static_cast<int>(VolumeUnit::ImperialGallons));
    currencyPreference_ = makePreference("CURRENCY", "currencyPreference");
    currencyPreference_->addItem("EUR (€)", static_cast<int>(CurrencyUnit::Euro));
    currencyPreference_->addItem("USD ($)", static_cast<int>(CurrencyUnit::UsDollar));
    distancePreference_ = makePreference("DISTANCE", "distancePreference");
    distancePreference_->addItem("Kilometers (km)", static_cast<int>(DistanceUnit::Kilometres));
    distancePreference_->addItem("Miles (mi)", static_cast<int>(DistanceUnit::Miles));

    auto* preferenceActions = new QHBoxLayout();
    preferenceActions->addStretch();
    savePreferencesButton_ = new QPushButton("SAVE PREFERENCES", preferencePage);
    savePreferencesButton_->setObjectName("savePreferencesButton");
    preferenceActions->addWidget(savePreferencesButton_);
    preferencePageLayout->addWidget(preferencePanel);
    preferencePageLayout->addLayout(preferenceActions);
    preferencePageLayout->addStretch();
    settingsTabs_->addWidget(preferencePage);

    auto* profilePage = new QWidget(settingsTabs_);
    auto* profileLayout = new QVBoxLayout(profilePage);
    profileLayout->setContentsMargins(0, 0, 0, 0);
    auto* profilePanel = new QGroupBox("PROFILE", profilePage);
    auto* profilePanelLayout = new QVBoxLayout(profilePanel);
    profilePanelLayout->setContentsMargins(18, 24, 18, 24);
    profilePanelLayout->setAlignment(Qt::AlignCenter);
    auto* comingSoon = new QLabel("COMING SOON", profilePanel);
    comingSoon->setObjectName("profileComingSoon");
    comingSoon->setAlignment(Qt::AlignCenter);
    comingSoon->setStyleSheet("font: 700 20px 'Segoe UI'; color: #A15E7D;");
    auto* profileDescription = new QLabel(
        "Profile management will be available in a future release.", profilePanel);
    profileDescription->setObjectName("profileComingSoonDescription");
    profileDescription->setAlignment(Qt::AlignCenter);
    profileDescription->setStyleSheet("color: #665B62; padding-top: 6px;");
    profilePanelLayout->addWidget(comingSoon);
    profilePanelLayout->addWidget(profileDescription);
    profileLayout->addWidget(profilePanel);
    profileLayout->addStretch();
    settingsTabs_->addWidget(profilePage);
    settingsLayout->addWidget(settingsTabs_, 1);
    pageStack_->addWidget(settingsPage);

    connect(preferenceTabButton_, &QPushButton::clicked, this, [this]()
    {
        settingsTabs_->setCurrentIndex(0);
        preferenceTabButton_->setProperty("active", true);
        profileTabButton_->setProperty("active", false);
        preferenceTabButton_->style()->unpolish(preferenceTabButton_);
        preferenceTabButton_->style()->polish(preferenceTabButton_);
        profileTabButton_->style()->unpolish(profileTabButton_);
        profileTabButton_->style()->polish(profileTabButton_);
    });
    connect(profileTabButton_, &QPushButton::clicked, this, [this]()
    {
        settingsTabs_->setCurrentIndex(1);
        preferenceTabButton_->setProperty("active", false);
        profileTabButton_->setProperty("active", true);
        preferenceTabButton_->style()->unpolish(preferenceTabButton_);
        preferenceTabButton_->style()->polish(preferenceTabButton_);
        profileTabButton_->style()->unpolish(profileTabButton_);
        profileTabButton_->style()->polish(profileTabButton_);
    });
    connect(savePreferencesButton_, &QPushButton::clicked,
            this, &DashboardMainWindow::savePreferences);
    rootLayout->addWidget(pageStack_, 1);
}

void DashboardMainWindow::showDashboard()
{
    pageStack_->setCurrentIndex(0);
    headerTitle_->setText("DASHBOARD");
    updateNavigationState(dashboardNavButton_);
}

void DashboardMainWindow::showCompletedJobs()
{
    pageStack_->setCurrentIndex(1);
    headerTitle_->setText("COMPLETED JOBS");
    updateNavigationState(completedJobsNavButton_);
    refreshCompletedJobs();
}

void DashboardMainWindow::showSettings()
{
    pageStack_->setCurrentIndex(2);
    headerTitle_->setText("SETTINGS");
    updateNavigationState(settingsNavButton_);
    speedPreference_->setCurrentIndex(speedPreference_->findData(static_cast<int>(preferences_.speed)));
    volumePreference_->setCurrentIndex(volumePreference_->findData(static_cast<int>(preferences_.volume)));
    currencyPreference_->setCurrentIndex(currencyPreference_->findData(static_cast<int>(preferences_.currency)));
    distancePreference_->setCurrentIndex(distancePreference_->findData(static_cast<int>(preferences_.distance)));
}

void DashboardMainWindow::updateNavigationState(QPushButton* activeButton)
{
    const QList<QPushButton*> buttons = {
        dashboardNavButton_, completedJobsNavButton_, settingsNavButton_
    };
    for (QPushButton* button : buttons)
    {
        if (!button)
        {
            continue;
        }
        button->setProperty("active", button == activeButton);
        button->style()->unpolish(button);
        button->style()->polish(button);
    }
}

void DashboardMainWindow::savePreferences()
{
    UserPreferences pending;
    pending.speed = static_cast<SpeedUnit>(speedPreference_->currentData().toInt());
    pending.volume = static_cast<VolumeUnit>(volumePreference_->currentData().toInt());
    pending.currency = static_cast<CurrencyUnit>(currencyPreference_->currentData().toInt());
    pending.distance = static_cast<DistanceUnit>(distancePreference_->currentData().toInt());

    QString error;
    if (!isValidPreferences(pending) || !profileStore_.save(pending, &error))
    {
        const QString message = error.isEmpty()
            ? "Preferences contain an unsupported setting value."
            : error;
        statusBar()->showMessage(uppercaseDisplayText("PREFERENCES ERROR: " + message));
        return;
    }
    preferences_ = pending;
    refreshDisplayedSnapshot();
    refreshCompletedJobs();
    statusBar()->showMessage("PREFERENCES SAVED.");
}

void DashboardMainWindow::refreshCompletedJobs()
{
    while (completedJobsLayout_ && completedJobsLayout_->count() > 0)
    {
        QLayoutItem* item = completedJobsLayout_->takeAt(0);
        if (item->widget())
        {
            delete item->widget();
        }
        delete item;
    }

    QString loadError;
    const std::vector<JobRecord> records = jobHistory_.completedJobs(&loadError);
    if (!loadError.isEmpty())
    {
        qWarning().noquote() << "NLSI job history:" << loadError;
        if (statusBar())
        {
            statusBar()->showMessage(uppercaseDisplayText("JOB HISTORY WARNING: " + loadError));
        }
    }

    if (records.empty())
    {
        completedEmptyLabel_ = new QLabel("NO COMPLETED OR CANCELLED JOBS YET.", pageStack_);
        completedEmptyLabel_->setObjectName("completedJobsEmpty");
        completedEmptyLabel_->setWordWrap(true);
        completedEmptyLabel_->setAlignment(Qt::AlignCenter);
        completedEmptyLabel_->setStyleSheet("color: #665B62; padding: 24px;");
        completedJobsLayout_->addWidget(completedEmptyLabel_);
        completedJobsLayout_->addStretch();
        return;
    }

    completedEmptyLabel_ = nullptr;
    for (const JobRecord& record : records)
    {
        auto* card = new QPushButton();
        card->setObjectName("completedJobCard");
        card->setProperty("jobId", QString::fromStdString(record.id));
        card->setCursor(Qt::PointingHandCursor);
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(8, 7, 8, 7);
        cardLayout->setSpacing(3);
        auto* heading = new QHBoxLayout();
        auto* id = new QLabel(uppercaseDisplayText(QString::fromStdString(record.id)), card);
        id->setAttribute(Qt::WA_TransparentForMouseEvents);
        id->setStyleSheet("font: 700 18px 'Segoe UI'; color: #28232A;");
        auto* state = new QLabel(QString::fromStdString(jobStatusToString(record.status)), card);
        state->setAttribute(Qt::WA_TransparentForMouseEvents);
        state->setStyleSheet(record.status == JobStatus::Cancelled
            ? "font: 700 10px 'Segoe UI'; color: #B3261E;"
            : "font: 700 10px 'Segoe UI'; color: #287A45;");
        heading->addWidget(id);
        heading->addStretch();
        heading->addWidget(state);
        cardLayout->addLayout(heading);

        auto* cargo = new QLabel("CARGO  " + recordText(record, "job.cargo"), card);
        cargo->setAttribute(Qt::WA_TransparentForMouseEvents);
        cargo->setWordWrap(true);
        cardLayout->addWidget(cargo);
        const QString origin = recordLocation(record, "job.source_city", "job.source_country");
        const QString destination = recordLocation(record, "job.destination_city", "job.destination_country");
        auto* route = new QLabel("FROM  " + origin + "     TO  " + destination, card);
        route->setAttribute(Qt::WA_TransparentForMouseEvents);
        route->setWordWrap(true);
        cardLayout->addWidget(route);
        auto* footer = new QLabel(
            "INCOME  " + recordCurrency(record, preferences_) + "     COMPLETED  " +
                (record.completedAt.isValid()
                    ? record.completedAt.toLocalTime().toString("yyyy-MM-dd  hh:mm")
                    : unavailable),
            card);
        footer->setAttribute(Qt::WA_TransparentForMouseEvents);
        footer->setWordWrap(true);
        footer->setStyleSheet("color: #665B62; font-size: 10px; font-weight: 700;");
        cardLayout->addWidget(footer);

        connect(card, &QPushButton::clicked, this, [this, record]()
        {
            showJobDetails(record);
        });
        completedJobsLayout_->addWidget(card);
    }
    completedJobsLayout_->addStretch();
}

void DashboardMainWindow::showJobDetails(const JobRecord& record)
{
    QDialog dialog(this);
    dialog.setWindowTitle(QString("%1 — %2")
        .arg(QString::fromStdString(record.id),
             QString::fromStdString(jobStatusToString(record.status))));
    dialog.setMinimumSize(720, 500);
    auto* dialogLayout = new QVBoxLayout(&dialog);
    auto* scroll = new QScrollArea(&dialog);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget(scroll);
    auto* sections = new QVBoxLayout(content);
    sections->setContentsMargins(3, 3, 3, 3);
    sections->setSpacing(8);

    QString reportHtml =
        "<html><head><style>"
        "body{font-family:'Segoe UI',sans-serif;color:#28232A;}"
        "h1{color:#D94F8C;font-size:18pt;}h2{font-size:12pt;margin-top:16px;}"
        "table{width:100%;border-collapse:collapse;}td{padding:5px;border-bottom:1px solid #FFD1E6;}"
        "td:first-child{font-weight:bold;color:#A15E7D;width:38%;}"
        "</style></head><body><h1>NLSI EXCLUSIVE LOGBOOK</h1><p>" +
        QString::fromStdString(record.id).toHtmlEscaped() + " — " +
        QString::fromStdString(jobStatusToString(record.status)).toHtmlEscaped() +
        "</p>";
    const auto addSection = [&](const QString& title, const std::vector<std::pair<QString, QString>>& fields)
    {
        reportHtml += "<h2>" + title.toHtmlEscaped() + "</h2><table>";
        for (const auto& field : fields)
        {
            reportHtml += "<tr><td>" + field.first.toHtmlEscaped() + "</td><td>" +
                field.second.toHtmlEscaped() + "</td></tr>";
        }
        reportHtml += "</table>";
        auto* group = new QGroupBox(title, content);
        auto* grid = new QGridLayout(group);
        grid->setContentsMargins(9, 13, 9, 8);
        grid->setHorizontalSpacing(16);
        grid->setVerticalSpacing(6);
        for (std::size_t index = 0; index < fields.size(); index += 2)
        {
            for (std::size_t column = 0; column < 2 && index + column < fields.size(); ++column)
            {
                auto* cell = new QWidget(group);
                auto* cellLayout = new QVBoxLayout(cell);
                cellLayout->setContentsMargins(0, 0, 0, 0);
                cellLayout->setSpacing(2);
                auto* label = new QLabel(fields[index + column].first, cell);
                label->setObjectName("telemetryLabel");
                label->setWordWrap(true);
                auto* value = makeValue(cell);
                value->setText(uppercaseDisplayText(fields[index + column].second));
                cellLayout->addWidget(label);
                cellLayout->addWidget(value);
                grid->addWidget(cell, static_cast<int>(index / 2), static_cast<int>(column));
            }
        }
        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 1);
        sections->addWidget(group);
    };

    addSection("JOB DETAILS", {
        {"CARGO", recordText(record, "job.cargo")},
        {"INCOME", recordCurrency(record, preferences_)},
        {"FROM: CITY, COUNTRY", recordLocation(record, "job.source_city", "job.source_country")},
        {"TO: CITY, COUNTRY", recordLocation(record, "job.destination_city", "job.destination_country")},
        {"FROM: COMPANY", recordText(record, "job.source_company")},
        {"TO: COMPANY", recordText(record, "job.destination_company")},
        {"PLANNED DISTANCE", recordDistance(record, "job.planned_distance_km", preferences_)},
        {"DRIVEN DISTANCE", recordDistance(record, "job.driven_distance_km", preferences_)},
        {"XP", recordNumber(record, "job.earned_xp", preferences_, "XP")},
        {"DAMAGE", recordDamage(record)},
        {"TIME TAKEN (REAL)", recordDuration(record, "job.real_duration_seconds")},
        {"TIME TAKEN (IN-GAME)", recordDuration(record, "job.ingame_duration_minutes", 60.0)}
    });
    addSection("VEHICLE & TRIP DETAILS", {
        {"TRUCK USED", recordText(record, "vehicle.make_model")},
        {"TRUCK PLATE", recordText(record, "vehicle.license_plate")},
        {"TRAILER", recordText(record, "trailer.details")},
        {"TRAILER PLATE", recordText(record, "trailer.license_plate")},
        {"FUEL COST", unavailable},
        {"REFUELED", recordNumber(record, "job.refueled", preferences_)},
        {"AVERAGE FUEL CONSUMPTION", recordAverageFuelConsumption(record, preferences_)},
        {"TOTAL FUEL CONSUMED", recordNumber(record, "job.total_fuel_consumed", preferences_)},
        {"MAX SPEED", recordNumber(record, "job.max_speed_kmh", preferences_)},
        {"AVERAGE SPEED", recordNumber(record, "job.average_speed_kmh", preferences_)}
    });
    const QString specialCargo = recordSpecialCargo(record);
    const QString status = QString::fromStdString(jobStatusToString(record.status));
    addSection("SPECIAL CARGO SUPPORT / STATUS", {
        {"SPECIAL CARGO", specialCargo},
        {"JOB STATUS", status},
        {"CANCELLATION PENALTY", recordNumber(record, "job.cancel_penalty", preferences_)}
    });
    reportHtml += "</body></html>";
    scroll->setWidget(content);
    dialogLayout->addWidget(scroll);
    auto* exportButton = new QPushButton("EXPORT PDF", &dialog);
    exportButton->setObjectName("exportJobPdfButton");
    exportButton->setCursor(Qt::PointingHandCursor);
    connect(exportButton, &QPushButton::clicked, &dialog, [this, &dialog, reportHtml]()
    {
        const QString filePath = QFileDialog::getSaveFileName(
            &dialog, "Export completed job", "completed-job.pdf", "PDF files (*.pdf)");
        if (filePath.isEmpty())
        {
            return;
        }
        QString error;
        if (!writePdfReport(filePath, reportHtml, &error))
        {
            QMessageBox::critical(&dialog, "PDF export failed", error);
            return;
        }
        statusBar()->showMessage("COMPLETED JOB PDF EXPORTED.");
    });
    dialogLayout->addWidget(exportButton, 0, Qt::AlignRight);
    dialog.exec();
}

void DashboardMainWindow::startTelemetryListener()
{
    localServer_ = new QLocalServer(this);
    connect(localServer_, &QLocalServer::newConnection, this, [this]()
    {
        while (localServer_->hasPendingConnections())
        {
            QLocalSocket* socket = localServer_->nextPendingConnection();
            if (!socket)
            {
                continue;
            }
            receiveBuffers_.insert(socket, {});
            TelemetrySnapshot connecting;
            connecting.status = ConnectionState::Connecting;
            clientSnapshots_.insert(socket, connecting);
            lastFrameTimes_.insert(socket, QDateTime::currentMSecsSinceEpoch());
            connect(socket, &QLocalSocket::readyRead, this, [this, socket]()
            {
                handleSocketData(socket);
            });
            connect(socket, &QLocalSocket::disconnected, this, [this, socket]()
            {
                receiveBuffers_.remove(socket);
                clientSnapshots_.remove(socket);
                lastFrameTimes_.remove(socket);
                if (selectedTelemetrySocket_ == socket)
                {
                    selectedTelemetrySocket_ = nullptr;
                }
                socket->deleteLater();
                refreshDisplayedSnapshot();
            });
            refreshDisplayedSnapshot();
        }
    });

    if (!localServer_->listen(QStringLiteral("nlsi_scs_telemetry")))
    {
        ipcError_ = true;
        connectionLabel_->setText(
            uppercaseDisplayText(QString("TELEMETRY CONNECTION: ERROR — %1").arg(localServer_->errorString())));
    }
}

void DashboardMainWindow::handleSocketData(QLocalSocket* socket)
{
    if (!receiveBuffers_.contains(socket))
    {
        return;
    }
    QByteArray& buffer = receiveBuffers_[socket];
    buffer.append(socket->readAll());
    if (buffer.size() > 1024 * 1024)
    {
        ipcError_ = true;
        socket->disconnectFromServer();
        refreshDisplayedSnapshot();
        return;
    }

    for (;;)
    {
        const int frameEnd = buffer.indexOf("\n\n");
        if (frameEnd < 0)
        {
            break;
        }
        const QByteArray frame = buffer.left(frameEnd + 1);
        buffer.remove(0, frameEnd + 2);
        const TelemetrySnapshot snapshot = decodeTelemetryFrame(frame.toStdString());
        lastFrameTimes_[socket] = QDateTime::currentMSecsSinceEpoch();
        if (snapshot.status != ConnectionState::Connected || snapshot.gameId.empty())
        {
            ipcError_ = true;
            clientSnapshots_.remove(socket);
            continue;
        }
        ipcError_ = false;
        clientSnapshots_[socket] = snapshot;
    }
    refreshDisplayedSnapshot();
}

void DashboardMainWindow::setProcessState(GameProcessState state)
{
    if (state != processState_)
    {
        processState_ = state;
        clientSnapshots_.clear();
        selectedTelemetrySocket_ = nullptr;
    }
    processLabel_->setText("GAME DETECTION: " + processStateText(processState_));
    refreshDisplayedSnapshot();
}

void DashboardMainWindow::refreshDisplayedSnapshot()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = lastFrameTimes_.cbegin(); it != lastFrameTimes_.cend(); ++it)
    {
        if (it.value() > 0 && now - it.value() > 2500)
        {
            clientSnapshots_.remove(it.key());
        }
    }
    const QString dateText = QDateTime::currentDateTime().toString("ddd d MMM yyyy  hh:mm:ss").toUpper();
    if (dateLabel_->text() != dateText)
    {
        dateLabel_->setText(dateText);
    }

    TelemetrySnapshot snapshot;
    bool connecting = false;
    if (processState_ == GameProcessState::BothRunning ||
        processState_ == GameProcessState::DetectionError)
    {
        snapshot.status = ConnectionState::Error;
    }
    else if (processState_ == GameProcessState::Ets2Running ||
             processState_ == GameProcessState::AtsRunning)
    {
        if (selectedTelemetrySocket_ &&
            (!clientSnapshots_.contains(selectedTelemetrySocket_) ||
             !snapshotMatchesProcess(clientSnapshots_.value(selectedTelemetrySocket_), processState_) ||
             now - lastFrameTimes_.value(selectedTelemetrySocket_, 0) > 2500))
        {
            selectedTelemetrySocket_ = nullptr;
        }
        if (!selectedTelemetrySocket_)
        {
            qint64 newestFrame = -1;
            for (auto it = clientSnapshots_.cbegin(); it != clientSnapshots_.cend(); ++it)
            {
                const qint64 frameTime = lastFrameTimes_.value(it.key(), 0);
                if (snapshotMatchesProcess(it.value(), processState_) &&
                    frameTime > newestFrame)
                {
                    newestFrame = frameTime;
                    selectedTelemetrySocket_ = it.key();
                }
            }
        }
        if (selectedTelemetrySocket_)
        {
            snapshot = clientSnapshots_.value(selectedTelemetrySocket_);
        }
        else
        {
            for (auto it = lastFrameTimes_.cbegin(); it != lastFrameTimes_.cend(); ++it)
            {
                if (now - it.value() <= 2500)
                {
                    connecting = true;
                    break;
                }
            }
            snapshot.status = ipcError_ ? ConnectionState::Error
                                        : (connecting ? ConnectionState::Connecting : ConnectionState::Disconnected);
        }
    }
    else
    {
        snapshot.status = ConnectionState::Disconnected;
    }
    if (ipcError_ && snapshot.status != ConnectionState::Connected)
    {
        snapshot.status = ConnectionState::Error;
    }
    applySnapshot(snapshot);
}

void DashboardMainWindow::applySnapshot(const TelemetrySnapshot& snapshot)
{
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(this, [this, snapshot]() { applySnapshot(snapshot); },
            Qt::QueuedConnection);
        return;
    }

    TelemetrySnapshot viewSnapshot = snapshot;
    const std::optional<JobRecord> previousActiveJob = jobHistory_.activeJob();
    const bool startsJob = [&]()
    {
        const auto status = snapshot.values.find("job.status");
        return status != snapshot.values.end() && status->second.available &&
            jobStatusFromString(status->second.value) == JobStatus::Active;
    }();
    const bool historyUpdated = jobHistory_.processSnapshot(snapshot, QDateTime::currentDateTimeUtc());

    const TelemetrySnapshot* retainedJobSnapshot = nullptr;
    if (jobHistory_.activeJob())
    {
        retainedJobSnapshot = &jobHistory_.activeJob()->snapshot;
    }
    else if (previousActiveJob)
    {
        retainedJobSnapshot = &previousActiveJob->snapshot;
    }
    else if (!startsJob)
    {
        retainedJobSnapshot = &displayedSnapshot_;
    }
    if (retainedJobSnapshot)
    {
        for (const auto& field : retainedJobSnapshot->values)
        {
            if (field.first.compare(0, 4, "job.") != 0)
            {
                continue;
            }
            const auto current = viewSnapshot.values.find(field.first);
            if (current == viewSnapshot.values.end() || !current->second.available ||
                trimString(current->second.value).empty())
            {
                viewSnapshot.values[field.first] = field.second;
            }
        }
    }
    if (jobHistory_.activeJob())
    {
        viewSnapshot.values["job.id"] = {
            "Job ID", jobHistory_.activeJob()->id, "", true
        };
    }
    if (historyUpdated && !jobHistory_.activeJob())
    {
        refreshCompletedJobs();
        if (statusBar())
        {
            if (!jobHistory_.errorString().isEmpty())
            {
                statusBar()->showMessage(
                    uppercaseDisplayText("JOB HISTORY WARNING: " + jobHistory_.errorString()));
            }
            else
            {
                statusBar()->showMessage(uppercaseDisplayText("JOB RECORD SAVED: " +
                    QString::fromStdString(viewSnapshot.values["job.id"].value)));
            }
        }
    }
    else if (!jobHistory_.errorString().isEmpty() && statusBar())
    {
        statusBar()->showMessage(uppercaseDisplayText("JOB HISTORY ERROR: " + jobHistory_.errorString()));
    }
    displayedSnapshot_ = viewSnapshot;
    const QString connectionText =
        "TELEMETRY CONNECTION: " +
        QString::fromStdString(connectionStateToString(viewSnapshot.status)).toUpper();
    if (connectionLabel_->text() != connectionText)
    {
        connectionLabel_->setText(connectionText);
    }

    const auto set = [](QLabel* label, const QString& text)
    {
        if (!label)
        {
            return;
        }
        const QString normalized = uppercaseDisplayText(text.isEmpty() ? unavailable : text);
        if (label->text() != normalized)
        {
            label->setText(normalized);
        }
    };

    const auto findField = [&](const std::string& key) -> const FieldValue* {
        const auto it = viewSnapshot.values.find(key);
        if (it == viewSnapshot.values.end())
        {
            return nullptr;
        }
        return &it->second;
    };

    const auto fieldText = [&](const std::string& key) -> QString {
        const auto* field = findField(key);
        if (!field || !field->available || field->value.empty())
        {
            return unavailable;
        }
        QString value = QString::fromStdString(field->value);
        const QString unit = QString::fromStdString(field->unit).trimmed();
        if (!unit.isEmpty())
        {
            value += " " + unit;
        }
        return value;
    };
    const auto rawValue = [&](const std::string& key) -> std::pair<QString, QString> {
        const auto* field = findField(key);
        if (!field || !field->available || field->value.empty())
        {
            return {unavailable, QString()};
        }
        return {QString::fromStdString(field->value), QString::fromStdString(field->unit).trimmed()};
    };

    set(currentJobValue_, fieldText("job.id"));
    set(statusValue_, fieldText("job.status"));
    set(cargoValue_, fieldText("job.cargo"));
    {
        const auto* incomeField = findField("job.income");
        if (incomeField && incomeField->available && !incomeField->value.empty())
        {
            double value = 0.0;
            if (parseDoubleValue(QString::fromStdString(incomeField->value), value))
            {
                set(incomeValue_, formatCurrencyAmount(
                    value, QString::fromStdString(incomeField->unit), preferences_));
            }
            else
            {
                set(incomeValue_, unavailable);
            }
        }
        else
        {
            set(incomeValue_, unavailable);
        }
    }
    set(fromValue_, currentJobCompanyCity(viewSnapshot, "job.source_company", "job.source_city"));
    set(toValue_, currentJobCompanyCity(viewSnapshot, "job.destination_company", "job.destination_city"));

    {
        const auto* planned = findField("job.planned_distance_km");
        const auto* remaining = findField("truck.navigation.distance");
        double plannedValue = 0.0;
        double remainingValue = 0.0;
        bool plannedOk = false;
        bool remainingOk = false;
        if (planned && planned->available && !planned->value.empty())
        {
            plannedOk = parseDoubleValue(QString::fromStdString(planned->value), plannedValue);
        }
        if (remaining && remaining->available && !remaining->value.empty())
        {
            remainingOk = parseDoubleValue(QString::fromStdString(remaining->value), remainingValue);
        }
        if (plannedOk)
        {
            const QString plannedUnit = QString::fromStdString(planned->unit).trimmed();
            set(plannedDistanceValue_, formatDistance(plannedValue, plannedUnit, preferences_));
        }
        else
        {
            set(plannedDistanceValue_, unavailable);
        }
        if (remainingOk)
        {
            const QString remainingUnit = QString::fromStdString(remaining->unit).trimmed();
            set(remainingDistanceValue_, formatDistance(remainingValue, remainingUnit, preferences_));
        }
        else
        {
            set(remainingDistanceValue_, unavailable);
        }
        if (plannedOk && remainingOk)
        {
            double plannedKm = plannedValue;
            double remainingKm = remainingValue;
            const QString plannedUnit = QString::fromStdString(planned->unit).trimmed().toLower();
            const QString remainingUnit = QString::fromStdString(remaining->unit).trimmed().toLower();
            if (plannedUnit == "m" || plannedUnit == "meters" || plannedUnit == "metres")
            {
                plannedKm = metresToKilometres(plannedValue);
            }
            if (remainingUnit == "m" || remainingUnit == "meters" || remainingUnit == "metres")
            {
                remainingKm = metresToKilometres(remainingValue);
            }
            const QString progressText = formatProgressPercent(plannedKm, remainingKm);
            const QString progressValue = progressText == unavailable ? unavailable : progressText;
            set(progressValue_, progressValue);
        }
        else
        {
            set(progressValue_, unavailable);
        }
    }

    {
        const auto* timeField = findField("truck.navigation.time");
        if (timeField && timeField->available && !timeField->value.empty())
        {
            double gameSeconds = 0.0;
            set(arrivalValue_, parseDoubleValue(QString::fromStdString(timeField->value), gameSeconds)
                ? formatArrivalEstimate(gameSeconds)
                : unavailable);
        }
        else
        {
            set(arrivalValue_, unavailable);
        }
    }

    {
        const auto fuel = rawValue("truck.fuel.amount");
        const auto capacity = rawValue("truck.fuel.capacity");
        double fuelAmount = std::numeric_limits<double>::quiet_NaN();
        double fuelCapacity = std::numeric_limits<double>::quiet_NaN();
        const bool fuelAvailable = fuel.first != unavailable && parseDoubleValue(fuel.first, fuelAmount);
        const bool capacityAvailable = capacity.first != unavailable && parseDoubleValue(capacity.first, fuelCapacity);
        set(fuelValue_, formatFuelDisplay(
            fuelAvailable ? fuelAmount : std::numeric_limits<double>::quiet_NaN(),
            capacityAvailable ? fuelCapacity : std::numeric_limits<double>::quiet_NaN(),
            preferences_));
        fuelValue_->setStyleSheet(fuelAvailable && capacityAvailable &&
            isFuelAtOrBelowWarningThreshold(fuelAmount, fuelCapacity)
                ? "color: #C62828;"
                : QString());
    }

    {
        const auto rpm = rawValue("truck.engine.rpm");
        const QString gear = fieldText("truck.engine.gear");
        const QString formattedRpm = formatFixedNumber(rpm.first, "RPM");
        set(rpmGearValue_, QString("%1 / %2").arg(formattedRpm, gear));
    }
    const auto cruise = rawValue("truck.cruise_control");
    double cruiseSpeed = 0.0;
    const bool cruiseActive = cruise.first != unavailable &&
        parseDoubleValue(cruise.first, cruiseSpeed) && cruiseSpeed > 0.0;
    {
        const auto throttle = rawValue("truck.effective.throttle");
        const QString throttlePercent = throttle.first == unavailable
            ? unavailable
            : formatNormalizedPercent(throttle.first, throttle.second);
        set(throttleValue_, throttlePercent == unavailable || !cruiseActive
            ? throttlePercent
            : throttlePercent + " - A");
    }
    {
        const auto brake = rawValue("truck.effective.brake");
        double brakeAmount = 0.0;
        if (brake.first == unavailable || !parseDoubleValue(brake.first, brakeAmount))
        {
            set(brakeValue_, unavailable);
        }
        else
        {
            const QString brakeText = formatNormalizedPercent(brake.first, brake.second);
            set(brakeValue_, cruiseActive && brakeText != unavailable
                ? brakeText + " - A"
                : brakeText);
        }
    }
    {
        if (cruise.first == unavailable || !parseDoubleValue(cruise.first, cruiseSpeed))
        {
            set(cruiseValue_, unavailable);
        }
        else
        {
            const QString speed = formatCruiseSpeed(cruise.first, cruise.second, preferences_);
            set(cruiseValue_, speed == unavailable
                ? unavailable
                : cruiseSpeed > 0.0 ? speed + " - A" : speed);
        }
    }
    {
        const auto retarder = rawValue("truck.brake.retarder");
        double retarderLevel = 0.0;
        if (retarder.first == unavailable || !parseDoubleValue(retarder.first, retarderLevel))
        {
            set(retarderValue_, unavailable);
        }
        else
        {
            const QString retarderText = fieldText("truck.brake.retarder");
            set(retarderValue_, cruiseActive && retarderText != unavailable
                ? retarderText + " - A"
                : retarderText);
        }
    }

    if (viewSnapshot.gameName.empty())
    {
        set(gameVersionValue_, unavailable);
        gameConfigPanel_->setTitle("GAME CONFIG | —");
    }
    else
    {
        const QString game = gameDisplayName(QString::fromStdString(viewSnapshot.gameId));
        const QString version = viewSnapshot.gameApiVersion.empty()
            ? unavailable
            : QString::fromStdString(viewSnapshot.gameApiVersion);
        gameConfigPanel_->setTitle(QString("GAME CONFIG | %1").arg(game));
        set(gameVersionValue_, version);
    }
    set(vehicleValue_, fieldText("vehicle.make_model"));
    set(vehiclePlateValue_, fieldText("vehicle.license_plate"));
    set(trailerValue_, fieldText("trailer.details"));
    set(trailerPlateValue_, fieldText("trailer.license_plate"));
    const QString latestEvent = fieldText("telemetry.event.latest");
    const QString summaryText = latestEvent == unavailable
        ? "NO EVENTS AVAILABLE."
        : uppercaseDisplayText("LATEST SDK EVENT: " + latestEvent);
    if (summaryValue_->text() != summaryText)
    {
        summaryValue_->setText(summaryText);
    }
}

} // namespace nlsi
