#pragma once

#include <QObject>
#include <functional>
#include <memory>

#include "core/Core.h"          // ProcessingJob, ProcessingStatus
#include "threading/CancellationToken.h"
#include "utils/Result.h"       // AudioError

class ProcessingWorker : public QObject
{
    Q_OBJECT
public:
    // Провайдер job'ов: возвращает true и заполняет out, если есть job;
    // возвращает false, если очередь пуста или отмена.
    using JobProvider = std::function<bool(ProcessingJob& out)>;

    explicit ProcessingWorker(QObject* parent = nullptr);
    ~ProcessingWorker() override;

    void setJobProvider(JobProvider provider) { m_jobProvider = std::move(provider); }
    void setCancellationToken(std::shared_ptr<CancellationToken> token) { m_cancel = std::move(token); }

public slots:
    // Основной цикл: тянет job'ы через m_jobProvider, обрабатывает каждый,
    // выходит когда провайдер вернёт false или сработает отмена.
    void runLoop();

    // На случай внешней отмены: попросить текущий job остановиться.
    void requestCancel();

signals:
    void started(int jobId);
    void progress(int jobId, int percent);
    void finished(int jobId, ProcessingStatus status, const QString& detail);
    void loopFinished();   // воркер завершил цикл — больше не берёт job'ы

private:
    AudioError processOne(const ProcessingJob& job);

    JobProvider                        m_jobProvider;
    std::shared_ptr<CancellationToken> m_cancel;
    ProcessingJob                      m_job;
};
