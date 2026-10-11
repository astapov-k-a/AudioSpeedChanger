#include "ui/MainWindow.h"

#include "app/AppSettings.h"
#include "core/Core.h"
#include "ui/AboutDialog.h"
#include "ui/SettingsDialog.h"
#include "ui/models/FileListModel.h"
#include "ui/models/FileListProxyModel.h"
#include "ui/widgets/LogWidget.h"
#include "ui/widgets/ParameterPanel.h"
#include "ui/widgets/PathSelector.h"
#include "ui/widgets/ProgressPanel.h"
#include "utils/Logger.h"

#include <QApplication>
#include <QAction>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QTableView>
#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QTimer>
#include <QCloseEvent>
#include <QMessageBox>
#include <QDir>

MainWindow::MainWindow(AppSettings* settings, QWidget* parent)
    : QMainWindow(parent)
    , m_settings(settings)
    , m_changer(new AudioSpeedChanger(this))
    , m_etaTimer(new QTimer(this))
{
    setWindowTitle(tr("Audio Speed Changer"));
    setAcceptDrops(true);
    setMinimumSize(900, 620);

    createActions();
    createMenus();
    createToolBar();
    createCentralLayout();
    createStatusBar();

    connect(m_changer, &AudioSpeedChanger::started,        this, &MainWindow::onProcessStarted);
    connect(m_changer, &AudioSpeedChanger::fileStarted,    this, &MainWindow::onFileStarted);
    connect(m_changer, &AudioSpeedChanger::fileProgress,   this, &MainWindow::onFileProgress);
    connect(m_changer, &AudioSpeedChanger::fileFinished,   this, &MainWindow::onFileFinished);
    connect(m_changer, &AudioSpeedChanger::overallProgress,this, &MainWindow::onOverallProgress);
    connect(m_changer, &AudioSpeedChanger::finished,       this, &MainWindow::onProcessFinished);
    connect(m_changer, &AudioSpeedChanger::error,          this, &MainWindow::onProcessError);

    m_etaTimer->setInterval(1000);
    connect(m_etaTimer, &QTimer::timeout, this, [this]() {
        if (m_changer->isRunning()) {
            m_progress->setEta(computeEta());
        }
    });

    restoreWindowState();

    // Первичная загрузка
    if (!m_settings->sourceDir().isEmpty()) m_sourcePath->setPath(m_settings->sourceDir());
    if (!m_settings->targetDir().isEmpty()) m_targetPath->setPath(m_settings->targetDir());
    m_params->setParameters(m_settings->parameters());
    if (!m_sourcePath->path().isEmpty()) loadFilesFromSource();

    appendLog(tr("Приложение запущено."));
}

MainWindow::~MainWindow() = default;

// ---------- actions / menus / toolbar ----------

void MainWindow::createActions()
{
    m_actOpenSrc = new QAction(style()->standardIcon(QStyle::SP_DirOpenIcon),
                               tr("Открыть папку A…"), this);
    m_actOpenSrc->setShortcut(QKeySequence("Ctrl+O"));
    connect(m_actOpenSrc, &QAction::triggered, this, &MainWindow::onOpenSource);

    m_actOpenDst = new QAction(style()->standardIcon(QStyle::SP_DirOpenIcon),
                               tr("Открыть папку Б…"), this);
    m_actOpenDst->setShortcut(QKeySequence("Ctrl+S"));
    connect(m_actOpenDst, &QAction::triggered, this, &MainWindow::onOpenTarget);

    m_actRefresh = new QAction(style()->standardIcon(QStyle::SP_BrowserReload),
                               tr("Обновить список"), this);
    m_actRefresh->setShortcut(QKeySequence("F5"));
    connect(m_actRefresh, &QAction::triggered, this, &MainWindow::onRefreshFiles);

    m_actStart = new QAction(style()->standardIcon(QStyle::SP_MediaPlay),
                             tr("Старт"), this);
    m_actStart->setShortcut(QKeySequence("Ctrl+R"));
    connect(m_actStart, &QAction::triggered, this, &MainWindow::onStart);

    m_actPause = new QAction(style()->standardIcon(QStyle::SP_MediaPause),
                             tr("Пауза"), this);
    m_actPause->setEnabled(false);
    connect(m_actPause, &QAction::triggered, this, &MainWindow::onPause);

    m_actCancel = new QAction(style()->standardIcon(QStyle::SP_MediaStop),
                              tr("Отмена"), this);
    m_actCancel->setShortcut(QKeySequence("Esc"));
    m_actCancel->setEnabled(false);
    connect(m_actCancel, &QAction::triggered, this, &MainWindow::onCancel);

    m_actSettings = new QAction(tr("Настройки…"), this);
    m_actSettings->setShortcut(QKeySequence("Ctrl+,"));
    connect(m_actSettings, &QAction::triggered, this, &MainWindow::onOpenSettings);

    m_actAbout = new QAction(tr("О программе…"), this);
    connect(m_actAbout, &QAction::triggered, this, &MainWindow::onAbout);

    m_actSelectAll = new QAction(tr("Выбрать все"), this);
    m_actSelectAll->setShortcut(QKeySequence("Ctrl+A"));
    connect(m_actSelectAll, &QAction::triggered, this, &MainWindow::onSelectAllFiles);

    m_actDeselectAll = new QAction(tr("Снять выделение"), this);
    connect(m_actDeselectAll, &QAction::triggered, this, &MainWindow::onDeselectAllFiles);

    m_actExit = new QAction(tr("Выход"), this);
    m_actExit->setShortcut(QKeySequence::Quit);
    connect(m_actExit, &QAction::triggered, this, &QWidget::close);
}

void MainWindow::createMenus()
{
    auto* fileMenu = menuBar()->addMenu(tr("Файл"));
    fileMenu->addAction(m_actOpenSrc);
    fileMenu->addAction(m_actOpenDst);
    fileMenu->addAction(m_actRefresh);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actSettings);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actExit);

    auto* editMenu = menuBar()->addMenu(tr("Правка"));
    editMenu->addAction(m_actSelectAll);
    editMenu->addAction(m_actDeselectAll);

    auto* procMenu = menuBar()->addMenu(tr("Обработка"));
    procMenu->addAction(m_actStart);
    procMenu->addAction(m_actPause);
    procMenu->addAction(m_actCancel);

    auto* helpMenu = menuBar()->addMenu(tr("Помощь"));
    helpMenu->addAction(m_actAbout);
}

void MainWindow::createToolBar()
{
    auto* tb = addToolBar(tr("Панель инструментов"));
    tb->setObjectName("mainToolBar");
    tb->setMovable(false);
    tb->addAction(m_actOpenSrc);
    tb->addAction(m_actOpenDst);
    tb->addAction(m_actRefresh);
    tb->addSeparator();
    tb->addAction(m_actStart);
    tb->addAction(m_actPause);
    tb->addAction(m_actCancel);
    tb->addSeparator();
    tb->addAction(m_actSettings);
}

// ---------- центральный layout ----------

void MainWindow::createCentralLayout()
{
    auto* splitter = new QSplitter(Qt::Vertical, this);

    // ===== Верх: пути + рекурсивно =====
    auto* pathsBox = new QGroupBox(tr("Папки"), splitter);
    auto* pathsLay = new QVBoxLayout(pathsBox);

    m_sourcePath = new PathSelector(PathSelector::SourceDirectory, pathsBox);
    m_targetPath = new PathSelector(PathSelector::TargetDirectory, pathsBox);

    m_recursive = new QCheckBox(tr("Включая подпапки"), pathsBox);
    m_recursive->setChecked(m_settings->includeSubdirs());

    auto* srcRow = new QHBoxLayout();
    srcRow->addWidget(new QLabel(tr("Источник (A):"), pathsBox));
    srcRow->addWidget(m_sourcePath, 1);

    auto* dstRow = new QHBoxLayout();
    dstRow->addWidget(new QLabel(tr("Назначение (Б):"), pathsBox));
    dstRow->addWidget(m_targetPath, 1);

    pathsLay->addLayout(srcRow);
    pathsLay->addLayout(dstRow);
    pathsLay->addWidget(m_recursive);

    // ===== Средний: параметры + файлы =====
    auto* midWidget = new QWidget(splitter);
    auto* midLay = new QHBoxLayout(midWidget);
    midLay->setContentsMargins(0, 0, 0, 0);

    m_params = new ParameterPanel(midWidget);
    m_params->setMinimumWidth(360);

    auto* filesBox = new QGroupBox(tr("Файлы"), midWidget);
    auto* filesLay = new QVBoxLayout(filesBox);

    m_model = new FileListModel(this);
    m_proxy = new FileListProxyModel(this);
    m_proxy->setSourceModel(m_model);

    m_table = new QTableView(filesBox);
    m_table->setModel(m_proxy);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(FileListModel::ColName, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(FileListModel::ColFormat, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(FileListModel::ColDuration, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(FileListModel::ColSize, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(FileListModel::ColStatus, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(FileListModel::ColProgress, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(FileListModel::ColChecked, QHeaderView::ResizeToContents);
    m_table->setSortingEnabled(true);

    filesLay->addWidget(m_table, 1);

    midLay->addWidget(m_params, 0);
    midLay->addWidget(filesBox, 1);

    // ===== Нижний: прогресс + лог =====
    auto* bottomBox = new QWidget(splitter);
    auto* bottomLay = new QVBoxLayout(bottomBox);
    bottomLay->setContentsMargins(0, 0, 0, 0);

    m_progress = new ProgressPanel(bottomBox);

    auto* logBox = new QGroupBox(tr("Журнал"), bottomBox);
    auto* logLay = new QVBoxLayout(logBox);
    logLay->setContentsMargins(4, 4, 4, 4);
    m_log = new LogWidget(logBox);
    logLay->addWidget(m_log);

    bottomLay->addWidget(m_progress);
    bottomLay->addWidget(logBox, 1);

    splitter->addWidget(pathsBox);
    splitter->addWidget(midWidget);
    splitter->addWidget(bottomBox);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 3);
    splitter->setStretchFactor(2, 2);

    setCentralWidget(splitter);

    // ---- connections ----
    connect(m_sourcePath, &PathSelector::pathChanged, this, &MainWindow::onSourceChanged);
    connect(m_sourcePath, &PathSelector::pathSelected, this, [this](const QString&) {
        loadFilesFromSource();
    });
    connect(m_targetPath, &PathSelector::pathChanged, this, [this](const QString& p) {
        m_settings->setTargetDir(p);
    });
    connect(m_recursive, &QCheckBox::toggled, this, [this](bool v) {
        m_settings->setIncludeSubdirs(v);
        loadFilesFromSource();
    });
    connect(m_params, &ParameterPanel::parametersChanged, this, &MainWindow::onParametersChanged);

    connect(m_progress, &ProgressPanel::pauseRequested,  this, &MainWindow::onPause);
    connect(m_progress, &ProgressPanel::resumeRequested, this, &MainWindow::onResume);
    connect(m_progress, &ProgressPanel::cancelRequested, this, &MainWindow::onCancel);
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage(tr("Готово"));
}

// ---------- слоты UI ----------

void MainWindow::onOpenSource()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Выберите папку-источник (A)"),
                                                          m_sourcePath->path().isEmpty() ? QDir::homePath() : m_sourcePath->path());
    if (dir.isEmpty()) return;
    m_sourcePath->setPath(dir);
    loadFilesFromSource();
}

void MainWindow::onOpenTarget()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Выберите папку-назначение (Б)"),
                                                          m_targetPath->path().isEmpty() ? QDir::homePath() : m_targetPath->path());
    if (dir.isEmpty()) return;
    m_targetPath->setPath(dir);
}

void MainWindow::onRefreshFiles()
{
    loadFilesFromSource();
}

void MainWindow::onSourceChanged(const QString& path)
{
    m_settings->setSourceDir(path);
}

void MainWindow::onParametersChanged()
{
    m_settings->setParameters(m_params->parameters());
}

void MainWindow::onSelectAllFiles()    { m_model->setAllChecked(true);  }
void MainWindow::onDeselectAllFiles()  { m_model->setAllChecked(false); }

void MainWindow::loadFilesFromSource()
{
    const QString src = m_sourcePath->path();
    if (src.isEmpty() || !QFileInfo(src).isDir()) {
        m_model->clear();
        return;
    }

    FileScanner::Options opts;
    opts.rootDir     = src;
    opts.recursive   = m_recursive->isChecked();
    opts.nameFilters = m_settings->extensionFilter().split(';', Qt::SkipEmptyParts);

    FileScanner scanner;
    const auto files = scanner.scan(opts);

    m_model->setFiles(files);
    appendLog(tr("Найдено %1 файлов в %2").arg(files.size()).arg(src));
}

// ---------- обработка ----------

void MainWindow::onStart()
{
    const QString src = m_sourcePath->path();
    const QString dst = m_targetPath->path();

    if (src.isEmpty() || !QFileInfo(src).isDir()) {
        QMessageBox::warning(this, tr("Нет источника"), tr("Выберите существующую папку-источник."));
        return;
    }
    if (dst.isEmpty() || !QFileInfo(dst).isDir()) {
        QMessageBox::warning(this, tr("Нет назначения"), tr("Выберите существующую папку-назначение."));
        return;
    }
    if (QDir(src) == QDir(dst)) {
        if (QMessageBox::question(this, tr("Совпадение папок"),
                                  tr("Папки источника и назначения совпадают. Продолжить?"))
            != QMessageBox::Yes) return;
    }

    const auto rows = m_model->checkedRows();
    if (rows.isEmpty()) {
        QMessageBox::information(this, tr("Нет файлов"), tr("Отметьте хотя бы один файл."));
        return;
    }

    const auto params = m_params->parameters();
    if (!params.isValid()) {
        QMessageBox::warning(this, tr("Некорректные параметры"),
                             tr("Коэффициент скорости должен быть больше 0 и не равен 1.0."));
        return;
    }

    QList<AudioFileInfo> inputs;
    inputs.reserve(rows.size());
    for (int r : rows) inputs.push_back(m_model->fileAt(r));

    // Сбросим статусы в модели перед запуском.
    for (int r : rows) m_model->setStatus(r, ProcessingStatus::Pending);
    m_progress->reset();
    m_lastDone = 0;
    m_lastTotal = inputs.size();
    m_lastEtaSec = -1.0;
    m_runTimer.start();
    m_etaTimer->start();

    updateUiForRunning(true);
    m_changer->start(src, dst, inputs, params);
}

void MainWindow::onPause()
{
    if (!m_changer->isRunning()) return;
    if (m_changer->isPaused()) m_changer->resume();
    else                       m_changer->pause();
}

void MainWindow::onResume() { m_changer->resume(); }

void MainWindow::onCancel()
{
    if (!m_changer->isRunning()) return;
    if (QMessageBox::question(this, tr("Отмена"),
                              tr("Прервать обработку?")) != QMessageBox::Yes) return;
    m_changer->cancel();
}

// ---------- слоты обработки ----------

void MainWindow::onProcessStarted(int total)
{
    m_progress->setRunning(true);
    m_progress->setOverall(0, total);
    appendLog(tr("Старт: %1 файлов.").arg(total));
}

void MainWindow::onFileStarted(int /*jobId*/, const QString& displayName)
{
    m_progress->setCurrentFile(tr("Текущий: %1").arg(displayName));
    m_progress->setCurrentProgress(0);
    appendLog(tr("Обработка: %1").arg(displayName));
}

void MainWindow::onFileProgress(int jobId, int percent)
{
    m_model->setProgress(jobId, percent);
    m_progress->setCurrentProgress(percent);
}

void MainWindow::onFileFinished(int jobId, ProcessingStatus status, const QString& detail)
{
    m_model->setStatus(jobId, status, detail);

    const QString name = m_model->displayNameAt(jobId);
    switch (status) {
    case ProcessingStatus::Done:
        appendLog(tr("✓ %1").arg(name));
        break;
    case ProcessingStatus::Failed:
        appendLog(tr("✗ %1: %2").arg(name, detail), Logger::Level::Error);
        break;
    case ProcessingStatus::Skipped:
        appendLog(tr("↷ %1 пропущен: %2").arg(name, detail), Logger::Level::Warning);
        break;
    case ProcessingStatus::Cancelled:
        appendLog(tr("⏹ %1 отменён").arg(name), Logger::Level::Warning);
        break;
    default: break;
    }
}

void MainWindow::onOverallProgress(int done, int total)
{
    m_lastDone  = done;
    m_lastTotal = total;
    m_progress->setOverall(done, total);
}

void MainWindow::onProcessFinished(int ok, int failed, int skipped)
{
    m_etaTimer->stop();
    updateUiForRunning(false);

    appendLog(tr("Готово: успешно %1, ошибок %2, пропущено %3.")
                  .arg(ok).arg(failed).arg(skipped));
    statusBar()->showMessage(tr("Завершено"), 5000);

    QMessageBox::information(this, tr("Обработка завершена"),
                             tr("Успешно: %1\nОшибок: %2\nПропущено: %3").arg(ok).arg(failed).arg(skipped));
}

void MainWindow::onProcessError(const AudioError& err)
{
    appendLog(tr("Ошибка: %1 (%2)").arg(err.message, err.filePath),
              Logger::Level::Error);
}

// ---------- служебное ----------

void MainWindow::updateUiForRunning(bool running)
{
    m_params->setEnabled(!running);
    m_sourcePath->setEnabled(!running);
    m_targetPath->setEnabled(!running);
    m_recursive->setEnabled(!running);
    m_table->setEnabled(!running);

    m_actStart->setEnabled(!running);
    m_actPause->setEnabled(running);
    m_actCancel->setEnabled(running);
    m_actOpenSrc->setEnabled(!running);
    m_actOpenDst->setEnabled(!running);
    m_actRefresh->setEnabled(!running);

    m_progress->setRunning(running);
    statusBar()->showMessage(running ? tr("Обработка…") : tr("Готово"));
}

void MainWindow::appendLog(const QString& msg, Logger::Level lvl)
{
    Logger::instance().log(lvl, msg);
}

double MainWindow::computeEta() const
{
    if (m_lastDone <= 0) return -1.0;
    const double elapsed = m_runTimer.elapsed() / 1000.0;
    const double perItem = elapsed / static_cast<double>(m_lastDone);
    const int remaining = qMax(0, m_lastTotal - m_lastDone);
    return perItem * remaining;
}

void MainWindow::onOpenSettings()
{
    SettingsDialog dlg(m_settings, this);
    connect(&dlg, &SettingsDialog::settingsChanged, this, [this]() {
        m_recursive->setChecked(m_settings->includeSubdirs());
        m_params->setParameters(m_settings->parameters());
    });
    dlg.exec();
}

void MainWindow::onAbout()
{
    AboutDialog::show(this);
}

// ---------- Drag & Drop ----------

void MainWindow::dragEnterEvent(QDragEnterEvent* e)
{
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* e)
{
    const auto urls = e->mimeData()->urls();
    if (urls.isEmpty()) return;

    const QString path = urls.first().toLocalFile();
    if (QFileInfo(path).isDir()) {
        m_sourcePath->setPath(path);
        loadFilesFromSource();
    } else if (QFileInfo(path).isFile()) {
        // Установим папку — родителя этого файла.
        m_sourcePath->setPath(QFileInfo(path).absolutePath());
        loadFilesFromSource();
    }
    e->acceptProposedAction();
}

// ---------- Сохранение состояния окна ----------

void MainWindow::restoreWindowState()
{
    const QByteArray g = m_settings->windowGeometry();
    if (!g.isEmpty()) restoreGeometry(g);
    const QByteArray s = m_settings->windowState();
    if (!s.isEmpty()) restoreState(s);
}

void MainWindow::saveWindowState()
{
    m_settings->setWindowGeometry(saveGeometry());
    m_settings->setWindowState(saveState());
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    if (m_changer->isRunning()) {
        if (QMessageBox::question(this, tr("Выход"),
                                  tr("Обработка ещё идёт. Прервать и выйти?"))
            != QMessageBox::Yes) {
            e->ignore();
            return;
        }
        m_changer->cancel();
    }

    m_settings->setSourceDir(m_sourcePath->path());
    m_settings->setTargetDir(m_targetPath->path());
    m_settings->setParameters(m_params->parameters());
    saveWindowState();

    e->accept();
}
