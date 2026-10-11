#pragma once

#include <QString>
#include <cstdint>
#include <cstddef>
#include <memory>

#include "utils/Result.h"
#include "core/Core.h"      // AudioEngine, ProcessingParameters

// =====================================================================
//  AudioStreamInfo — метаинформация о декодируемом потоке
// =====================================================================
struct AudioStreamInfo
{
    int     sampleRate  = 0;
    int     channels    = 0;
    double  durationSec = -1.0;
    QString codecName;
};

// =====================================================================
//  IAudioDecoder — декодирование любого входного формата в float32
// =====================================================================
class IAudioDecoder
{
public:
    virtual ~IAudioDecoder() = default;

    virtual AudioError open(const QString& filePath) = 0;
    virtual const AudioStreamInfo& streamInfo() const = 0;

    // Возвращает фактическое число прочитанных фреймов; 0 = конец файла.
    virtual int  readFrames(float* out, int maxFrames, AudioError& outError) = 0;

    virtual void rewind() = 0;
    virtual void close()  = 0;
};

// =====================================================================
//  IAudioEncoder — кодирование float32 в целевой формат
// =====================================================================
class IAudioEncoder
{
public:
    virtual ~IAudioEncoder() = default;

    virtual AudioError open(const QString& filePath,
                            int sampleRate,
                            int channels,
                            const ProcessingParameters& params) = 0;

    virtual AudioError writeFrames(const float* in, int frames) = 0;
    virtual AudioError finalize() = 0;
    virtual void close() = 0;
};

// =====================================================================
//  ITimeStretcher — изменение скорости с сохранением питча
// =====================================================================
class ITimeStretcher
{
public:
    virtual ~ITimeStretcher() = default;

    virtual void setSampleRate(int hz)      = 0;
    virtual void setChannels(int n)         = 0;
    virtual void setTempo(double tempo)     = 0;    // 2.0 → ×2 быстрее
    virtual void setPitch(double semitones) = 0;    // всегда 0 в нашей задаче
    virtual void setQuality(int quality)    = 0;

    virtual void putSamples(const float* in, std::size_t frames) = 0;
    virtual std::size_t receiveSamples(float* out, std::size_t maxFrames) = 0;
    virtual void flush() = 0;
    virtual void reset() = 0;
};

// =====================================================================
//  AudioBackendFactory — фабрика бэкендов
// =====================================================================
class AudioBackendFactory
{
public:
    static std::unique_ptr<IAudioDecoder>  createDecoder();
    static std::unique_ptr<IAudioEncoder>  createEncoder();
    static std::unique_ptr<ITimeStretcher> createStretcher(AudioEngine engine);
};
