#include "audio/AudioBackends.h"

#include <QFileInfo>
#include <QDir>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

// =====================================================================
//  Подключение внешних библиотек
// =====================================================================
#ifdef HAVE_FFMPEG
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libavutil/version.h>
#include <libswresample/swresample.h>
}
#endif

#ifdef HAVE_SOUNDTOUCH
#  include <soundtouch/SoundTouch.h>
#endif

#ifdef HAVE_RUBBERBAND
#  include <rubberband/RubberBandStretcher.h>
#endif

// =====================================================================
//  Общие помощники
// =====================================================================
#ifdef HAVE_FFMPEG
namespace {

QString avErrorString(int err)
{
    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(err, buf, sizeof(buf));
    return QString::fromUtf8(buf);
}

// Выбор кодека по расширению выходного файла.
AVCodecID pickCodecIdForExtension(const QString& ext)
{
    if (ext == "wav"  || ext == "wave") return AV_CODEC_ID_PCM_S16LE;
    if (ext == "flac")                  return AV_CODEC_ID_FLAC;
    if (ext == "mp3")                   return AV_CODEC_ID_MP3;
    if (ext == "ogg"  || ext == "oga")  return AV_CODEC_ID_VORBIS;
    if (ext == "m4a"  || ext == "aac")  return AV_CODEC_ID_AAC;
    if (ext == "aiff" || ext == "aif")  return AV_CODEC_ID_PCM_S16BE;
    if (ext == "opus")                  return AV_CODEC_ID_OPUS;
    return AV_CODEC_ID_NONE;
}

// Возвращает первый поддерживаемый кодеком формат сэмплов.
// В FFmpeg >= 7.1 поле AVCodec::sample_fmts удалено, используем
// avcodec_get_supported_config().
AVSampleFormat pickFirstSampleFormat(const AVCodec* codec)
{
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 13, 100)
    const void* config = nullptr;
    int count = 0;

    int ret = avcodec_get_supported_config(
        nullptr,
        codec,
        AV_CODEC_CONFIG_SAMPLE_FORMAT,
        0,
        &config,
        &count);

    if (ret >= 0 && config && count > 0) {
        const AVSampleFormat* formats =
            static_cast<const AVSampleFormat*>(config);
        if (formats[0] != AV_SAMPLE_FMT_NONE)
            return formats[0];
    }
    return AV_SAMPLE_FMT_FLT;   // разумный дефолт
#else
    if (codec->sample_fmts && codec->sample_fmts[0] != AV_SAMPLE_FMT_NONE)
        return codec->sample_fmts[0];
    return AV_SAMPLE_FMT_S16;
#endif
}

} // namespace
#endif // HAVE_FFMPEG

// #####################################################################
//
//                          FFmpegDecoder
//
// #####################################################################

#ifdef HAVE_FFMPEG

struct FFmpegDecoder::Impl
{
    AVFormatContext* fmt         = nullptr;
    AVCodecContext*  codec       = nullptr;
    SwrContext*      swr         = nullptr;
    AVPacket*        pkt         = nullptr;
    AVFrame*         frame       = nullptr;
    int              streamIndex = -1;
    AudioStreamInfo  info;

    // Накопитель уже сконвертированных float32-сэмплов (interleaved).
    std::vector<float> sampleBuffer;
    bool eof = false;

    void cleanup()
    {
        if (swr)   swr_free(&swr);
        if (frame) av_frame_free(&frame);
        if (pkt)   av_packet_free(&pkt);
        if (codec) avcodec_free_context(&codec);
        if (fmt)   avformat_close_input(&fmt);
        streamIndex = -1;
        sampleBuffer.clear();
        eof  = false;
        info = {};
    }

    // Декодирует один кадр и дополняет sampleBuffer.
    // Возвращает false, если данных больше нет (нормальный EOF) или ошибка.
    // outError заполняется только при настоящей ошибке.
    bool decodeNextFrame(AudioError& outError)
    {
        while (true) {
            int ret = avcodec_receive_frame(codec, frame);
            if (ret == 0) {
                const int outFrames = frame->nb_samples;
                const int ch        = info.channels;

                const size_t oldSize = sampleBuffer.size();
                sampleBuffer.resize(oldSize + static_cast<size_t>(outFrames) * ch);

                uint8_t* outPtr =
                    reinterpret_cast<uint8_t*>(sampleBuffer.data() + oldSize);

                const int converted =
                    swr_convert(swr,
                                &outPtr, outFrames,
                                const_cast<const uint8_t**>(frame->extended_data),
                                frame->nb_samples);

                if (converted < 0) {
                    outError = AudioError::make(AudioError::Code::DecodeError,
                                                QString("swr_convert: %1").arg(avErrorString(converted)));
                    return false;
                }
                if (converted < outFrames) {
                    sampleBuffer.resize(oldSize
                                        + static_cast<size_t>(converted) * ch);
                }
                return true;
            }

            if (ret == AVERROR(EAGAIN)) {
                // Нужно ещё пакетов — читаем дальше.
            } else if (ret == AVERROR_EOF) {
                return false;
            } else {
                outError = AudioError::make(AudioError::Code::DecodeError,
                                            QString("avcodec_receive_frame: %1").arg(avErrorString(ret)));
                return false;
            }

            const int readRet = av_read_frame(fmt, pkt);
            if (readRet == AVERROR_EOF) {
                // Сообщаем кодеку о конце потока.
                avcodec_send_packet(codec, nullptr);
                continue;
            }
            if (readRet < 0) {
                outError = AudioError::make(AudioError::Code::DecodeError,
                                            QString("av_read_frame: %1").arg(avErrorString(readRet)));
                return false;
            }

            if (pkt->stream_index != streamIndex) {
                av_packet_unref(pkt);
                continue;
            }

            const int sendRet = avcodec_send_packet(codec, pkt);
            av_packet_unref(pkt);
            if (sendRet < 0 && sendRet != AVERROR(EAGAIN)) {
                outError = AudioError::make(AudioError::Code::DecodeError,
                                            QString("avcodec_send_packet: %1").arg(avErrorString(sendRet)));
                return false;
            }
        }
    }
};

FFmpegDecoder::FFmpegDecoder() : m_impl(std::make_unique<Impl>()) {}
FFmpegDecoder::~FFmpegDecoder() { close(); }

AudioError FFmpegDecoder::open(const QString& filePath)
{
    m_impl->cleanup();

    const QByteArray pathUtf8 =
        QFileInfo(filePath).absoluteFilePath().toUtf8();

    int ret = avformat_open_input(&m_impl->fmt, pathUtf8.constData(),
                                  nullptr, nullptr);
    if (ret < 0) {
        return AudioError::make(AudioError::Code::FileNotFound,
                                QString("avformat_open_input: %1").arg(avErrorString(ret)),
                                filePath);
    }

    ret = avformat_find_stream_info(m_impl->fmt, nullptr);
    if (ret < 0) {
        m_impl->cleanup();
        return AudioError::make(AudioError::Code::DecodeError,
                                QString("avformat_find_stream_info: %1").arg(avErrorString(ret)),
                                filePath);
    }

    const AVCodec* decoder = nullptr;
    ret = av_find_best_stream(m_impl->fmt, AVMEDIA_TYPE_AUDIO,
                              -1, -1, &decoder, 0);
    if (ret < 0) {
        m_impl->cleanup();
        return AudioError::make(AudioError::Code::UnsupportedFormat,
                                QString("Аудиопоток не найден: %1").arg(avErrorString(ret)),
                                filePath);
    }
    m_impl->streamIndex = ret;

    AVStream* stream = m_impl->fmt->streams[m_impl->streamIndex];

    m_impl->codec = avcodec_alloc_context3(decoder);
    if (!m_impl->codec) {
        m_impl->cleanup();
        return AudioError::make(AudioError::Code::Unknown,
                                "avcodec_alloc_context3 failed");
    }

    ret = avcodec_parameters_to_context(m_impl->codec, stream->codecpar);
    if (ret < 0) {
        m_impl->cleanup();
        return AudioError::make(AudioError::Code::DecodeError,
                                QString("avcodec_parameters_to_context: %1").arg(avErrorString(ret)),
                                filePath);
    }

    ret = avcodec_open2(m_impl->codec, decoder, nullptr);
    if (ret < 0) {
        m_impl->cleanup();
        return AudioError::make(AudioError::Code::DecodeError,
                                QString("avcodec_open2: %1").arg(avErrorString(ret)),
                                filePath);
    }

    // Заполняем публичную метаинформацию.
    m_impl->info.sampleRate = m_impl->codec->sample_rate;
    m_impl->info.channels   = m_impl->codec->ch_layout.nb_channels;
    m_impl->info.codecName  = QString::fromUtf8(decoder->name);

    if (stream->duration != AV_NOPTS_VALUE && stream->time_base.den > 0) {
        m_impl->info.durationSec =
            stream->duration * av_q2d(stream->time_base);
    } else if (m_impl->fmt->duration != AV_NOPTS_VALUE) {
        m_impl->info.durationSec =
            m_impl->fmt->duration / static_cast<double>(AV_TIME_BASE);
    }

    if (m_impl->info.sampleRate <= 0 || m_impl->info.channels <= 0) {
        m_impl->cleanup();
        return AudioError::make(AudioError::Code::UnsupportedFormat,
                                "Некорректные параметры аудиопотока.", filePath);
    }

    // swr: кодек → float32 interleaved, частота и число каналов сохраняются.
    AVChannelLayout outLayout;
    av_channel_layout_default(&outLayout, m_impl->info.channels);

    ret = swr_alloc_set_opts2(&m_impl->swr,
                              &outLayout,
                              AV_SAMPLE_FMT_FLT,
                              m_impl->info.sampleRate,
                              &m_impl->codec->ch_layout,
                              m_impl->codec->sample_fmt,
                              m_impl->codec->sample_rate,
                              0, nullptr);
    av_channel_layout_uninit(&outLayout);

    if (ret < 0) {
        m_impl->cleanup();
        return AudioError::make(AudioError::Code::DecodeError,
                                QString("swr_alloc_set_opts2: %1").arg(avErrorString(ret)),
                                filePath);
    }

    ret = swr_init(m_impl->swr);
    if (ret < 0) {
        m_impl->cleanup();
        return AudioError::make(AudioError::Code::DecodeError,
                                QString("swr_init: %1").arg(avErrorString(ret)), filePath);
    }

    m_impl->pkt   = av_packet_alloc();
    m_impl->frame = av_frame_alloc();
    if (!m_impl->pkt || !m_impl->frame) {
        m_impl->cleanup();
        return AudioError::make(AudioError::Code::Unknown,
                                "av_packet_alloc/av_frame_alloc failed");
    }

    return AudioError::ok();
}

const AudioStreamInfo& FFmpegDecoder::streamInfo() const
{
    return m_impl->info;
}

int FFmpegDecoder::readFrames(float* out, int maxFrames, AudioError& outError)
{
    if (!m_impl->codec || maxFrames <= 0 || m_impl->info.channels <= 0)
        return 0;

    const int    ch         = m_impl->info.channels;
    const size_t wantFloat  = static_cast<size_t>(maxFrames) * ch;

    while (m_impl->sampleBuffer.size() < wantFloat && !m_impl->eof) {
        AudioError err;
        if (!m_impl->decodeNextFrame(err)) {
            if (err.isError()) {
                outError = err;
                return 0;
            }
            m_impl->eof = true;
            break;
        }
    }

    if (m_impl->sampleBuffer.empty())
        return 0;

    const size_t take = std::min(wantFloat, m_impl->sampleBuffer.size());
    std::memcpy(out, m_impl->sampleBuffer.data(), take * sizeof(float));
    m_impl->sampleBuffer.erase(m_impl->sampleBuffer.begin(),
                               m_impl->sampleBuffer.begin() + take);
    return static_cast<int>(take / ch);
}

void FFmpegDecoder::rewind()
{
    if (!m_impl->fmt || m_impl->streamIndex < 0)
        return;

    av_seek_frame(m_impl->fmt, m_impl->streamIndex, 0, AVSEEK_FLAG_BACKWARD);
    if (m_impl->codec) avcodec_flush_buffers(m_impl->codec);
    if (m_impl->swr)   swr_convert(m_impl->swr, nullptr, 0, nullptr, 0);
    m_impl->sampleBuffer.clear();
    m_impl->eof = false;
}

void FFmpegDecoder::close()
{
    if (m_impl) m_impl->cleanup();
}

#else // !HAVE_FFMPEG

struct FFmpegDecoder::Impl { AudioStreamInfo info; };

FFmpegDecoder::FFmpegDecoder() : m_impl(std::make_unique<Impl>()) {}
FFmpegDecoder::~FFmpegDecoder() = default;

AudioError FFmpegDecoder::open(const QString&)
{
    return AudioError::make(AudioError::Code::UnsupportedFormat,
                            "FFmpeg-бэкенд не скомпилирован (определите HAVE_FFMPEG в .pro).");
}

const AudioStreamInfo& FFmpegDecoder::streamInfo() const { return m_impl->info; }

int FFmpegDecoder::readFrames(float*, int, AudioError& outError)
{
    outError = AudioError::make(AudioError::Code::DecodeError, "not implemented");
    return 0;
}
void FFmpegDecoder::rewind() {}
void FFmpegDecoder::close()  {}

#endif // HAVE_FFMPEG

// #####################################################################
//
//                          FFmpegEncoder
//
// #####################################################################

#ifdef HAVE_FFMPEG

struct FFmpegEncoder::Impl
{
    AVFormatContext* fmt     = nullptr;
    AVCodecContext*  codec   = nullptr;
    AVStream*        stream  = nullptr;
    SwrContext*      swr     = nullptr;
    AVFrame*         frame   = nullptr;
    AVPacket*        pkt     = nullptr;

    int sampleRate       = 0;
    int channels         = 0;
    int codecFrameSize   = 1024; // предпочтительный размер кадра кодека

    // Накопитель входных float32-сэмплов (interleaved).
    std::vector<float> pending;

    int64_t nextPts        = 0;
    bool    headerWritten  = false;
    bool    finalized      = false;
    QString path;

    void cleanup()
    {
        if (swr)   swr_free(&swr);
        if (frame) av_frame_free(&frame);
        if (pkt)   av_packet_free(&pkt);
        if (codec) avcodec_free_context(&codec);
        if (fmt) {
            if (headerWritten && !finalized)
                av_write_trailer(fmt);
            if (fmt->pb) avio_closep(&fmt->pb);
            avformat_free_context(fmt);
        }
        fmt    = nullptr;
        codec  = nullptr;
        stream = nullptr;
        pending.clear();
    }

    AudioError encodeOneFrame(const float* data, int nbSamples);
    AudioError flushEncoder();
};

AudioError FFmpegEncoder::Impl::encodeOneFrame(const float* data, int nbSamples)
{
    // Защита: буфер кадра выделен под codecFrameSize сэмплов.
    if (nbSamples <= 0 || nbSamples > codecFrameSize) {
        return AudioError::make(AudioError::Code::EncodeError,
                                QString("encodeOneFrame: недопустимое число сэмплов (%1, максимум %2)")
                                    .arg(nbSamples).arg(codecFrameSize));
    }

    // Предыдущий вызов мог оставить в frame->nb_samples меньшее значение
    // (последний неполный кадр). Возвращаем полный размер, чтобы
    // av_frame_make_writable корректно выделил/скопировал буфер.
    frame->nb_samples = codecFrameSize;

    int ret = av_frame_make_writable(frame);
    if (ret < 0) {
        return AudioError::make(AudioError::Code::EncodeError,
                                QString("av_frame_make_writable: %1").arg(avErrorString(ret)));
    }

    const uint8_t* inPtr = reinterpret_cast<const uint8_t*>(data);

    // ВАЖНО: для planar-форматов (FLTP, S16P, S32P — AAC, MP3, Vorbis)
    // swr_convert ожидает массив указателей по числу каналов.
    // frame->extended_data содержит указатели на все плоскости
    // (для interleaved-форматов — один указатель), поэтому подходит всегда.
    const int got = swr_convert(swr,
                                frame->extended_data, nbSamples,
                                &inPtr,               nbSamples);
    if (got < 0) {
        return AudioError::make(AudioError::Code::EncodeError,
                                QString("swr_convert: %1").arg(avErrorString(got)));
    }

    frame->nb_samples = got;
    frame->pts        = nextPts;
    nextPts          += got;

    ret = avcodec_send_frame(codec, frame);
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
        return AudioError::make(AudioError::Code::EncodeError,
                                QString("avcodec_send_frame: %1").arg(avErrorString(ret)));
    }

    while (true) {
        ret = avcodec_receive_packet(codec, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) {
            return AudioError::make(AudioError::Code::EncodeError,
                                    QString("avcodec_receive_packet: %1").arg(avErrorString(ret)));
        }

        av_packet_rescale_ts(pkt,
                             AVRational{1, sampleRate},
                             stream->time_base);
        pkt->stream_index = stream->index;

        ret = av_interleaved_write_frame(fmt, pkt);
        av_packet_unref(pkt);
        if (ret < 0) {
            return AudioError::make(AudioError::Code::EncodeError,
                                    QString("av_interleaved_write_frame: %1").arg(avErrorString(ret)));
        }
    }
    return AudioError::ok();
}

AudioError FFmpegEncoder::Impl::flushEncoder()
{
    // Дописываем хвост pending.
    if (!pending.empty()) {
        const int haveFrames = static_cast<int>(pending.size() / channels);
        if (haveFrames > 0) {
            // Если у кодека фиксированный размер кадра — добиваем тишиной.
            const int realFrameSize = codec->frame_size;
            if (realFrameSize > 0 && haveFrames < realFrameSize) {
                pending.resize(static_cast<size_t>(realFrameSize) * channels, 0.0f);
                AudioError err = encodeOneFrame(pending.data(), realFrameSize);
                if (err.isError()) return err;
            } else {
                AudioError err = encodeOneFrame(pending.data(), haveFrames);
                if (err.isError()) return err;
            }
        }
        pending.clear();
    }

    // Сообщаем кодеку, что вход закончился.
    int ret = avcodec_send_frame(codec, nullptr);
    if (ret < 0 && ret != AVERROR_EOF) {
        return AudioError::make(AudioError::Code::EncodeError,
                                QString("avcodec_send_frame(nullptr): %1").arg(avErrorString(ret)));
    }

    while (true) {
        ret = avcodec_receive_packet(codec, pkt);
        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) break;
        if (ret < 0) {
            return AudioError::make(AudioError::Code::EncodeError,
                                    QString("avcodec_receive_packet: %1").arg(avErrorString(ret)));
        }

        av_packet_rescale_ts(pkt,
                             AVRational{1, sampleRate},
                             stream->time_base);
        pkt->stream_index = stream->index;

        ret = av_interleaved_write_frame(fmt, pkt);
        av_packet_unref(pkt);
        if (ret < 0) {
            return AudioError::make(AudioError::Code::EncodeError,
                                    QString("av_interleaved_write_frame: %1").arg(avErrorString(ret)));
        }
    }
    return AudioError::ok();
}

FFmpegEncoder::FFmpegEncoder() : m_impl(std::make_unique<Impl>()) {}
FFmpegEncoder::~FFmpegEncoder() { close(); }

AudioError FFmpegEncoder::open(const QString& filePath,
                               int sampleRate,
                               int channels,
                               const ProcessingParameters& params)
{
    m_impl->cleanup();
    m_impl->path = filePath;

    if (sampleRate <= 0 || channels <= 0) {
        return AudioError::make(AudioError::Code::EncodeError,
                                "Некорректные sampleRate/channels", filePath);
    }

    // Создаём output-контекст по расширению файла.
    const QByteArray pathUtf8 = filePath.toUtf8();
    int ret = avformat_alloc_output_context2(&m_impl->fmt, nullptr,
                                             nullptr, pathUtf8.constData());
    if (ret < 0 || !m_impl->fmt) {
        return AudioError::make(AudioError::Code::EncodeError,
                                QString("avformat_alloc_output_context2: %1").arg(avErrorString(ret)),
                                filePath);
    }

    // Подбираем кодер.
    const QString ext = QFileInfo(filePath).suffix().toLower();
    const AVCodec* codec = nullptr;
    const AVCodecID wantId = pickCodecIdForExtension(ext);
    if (wantId != AV_CODEC_ID_NONE)
        codec = avcodec_find_encoder(wantId);
    if (!codec && m_impl->fmt->oformat->audio_codec != AV_CODEC_ID_NONE)
        codec = avcodec_find_encoder(m_impl->fmt->oformat->audio_codec);

    if (!codec) {
        m_impl->cleanup();
        return AudioError::make(AudioError::Code::UnsupportedFormat,
                                QString("Не найден кодек для расширения '%1'").arg(ext), filePath);
    }

    m_impl->codec = avcodec_alloc_context3(codec);
    if (!m_impl->codec) {
        m_impl->cleanup();
        return AudioError::make(AudioError::Code::Unknown,
                                "avcodec_alloc_context3 failed");
    }

    m_impl->codec->sample_rate = sampleRate;
    m_impl->codec->bit_rate    = params.bitrateKbps * 1000;
    av_channel_layout_default(&m_impl->codec->ch_layout, channels);

    // Формат сэмплов — первый поддерживаемый кодеком.
    // В FFmpeg 9 поле AVCodec::sample_fmts удалено, используем
    // avcodec_get_supported_config() (см. pickFirstSampleFormat).
    m_impl->codec->sample_fmt = pickFirstSampleFormat(codec);

    if (m_impl->fmt->oformat->flags & AVFMT_GLOBALHEADER)
        m_impl->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    ret = avcodec_open2(m_impl->codec, codec, nullptr);
    if (ret < 0) {
        m_impl->cleanup();
        return AudioError::make(AudioError::Code::EncodeError,
                                QString("avcodec_open2: %1").arg(avErrorString(ret)), filePath);
    }

    m_impl->sampleRate     = m_impl->codec->sample_rate;
    m_impl->channels       = m_impl->codec->ch_layout.nb_channels;
    m_impl->codecFrameSize = m_impl->codec->frame_size > 0
                                 ? m_impl->codec->frame_size
                                 : 1024;

    // Создаём поток.
    m_impl->stream = avformat_new_stream(m_impl->fmt, nullptr);
    if (!m_impl->stream) {
        m_impl->cleanup();
        return AudioError::make(AudioError::Code::EncodeError,
                                "avformat_new_stream failed");
    }

    ret = avcodec_parameters_from_context(m_impl->stream->codecpar,
                                          m_impl->codec);
    if (ret < 0) {
        m_impl->cleanup();
        return AudioError::make(AudioError::Code::EncodeError,
                                QString("avcodec_parameters_from_context: %1").arg(avErrorString(ret)));
    }
    m_impl->stream->time_base = AVRational{1, m_impl->sampleRate};

    // swr: float32 interleaved → формат кодека.
    AVChannelLayout inLayout;
    av_channel_layout_default(&inLayout, channels);
    ret = swr_alloc_set_opts2(&m_impl->swr,
                              &m_impl->codec->ch_layout,
                              m_impl->codec->sample_fmt,
                              m_impl->sampleRate,
                              &inLayout,
                              AV_SAMPLE_FMT_FLT,
                              sampleRate,
                              0, nullptr);
    av_channel_layout_uninit(&inLayout);
    if (ret < 0) {
        m_impl->cleanup();
        return AudioError::make(AudioError::Code::EncodeError,
                                QString("swr_alloc_set_opts2: %1").arg(avErrorString(ret)));
    }

    ret = swr_init(m_impl->swr);
    if (ret < 0) {
        m_impl->cleanup();
        return AudioError::make(AudioError::Code::EncodeError,
                                QString("swr_init: %1").arg(avErrorString(ret)));
    }

    // Открываем выходной файл.
    if (!(m_impl->fmt->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&m_impl->fmt->pb, pathUtf8.constData(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            m_impl->cleanup();
            return AudioError::make(AudioError::Code::IoError,
                                    QString("avio_open: %1").arg(avErrorString(ret)), filePath);
        }
    }

    ret = avformat_write_header(m_impl->fmt, nullptr);
    if (ret < 0) {
        m_impl->cleanup();
        return AudioError::make(AudioError::Code::EncodeError,
                                QString("avformat_write_header: %1").arg(avErrorString(ret)),
                                filePath);
    }
    m_impl->headerWritten = true;

    // Готовим AVFrame для отправки в кодек.
    m_impl->frame = av_frame_alloc();
    m_impl->frame->format      = m_impl->codec->sample_fmt;
    m_impl->frame->sample_rate = m_impl->sampleRate;
    av_channel_layout_copy(&m_impl->frame->ch_layout,
                           &m_impl->codec->ch_layout);
    m_impl->frame->nb_samples  = m_impl->codecFrameSize;

    ret = av_frame_get_buffer(m_impl->frame, 0);
    if (ret < 0) {
        m_impl->cleanup();
        return AudioError::make(AudioError::Code::EncodeError,
                                QString("av_frame_get_buffer: %1").arg(avErrorString(ret)));
    }

    m_impl->pkt = av_packet_alloc();
    if (!m_impl->pkt) {
        m_impl->cleanup();
        return AudioError::make(AudioError::Code::Unknown,
                                "av_packet_alloc failed");
    }

    return AudioError::ok();
}

AudioError FFmpegEncoder::writeFrames(const float* in, int frames)
{
    if (!m_impl->codec || !in || frames <= 0)
        return AudioError::ok();

    // Пополняем накопитель.
    const size_t addSamples = static_cast<size_t>(frames) * m_impl->channels;
    m_impl->pending.insert(m_impl->pending.end(), in, in + addSamples);

    // Кодируем, пока в буфере хватает на целый кадр.
    const size_t frameSamples =
        static_cast<size_t>(m_impl->codecFrameSize) * m_impl->channels;

    while (m_impl->pending.size() >= frameSamples) {
        AudioError err = m_impl->encodeOneFrame(m_impl->pending.data(),
                                                m_impl->codecFrameSize);
        if (err.isError()) return err;
        m_impl->pending.erase(m_impl->pending.begin(),
                              m_impl->pending.begin() + frameSamples);
    }
    return AudioError::ok();
}

AudioError FFmpegEncoder::finalize()
{
    if (!m_impl->codec || m_impl->finalized)
        return AudioError::ok();

    AudioError err = m_impl->flushEncoder();
    if (err.isError()) return err;

    const int ret = av_write_trailer(m_impl->fmt);
    if (ret < 0) {
        return AudioError::make(AudioError::Code::EncodeError,
                                QString("av_write_trailer: %1").arg(avErrorString(ret)));
    }
    m_impl->finalized = true;
    return AudioError::ok();
}

void FFmpegEncoder::close()
{
    if (m_impl) m_impl->cleanup();
}

#else // !HAVE_FFMPEG

struct FFmpegEncoder::Impl {};

FFmpegEncoder::FFmpegEncoder() : m_impl(std::make_unique<Impl>()) {}
FFmpegEncoder::~FFmpegEncoder() = default;

AudioError FFmpegEncoder::open(const QString&,
                               int, int,
                               const ProcessingParameters&)
{
    return AudioError::make(AudioError::Code::UnsupportedFormat,
                            "FFmpeg-энкодер не скомпилирован (определите HAVE_FFMPEG в .pro).");
}
AudioError FFmpegEncoder::writeFrames(const float*, int)
{
    return AudioError::make(AudioError::Code::EncodeError, "not implemented");
}
AudioError FFmpegEncoder::finalize() { return AudioError::ok(); }
void       FFmpegEncoder::close()    {}

#endif // HAVE_FFMPEG

// #####################################################################
//
//                          SoundTouchStretcher
//
// #####################################################################

struct SoundTouchStretcher::Impl
{
#ifdef HAVE_SOUNDTOUCH
    soundtouch::SoundTouch st;
#endif
};

SoundTouchStretcher::SoundTouchStretcher()
    : m_impl(std::make_unique<Impl>())
{}
SoundTouchStretcher::~SoundTouchStretcher() = default;

void SoundTouchStretcher::setSampleRate(int hz)
{
#ifdef HAVE_SOUNDTOUCH
    m_impl->st.setSampleRate(static_cast<unsigned>(hz));
#else
    Q_UNUSED(hz);
#endif
}

void SoundTouchStretcher::setChannels(int n)
{
#ifdef HAVE_SOUNDTOUCH
    m_impl->st.setChannels(static_cast<unsigned>(n));
#else
    Q_UNUSED(n);
#endif
}

void SoundTouchStretcher::setTempo(double tempo)
{
#ifdef HAVE_SOUNDTOUCH
    // tempo = коэффициент скорости; 2.0 → вдвое быстрее, питч сохранён.
    m_impl->st.setTempo(static_cast<float>(tempo));
#else
    Q_UNUSED(tempo);
#endif
}

void SoundTouchStretcher::setPitch(double semitones)
{
#ifdef HAVE_SOUNDTOUCH
    m_impl->st.setPitchSemiTones(static_cast<float>(semitones));
#else
    Q_UNUSED(semitones);
#endif
}

void SoundTouchStretcher::setQuality(int quality)
{
#ifdef HAVE_SOUNDTOUCH
    // 0..100. Три профиля.
    if (quality < 34) {
        // Быстрый режим.
        m_impl->st.setSetting(SETTING_USE_QUICKSEEK, 1);
        m_impl->st.setSetting(SETTING_USE_AA_FILTER, 0);
    } else if (quality < 67) {
        // Сбалансированный.
        m_impl->st.setSetting(SETTING_USE_QUICKSEEK, 0);
        m_impl->st.setSetting(SETTING_USE_AA_FILTER, 1);
    } else {
        // Максимальное качество.
        m_impl->st.setSetting(SETTING_USE_QUICKSEEK, 0);
        m_impl->st.setSetting(SETTING_USE_AA_FILTER, 1);
        m_impl->st.setSetting(SETTING_AA_FILTER_LENGTH, 64);
        m_impl->st.setSetting(SETTING_SEQUENCE_MS,     60);
        m_impl->st.setSetting(SETTING_SEEKWINDOW_MS,   28);
        m_impl->st.setSetting(SETTING_OVERLAP_MS,      12);
    }
#else
    Q_UNUSED(quality);
#endif
}

void SoundTouchStretcher::putSamples(const float* in, std::size_t frames)
{
#ifdef HAVE_SOUNDTOUCH
    m_impl->st.putSamples(in, static_cast<unsigned>(frames));
#else
    Q_UNUSED(in);
    Q_UNUSED(frames);
#endif
}

std::size_t SoundTouchStretcher::receiveSamples(float* out, std::size_t maxFrames)
{
#ifdef HAVE_SOUNDTOUCH
    return m_impl->st.receiveSamples(out, static_cast<unsigned>(maxFrames));
#else
    Q_UNUSED(out);
    Q_UNUSED(maxFrames);
    return 0;
#endif
}

void SoundTouchStretcher::flush()
{
#ifdef HAVE_SOUNDTOUCH
    m_impl->st.flush();
#endif
}

void SoundTouchStretcher::reset()
{
#ifdef HAVE_SOUNDTOUCH
    m_impl->st.clear();
#endif
}

// #####################################################################
//
//                          RubberBandStretcher
//
// #####################################################################

#ifdef HAVE_RUBBERBAND

struct RubberBandStretcher::Impl
{
    int    sampleRate = 0;
    int    channels   = 0;
    double timeRatio  = 1.0;   // 1.0 = без изменений
    double pitchScale = 1.0;   // 1.0 = без изменений
    int    quality    = 80;

    std::unique_ptr<RubberBand::RubberBandStretcher> rb;

    // Планарные буферы для конвертации interleaved ↔ planar.
    std::vector<std::vector<float>> inPlanar;
    std::vector<std::vector<float>> outPlanar;
    std::vector<const float*>       inPtrs;
    std::vector<float*>             outPtrs;

    bool created = false;

    void destroy()
    {
        rb.reset();
        created = false;
    }

    bool ensureCreated()
    {
        if (created) return true;
        if (sampleRate <= 0 || channels <= 0) return false;

        using RB = RubberBand::RubberBandStretcher;
        int options = RB::OptionProcessRealTime;
        if (quality >= 50)
            options |= RB::OptionStretchPrecise;

        try {
            rb = std::make_unique<RB>(
                static_cast<size_t>(sampleRate),
                static_cast<size_t>(channels),
                options,
                timeRatio,
                pitchScale);
        } catch (...) {
            rb.reset();
            return false;
        }

        inPlanar.resize(channels);
        outPlanar.resize(channels);
        inPtrs.resize(channels);
        outPtrs.resize(channels);

        created = true;
        return true;
    }
};

RubberBandStretcher::RubberBandStretcher()
    : m_impl(std::make_unique<Impl>())
{}
RubberBandStretcher::~RubberBandStretcher() = default;

void RubberBandStretcher::setSampleRate(int hz)
{
    m_impl->sampleRate = hz;
    m_impl->destroy(); // пересоздадим при следующем использовании
}
void RubberBandStretcher::setChannels(int n)
{
    m_impl->channels = n;
    m_impl->destroy();
}
void RubberBandStretcher::setTempo(double tempo)
{
    // RubberBand: timeRatio = 1/tempo.
    //   tempo=2.0 (быстрее вдвое) → timeRatio = 0.5
    if (tempo > 0.0) {
        m_impl->timeRatio = 1.0 / tempo;
        if (m_impl->rb) m_impl->rb->setTimeRatio(m_impl->timeRatio);
    }
}
void RubberBandStretcher::setPitch(double semitones)
{
    // semitones → pitchScale = 2^(semitones/12)
    m_impl->pitchScale = std::pow(2.0, semitones / 12.0);
    if (m_impl->rb) m_impl->rb->setPitchScale(m_impl->pitchScale);
}
void RubberBandStretcher::setQuality(int quality)
{
    m_impl->quality = quality;
    m_impl->destroy();
}

void RubberBandStretcher::putSamples(const float* in, std::size_t frames)
{
    if (!m_impl->ensureCreated() || frames == 0) return;

    const int ch = m_impl->channels;
    for (int c = 0; c < ch; ++c) {
        m_impl->inPlanar[c].resize(frames);
        for (std::size_t i = 0; i < frames; ++i)
            m_impl->inPlanar[c][i] = in[i * ch + c];
        m_impl->inPtrs[c] = m_impl->inPlanar[c].data();
    }
    m_impl->rb->process(m_impl->inPtrs.data(), frames, /*final=*/false);
}

std::size_t RubberBandStretcher::receiveSamples(float* out, std::size_t maxFrames)
{
    if (!m_impl->created || maxFrames == 0) return 0;

    const int avail = m_impl->rb->available();
    if (avail <= 0) return 0;

    const std::size_t n = std::min(static_cast<std::size_t>(avail), maxFrames);
    const int ch = m_impl->channels;

    for (int c = 0; c < ch; ++c) {
        m_impl->outPlanar[c].resize(n);
        m_impl->outPtrs[c] = m_impl->outPlanar[c].data();
    }
    m_impl->rb->retrieve(m_impl->outPtrs.data(), n);

    // Interleave.
    for (std::size_t i = 0; i < n; ++i)
        for (int c = 0; c < ch; ++c)
            out[i * ch + c] = m_impl->outPlanar[c][i];

    return n;
}

void RubberBandStretcher::flush()
{
    if (!m_impl->created) return;

    const int ch = m_impl->channels;
    for (int c = 0; c < ch; ++c) {
        m_impl->inPlanar[c].clear();
        m_impl->inPtrs[c] = m_impl->inPlanar[c].data();
    }
    m_impl->rb->process(m_impl->inPtrs.data(), 0, /*final=*/true);
}

void RubberBandStretcher::reset()
{
    m_impl->destroy();
}

#else // !HAVE_RUBBERBAND

struct RubberBandStretcher::Impl {};

RubberBandStretcher::RubberBandStretcher()
    : m_impl(std::make_unique<Impl>())
{}
RubberBandStretcher::~RubberBandStretcher() = default;

void RubberBandStretcher::setSampleRate(int)      {}
void RubberBandStretcher::setChannels(int)        {}
void RubberBandStretcher::setTempo(double)        {}
void RubberBandStretcher::setPitch(double)        {}
void RubberBandStretcher::setQuality(int)         {}
void RubberBandStretcher::putSamples(const float*, std::size_t) {}

std::size_t RubberBandStretcher::receiveSamples(float*, std::size_t)
{
    return 0;
}
void RubberBandStretcher::flush() {}
void RubberBandStretcher::reset() {}

#endif // HAVE_RUBBERBAND
