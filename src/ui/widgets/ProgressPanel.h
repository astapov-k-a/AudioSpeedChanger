#pragma once

#include <QWidget>

class QProgressBar;
class QLabel;
class QPushButton;

class ProgressPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ProgressPanel(QWidget* parent = nullptr);

    void reset();
    void setOverall(int done, int total);
    void setCurrentFile(const QString& name);
    void setCurrentProgress(int percent);
    void setEta(double seconds);
    void setRunning(bool running);
    void setPaused(bool paused);

signals:
    void pauseRequested();
    void resumeRequested();
    void cancelRequested();

private slots:
    void onPauseClicked();

private:
    QProgressBar* m_overall  = nullptr;
    QProgressBar* m_current  = nullptr;
    QLabel*       m_overallLbl = nullptr;
    QLabel*       m_currentLbl = nullptr;
    QLabel*       m_etaLbl     = nullptr;
    QPushButton*  m_pauseBtn   = nullptr;
    QPushButton*  m_cancelBtn  = nullptr;

    bool m_paused = false;
};
