#include "core/Core.h"

#include "threading/WorkerPool.h"
#include "utils/FileUtils.h"
#include "utils/Logger.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

// =====================================================================
//  FileScanner
// =====================================================================
QList<AudioFileInfo> FileScanner::scan(const Options& opts) const
{
    QList<AudioFileInfo> result;
    QDir root(opts.rootDir);
    if (!root.exists())
        return result;

    QDirIterator::IteratorFlags flags = QDirIterator::NoIteratorFlags;
    if (opts.recursive)
        flags |= QDirIterator::Subdirectories;

    QDirIterator it(root.absolutePath(),
                    opts.nameFilters,
                    QDir::Files | QDir::Readable,
                    flags);

    while (it.hasNext()) {
        it.next();
        const QFileInfo fi = it.fileInfo();

        AudioFileInfo info;
        info.filePath     = fi.absoluteFilePath();
        info.relativePath = root.relativeFilePath(info.filePath);
        info.format       = fi.suffix().toLower();
        info.sizeBytes    = fi.size();
        info.durationSec  = -1.0;
        result.push_back(info);
    }
    return result;
}

// =====================================================================
//  JobBuilder
// =====================================================================
namespace {

QString formatExtension(OutputFormat f)
{
    switch (f) {
    case OutputFormat::Wav:  return "wav";
    case OutputFormat::Flac: return "flac";
    case OutputFormat::Mp3:  return "mp3";
    case OutputFormat::Ogg:  return "ogg";
    case OutputFormat::M4a:  return "m4a";
    case OutputFormat::SameAsSource: break;
    }
    return {};
}

} // namespace

QString JobBuilder::makeOutputPath(const AudioFileInfo& in,
                                   const QString& targetRoot,
                                   const ProcessingParameters& params)
{
    QFileInfo srcInfo(in.filePath);

    // Сохраняем относительную структуру подпапок.
    QFileInfo relInfo(in.relativePath);
    QString subDir = relInfo.path();
    if (subDir == ".")
        subDir.clear();

    const QString outDir = subDir.isEmpty()
                               ? targetRoot
                               : QDir(targetRoot).filePath(subDir);

    QString ext = formatExtension(params.format);
    if (ext.isEmpty())
        ext = srcInfo.suffix().toLower();
    if (ext.isEmpty())
        ext = "wav";

    const QString baseName = srcInfo.completeBaseName();
    return QDir(outDir).filePath(baseName + "." + ext);
}

QList<ProcessingJob> JobBuilder::build(const QList<AudioFileInfo>& inputs,
                                       const QString& sourceRoot,
                                       const QString& targetRoot,
                                       const ProcessingParameters& params) const
{
    Q_UNUSED(sourceRoot);

    QList<ProcessingJob> jobs;
    jobs.reserve(inputs.size());

    int id = 0;
    for (const AudioFileInfo& in : inputs) {
        ProcessingJob job;
        job.id         = id++;
        job.source     = in;
        job.params     = params;
        job.outputPath = makeOutputPath(in, targetRoot, params);

        if (params.conflict == ConflictPolicy::Rename)
            job.outputPath = FileUtils::uniquePath(job.outputPath);

        jobs.push_back(job);
    }
    return jobs;
}

// =====================================================================
//  AudioSpeedChanger
// =====================================================================
AudioSpeedChanger::AudioSpeedChanger(QObject* parent)
    : QObject(parent)
    , m_pool(new WorkerPool(this))
{
    connect(m_pool, &WorkerPool::fileStarted,
            this, &AudioSpeedChanger::fileStarted);
    connect(m_pool, &WorkerPool::fileProgress,
            this, &AudioSpeedChanger::fileProgress);
    connect(m_pool, &WorkerPool::fileFinished,
            this, &AudioSpeedChanger::fileFinished);
    connect(m_pool, &WorkerPool::overallProgress,
            this, &AudioSpeedChanger::overallProgress);
    connect(m_pool, &WorkerPool::finished,
            this, &AudioSpeedChanger::finished);
    connect(m_pool, &WorkerPool::error,
            this, &AudioSpeedChanger::error);
}

AudioSpeedChanger::~AudioSpeedChanger() = default;

bool AudioSpeedChanger::isRunning() const { return m_pool->isRunning(); }
bool AudioSpeedChanger::isPaused()  const { return m_pool->isPaused();  }

void AudioSpeedChanger::start(const QString& sourceRoot,
                              const QString& targetRoot,
                              const QList<AudioFileInfo>& inputs,
                              const ProcessingParameters& params)
{
    JobBuilder builder;
    const QList<ProcessingJob> jobs =
        builder.build(inputs, sourceRoot, targetRoot, params);

    Logger::instance().log(Logger::Level::Info,
                           QString("Запуск обработки: %1 файлов, скорость %2×, движок %3")
                               .arg(jobs.size())
                               .arg(params.speedFactor, 0, 'f', 2)
                               .arg(params.engine == AudioEngine::SoundTouch ? "SoundTouch"
                                                                             : "RubberBand"));

    emit started(jobs.size());
    m_pool->start(jobs, params.threadCount);
}

void AudioSpeedChanger::pause()  { m_pool->pause();  emit paused();  }
void AudioSpeedChanger::resume() { m_pool->resume(); emit resumed(); }
void AudioSpeedChanger::cancel() { m_pool->cancel(); }
