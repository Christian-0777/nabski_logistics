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
    app.setApplicationVersion("1.7.9 Beta");

    const QString iconPath = QFileInfo(QDir::currentPath(), "resources/nlsi.ico").exists()
        ? QDir::currentPath() + "/resources/nlsi.ico"
        : QCoreApplication::applicationDirPath() + "/resources/nlsi.ico";
    app.setWindowIcon(QIcon(iconPath));

    nlsi::DashboardMainWindow window;
    window.show();
    return app.exec();
}
