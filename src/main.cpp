#include "MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>

int main(int argc, char *argv[]) {
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Wsalauncher"));
    QApplication::setOrganizationName(QStringLiteral("wsamiaw"));

    // Prefer plugins/DLLs next to the .exe (portable Windows / macOS deploy)
    const QString appDir = QCoreApplication::applicationDirPath();
    QCoreApplication::addLibraryPath(appDir);
    QCoreApplication::addLibraryPath(appDir + QStringLiteral("/plugins"));

    MainWindow w;
    w.show();
    return app.exec();
}
