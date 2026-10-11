#pragma once

#include <QObject>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <memory>

class Logger : public QObject
{
    Q_OBJECT
public:
    enum class Level { Debug, Info, Warning, Error };

    static Logger& instance();

    void initialize(const QString& logFilePath);
    void setMinLevel(Level level) { m_minLevel = level; }

    void log(Level lvl, const QString& message);

signals:
    void newMessage(Logger::Level level, const QString& text);

private:
    explicit Logger(QObject* parent = nullptr);
    ~Logger() override;

    void writeToFile(Level lvl, const QString& message);

    Level m_minLevel = Level::Info;
    std::unique_ptr<QFile>       m_file;
    std::unique_ptr<QTextStream> m_stream;
};
