#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>

#include "core/Core.h"

class AppSettings : public QObject
{
    Q_OBJECT
public:
    explicit AppSettings(QObject* parent = nullptr);

    // ---- Папки ----
    QString sourceDir() const;
    void    setSourceDir(const QString& path);

    QString targetDir() const;
    void    setTargetDir(const QString& path);

    bool    includeSubdirs() const;
    void    setIncludeSubdirs(bool v);

    QString extensionFilter() const; // "*.wav;*.mp3;..."
    void    setExtensionFilter(const QString& f);

    // ---- Параметры обработки ----
    ProcessingParameters parameters() const;
    void                 setParameters(const ProcessingParameters& p);

    // ---- Прочее ----
    bool    darkTheme() const;
    void    setDarkTheme(bool v);

    // ---- Геометрия окна ----
    QByteArray windowGeometry() const;
    void       setWindowGeometry(const QByteArray& g);

    QByteArray windowState() const;
    void       setWindowState(const QByteArray& s);
};
