#include "app/AppPaths.h"

#include <QStandardPaths>
#include <QDir>

namespace {
QString g_configDir;
QString g_logDir;
bool    g_initialized = false;
} // namespace

namespace AppPaths {

void ensureInitialized()
{
    if (g_initialized) return;

    g_configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (g_configDir.isEmpty())
        g_configDir = QDir::homePath() + "/.audio-speed-changer";

    g_logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
    if (g_logDir.isEmpty() || g_logDir == "/logs")
        g_logDir = g_configDir + "/logs";

    QDir().mkpath(g_configDir);
    QDir().mkpath(g_logDir);

    g_initialized = true;
}

QString configDir() { return g_configDir; }
QString logDir()    { return g_logDir; }

QString logFilePath()
{
    return logDir() + "/app.log";
}

QString defaultSourceDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
}

QString defaultTargetDir()
{
    return defaultSourceDir().isEmpty()
    ? QDir::homePath() + "/AudioSpeedChangerOutput"
    : defaultSourceDir() + "/converted";
}

} // namespace AppPaths
