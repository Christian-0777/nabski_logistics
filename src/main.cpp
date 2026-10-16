#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>

#include "DashboardMainWindow.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setApplicationName("NLSI Exclusive Logbook");
    app.setApplicationDisplayName("NLSI Exclusive Logbook");
    app.setApplicationVersion("1.8.5 Beta");

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
