#include "audio/stretchers/RubberBandStretcher.h"

// TODO: реализовать через librubberband при необходимости.
// Структура идентична SoundTouch-версии — можно скопировать и адаптировать.

struct RubberBandStretcher::Impl {};

RubberBandStretcher::RubberBandStretcher() : m_impl(std::make_unique<Impl>()) {}
RubberBandStretcher::~RubberBandStretcher() = default;
void RubberBandStretcher::setSampleRate(int)  {}
void RubberBandStretcher::setChannels(int)    {}
void RubberBandStretcher::setTempo(double)    {}
void RubberBandStretcher::setPitch(double)    {}
void RubberBandStretcher::setQuality(int)     {}
void RubberBandStretcher::putSamples(const float*, std::size_t) {}
std::size_t RubberBandStretcher::receiveSamples(float*, std::size_t) { return 0; }
void RubberBandStretcher::flush() {}
void RubberBandStretcher::reset() {}
