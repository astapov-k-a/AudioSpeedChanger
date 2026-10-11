#pragma once

#include <QWidget>

class QLineEdit;
class QToolButton;
class QLabel;

class PathSelector : public QWidget
{
    Q_OBJECT
public:
    enum Mode { SourceDirectory, TargetDirectory };

    explicit PathSelector(Mode mode, QWidget* parent = nullptr);

    QString path() const;
    void    setPath(const QString& p);
    void    setDialogTitle(const QString& t) { m_dialogTitle = t; }

signals:
    void pathChanged(const QString& path);
    void pathSelected(const QString& path);   // пользователь выбрал через диалог

private slots:
    void onBrowse();
    void onOpen();
    void onRefresh();
    void onTextEdited(const QString& text);

private:
    void validate();

    Mode          m_mode;
    QString       m_dialogTitle;
    QLabel*       m_icon        = nullptr;
    QLineEdit*    m_edit        = nullptr;
    QToolButton*  m_browseBtn   = nullptr;
    QToolButton*  m_openBtn     = nullptr;
    QToolButton*  m_refreshBtn  = nullptr;
};
