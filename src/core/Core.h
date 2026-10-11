#pragma once

#include <QString>
#include <QStringList>
#include <QList>
#include <QObject>
#include <QMetaType>

#include "utils/Result.h"

// =====================================================================
//  AudioFileInfo — информация об одном найденном файле
// =====================================================================
struct AudioFileInfo
{
    QString filePath;      // абсолютный путь
    QString relativePath;  // относительно корня источника
    QString format;        // "wav", "mp3", ...
    qint64  sizeBytes  = 0;
    double  durationSec = -1.0; // -1 = неизвестно
};
Q_DECLARE_METATYPE(AudioFileInfo)

// =====================================================================
//  ProcessingParameters — параметры пакетной обработки
// =====================================================================
enum class OutputFormat {
    SameAsSource = 0,
    Wav,
    Flac,
    Mp3,
    Ogg,
    M4a,
};

enum class ConflictPolicy {
    Overwrite = 0,
    Skip,
    Rename,
    Ask,
};

enum class AudioEngine {
    SoundTouch = 0,
    RubberBand,
};

struct ProcessingParameters
{
    double          speedFactor     = 1.0;       // 0.25 .. 4.0
    OutputFormat    format          = OutputFormat::SameAsSource;
    int             sampleRate      = 0;         // 0 = как в источнике
    int             bitDepth        = 0;         // 0 = как в источнике (lossless)
    int             bitrateKbps     = 192;       // для lossy
    ConflictPolicy  conflict        = ConflictPolicy::Rename;
    bool            copyMetadata    = true;
    bool            normalizeVolume = false;
    AudioEngine     engine          = AudioEngine::SoundTouch;
    int             threadCount     = 4;

    bool isValid() const
    {
        return speedFactor > 0.0
               && speedFactor != 1.0
               && threadCount > 0
               && threadCount <= 256;
    }
};
Q_DECLARE_METATYPE(ProcessingParameters)

// =====================================================================
//  ProcessingStatus — статус обработки одного файла
// =====================================================================
enum class ProcessingStatus {
    Pending = 0,
    Running,
    Done,
    Failed,
    Skipped,
    Cancelled,
};
Q_DECLARE_METATYPE(ProcessingStatus)

// =====================================================================
//  ProcessingJob — одна задача обработки
// =====================================================================
struct ProcessingJob
{
    int                  id = -1;
    AudioFileInfo        source;
    QString              outputPath;
    ProcessingParameters params;
    ProcessingStatus     status = ProcessingStatus::Pending;
};

// =====================================================================
//  FileScanner — обход папки и фильтрация по маске расширений
// =====================================================================
class FileScanner
{
public:
    struct Options
    {
        QString     rootDir;
        QStringList nameFilters;   // {"*.wav", "*.mp3", ...}
        bool        recursive = true;
    };

    FileScanner() = default;

    // Синхронный скан. При ошибке — пустой список.
    QList<AudioFileInfo> scan(const Options& opts) const;
};

// =====================================================================
//  JobBuilder — построение списка ProcessingJob
// =====================================================================
class JobBuilder
{
public:
    JobBuilder() = default;

    QList<ProcessingJob> build(const QList<AudioFileInfo>& inputs,
                               const QString& sourceRoot,
                               const QString& targetRoot,
                               const ProcessingParameters& params) const;

    static QString makeOutputPath(const AudioFileInfo& in,
                                  const QString& targetRoot,
                                  const ProcessingParameters& params);
};

// =====================================================================
//  AudioSpeedChanger — фасад над WorkerPool для UI
// =====================================================================
class WorkerPool;

class AudioSpeedChanger : public QObject
{
    Q_OBJECT
public:
    explicit AudioSpeedChanger(QObject* parent = nullptr);
    ~AudioSpeedChanger() override;

    bool isRunning() const;
    bool isPaused()  const;

public slots:
    void start(const QString& sourceRoot,
               const QString& targetRoot,
               const QList<AudioFileInfo>& inputs,
               const ProcessingParameters& params);

    void pause();
    void resume();
    void cancel();

signals:
    void started(int totalJobs);
    void fileStarted(int jobId, const QString& displayName);
    void fileProgress(int jobId, int percent);
    void fileFinished(int jobId, ProcessingStatus status, const QString& detail);
    void overallProgress(int done, int total);
    void paused();
    void resumed();
    void finished(int succeeded, int failed, int skipped);
    void error(const AudioError& err);

private:
    WorkerPool* m_pool = nullptr;
};
