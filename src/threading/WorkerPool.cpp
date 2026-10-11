#include "threading/WorkerPool.h"
#include "threading/ProcessingWorker.h"
#include "utils/Logger.h"

#include <QThread>
#include <QQueue>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QFileInfo>

// ---------------------------------------------------------------------
// Общее состояние пула. Живёт в shared_ptr, чтобы лямбда-провайдер
// job'ов в каждом воркере держала ссылку на него.
// ---------------------------------------------------------------------
struct WorkerPool::State
{
    QMutex                            mutex;
    QWaitCondition                    pauseCond;
    QQueue<ProcessingJob>             queue;
    std::shared_ptr<CancellationToken> token;
    bool                              paused = false;
};

WorkerPool::WorkerPool(QObject* parent)
    : QObject(parent)
    , m_state(std::make_shared<State>())
{
    m_state->token = std::make_shared<CancellationToken>();
}

WorkerPool::~WorkerPool()
{
    cancel();
    cleanupThreads();
}

bool WorkerPool::isRunning() const { return m_running; }
bool WorkerPool::isPaused()  const { return m_paused;  }

// ---------------------------------------------------------------------
// start
// ---------------------------------------------------------------------
void WorkerPool::start(const QList<ProcessingJob>& jobs, int threadCount)
{
    if (m_running) {
        Logger::instance().log(Logger::Level::Warning,
                               "WorkerPool::start: обработка уже идёт.");
        return;
    }
    if (jobs.isEmpty() || threadCount <= 0) {
        emit finished(0, 0, 0);
        return;
    }

    // Сброс состояния.
    m_state->token->reset();
    {
        QMutexLocker lk(&m_state->mutex);
        m_state->queue.clear();
        for (const ProcessingJob& j : jobs)
            m_state->queue.enqueue(j);
        m_state->paused = false;
    }

    m_total          = jobs.size();
    m_done           = 0;
    m_succeeded      = 0;
    m_failed         = 0;
    m_skipped        = 0;
    m_loopsFinished  = 0;
    m_paused         = false;
    m_running        = true;

    Logger::instance().log(Logger::Level::Info,
                           QString("WorkerPool: запуск %1 потоков для %2 файлов")
                               .arg(threadCount).arg(jobs.size()));

    // Ограничим число потоков разумными рамками.
    threadCount = qBound(1, threadCount, 64);
    threadCount = qMin(threadCount, jobs.size());

    // Создаём воркеры и потоки.
    auto state = m_state;   // захватываем shared_ptr для лямбды

    for (int i = 0; i < threadCount; ++i) {
        auto* thread = new QThread(this);
        auto* worker = new ProcessingWorker();   // без parent — переедет в другой поток

        worker->moveToThread(thread);
        worker->setCancellationToken(state->token);

        // Провайдер job'ов: ждёт на condvar при паузе, проверяет отмену,
        // декьюит из очереди. Вызывается из потока воркера.
        worker->setJobProvider([state](ProcessingJob& out) -> bool {
            QMutexLocker lk(&state->mutex);
            while (state->paused && !state->token->isCancelled()) {
                state->pauseCond.wait(&state->mutex);
            }
            if (state->token->isCancelled()) return false;
            if (state->queue.isEmpty())      return false;
            out = state->queue.dequeue();
            return true;
        });

        // Сигналы воркера → слоты пула (QueuedConnection по умолчанию,
        // т.к. воркер живёт в другом потоке).
        connect(worker, &ProcessingWorker::started,
                this,   &WorkerPool::onWorkerStarted);
        connect(worker, &ProcessingWorker::progress,
                this,   &WorkerPool::onWorkerProgress);
        connect(worker, &ProcessingWorker::finished,
                this,   &WorkerPool::onWorkerFinished);
        connect(worker, &ProcessingWorker::loopFinished,
                this,   &WorkerPool::onWorkerLoopFinished);

        // Запуск цикла воркера в его потоке.
        connect(thread, &QThread::started, worker, &ProcessingWorker::runLoop);

        m_threads.push_back(thread);
        m_workers.push_back(worker);

        thread->start();
    }
}

// ---------------------------------------------------------------------
// Пауза / возобновление / отмена
// ---------------------------------------------------------------------
void WorkerPool::pause()
{
    if (!m_running) return;
    {
        QMutexLocker lk(&m_state->mutex);
        m_state->paused = true;
    }
    m_paused = true;
    Logger::instance().log(Logger::Level::Info, "WorkerPool: пауза.");
}

void WorkerPool::resume()
{
    if (!m_running) return;
    {
        QMutexLocker lk(&m_state->mutex);
        m_state->paused = false;
    }
    m_paused = false;
    m_state->pauseCond.wakeAll();
    Logger::instance().log(Logger::Level::Info, "WorkerPool: продолжение.");
}

void WorkerPool::cancel()
{
    if (!m_running && m_threads.isEmpty()) return;

    m_state->token->cancel();
    {
        QMutexLocker lk(&m_state->mutex);
        m_state->paused = false;
        m_state->queue.clear();
    }
    m_state->pauseCond.wakeAll();
    Logger::instance().log(Logger::Level::Info, "WorkerPool: отмена.");
}

// ---------------------------------------------------------------------
// Слоты воркеров
// ---------------------------------------------------------------------
void WorkerPool::onWorkerStarted(int jobId)
{
    emit fileStarted(jobId, displayNameForJob(jobId));
}

void WorkerPool::onWorkerProgress(int jobId, int percent)
{
    emit fileProgress(jobId, percent);
}

void WorkerPool::onWorkerFinished(int jobId,
                                  ProcessingStatus status,
                                  const QString& detail)
{
    ++m_done;
    switch (status) {
    case ProcessingStatus::Done:      ++m_succeeded; break;
    case ProcessingStatus::Failed:    ++m_failed;    break;
    case ProcessingStatus::Skipped:   ++m_skipped;   break;
    case ProcessingStatus::Cancelled:                break;
    default: break;
    }

    emit fileFinished(jobId, status, detail);
    emit overallProgress(m_done, m_total);

    // Если все файлы закончились — финализируем.
    // (Проверка m_done == m_total срабатывает один раз, потому что
    // сигналы из воркеров приходят строго последовательно в главный поток.)
    if (m_done >= m_total) {
        const int ok      = m_succeeded;
        const int failed  = m_failed;
        const int skipped = m_skipped;
        m_running = false;
        m_paused  = false;

        Logger::instance().log(Logger::Level::Info,
                               QString("WorkerPool: завершено. ok=%1, failed=%2, skipped=%3")
                                   .arg(ok).arg(failed).arg(skipped));

        emit finished(ok, failed, skipped);
    }
}

void WorkerPool::onWorkerLoopFinished()
{
    ++m_loopsFinished;

    // Когда все воркеры вышли из цикла — останавливаем их потоки.
    if (m_loopsFinished >= m_workers.size()) {
        // Отложенный cleanup: даём текущим сигналам завершиться,
        // потом останавливаем потоки.
        // Простейший вариант — оставить потоки жить, они всё равно
        // уже вышли из цикла. Удалим их при следующем start() или
        // в деструкторе.
    }
}

// ---------------------------------------------------------------------
// Уборка потоков
// ---------------------------------------------------------------------
void WorkerPool::cleanupThreads()
{
    for (QThread* t : m_threads) {
        t->quit();
        t->wait(3000);
    }
    qDeleteAll(m_workers);
    qDeleteAll(m_threads);
    m_workers.clear();
    m_threads.clear();
    m_loopsFinished = 0;
}

// ---------------------------------------------------------------------
// Служебное
// ---------------------------------------------------------------------
QString WorkerPool::displayNameForJob(int jobId) const
{
    // Ищем job в очереди-источнике нельзя — она уже пуста.
    // Поэтому имя восстановим через сигнал fileStarted у UI.
    // Здесь возвращаем только id, UI сам подставит имя из модели.
    Q_UNUSED(jobId);
    return QString();
}
