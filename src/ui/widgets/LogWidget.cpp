#include "ui/widgets/LogWidget.h"

#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QDateTime>

namespace {

QString colorFor(Logger::Level l)
{
    switch (l) {
    case Logger::Level::Debug:   return "#7f8c8d";
    case Logger::Level::Info:    return "#2c3e50";
    case Logger::Level::Warning: return "#d35400";
    case Logger::Level::Error:   return "#c0392b";
    }
    return "#000000";
}

QString escapeHtml(const QString& s)
{
    QString r = s;
    r.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;");
    return r;
}

} // namespace

LogWidget::LogWidget(QWidget* parent) : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);

    m_view = new QPlainTextEdit(this);
    m_view->setReadOnly(true);
    m_view->setMaximumBlockCount(5000);
    m_view->setLineWrapMode(QPlainTextEdit::NoWrap);
    lay->addWidget(m_view);

    // Подписываемся на Logger один раз на уровне виджета — это безопасно,
    // сигнал приходит в главном потоке (логгер — singleton в main-потоке).
    connect(&Logger::instance(), &Logger::newMessage,
            this, &LogWidget::append, Qt::QueuedConnection);
}

void LogWidget::append(Logger::Level level, const QString& message)
{
    const QString time = QDateTime::currentDateTime().toString("HH:mm:ss");
    const QString html = QString(
                             "<span style='color:#95a5a6;'>[%1]</span> "
                             "<span style='color:%2;'>%3</span>")
                             .arg(time, colorFor(level), escapeHtml(message));

    m_view->appendHtml(html);
}

void LogWidget::clear()
{
    m_view->clear();
}
