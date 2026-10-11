#include <QApplication>
#include <QFile>
#include <QDir>
#include <QStandardPaths>

#include "app/AppPaths.h"
#include "app/AppSettings.h"
#include "ui/MainWindow.h"
#include "utils/Logger.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("AudioSpeedChanger");
    QCoreApplication::setApplicationName("AudioSpeedChanger");
    QCoreApplication::setApplicationVersion("0.1.0");

    // Инициализация путей (создаёт каталоги конфигов/логов при необходимости).
    AppPaths::ensureInitialized();

    // Инициализация логгера: файл + сигналы в UI.
    Logger::instance().initialize(AppPaths::logFilePath());

    AppSettings settings;

    MainWindow w(&settings);
    w.show();

    return app.exec();
}
