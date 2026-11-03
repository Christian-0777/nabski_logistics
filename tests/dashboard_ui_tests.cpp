#include <iostream>
#include <vector>

#include <QApplication>
#include <QAbstractButton>
#include <QComboBox>
#include <QCloseEvent>
#include <QDialog>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QGroupBox>
#include <QImage>
#include <QJsonObject>
#include <QColor>
#include <QLayout>
#include <QLabel>
#include <QLoggingCategory>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSizePolicy>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimer>

#include "DashboardMainWindow.h"
#include "JobReport.h"
#include "ProfileImageLabel.h"

namespace {

QStringList* integrationLogMessages = nullptr;

void captureIntegrationLog(QtMsgType, const QMessageLogContext&, const QString& message)
{
    if (integrationLogMessages)
    {
        integrationLogMessages->append(message);
    }
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    ProfileImageLabel imageHolder(true);
    imageHolder.resize(72, 72);
    QImage profileFixture(4, 2, QImage::Format_ARGB32_Premultiplied);
    profileFixture.fill(Qt::green);
    profileFixture.setPixelColor(0, 0, Qt::red);
    profileFixture.setPixelColor(3, 0, Qt::blue);
    profileFixture.setPixelColor(0, 1, Qt::red);
    profileFixture.setPixelColor(3, 1, Qt::blue);
    imageHolder.setImage(QPixmap::fromImage(profileFixture));
    QImage profileRender(imageHolder.size(), QImage::Format_ARGB32_Premultiplied);
    profileRender.fill(Qt::transparent);
    imageHolder.render(&profileRender);
    if (profileRender.pixelColor(36, 36) != QColor(Qt::green) ||
        profileRender.pixelColor(0, 0) == QColor(Qt::green))
    {
        std::cerr << "A valid profile image was stretched or escaped its circular holder.\n";
        return 1;
    }
    imageHolder.resize(90, 90);
    profileRender = QImage(imageHolder.size(), QImage::Format_ARGB32_Premultiplied);
    profileRender.fill(Qt::transparent);
    imageHolder.render(&profileRender);
    if (profileRender.pixelColor(45, 45) != QColor(Qt::green) ||
        profileRender.pixelColor(0, 0) == QColor(Qt::green))
    {
        std::cerr << "Resizing the profile image holder distorted or unclipped its image.\n";
        return 1;
    }
    imageHolder.setImage(QPixmap());
    imageHolder.setText("?");
    if (imageHolder.text() != "?")
    {
        std::cerr << "The profile image fallback placeholder was not retained.\n";
        return 1;
    }
    QTemporaryDir dashboardProfileDirectory;
    if (!dashboardProfileDirectory.isValid())
    {
        std::cerr << "Could not create the temporary dashboard profile directory.\n";
        return 1;
    }
    const QString dashboardProfilePath = dashboardProfileDirectory.filePath("profile.nlsi");
    nlsi::DashboardMainWindow window(nullptr, QString(), dashboardProfilePath,
        [] { return nlsi::GameProcessState::NotRunning; });
    nlsi::GameProcessDetectionResult simulatedProcessDetection{
        nlsi::GameProcessState::Ets2Running};
    nlsi::DashboardMainWindow closeSafetyWindow(nullptr, QString(), QString(),
        [&simulatedProcessDetection] { return simulatedProcessDetection; });
    closeSafetyWindow.show();
    auto verifyBlockedClose = [&](nlsi::GameProcessDetectionResult detection,
                                  const QString& expectedMessage,
                                  const char* failure)
    {
        simulatedProcessDetection = detection;
        bool warningMatched = false;
        QTimer::singleShot(0, [&]()
        {
            auto* warning = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (warning)
            {
                warningMatched = warning->text() == expectedMessage;
                warning->accept();
            }
        });
        const bool closed = closeSafetyWindow.close();
        if (closed || !closeSafetyWindow.isVisible() || !warningMatched)
        {
            std::cerr << failure << '\n';
            return false;
        }
        return true;
    };
    if (!verifyBlockedClose(nlsi::GameProcessState::Ets2Running,
            "NLSI Exclusive Logbook cannot be closed while a supported game is running. "
            "Please close Euro Truck Simulator 2 or American Truck Simulator first.",
            "Closing while ETS2 runs was not canceled with the expected warning.") ||
        !verifyBlockedClose(nlsi::GameProcessState::AtsRunning,
            "NLSI Exclusive Logbook cannot be closed while a supported game is running. "
            "Please close Euro Truck Simulator 2 or American Truck Simulator first.",
            "Closing while ATS runs was not canceled with the expected warning."))
    {
        return 1;
    }

    simulatedProcessDetection = nlsi::GameProcessState::Ets2Running;
    bool altF4WarningMatched = false;
    QTimer::singleShot(0, [&]()
    {
        auto* warning = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (warning)
        {
            altF4WarningMatched = warning->text().contains("cannot be closed");
            warning->accept();
        }
    });
    QCloseEvent altF4CloseEvent;
    QApplication::sendEvent(&closeSafetyWindow, &altF4CloseEvent);
    if (altF4CloseEvent.isAccepted() || !closeSafetyWindow.isVisible() || !altF4WarningMatched)
    {
        std::cerr << "The Alt+F4 close event was not canceled while ETS2 runs.\n";
        return 1;
    }

    if (!verifyBlockedClose(
            nlsi::GameProcessDetectionResult{nlsi::GameProcessState::DetectionError, 5},
            "Process enumeration failed with Windows error 5. NLSI Exclusive Logbook cannot "
            "verify whether a supported game is running, so closure has been blocked for safety.",
            "Closing after process-detection failure was not blocked safely."))
    {
        return 1;
    }
    simulatedProcessDetection = nlsi::GameProcessState::NotRunning;
    if (!closeSafetyWindow.close() || closeSafetyWindow.isVisible())
    {
        std::cerr << "Closing after both games are closed was not allowed.\n";
        return 1;
    }

    QTemporaryDir trailerJobDirectory;
    nlsi::DashboardMainWindow trailerWindow(nullptr,
        QDir(trailerJobDirectory.path()).filePath("jobs"), QString(),
        [] { return nlsi::GameProcessState::NotRunning; });
    nlsi::TelemetrySnapshot preJobSnapshot;
    preJobSnapshot.status = nlsi::ConnectionState::Connected;
    preJobSnapshot.gameId = "eut2";
    preJobSnapshot.values["trailer.details"] = {"Trailers", "Owned Flatbed", "", true};
    preJobSnapshot.values["trailer.license_plate"] = {"Trailer Plate", "OWNED-1", "", true};
    trailerWindow.applySnapshot(preJobSnapshot);
    nlsi::TelemetrySnapshot activeTrailerJob = preJobSnapshot;
    activeTrailerJob.values["job.status"] = {"Status", "ACTIVE", "", true};
    activeTrailerJob.values["job.current"] = {"Current Job", "true", "", true};
    activeTrailerJob.values["job.cargo"] = {"Cargo", "Steel", "", true};
    activeTrailerJob.values["trailer.details"] = {"Trailers", "Assigned Trailer", "", true};
    activeTrailerJob.values["trailer.license_plate"] = {"Trailer Plate", "JOB-9", "", true};
    trailerWindow.applySnapshot(activeTrailerJob);
    auto* activeTrailerValue = trailerWindow.findChild<QLabel*>("trailerValue");
    auto* activeTrailerPlateValue = trailerWindow.findChild<QLabel*>("trailerPlateValue");
    if (!activeTrailerValue || activeTrailerValue->text() != "ASSIGNED TRAILER" ||
        !activeTrailerPlateValue || activeTrailerPlateValue->text() != "JOB-9")
    {
        std::cerr << "The dashboard did not display the active job's trailer details.\n";
        return 1;
    }
    activeTrailerJob.values["trailer.details"] = {"Trailers", "Another Active Trailer", "", true};
    activeTrailerJob.values["trailer.license_plate"] = {"Trailer Plate", "JOB-10", "", true};
    trailerWindow.applySnapshot(activeTrailerJob);
    if (activeTrailerValue->text() != "ANOTHER ACTIVE TRAILER" ||
        activeTrailerPlateValue->text() != "JOB-10")
    {
        std::cerr << "Updated active trailer telemetry was not displayed.\n";
        return 1;
    }
    QTemporaryDir noTrailerJobDirectory;
    nlsi::DashboardMainWindow noTrailerWindow(nullptr,
        QDir(noTrailerJobDirectory.path()).filePath("jobs"), QString(),
        [] { return nlsi::GameProcessState::NotRunning; });
    nlsi::TelemetrySnapshot noTrailerPreJob;
    noTrailerPreJob.status = nlsi::ConnectionState::Connected;
    noTrailerPreJob.gameId = "eut2";
    noTrailerWindow.applySnapshot(noTrailerPreJob);
    auto activeWithoutPreJobTrailer = noTrailerPreJob;
    activeWithoutPreJobTrailer.values["job.status"] = {"Status", "ACTIVE", "", true};
    activeWithoutPreJobTrailer.values["job.cargo"] = {"Cargo", "Steel", "", true};
    activeWithoutPreJobTrailer.values["trailer.details"] = {"Trailers", "Assigned Trailer", "", true};
    activeWithoutPreJobTrailer.values["trailer.license_plate"] =
        {"Trailer Plate", "JOB-11", "", true};
    noTrailerWindow.applySnapshot(activeWithoutPreJobTrailer);
    auto* noPreJobTrailerValue = noTrailerWindow.findChild<QLabel*>("trailerValue");
    auto* noPreJobTrailerPlate = noTrailerWindow.findChild<QLabel*>("trailerPlateValue");
    if (!noPreJobTrailerValue || noPreJobTrailerValue->text() != "ASSIGNED TRAILER" ||
        !noPreJobTrailerPlate || noPreJobTrailerPlate->text() != "JOB-11")
    {
        std::cerr << "The dashboard did not display a job trailer without pre-job trailer data.\n";
        return 1;
    }
    QJsonObject representativeInactiveAccount;
    representativeInactiveAccount.insert("status", "active");
    representativeInactiveAccount.insert("is_inactive", true);
    representativeInactiveAccount.insert("is_representative", true);
    if (!nlsi::isAccountInactive(representativeInactiveAccount) ||
        !nlsi::isAccountRepresentative(representativeInactiveAccount) ||
        nlsi::accountDesignationLabels(representativeInactiveAccount).size() != 1 ||
        nlsi::displayAccountRole("DRIVER", representativeInactiveAccount) != "DRIVER • REPRESENTATIVE")
    {
        std::cerr << "Representative and inactive account designation handling is incorrect.\n";
        return 1;
    }
    if (window.windowTitle() != "NLSI Exclusive Logbook 1.9.6 Beta" ||
        window.statusBar()->currentMessage() != "LOCAL TELEMETRY • VERSION 1.9.6 BETA")
    {
        std::cerr << "The dashboard title or status bar does not report v1.9.6 Beta.\n";
        return 1;
    }
    auto* taskCard = window.findChild<QGroupBox*>("profileTaskCard");
    auto* taskProgress = window.findChild<QProgressBar*>("profileTaskProgress");
    auto* taskStatus = window.findChild<QLabel*>("profileTaskStatus");
    auto* rankCard = window.findChild<QGroupBox*>("profileRankCard");
    auto* rankProgress = window.findChild<QProgressBar*>("profileRankProgress");
    auto* taskTitle = window.findChild<QLabel*>("profileTaskTitle");
    auto* rankTitle = window.findChild<QLabel*>("profileRankTitle");
    if (!taskCard || !taskProgress || taskProgress->isEnabled() ||
        taskProgress->format() != "TASK DATA UNAVAILABLE" ||
        taskProgress->minimum() != 0 || taskProgress->maximum() != 100 ||
        !taskTitle || taskTitle->text() != "MONTHLY TASK" ||
        !rankTitle || rankTitle->text() != "RANK PROGRESSION" ||
        !taskStatus || !taskStatus->text().contains("Not connected") ||
        !rankCard || !rankProgress || taskProgress == rankProgress ||
        taskCard->parentWidget() != rankCard->parentWidget() ||
        !taskCard->parentWidget()->layout() ||
        taskCard->parentWidget()->layout()->indexOf(taskCard) >=
            taskCard->parentWidget()->layout()->indexOf(rankCard))
    {
        std::cerr << "Profile task unavailable state or TASK-before-RANK layout is incorrect.\n";
        return 1;
    }
    auto* jobSyncTimer = window.findChild<QTimer*>("jobSyncTimer");
    if (!jobSyncTimer || !jobSyncTimer->isActive() || jobSyncTimer->interval() != 60000)
    {
        std::cerr << "The authenticated job synchronization poll is not configured.\n";
        return 1;
    }
    if (window.minimumSize() != QSize(900, 600))
    {
        std::cerr << "Dashboard minimum size is not 900x600.\n";
        return 1;
    }

    auto* scroll = window.findChild<QScrollArea*>();
    if (!scroll || !scroll->widgetResizable())
    {
        std::cerr << "Dashboard main content is not in a resizable scroll area.\n";
        return 1;
    }

    const QList<QGroupBox*> panels = scroll->widget()->findChildren<QGroupBox*>();
    const std::vector<QString> expected = {
        "CURRENT JOB",
        "DRIVING",
        "GAME CONFIG | —",
        "SUMMARY"
    };
    if (panels.size() != static_cast<int>(expected.size()))
    {
        std::cerr << "Dashboard does not contain exactly four expected panels.\n";
        return 1;
    }
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        if (panels.at(static_cast<int>(index))->title() != expected[index])
        {
            std::cerr << "Dashboard panel order is incorrect.\n";
            return 1;
        }
    }
    bool hasUnavailableDash = false;
    bool hasNeutralSummary = false;
    for (QLabel* label : scroll->widget()->findChildren<QLabel*>())
    {
        hasUnavailableDash = hasUnavailableDash || label->text() == QString::fromUtf8("—");
        hasNeutralSummary = hasNeutralSummary || label->text() == "NO EVENTS AVAILABLE.";
    }
    if (!hasUnavailableDash || !hasNeutralSummary)
    {
        std::cerr << "Dashboard unavailable values or neutral summary state are incorrect.\n";
        return 1;
    }
    if (!scroll->widget()->findChildren<QProgressBar*>().isEmpty())
    {
        std::cerr << "Dashboard still contains a graphical progress bar.\n";
        return 1;
    }
    auto* sidebar = window.findChild<QWidget*>("sidebar");
    auto* dashboardItem = window.findChild<QAbstractButton*>("dashboardNavButton");
    auto* completedItem = window.findChild<QAbstractButton*>("completedJobsNavButton");
    auto* settingsItem = window.findChild<QAbstractButton*>("settingsNavButton");
    auto* pageStack = window.findChild<QStackedWidget*>("mainPages");
    auto* settingsTabs = window.findChild<QStackedWidget*>("settingsTabs");
    auto* dashboardScroll = window.findChild<QScrollArea*>("dashboardScroll");
    auto* completedJobsScroll = window.findChild<QScrollArea*>("completedJobsScroll");
    auto* profileScroll = window.findChild<QScrollArea*>("profileScroll");
    auto* brandName = window.findChild<QLabel*>("sidebarTitle");
    auto* brandSubtitle = window.findChild<QLabel*>("sidebarSubtitle");
    bool hasSidebarReleaseVersion = false;
    for (auto* label : window.findChildren<QLabel*>("sidebarSubtitle"))
    {
        if (label->text() == "1.9.6 BETA")
        {
            hasSidebarReleaseVersion = true;
            break;
        }
    }
    auto* headerTitle = window.findChild<QLabel*>("headerTitle");
    auto* fieldLabel = window.findChild<QLabel*>("telemetryLabel");
    auto* fieldValue = window.findChild<QLabel*>("telemetryValue");
    auto* cardTitle = panels.at(0);
    if (!sidebar || !dashboardItem || !completedItem || !settingsItem || !pageStack ||
        !settingsTabs || settingsTabs->count() != 3 || !dashboardScroll ||
        !completedJobsScroll || !profileScroll ||
        dashboardItem->sizePolicy().horizontalPolicy() != QSizePolicy::Expanding ||
        !dashboardItem->property("sidebarNavButton").toBool() ||
        !completedItem->property("sidebarNavButton").toBool() ||
        !settingsItem->property("sidebarNavButton").toBool() ||
        !brandName || !brandSubtitle || !hasSidebarReleaseVersion || brandSubtitle->wordWrap() ||
        !dashboardItem->property("active").toBool() ||
        completedItem->property("active").toBool() ||
        dashboardScroll->horizontalScrollBarPolicy() != Qt::ScrollBarAlwaysOff ||
        dashboardScroll->verticalScrollBarPolicy() != Qt::ScrollBarAsNeeded)
    {
        std::cerr << "Sidebar styles are incorrect: name="
                  << (brandName ? 1 : 0)
                  << " subtitle=" << (brandSubtitle ? 1 : 0)
                  << " wrap=" << (brandSubtitle ? brandSubtitle->wordWrap() : true) << ".\n";
        return 1;
    }

    nlsi::TelemetrySnapshot snapshot;
    snapshot.gameId = "eut2";
    snapshot.gameName = "Euro Truck Simulator 2";
    snapshot.gameApiVersion = "1.15";
    window.applySnapshot(snapshot);
    if (panels.at(2)->title() != "GAME CONFIG | EURO TRUCK SIMULATOR 2")
    {
        std::cerr << "Game config heading does not show the detected game name.\n";
        return 1;
    }

    window.resize(900, 600);
    window.show();
    scroll->setFixedHeight(150);
    app.processEvents();
    if (sidebar->width() < 260)
    {
        std::cerr << "Sidebar width is less than 260 pixels.\n";
        return 1;
    }
    if (dashboardItem->width() != sidebar->width() - 24 ||
        completedItem->width() != sidebar->width() - 24 ||
        settingsItem->width() != sidebar->width() - 24 ||
        brandName->font().pixelSize() != 16 || brandSubtitle->font().pixelSize() != 13 ||
        !headerTitle || headerTitle->font().pixelSize() != 15 ||
        !fieldLabel || fieldLabel->font().pixelSize() != 14 ||
        !fieldValue || fieldValue->font().pixelSize() != 14 ||
        !fieldLabel->font().bold() || !fieldValue->font().bold() ||
        cardTitle->font().pixelSize() != 13)
    {
        std::cerr << "Dashboard typography or sidebar sizing does not match the defined pixel sizes.\n";
        return 1;
    }
    if (scroll->verticalScrollBar()->maximum() <= 0)
    {
        std::cerr << "Dashboard scroll area did not become vertically scrollable.\n";
        return 1;
    }
    auto* dashboardScrollBar = dashboardScroll->verticalScrollBar();
    const int initialScrollValue = dashboardScrollBar->value();
    dashboardScrollBar->triggerAction(QAbstractSlider::SliderPageStepAdd);
    if (dashboardScrollBar->value() <= initialScrollValue)
    {
        std::cerr << "Dashboard vertical scrollbar page interaction did not scroll the content.\n";
        return 1;
    }
    dashboardScrollBar->setValue(0);
    dashboardScrollBar->setSliderDown(true);
    if (!dashboardScrollBar->isSliderDown())
    {
        std::cerr << "Dashboard vertical scrollbar cannot enter its dragging state.\n";
        return 1;
    }
    dashboardScrollBar->setSliderDown(false);
    if (!window.styleSheet().contains("QPushButton[sidebarNavButton=\"true\"]:hover") ||
        !window.styleSheet().contains("QPushButton[sidebarNavButton=\"true\"]:pressed") ||
        !window.styleSheet().contains("QPushButton[sidebarNavButton=\"true\"][active=\"true\"]") ||
        !window.styleSheet().contains("background: #D94F8C") ||
        !window.styleSheet().contains("QScrollBar:vertical { width: 10px;") ||
        !window.styleSheet().contains("QScrollBar:horizontal { height: 10px;") ||
        !window.styleSheet().contains("QScrollBar::handle:vertical { min-height: 28px;") ||
        !window.styleSheet().contains("QScrollBar::handle:horizontal { min-width: 28px;") ||
        !window.styleSheet().contains("QScrollBar::handle:vertical:hover") ||
        !window.styleSheet().contains("QScrollBar::handle:horizontal:hover") ||
        !window.styleSheet().contains("QScrollBar::handle:vertical:pressed") ||
        !window.styleSheet().contains("QScrollBar::handle:horizontal:pressed") ||
        !window.styleSheet().contains("QScrollBar::add-page:vertical") ||
        !window.styleSheet().contains("QAbstractScrollArea::corner"))
    {
        std::cerr << "Sidebar or application scrollbar interaction states are missing.\n";
        return 1;
    }

    QScrollArea horizontalScrollArea(&window);
    horizontalScrollArea.resize(120, 100);
    horizontalScrollArea.setWidgetResizable(false);
    horizontalScrollArea.setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    horizontalScrollArea.setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    auto* oversizedContent = new QWidget();
    oversizedContent->setFixedSize(700, 500);
    horizontalScrollArea.setWidget(oversizedContent);
    horizontalScrollArea.show();
    app.processEvents();
    if (horizontalScrollArea.horizontalScrollBar()->maximum() <= 0 ||
        horizontalScrollArea.verticalScrollBar()->maximum() <= 0)
    {
        std::cerr << "Application scrollbars do not preserve horizontal and vertical scrolling.\n";
        return 1;
    }
    const int horizontalScrollStart = horizontalScrollArea.horizontalScrollBar()->value();
    horizontalScrollArea.horizontalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepAdd);
    if (horizontalScrollArea.horizontalScrollBar()->value() <= horizontalScrollStart)
    {
        std::cerr << "Application horizontal scrollbar page interaction did not scroll content.\n";
        return 1;
    }
    horizontalScrollArea.hide();

    nlsi::TelemetrySnapshot drivingSnapshot;
    drivingSnapshot.values["truck.fuel.amount"] = {"Fuel", "23.11", "L", true};
    drivingSnapshot.values["truck.fuel.capacity"] = {"Fuel Capacity", "100", "L", true};
    drivingSnapshot.values["truck.engine.rpm"] = {"Engine RPM", "1171.96", "RPM", true};
    drivingSnapshot.values["truck.engine.gear"] = {"Gear", "3", "", true};
    drivingSnapshot.values["truck.effective.throttle"] = {"Throttle", "0.5", "normalized [0,1]", true};
    drivingSnapshot.values["truck.effective.brake"] = {"Brake", "0.1", "normalized [0,1]", true};
    drivingSnapshot.values["truck.cruise_control"] = {"Cruise Control", "25", "m/s", true};
    drivingSnapshot.values["truck.brake.retarder"] = {"Retarder", "2", "level", true};
    drivingSnapshot.values["truck.navigation.time"] = {"Navigation ETA", "900", "s", true};
    window.applySnapshot(drivingSnapshot);
    auto* fuelValue = window.findChild<QLabel*>("fuelValue");
    auto* rpmGearValue = window.findChild<QLabel*>("rpmGearValue");
    auto* throttleValue = window.findChild<QLabel*>("throttleValue");
    auto* brakeValue = window.findChild<QLabel*>("brakeValue");
    auto* cruiseValue = window.findChild<QLabel*>("cruiseValue");
    auto* retarderValue = window.findChild<QLabel*>("retarderValue");
    auto* arrivalValue = window.findChild<QLabel*>("arrivalValue");
    if (!fuelValue || !rpmGearValue || !throttleValue || !brakeValue ||
        !cruiseValue || !retarderValue || !arrivalValue ||
        fuelValue->text() != "23.11 L / 100.00 L (23.11%)" ||
        fuelValue->styleSheet().contains("#C62828") ||
        rpmGearValue->text() != "1171.96 RPM / 3" ||
        throttleValue->text() != "50.00% - A" ||
        brakeValue->text() != "10.00% - A" ||
        cruiseValue->text() != "90.00 KM/H - A" ||
        retarderValue->text() != "2 LEVEL - A" ||
        !arrivalValue->text().startsWith("00:01:00 - "))
    {
        std::cerr << "Dashboard numeric formatting or active driving indicators are incorrect.\n";
        return 1;
    }
    for (QLabel* label : scroll->widget()->findChildren<QLabel*>())
    {
        if ((label->objectName() == "telemetryLabel" ||
             label->objectName() == "telemetryValue") &&
            (label->text() != label->text().toUpper() || !label->font().bold()))
        {
            std::cerr << "Dashboard field labels or values are not uppercase and bold.\n";
            return 1;
        }
    }

    drivingSnapshot.values["truck.fuel.amount"].value = "20";
    drivingSnapshot.values["truck.effective.brake"].value = "0";
    drivingSnapshot.values["truck.cruise_control"].value = "0";
    drivingSnapshot.values["truck.brake.retarder"].value = "0";
    window.applySnapshot(drivingSnapshot);
    if (fuelValue->text() != "20.00 L / 100.00 L (20.00%)" ||
        !fuelValue->styleSheet().contains("#C62828") ||
        throttleValue->text() != "50.00%" ||
        brakeValue->text() != "0.00%" ||
        cruiseValue->text() != "0.00 KM/H" ||
        retarderValue->text() != "0 LEVEL")
    {
        std::cerr << "Dashboard inactive indicators or low-fuel warning behavior is incorrect: fuel="
                  << fuelValue->text().toStdString() << " style=" << fuelValue->styleSheet().toStdString()
                  << " throttle=" << throttleValue->text().toStdString()
                  << " brake=" << brakeValue->text().toStdString()
                  << " cruise=" << cruiseValue->text().toStdString()
                  << " retarder=" << retarderValue->text().toStdString() << ".\n";
        return 1;
    }
    drivingSnapshot.values["truck.fuel.amount"].value = "20.01";
    window.applySnapshot(drivingSnapshot);
    if (fuelValue->styleSheet().contains("#C62828"))
    {
        std::cerr << "Dashboard does not clear the low-fuel warning above 20 percent.\n";
        return 1;
    }
    drivingSnapshot.values["truck.effective.brake"].value = "0.1";
    drivingSnapshot.values["truck.cruise_control"].value = "0";
    drivingSnapshot.values["truck.brake.retarder"].value = "2";
    window.applySnapshot(drivingSnapshot);
    if (brakeValue->text() != "10.00%" ||
        cruiseValue->text() != "0.00 KM/H" ||
        retarderValue->text() != "2 LEVEL")
    {
        std::cerr << "Brake or retarder activity incorrectly enabled the cruise-control indicator.\n";
        return 1;
    }
    nlsi::TelemetrySnapshot partialRoute = drivingSnapshot;
    partialRoute.values["job.source_city"] = {"From", "berlin", "", true};
    partialRoute.values["job.source_company"] = {"Origin Company", "", "", false};
    partialRoute.values["job.destination_company"] = {"Destination Company", "nbfc", "", true};
    partialRoute.values["job.destination_city"] = {"To", "", "", false};
    window.applySnapshot(partialRoute);
    auto* partialFromValue = window.findChild<QLabel*>("fromValue");
    auto* partialToValue = window.findChild<QLabel*>("toValue");
    if (!partialFromValue || partialFromValue->text() != "BERLIN" ||
        !partialToValue || partialToValue->text() != "NBFC")
    {
        std::cerr << "Unavailable Current Job route components are not handled gracefully.\n";
        return 1;
    }

    auto* settingsNavigation = window.findChild<QPushButton*>("settingsNavButton");
    auto* preferenceTab = window.findChild<QPushButton*>("preferenceTabButton");
    auto* configTab = window.findChild<QPushButton*>("configTabButton");
    auto* profileTab = window.findChild<QPushButton*>("profileTabButton");
    auto* apiUrlInput = window.findChild<QLineEdit*>("apiUrlInput");
    auto* apiKeyInput = window.findChild<QLineEdit*>("apiKeyInput");
    auto* speedPreference = window.findChild<QComboBox*>("speedPreference");
    auto* volumePreference = window.findChild<QComboBox*>("volumePreference");
    auto* currencyPreference = window.findChild<QComboBox*>("currencyPreference");
    auto* distancePreference = window.findChild<QComboBox*>("distancePreference");
    auto* savePreferences = window.findChild<QPushButton*>("savePreferencesButton");
    if (!settingsNavigation || !preferenceTab || !configTab || !profileTab ||
        !apiUrlInput || !apiKeyInput || !speedPreference ||
        !volumePreference || !currencyPreference || !distancePreference || !savePreferences)
    {
        std::cerr << "Settings navigation or preference controls are missing.\n";
        return 1;
    }
    settingsNavigation->click();
    if (pageStack->currentIndex() != 2 ||
        !settingsNavigation->property("active").toBool() ||
        speedPreference->currentData().toInt() != static_cast<int>(nlsi::SpeedUnit::KilometresPerHour) ||
        volumePreference->currentData().toInt() != static_cast<int>(nlsi::VolumeUnit::Liters) ||
        currencyPreference->currentData().toInt() != static_cast<int>(nlsi::CurrencyUnit::Euro) ||
        distancePreference->currentData().toInt() != static_cast<int>(nlsi::DistanceUnit::Kilometres))
    {
        std::cerr << "Settings page navigation or default selections are incorrect.\n";
        return 1;
    }
    profileTab->click();
    auto* profileProgress = window.findChild<QProgressBar*>("profileRankProgress");
    auto* profileAvatar = window.findChild<QLabel*>("profileAvatar");
    auto* profileBanner = window.findChild<QLabel*>("profileBanner");
    auto* profileName = window.findChild<QLabel*>("profileHeaderName");
    auto* profileUsername = window.findChild<QLabel*>("profileHeaderUsername");
    auto* profileStatusBadge = window.findChild<QLabel*>("profileHeaderStatus");
    auto* profileRoleBadge = window.findChild<QLabel*>("profileHeaderRole");
    auto* refreshProfile = window.findChild<QPushButton*>("refreshProfileButton");
    if (!refreshProfile)
    {
        std::cerr << "The profile refresh action is missing.\n";
        return 1;
    }
    app.processEvents();
    if (!profileAvatar || profileAvatar->size() != QSize(72, 72) ||
        !profileAvatar->parentWidget()->rect().contains(profileAvatar->geometry()))
    {
        std::cerr << "The profile avatar holder is not clipped within its identity layout.\n";
        return 1;
    }
    window.resize(1200, 720);
    app.processEvents();
    if (profileAvatar->size() != QSize(72, 72) ||
        !profileAvatar->parentWidget()->rect().contains(profileAvatar->geometry()))
    {
        std::cerr << "Resizing moved or distorted the profile avatar holder.\n";
        return 1;
    }
    window.resize(900, 600);
    app.processEvents();
    refreshProfile->click();
    if (settingsTabs->currentIndex() != 2 ||
        !window.findChild<QLabel*>("profileStatus") ||
        !window.findChild<QLabel*>("profileStatus")->text().contains("Not connected") ||
        !window.findChild<QLabel*>("headerMode")->text().contains("LOCAL MODE") ||
        apiKeyInput->echoMode() != QLineEdit::Password ||
        !profileProgress || profileProgress->format() != "Not connected" ||
        !profileAvatar || !profileBanner ||
        !profileName || profileName->text() != "Not connected" ||
        !profileUsername || profileUsername->text() != "Not connected" ||
        !profileStatusBadge || profileStatusBadge->text() != "Not connected" ||
        !profileRoleBadge || profileRoleBadge->text() != "Not connected" ||
        window.findChild<QWidget*>("profileDetailsCard"))
    {
        std::cerr << "Profile tab does not show a safe unauthenticated state.\n";
        return 1;
    }
    for (const char* profileField : {
             "profileRankValue", "profileDistanceValue"})
    {
        const auto* value = window.findChild<QLabel*>(profileField);
        if (!value || value->text() != "Not connected")
        {
            std::cerr << "A profile value is missing its disconnected placeholder.\n";
            return 1;
        }
    }
    auto* headerMode = window.findChild<QLabel*>("headerMode");
    auto* pageCompletedNavigation = window.findChild<QPushButton*>("completedJobsNavButton");
    auto* pageDashboardNavigation = window.findChild<QPushButton*>("dashboardNavButton");
    if (!headerMode || !pageCompletedNavigation || !pageDashboardNavigation)
    {
        std::cerr << "A shared connection indicator or application page is missing.\n";
        return 1;
    }
    pageCompletedNavigation->click();
    if (headerMode->text() != "LOCAL MODE")
    {
        std::cerr << "The shared header does not retain LOCAL MODE on completed jobs.\n";
        return 1;
    }
    pageDashboardNavigation->click();
    if (headerMode->text() != "LOCAL MODE")
    {
        std::cerr << "The shared header does not retain LOCAL MODE on the dashboard.\n";
        return 1;
    }
    configTab->click();
    if (settingsTabs->currentIndex() != 1 ||
        !apiUrlInput->text().isEmpty())
    {
        std::cerr << "Config tab does not show the configured API link.\n";
        return 1;
    }
    auto* saveApiConfig = window.findChild<QPushButton*>("saveApiSettingsButton");
    auto* testApiConnection = window.findChild<QPushButton*>("testApiConnectionButton");
    auto* removeApiConfiguration =
        window.findChild<QPushButton*>("removeApiConfigurationButton");
    auto* apiConfigStatus = window.findChild<QLabel*>("apiConfigStatus");
    if (!saveApiConfig || !testApiConnection || !removeApiConfiguration || !apiConfigStatus ||
        saveApiConfig->property("settingsAction").toString() != "primary" ||
        testApiConnection->property("settingsAction").toString() != "secondary" ||
        removeApiConfiguration->property("settingsAction").toString() != "danger")
    {
        std::cerr << "API configuration actions are missing their consistent visual hierarchy.\n";
        return 1;
    }
    apiUrlInput->setText("https://example.test/nlsi-api");
    apiKeyInput->setText("AbC123xYz789");
    saveApiConfig->click();
    nlsi::ApiSettings savedApiSettings;
    QString apiSettingsError;
    nlsi::ProfileStore dashboardProfileStore(dashboardProfilePath);
    if (!dashboardProfileStore.loadApiSettings(savedApiSettings, &apiSettingsError) ||
        savedApiSettings.baseUrl != "https://example.test/nlsi-api" ||
        savedApiSettings.apiKey != "AbC123xYz789" ||
        !window.statusBar()->currentMessage().contains("API CONFIGURATION SAVED"))
    {
        std::cerr << "Save Config did not persist the API URL and key.\n";
        return 1;
    }
    apiUrlInput->clear();
    apiKeyInput->clear();
    testApiConnection->click();
    if (!apiConfigStatus->text().contains("both the API URL and driver API key") ||
        !window.statusBar()->currentMessage().contains("API URL AND DRIVER KEY ARE REQUIRED"))
    {
        std::cerr << "Test Connection did not report missing API credentials.\n";
        return 1;
    }
    preferenceTab->click();
    speedPreference->setCurrentIndex(
        speedPreference->findData(static_cast<int>(nlsi::SpeedUnit::MilesPerHour)));
    volumePreference->setCurrentIndex(
        volumePreference->findData(static_cast<int>(nlsi::VolumeUnit::UsGallons)));
    currencyPreference->setCurrentIndex(
        currencyPreference->findData(static_cast<int>(nlsi::CurrencyUnit::UsDollar)));
    distancePreference->setCurrentIndex(
        distancePreference->findData(static_cast<int>(nlsi::DistanceUnit::Miles)));
    savePreferences->click();
    if (!window.statusBar()->currentMessage().contains("PREFERENCES SAVED"))
    {
        std::cerr << "Saving preferences did not provide success feedback.\n";
        return 1;
    }
    if (!removeApiConfiguration)
    {
        std::cerr << "The local API configuration removal action is missing.\n";
        return 1;
    }
    configTab->click();
    QTimer::singleShot(0, []()
    {
        auto* confirmation = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (confirmation &&
            confirmation->text().contains("does not revoke or delete the server-side key"))
        {
            confirmation->button(QMessageBox::Yes)->click();
        }
        else
        {
            QCoreApplication::exit(3);
        }
    });
    removeApiConfiguration->click();
    if (headerMode->text() != "LOCAL MODE" ||
        !window.findChild<QLabel*>("apiConfigStatus")->text().contains("has not been revoked"))
    {
        std::cerr << "Confirmed local API removal did not retain the server-key distinction.\n";
        return 1;
    }
    nlsi::ApiSettings removedApiSettings;
    nlsi::UserPreferences preservedPreferences;
    QString removalError;
    if (!dashboardProfileStore.loadApiSettings(removedApiSettings, &removalError) ||
        !removedApiSettings.baseUrl.isEmpty() || !removedApiSettings.apiKey.isEmpty() ||
        !dashboardProfileStore.load(preservedPreferences, &removalError) ||
        preservedPreferences.speed != nlsi::SpeedUnit::MilesPerHour)
    {
        std::cerr << "Removing API config failed to preserve local preferences or clear credentials.\n";
        return 1;
    }
    nlsi::TelemetrySnapshot preferenceSnapshot;
    preferenceSnapshot.values["truck.fuel.amount"] = {"Fuel", "20.01", "L", true};
    preferenceSnapshot.values["truck.fuel.capacity"] = {"Fuel Capacity", "100", "L", true};
    preferenceSnapshot.values["truck.cruise_control"] = {"Cruise Control", "25", "m/s", true};
    preferenceSnapshot.values["job.planned_distance_km"] = {"Planned Distance", "16.09344", "km", true};
    preferenceSnapshot.values["truck.navigation.distance"] = {"Navigation Distance", "16093.44", "m", true};
    preferenceSnapshot.values["job.income"] = {"Income", "1234", "game currency", true};
    window.applySnapshot(preferenceSnapshot);
    auto* plannedDistance = window.findChild<QLabel*>("plannedDistanceValue");
    auto* remainingDistance = window.findChild<QLabel*>("remainingDistanceValue");
    auto* selectedFuel = window.findChild<QLabel*>("fuelValue");
    auto* selectedCruise = window.findChild<QLabel*>("cruiseValue");
    auto* selectedIncome = window.findChild<QLabel*>("incomeValue");
    if (!plannedDistance || plannedDistance->text() != "10.00 MI" ||
        !remainingDistance || remainingDistance->text() != "10.00 MI" ||
        !selectedFuel || selectedFuel->text() != "5.29 US GAL / 26.42 US GAL (20.01%)" ||
        !selectedCruise || selectedCruise->text() != "55.92 MPH - A" ||
        !selectedIncome || selectedIncome->text() != "1,234.00 GAME CURRENCY")
    {
        std::cerr << "Dashboard did not apply the selected units without changing currency denomination.\n";
        return 1;
    }

    window.close();
    nlsi::DashboardMainWindow reloadedWindow(nullptr, QString(), dashboardProfilePath,
        [] { return nlsi::GameProcessState::NotRunning; });
    reloadedWindow.applySnapshot(preferenceSnapshot);
    auto* reloadedFuel = reloadedWindow.findChild<QLabel*>("fuelValue");
    auto* reloadedDistance = reloadedWindow.findChild<QLabel*>("plannedDistanceValue");
    auto* reloadedSettingsNavigation =
        reloadedWindow.findChild<QPushButton*>("settingsNavButton");
    if (reloadedSettingsNavigation)
    {
        reloadedSettingsNavigation->click();
    }
    auto* reloadedSpeed = reloadedWindow.findChild<QComboBox*>("speedPreference");
    auto* reloadedVolume = reloadedWindow.findChild<QComboBox*>("volumePreference");
    auto* reloadedCurrency = reloadedWindow.findChild<QComboBox*>("currencyPreference");
    auto* reloadedDistancePreference =
        reloadedWindow.findChild<QComboBox*>("distancePreference");
    if (!reloadedFuel || reloadedFuel->text() != "5.29 US GAL / 26.42 US GAL (20.01%)" ||
        !reloadedDistance || reloadedDistance->text() != "10.00 MI" ||
        !reloadedSpeed ||
        reloadedSpeed->currentData().toInt() != static_cast<int>(nlsi::SpeedUnit::MilesPerHour) ||
        !reloadedVolume ||
        reloadedVolume->currentData().toInt() != static_cast<int>(nlsi::VolumeUnit::UsGallons) ||
        !reloadedCurrency ||
        reloadedCurrency->currentData().toInt() != static_cast<int>(nlsi::CurrencyUnit::UsDollar) ||
        !reloadedDistancePreference ||
        reloadedDistancePreference->currentData().toInt() != static_cast<int>(nlsi::DistanceUnit::Miles))
    {
        std::cerr << "Dashboard did not load saved preferences and selections on restart.\n";
        return 1;
    }
    reloadedWindow.close();

    QTemporaryDir jobDirectory;
    QTemporaryDir historyProfileDirectory;
    nlsi::UserPreferences historyPreferences;
    historyPreferences.speed = nlsi::SpeedUnit::MilesPerHour;
    historyPreferences.volume = nlsi::VolumeUnit::UsGallons;
    historyPreferences.currency = nlsi::CurrencyUnit::Euro;
    historyPreferences.distance = nlsi::DistanceUnit::Miles;
    nlsi::ProfileStore historyProfileStore(historyProfileDirectory.filePath("profile.nlsi"));
    QString profileSaveError;
    if (!historyProfileStore.save(historyPreferences, &profileSaveError))
    {
        std::cerr << "Could not create completed-job preference fixture: "
                  << profileSaveError.toStdString() << ".\n";
        return 1;
    }
    nlsi::DashboardMainWindow historyWindow(
        nullptr, jobDirectory.path(), historyProfileDirectory.filePath("profile.nlsi"),
        [] { return nlsi::GameProcessState::NotRunning; });
    nlsi::TelemetrySnapshot historyPreJob;
    historyPreJob.status = nlsi::ConnectionState::Connected;
    historyPreJob.gameId = "eut2";
    historyPreJob.values["trailer.details"] = {"Trailers", "Owned Flatbed", "", true};
    historyPreJob.values["trailer.license_plate"] = {"Trailer Plate", "OWNED-1", "", true};
    historyWindow.applySnapshot(historyPreJob);
    nlsi::TelemetrySnapshot activeJob;
    activeJob.status = nlsi::ConnectionState::Connected;
    activeJob.gameId = "eut2";
    activeJob.gameName = "Euro Truck Simulator 2";
    activeJob.values["job.income"] = {"Income", "1200", "game currency", true};
    activeJob.values["job.planned_distance_km"] = {"Planned Distance", "100", "km", true};
    activeJob.values["job.driven_distance_km"] = {"Driven Distance", "80", "km", true};
    activeJob.values["job.refueled"] = {"Refueled", "3.785411784", "L", true};
    activeJob.values["job.total_fuel_consumed"] = {"Total Fuel Consumed", "10", "L", true};
    activeJob.values["truck.fuel.average_consumption"] = {"Average Fuel Consumption", "0.05", "L/km", true};
    activeJob.values["job.max_speed_kmh"] = {"Maximum Speed", "90", "km/h", true};
    activeJob.values["job.average_speed_kmh"] = {"Average Speed", "60", "km/h", true};
    activeJob.values["job.status"] = {"Status", "ACTIVE", "", true};
    activeJob.values["job.current"] = {"Current Job", "true", "", true};
    activeJob.values["game.driving_mode"] = {"Driving Mode", "DRIVING", "", true};
    activeJob.values["job.cargo"] = {"Cargo", "steel", "", true};
    activeJob.values["job.source_company"] = {"Origin Company", "tramer", "", true};
    activeJob.values["job.source_city"] = {"From", "berlin", "", true};
    activeJob.values["job.destination_company"] = {"Destination Company", "nbfc", "", true};
    activeJob.values["job.destination_city"] = {"To", "hamburg", "", true};
    activeJob.values["trailer.details"] = {"Trailers", "Assigned Job Trailer", "", true};
    activeJob.values["trailer.license_plate"] = {"Trailer Plate", "JOB-9", "", true};
    historyWindow.applySnapshot(activeJob);
    auto* jobIdValue = historyWindow.findChild<QLabel*>("currentJobValue");
    auto* statusValue = historyWindow.findChild<QLabel*>("statusValue");
    auto* fromValue = historyWindow.findChild<QLabel*>("fromValue");
    auto* toValue = historyWindow.findChild<QLabel*>("toValue");
    if (!jobIdValue || !jobIdValue->text().startsWith("NLSI-") ||
        !statusValue || statusValue->text() != "ACTIVE" ||
        !fromValue || fromValue->text() != "TRAMER - BERLIN" ||
        !toValue || toValue->text() != "NBFC - HAMBURG")
    {
        std::cerr << "The active job ID, status, or origin/destination formatting is incorrect.\n";
        return 1;
    }
    const QString persistentJobId = jobIdValue->text();
    auto transientJob = activeJob;
    transientJob.values["job.status"].available = false;
    transientJob.values["job.status"].value.clear();
    transientJob.values["job.source_company"].available = false;
    transientJob.values["job.source_company"].value.clear();
    transientJob.values["job.source_city"].available = false;
    transientJob.values["job.source_city"].value.clear();
    transientJob.values["job.destination_company"].available = false;
    transientJob.values["job.destination_company"].value.clear();
    transientJob.values["job.destination_city"].available = false;
    transientJob.values["job.destination_city"].value.clear();
    historyWindow.applySnapshot(transientJob);
    if (jobIdValue->text() != persistentJobId)
    {
        std::cerr << "A transiently unavailable job field changed the persistent job ID.\n";
        return 1;
    }
    if (statusValue->text() != "ACTIVE" ||
        fromValue->text() != "TRAMER - BERLIN" ||
        toValue->text() != "NBFC - HAMBURG")
    {
        std::cerr << "Transiently unavailable status or route fields overwrote valid displayed values.\n";
        return 1;
    }
    historyWindow.applySnapshot(activeJob);
    if (statusValue->text() != "ACTIVE")
    {
        std::cerr << "The job status changed during an unchanged telemetry refresh.\n";
        return 1;
    }
    activeJob.values["job.status"].value = "PAUSED";
    activeJob.values["game.driving_mode"].value = "PAUSED";
    historyWindow.applySnapshot(activeJob);
    if (statusValue->text() != "PAUSED" ||
        jobIdValue->text() != QString::fromUtf8("—"))
    {
        std::cerr << "The paused job status or hidden Current Job field is incorrect.\n";
        return 1;
    }
    nlsi::TelemetrySnapshot pausedGameClosed;
    pausedGameClosed.status = nlsi::ConnectionState::Disconnected;
    historyWindow.applySnapshot(pausedGameClosed);
    nlsi::JobHistory pausedJobRecovery(jobDirectory.path());
    if (jobIdValue->text() != QString::fromUtf8("—") ||
        !pausedJobRecovery.activeJob() ||
        pausedJobRecovery.activeJob()->id != persistentJobId.toStdString() ||
        pausedJobRecovery.activeJob()->status != nlsi::JobStatus::Paused)
    {
        std::cerr << "Closing the game while paused hid Current Job or lost its persisted state.\n";
        return 1;
    }
    activeJob.values["job.status"].value = "ACTIVE";
    activeJob.values["game.driving_mode"].value = "DRIVING";
    historyWindow.applySnapshot(activeJob);
    if (jobIdValue->text() != persistentJobId)
    {
        std::cerr << "The Current Job field did not return after driving resumed.\n";
        return 1;
    }

    auto noCurrentJob = activeJob;
    noCurrentJob.values["job.current"].value = "false";
    historyWindow.applySnapshot(noCurrentJob);
    if (jobIdValue->text() != QString::fromUtf8("—"))
    {
        std::cerr << "The Current Job field remained populated when telemetry reported no job.\n";
        return 1;
    }
    noCurrentJob.values["job.current"].value = "true";
    historyWindow.applySnapshot(noCurrentJob);
    nlsi::TelemetrySnapshot closedGame;
    closedGame.status = nlsi::ConnectionState::Disconnected;
    historyWindow.applySnapshot(closedGame);
    nlsi::JobHistory recoveredHistory(jobDirectory.path());
    if (jobIdValue->text() != QString::fromUtf8("—") ||
        !recoveredHistory.activeJob() ||
        recoveredHistory.activeJob()->id != persistentJobId.toStdString() ||
        recoveredHistory.activeJob()->status != nlsi::JobStatus::Active)
    {
        std::cerr << "Closing the game hid Current Job or changed the persisted active record.\n";
        return 1;
    }
    historyWindow.applySnapshot(activeJob);
    if (jobIdValue->text() != persistentJobId)
    {
        std::cerr << "The Current Job field did not recover after the game returned to driving mode.\n";
        return 1;
    }
    auto* completedNavigation = historyWindow.findChild<QPushButton*>("completedJobsNavButton");
    auto* historyDashboardNavigation = historyWindow.findChild<QPushButton*>("dashboardNavButton");
    auto* historyPageStack = historyWindow.findChild<QStackedWidget*>("mainPages");
    if (!completedNavigation || !historyDashboardNavigation || !historyPageStack)
    {
        std::cerr << "Completed Jobs navigation is missing.\n";
        return 1;
    }
    completedNavigation->click();
    if (historyPageStack->currentIndex() != 1 ||
        historyDashboardNavigation->property("active").toBool() ||
        !completedNavigation->property("active").toBool())
    {
        std::cerr << "Completed Jobs navigation did not activate its page exclusively.\n";
        return 1;
    }
    auto* noJobs = historyWindow.findChild<QLabel*>("completedJobsEmpty");
    if (!noJobs || noJobs->text() != "NO COMPLETED OR CANCELLED JOBS YET.")
    {
        std::cerr << "Completed Jobs empty state is incorrect before finalization.\n";
        return 1;
    }
    historyDashboardNavigation->click();
    if (historyPageStack->currentIndex() != 0 ||
        !historyDashboardNavigation->property("active").toBool() ||
        completedNavigation->property("active").toBool())
    {
        std::cerr << "Dashboard navigation did not activate its page exclusively.\n";
        return 1;
    }
    completedNavigation->click();
    auto deliveredJob = activeJob;
    deliveredJob.values["job.status"] = {"Status", "DELIVERED", "", true};
    deliveredJob.values["job.current"] = {"Current Job", "false", "", true};
    deliveredJob.values["job.earned_xp"] = {"XP", "90", "", true};
    historyWindow.applySnapshot(deliveredJob);
    app.processEvents();
    const auto completedCards = historyWindow.findChildren<QPushButton*>("completedJobCard");
    if (completedCards.size() != 1 ||
        !completedCards.front()->property("jobId").toString().startsWith("NLSI-"))
    {
        std::cerr << "Delivered job was not automatically added exactly once to Completed Jobs.\n";
        return 1;
    }
    bool gameCurrencyPreserved = false;
    for (QLabel* label : completedCards.front()->findChildren<QLabel*>())
    {
        gameCurrencyPreserved = gameCurrencyPreserved ||
            label->text().contains("€1,200.00");
    }
    if (!gameCurrencyPreserved)
    {
        std::cerr << "ETS2 completed-job card did not show native EUR income.\n";
        return 1;
    }
    bool completedCardValuesFormatted = false;
    for (QLabel* label : completedCards.front()->findChildren<QLabel*>())
    {
        if (label->text().startsWith("CARGO "))
        {
            completedCardValuesFormatted =
                label->text() == label->text().toUpper() && label->font().bold();
        }
    }
    if (!completedCardValuesFormatted)
    {
        std::cerr << "Completed-job card values are not uppercase and bold.\n";
        return 1;
    }
    auto cancelledJob = activeJob;
    cancelledJob.values["job.cargo"].value = "Lumber";
    historyWindow.applySnapshot(cancelledJob);
    cancelledJob.values["job.status"] = {"Status", "CANCELLED", "", true};
    cancelledJob.values["job.current"] = {"Current Job", "false", "", true};
    historyWindow.applySnapshot(cancelledJob);
    app.processEvents();
    if (historyWindow.findChildren<QPushButton*>("completedJobCard").size() != 2)
    {
        std::cerr << "Cancelled jobs are not shown in Completed Jobs.\n";
        return 1;
    }

    bool detailDialogShowedFields = false;
    bool completedMeasurementsConverted = false;
    QTimer::singleShot(0, [&]()
    {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (dialog)
        {
            const auto labels = dialog->findChildren<QLabel*>();
            bool hasFuelCost = false;
            bool fuelCostUnavailable = false;
            bool hasStatus = false;
            bool hasPdfExport = dialog->findChild<QPushButton*>("exportJobPdfButton") != nullptr;
            bool hasCurrency = false;
            bool hasDistance = false;
            bool hasFuel = false;
            bool hasSpeed = false;
            bool hasCapturedTrailer = false;
            bool hasCapturedTrailerPlate = false;
            bool fieldValuesFormatted = true;
            for (QLabel* label : labels)
            {
                if (label->objectName() == "telemetryLabel" &&
                    (label->text() != label->text().toUpper() || !label->font().bold()))
                {
                    fieldValuesFormatted = false;
                }
                if (label->text() == "FUEL COST")
                {
                    hasFuelCost = true;
                    auto* value = label->parentWidget()->findChild<QLabel*>("telemetryValue");
                    fuelCostUnavailable = value && value->text() == QString::fromUtf8("—");
                }
                hasStatus = hasStatus || label->text() == "JOB STATUS";
                if (label->objectName() == "telemetryLabel")
                {
                    auto* fieldValue = label->parentWidget()->findChild<QLabel*>("telemetryValue");
                    if (label->text() == "INCOME")
                    {
                        hasCurrency = fieldValue && fieldValue->text() == "€1,200.00";
                    }
                    else if (label->text() == "PLANNED DISTANCE")
                    {
                        hasDistance = fieldValue && fieldValue->text() == "62.14 MI";
                    }
                    else if (label->text() == "REFUELED")
                    {
                        hasFuel = fieldValue && fieldValue->text() == "1.00 US GAL";
                    }
                    else if (label->text() == "MAX SPEED")
                    {
                        hasSpeed = fieldValue && fieldValue->text() == "55.92 MPH";
                    }
                    else if (label->text() == "TRAILER")
                    {
                        hasCapturedTrailer = fieldValue &&
                            fieldValue->text() == "ASSIGNED JOB TRAILER";
                    }
                    else if (label->text() == "TRAILER PLATE")
                    {
                        hasCapturedTrailerPlate = fieldValue && fieldValue->text() == "JOB-9";
                    }
                }
                if (label->objectName() == "telemetryValue" &&
                    (label->text() != label->text().toUpper() || !label->font().bold()))
                {
                    fieldValuesFormatted = false;
                }
            }
            detailDialogShowedFields = hasFuelCost && fuelCostUnavailable &&
                hasStatus && hasPdfExport && fieldValuesFormatted &&
                hasCapturedTrailer && hasCapturedTrailerPlate;
            if (!detailDialogShowedFields)
            {
                std::cerr << "Completed-job detail values or captured trailer data are incorrect.\n";
            }
            completedMeasurementsConverted =
                hasCurrency && hasDistance && hasFuel && hasSpeed;
            dialog->accept();
        }
    });
    QPushButton* capturedTrailerCard = nullptr;
    for (QPushButton* card : historyWindow.findChildren<QPushButton*>("completedJobCard"))
    {
        if (card->property("jobId").toString() == "NLSI-0001")
        {
            capturedTrailerCard = card;
            break;
        }
    }
    if (!capturedTrailerCard)
    {
        std::cerr << "The completed card for the captured-trailer job is missing.\n";
        return 1;
    }
    capturedTrailerCard->click();
    if (!detailDialogShowedFields)
    {
        std::cerr << "Selecting a completed-job card did not show the full detail layout.\n";
        return 1;
    }
    if (!completedMeasurementsConverted)
    {
        std::cerr << "Completed-job detail measurements do not use the saved preferences.\n";
        return 1;
    }

    QTemporaryDir reportDirectory;
    const QString reportPath = reportDirectory.filePath("completed-job.pdf");
    QString reportError;
    if (!nlsi::writePdfReport(reportPath,
            "<html><body><h1>NLSI Exclusive Logbook</h1><p>62.14 MI</p></body></html>",
            &reportError))
    {
        std::cerr << "Completed-job PDF output failed: " << reportError.toStdString() << ".\n";
        return 1;
    }
    QFile reportFile(reportPath);
    if (!reportFile.open(QIODevice::ReadOnly) || !reportFile.read(5).startsWith("%PDF-"))
    {
        std::cerr << "Completed-job PDF export did not produce a PDF document.\n";
        return 1;
    }
    historyWindow.close();

    const QString integrationProfilePath =
        qEnvironmentVariable("NLSI_API_INTEGRATION_PROFILE");
    if (!integrationProfilePath.isEmpty())
    {
        QTemporaryDir integrationData;
        if (!integrationData.isValid())
        {
            std::cerr << "Could not create isolated API integration-test data.\n";
            return 1;
        }
        QStringList integrationMessages;
        integrationLogMessages = &integrationMessages;
        const QtMessageHandler originalMessageHandler =
            qInstallMessageHandler(captureIntegrationLog);
        nlsi::DashboardMainWindow integrationWindow(
            nullptr, integrationData.filePath("jobs"), integrationProfilePath,
            [] { return nlsi::GameProcessState::NotRunning; });
        auto* username = integrationWindow.findChild<QLabel*>("profileHeaderUsername");
        auto* displayName = integrationWindow.findChild<QLabel*>("profileHeaderName");
        auto* accountStatus = integrationWindow.findChild<QLabel*>("profileHeaderStatus");
        auto* role = integrationWindow.findChild<QLabel*>("profileHeaderRole");
        auto* rank = integrationWindow.findChild<QLabel*>("profileRankValue");
        auto* profileStatus = integrationWindow.findChild<QLabel*>("profileStatus");
        auto* integrationTaskProgress =
            integrationWindow.findChild<QProgressBar*>("profileTaskProgress");
        auto* integrationTaskStatus =
            integrationWindow.findChild<QLabel*>("profileTaskStatus");
        auto* connectionMode = integrationWindow.findChild<QLabel*>("headerMode");
        QEventLoop integrationLoop;
        QTimer pollTimer;
        pollTimer.setInterval(100);
        QObject::connect(&pollTimer, &QTimer::timeout, &integrationLoop, [&]()
        {
            const bool taskLoaded = integrationTaskStatus &&
                (integrationTaskStatus->text().startsWith("No task record is initialized") ||
                 integrationTaskStatus->text().contains(" completed of "));
            if (profileStatus &&
                profileStatus->text().contains("Server jobs refreshed") && taskLoaded)
            {
                integrationLoop.quit();
            }
        });
        pollTimer.start();
        QTimer::singleShot(30000, &integrationLoop, &QEventLoop::quit);
        integrationLoop.exec();
        qInstallMessageHandler(originalMessageHandler);
        integrationLogMessages = nullptr;
        const bool validRank = rank &&
            (rank->text() == "Trainee" || rank->text() == "Driver" ||
             rank->text() == "Elder" || rank->text() == "Veteran" ||
             rank->text() == "Expert" || rank->text() == "Legend" ||
             rank->text() == "Master" || rank->text() == "Ultimate" ||
             rank->text() == "Mythic");
        const QString roleValue = role ? role->text().section(QString::fromUtf8(" • "), 0, 0) : QString();
        if (!username || !username->text().startsWith('@') ||
            username->text().size() <= 1 ||
            !displayName || displayName->text().isEmpty() ||
            !accountStatus || accountStatus->text() != "Active" ||
            !role || !nlsi::isSupportedAccountRole(roleValue) || !validRank ||
            !profileStatus || !profileStatus->text().contains("Server jobs refreshed") ||
            !integrationTaskProgress || !integrationTaskStatus ||
            !(integrationTaskStatus->text().startsWith("No task record is initialized") ||
              integrationTaskStatus->text().contains(" completed of ")) ||
            (integrationTaskStatus->text().startsWith("No task record is initialized")
                ? integrationTaskProgress->isEnabled() ||
                    integrationTaskProgress->format() != "NO TASK RECORD"
                : !integrationTaskProgress->isEnabled() ||
                    !integrationTaskProgress->format().startsWith("TASK %p% | ")) ||
            !connectionMode || connectionMode->text() != "ONLINE")
        {
            std::cerr << "Actual API integration failed validation: "
                      << "username=" << (username && username->text().startsWith('@'))
                      << " display_name=" << (displayName && !displayName->text().isEmpty())
                      << " account_status=" << (accountStatus && accountStatus->text() == "Active")
                      << " role=" << nlsi::isSupportedAccountRole(roleValue)
                      << " rank=" << validRank
                      << " job_refresh=" << (profileStatus &&
                          profileStatus->text().contains("Server jobs refreshed"))
                      << " task_loaded=" << (integrationTaskStatus &&
                          (integrationTaskStatus->text().startsWith(
                               "No task record is initialized") ||
                           integrationTaskStatus->text().contains(" completed of ")))
                      << " connection="
                      << (connectionMode ? connectionMode->text().toStdString() : "missing")
                      << " footer="
                      << integrationWindow.statusBar()->currentMessage().toStdString()
                      << " profile_status="
                      << (profileStatus ? profileStatus->text().toStdString() : "missing")
                      << ".\n";
            for (const QString& diagnostic : integrationMessages)
            {
                if (diagnostic.startsWith("NLSI API response failure:"))
                {
                    std::cerr << diagnostic.toStdString() << "\n";
                }
            }
            return 1;
        }
        integrationWindow.close();
        std::cout << "Actual API profile and read-only job refresh passed.\n";
    }

    std::cout << "Dashboard minimum size, section order, and vertical scrolling passed.\n";
    return 0;
}
