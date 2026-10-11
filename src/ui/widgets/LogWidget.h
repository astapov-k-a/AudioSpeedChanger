#pragma once

#include <QWidget>
#include "utils/Logger.h"

class QPlainTextEdit;

class LogWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LogWidget(QWidget* parent = nullptr);

public slots:
    void append(Logger::Level level, const QString& message);
    void clear();

private:
    QPlainTextEdit* m_view = nullptr;
};
