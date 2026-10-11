#include "audio/AudioInterfaces.h"
#include "audio/AudioBackends.h"   // конкретные реализации

std::unique_ptr<IAudioDecoder> AudioBackendFactory::createDecoder()
{
    return std::make_unique<FFmpegDecoder>();
}

std::unique_ptr<IAudioEncoder> AudioBackendFactory::createEncoder()
{
    return std::make_unique<FFmpegEncoder>();
}

std::unique_ptr<ITimeStretcher>
AudioBackendFactory::createStretcher(AudioEngine engine)
{
    switch (engine) {
    case AudioEngine::RubberBand:
        return std::make_unique<RubberBandStretcher>();
    case AudioEngine::SoundTouch:
    default:
        return std::make_unique<SoundTouchStretcher>();
    }
}
