#pragma once

#include <QObject>
#include <QList>
#include <memory>

#include "core/Core.h"
#include "threading/CancellationToken.h"
#include "utils/Result.h"

class ProcessingWorker;
class QThread;

class WorkerPool : public QObject
{
    Q_OBJECT
public:
    explicit WorkerPool(QObject* parent = nullptr);
    ~WorkerPool() override;

    bool isRunning() const;
    bool isPaused()  const;

public slots:
    void start(const QList<ProcessingJob>& jobs, int threadCount);
    void pause();
    void resume();
    void cancel();

signals:
    void fileStarted(int jobId, const QString& displayName);
    void fileProgress(int jobId, int percent);
    void fileFinished(int jobId, ProcessingStatus status, const QString& detail);
    void overallProgress(int done, int total);
    void finished(int succeeded, int failed, int skipped);
    void error(const AudioError& err);

private slots:
    void onWorkerStarted(int jobId);
    void onWorkerProgress(int jobId, int percent);
    void onWorkerFinished(int jobId, ProcessingStatus status, const QString& detail);
    void onWorkerLoopFinished();

private:
    struct State;
    std::shared_ptr<State> m_state;

    QList<QThread*>         m_threads;
    QList<ProcessingWorker*> m_workers;

    // Счётчики общие для всех воркеров (обновляются в главном потоке,
    // потому что onWorkerFinished вызывается по QueuedConnection).
    int m_done      = 0;
    int m_succeeded = 0;
    int m_failed    = 0;
    int m_skipped   = 0;
    int m_total     = 0;
    int m_loopsFinished = 0;
    bool m_running  = false;
    bool m_paused   = false;

    QString displayNameForJob(int jobId) const;

    void cleanupThreads();
};
