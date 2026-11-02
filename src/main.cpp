#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QMessageBox>

#include "DashboardMainWindow.h"
#include "GameProcessDetection.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setApplicationName("NLSI Exclusive Logbook");
    app.setApplicationDisplayName("NLSI Exclusive Logbook");
    app.setApplicationVersion("1.9.4 Beta");

    const nlsi::GameProcessDetectionResult detection = nlsi::detectGameProcesses();
    if (!nlsi::gameProcessStateAllowsStartup(detection.state))
    {
        const QString message = detection.state == nlsi::GameProcessState::DetectionError
            ? detection.windowsErrorCode != 0
                ? QString("Process enumeration failed with Windows error %1. "
                          "Startup has been blocked because the game state cannot be verified.")
                      .arg(detection.windowsErrorCode)
                : "Windows process enumeration is unavailable. Startup has been blocked because "
                  "the game state cannot be verified."
            : "NLSI Exclusive Logbook cannot be opened while a supported game is running. "
              "Please close Euro Truck Simulator 2 or American Truck Simulator first.";
        QMessageBox::warning(nullptr, "Application cannot be opened", message);
        return 1;
    }

    QString isolatedDataDirectory;
    const QString smokeArgumentPrefix = "--isolated-smoke-data-dir=";
    for (const QString& argument : app.arguments())
    {
        if (argument.startsWith(smokeArgumentPrefix))
        {
            isolatedDataDirectory = argument.mid(smokeArgumentPrefix.size());
            break;
        }
    }
    QString jobHistoryDirectory;
    QString profileFilePath;
    if (!isolatedDataDirectory.isEmpty())
    {
        QDir dataDirectory(isolatedDataDirectory);
        if (!dataDirectory.mkpath("."))
        {
            return 1;
        }
        jobHistoryDirectory = dataDirectory.filePath("jobs");
        profileFilePath = dataDirectory.filePath("profile.nlsi");
    }

    const QString iconPath = QFileInfo(QDir::currentPath(), "resources/nlsi.ico").exists()
        ? QDir::currentPath() + "/resources/nlsi.ico"
        : QCoreApplication::applicationDirPath() + "/resources/nlsi.ico";
    app.setWindowIcon(QIcon(iconPath));

    nlsi::DashboardMainWindow window(nullptr, jobHistoryDirectory, profileFilePath);
    window.show();
    return app.exec();
}
