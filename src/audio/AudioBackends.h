#pragma once

#include "audio/AudioInterfaces.h"
#include <memory>

// =====================================================================
//  FFmpegDecoder — обёртка над libavformat/libavcodec/libswresample
// =====================================================================
class FFmpegDecoder final : public IAudioDecoder
{
public:
    FFmpegDecoder();
    ~FFmpegDecoder() override;

    AudioError open(const QString& filePath) override;
    const AudioStreamInfo& streamInfo() const override;
    int  readFrames(float* out, int maxFrames, AudioError& outError) override;
    void rewind() override;
    void close() override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// =====================================================================
//  FFmpegEncoder — обёртка над libavformat/libavcodec/libswresample
// =====================================================================
class FFmpegEncoder final : public IAudioEncoder
{
public:
    FFmpegEncoder();
    ~FFmpegEncoder() override;

    AudioError open(const QString& filePath,
                    int sampleRate,
                    int channels,
                    const ProcessingParameters& params) override;
    AudioError writeFrames(const float* in, int frames) override;
    AudioError finalize() override;
    void close() override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// =====================================================================
//  SoundTouchStretcher — time-stretch через soundtouch::SoundTouch
// =====================================================================
class SoundTouchStretcher final : public ITimeStretcher
{
public:
    SoundTouchStretcher();
    ~SoundTouchStretcher() override;

    void setSampleRate(int hz) override;
    void setChannels(int n) override;
    void setTempo(double tempo) override;
    void setPitch(double semitones) override;
    void setQuality(int quality) override;

    void putSamples(const float* in, std::size_t frames) override;
    std::size_t receiveSamples(float* out, std::size_t maxFrames) override;
    void flush() override;
    void reset() override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// =====================================================================
//  RubberBandStretcher — time-stretch через librubberband
// =====================================================================
class RubberBandStretcher final : public ITimeStretcher
{
public:
    RubberBandStretcher();
    ~RubberBandStretcher() override;

    void setSampleRate(int hz) override;
    void setChannels(int n) override;
    void setTempo(double tempo) override;
    void setPitch(double semitones) override;
    void setQuality(int quality) override;

    void putSamples(const float* in, std::size_t frames) override;
    std::size_t receiveSamples(float* out, std::size_t maxFrames) override;
    void flush() override;
    void reset() override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
