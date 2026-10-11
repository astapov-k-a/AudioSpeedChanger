#include "utils/Logger.h"
#include "app/AppPaths.h"

#include <QDateTime>
#include <QDir>

namespace {
QString levelToString(Logger::Level l) {
    switch (l) {
    case Logger::Level::Debug:   return QStringLiteral("DEBUG");
    case Logger::Level::Info:    return QStringLiteral("INFO ");
    case Logger::Level::Warning: return QStringLiteral("WARN ");
    case Logger::Level::Error:   return QStringLiteral("ERROR");
    }
    return QStringLiteral("INFO ");
}
} // namespace

Logger& Logger::instance()
{
    static Logger inst;
    return inst;
}

Logger::Logger(QObject* parent) : QObject(parent) {}

Logger::~Logger() = default;

void Logger::initialize(const QString& logFilePath)
{
    QDir().mkpath(QFileInfo(logFilePath).absolutePath());
    m_file = std::make_unique<QFile>(logFilePath);
    if (m_file->open(QIODevice::Append | QIODevice::Text)) {
        m_stream = std::make_unique<QTextStream>(m_file.get());
    }
}

void Logger::log(Level lvl, const QString& message)
{
    if (static_cast<int>(lvl) < static_cast<int>(m_minLevel))
        return;

    writeToFile(lvl, message);
    emit newMessage(lvl, message);
}

void Logger::writeToFile(Level lvl, const QString& message)
{
    if (!m_stream)
        return;
    const QString line = QString("[%1] [%2] %3")
                             .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"),
                                  levelToString(lvl),
                                  message);
    (*m_stream) << line << '\n';
    m_stream->flush();
}
