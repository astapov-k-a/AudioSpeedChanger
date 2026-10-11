#pragma once

#include <QMainWindow>
#include <QElapsedTimer>
#include "core/Core.h"
#include "utils/Result.h"
#include "utils/Logger.h"

class AppSettings;
class AudioSpeedChanger;
class PathSelector;
class ParameterPanel;
class ProgressPanel;
class LogWidget;
class FileListModel;
class FileListProxyModel;

class QTableView;
class QCheckBox;
class QToolBar;
class QAction;
class QTimer;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(AppSettings* settings, QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private slots:
    void onOpenSource();
    void onOpenTarget();
    void onRefreshFiles();
    void onSourceChanged(const QString& path);
    void onParametersChanged();

    void onStart();
    void onPause();
    void onResume();
    void onCancel();

    void onProcessStarted(int total);
    void onFileStarted(int jobId, const QString& displayName);
    void onFileProgress(int jobId, int percent);
    void onFileFinished(int jobId, ProcessingStatus status, const QString& detail);
    void onOverallProgress(int done, int total);
    void onProcessFinished(int ok, int failed, int skipped);
    void onProcessError(const AudioError& err);

    void onOpenSettings();
    void onAbout();
    void onSelectAllFiles();
    void onDeselectAllFiles();

private:
    void createActions();
    void createMenus();
    void createToolBar();
    void createCentralLayout();
    void createStatusBar();
    void restoreWindowState();
    void saveWindowState();

    void loadFilesFromSource();
    void updateUiForRunning(bool running);
    void appendLog(const QString& msg, Logger::Level lvl = Logger::Level::Info);
    double computeEta() const;

    AppSettings*          m_settings     = nullptr;
    AudioSpeedChanger*    m_changer      = nullptr;

    PathSelector*         m_sourcePath   = nullptr;
    PathSelector*         m_targetPath   = nullptr;
    QCheckBox*            m_recursive    = nullptr;

    ParameterPanel*       m_params       = nullptr;
    ProgressPanel*        m_progress     = nullptr;
    LogWidget*            m_log          = nullptr;

    FileListModel*        m_model        = nullptr;
    FileListProxyModel*   m_proxy        = nullptr;
    QTableView*           m_table        = nullptr;

    QAction*              m_actOpenSrc   = nullptr;
    QAction*              m_actOpenDst   = nullptr;
    QAction*              m_actRefresh   = nullptr;
    QAction*              m_actStart     = nullptr;
    QAction*              m_actPause     = nullptr;
    QAction*              m_actCancel    = nullptr;
    QAction*              m_actSettings  = nullptr;
    QAction*              m_actAbout     = nullptr;
    QAction*              m_actSelectAll = nullptr;
    QAction*              m_actDeselectAll = nullptr;
    QAction*              m_actExit      = nullptr;

    QElapsedTimer         m_runTimer;
    QTimer*               m_etaTimer     = nullptr;
    double                m_lastEtaSec   = -1.0;
    int                   m_lastDone     = 0;
    int                   m_lastTotal    = 0;
};
