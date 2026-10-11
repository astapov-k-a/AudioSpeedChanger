#include "threading/ProcessingWorker.h"

#include "audio/AudioInterfaces.h"      // AudioBackendFactory
#include "utils/Logger.h"
#include "utils/FileUtils.h"

#include <QFileInfo>
#include <vector>

ProcessingWorker::ProcessingWorker(QObject* parent) : QObject(parent) {}
ProcessingWorker::~ProcessingWorker() = default;

void ProcessingWorker::requestCancel()
{
    if (m_cancel) m_cancel->cancel();
}

void ProcessingWorker::runLoop()
{
    while (true) {
        // Токен отмены?
        if (m_cancel && m_cancel->isCancelled())
            break;

        // Провайдер сам ждёт на condvar, если пул на паузе,
        // и сам проверяет отмену/пустоту очереди.
        if (!m_jobProvider)
            break;

        ProcessingJob job;
        if (!m_jobProvider(job))
            break;

        m_job = job;

        emit started(m_job.id);

        const AudioError err = processOne(m_job);

        if (err.isError()) {
            if (err.code == AudioError::Code::Cancelled)
                emit finished(m_job.id, ProcessingStatus::Cancelled, err.message);
            else
                emit finished(m_job.id, ProcessingStatus::Failed, err.message);
        } else {
            emit finished(m_job.id, ProcessingStatus::Done, QString());
        }
    }

    emit loopFinished();
}

// Реализация не менялась — тот же скелет с FFmpeg/stretcher/encoder.
AudioError ProcessingWorker::processOne(const ProcessingJob& job)
{
    auto decoder   = AudioBackendFactory::createDecoder();
    auto encoder   = AudioBackendFactory::createEncoder();
    auto stretcher = AudioBackendFactory::createStretcher(job.params.engine);

    AudioError err = decoder->open(job.source.filePath);
    if (err.isError()) return err;

    const AudioStreamInfo info = decoder->streamInfo();

    stretcher->setSampleRate(info.sampleRate);
    stretcher->setChannels(info.channels);
    stretcher->setTempo(job.params.speedFactor);
    stretcher->setPitch(0.0);
    stretcher->setQuality(80);

    if (!FileUtils::ensureParentDirExists(job.outputPath)) {
        return AudioError::make(AudioError::Code::IoError,
                                "Не удалось создать каталог для выходного файла",
                                job.outputPath);
    }

    err = encoder->open(job.outputPath, info.sampleRate, info.channels, job.params);
    if (err.isError()) return err;

    constexpr int kFramesPerBlock = 4096;
    std::vector<float> inBuf (static_cast<size_t>(kFramesPerBlock) * info.channels);
    std::vector<float> outBuf(static_cast<size_t>(kFramesPerBlock) * info.channels);

    const double totalFrames = info.durationSec > 0
                                   ? info.durationSec * info.sampleRate
                                   : -1.0;
    double processedFrames = 0.0;
    int lastPercent = -1;

    while (true) {
        if (m_cancel && m_cancel->isCancelled())
            return AudioError::make(AudioError::Code::Cancelled, "Отменено пользователем");

        AudioError readErr;
        const int got = decoder->readFrames(inBuf.data(), kFramesPerBlock, readErr);
        if (readErr.isError()) return readErr;

        if (got > 0) {
            stretcher->putSamples(inBuf.data(), static_cast<size_t>(got));
            processedFrames += got;
        } else {
            stretcher->flush();
        }

        std::size_t produced = 0;
        do {
            produced = stretcher->receiveSamples(outBuf.data(), kFramesPerBlock);
            if (produced > 0) {
                err = encoder->writeFrames(outBuf.data(), static_cast<int>(produced));
                if (err.isError()) return err;
            }
        } while (produced > 0);

        if (totalFrames > 0) {
            const int pct = qBound(0, static_cast<int>(processedFrames * 100.0 / totalFrames), 100);
            if (pct != lastPercent) {
                lastPercent = pct;
                emit progress(job.id, pct);
            }
        }

        if (got == 0) break;
    }

    err = encoder->finalize();
    if (err.isError()) return err;

    emit progress(job.id, 100);
    return AudioError::ok();
}
