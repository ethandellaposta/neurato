#include "engine/core/Transport.hpp"

namespace ampl
{

Transport::Transport() = default;

void Transport::play()
{
    state_.store(State::Playing, std::memory_order_release);
}

void Transport::pause()
{
    state_.store(State::Paused, std::memory_order_release);
}

void Transport::stop()
{
    state_.store(State::Stopped, std::memory_order_release);
    positionInSamples_.store(0, std::memory_order_release);
}

void Transport::togglePlayStop()
{
    if (getState() == State::Playing)
        pause();
    else
        play();
}

void Transport::setBpm(double bpm)
{
    if (bpm > 20.0 && bpm < 999.0)
        bpm_.store(bpm, std::memory_order_release);
}

void Transport::setTimeSignature(int numerator, int denominator)
{
    timeSigNumerator_.store(numerator, std::memory_order_release);
    timeSigDenominator_.store(denominator, std::memory_order_release);
}

void Transport::setPositionInSamples(SampleCount position)
{
    positionInSamples_.store(position, std::memory_order_release);
}

void Transport::setSampleRate(SampleRate sr)
{
    sampleRate_.store(sr, std::memory_order_release);
}

void Transport::advance(int numSamples) noexcept
{
    if (state_.load(std::memory_order_acquire) == State::Playing)
    {
        positionInSamples_.fetch_add(static_cast<SampleCount>(numSamples),
                                     std::memory_order_relaxed);
    }
}

Transport::State Transport::getState() const noexcept
{
    return state_.load(std::memory_order_acquire);
}

double Transport::getBpm() const noexcept
{
    return bpm_.load(std::memory_order_acquire);
}

SampleCount Transport::getPositionInSamples() const noexcept
{
    return positionInSamples_.load(std::memory_order_acquire);
}

double Transport::getPositionInBeats() const noexcept
{
    return samplesToBeats(getPositionInSamples());
}

double Transport::getPositionInSeconds() const noexcept
{
    auto sr = getSampleRate();
    if (sr <= 0.0)
        return 0.0;
    return static_cast<double>(getPositionInSamples()) / sr;
}

SampleRate Transport::getSampleRate() const noexcept
{
    return sampleRate_.load(std::memory_order_acquire);
}

int Transport::getTimeSigNumerator() const noexcept
{
    return timeSigNumerator_.load(std::memory_order_acquire);
}

int Transport::getTimeSigDenominator() const noexcept
{
    return timeSigDenominator_.load(std::memory_order_acquire);
}

double Transport::samplesToBeats(SampleCount samples) const noexcept
{
    auto sr = getSampleRate();
    auto bpm = getBpm();
    if (sr <= 0.0 || bpm <= 0.0)
        return 0.0;
    double seconds = static_cast<double>(samples) / sr;
    return seconds * (bpm / 60.0);
}

SampleCount Transport::beatsToSamples(double beats) const noexcept
{
    auto sr = getSampleRate();
    auto bpm = getBpm();
    if (bpm <= 0.0)
        return 0;
    double seconds = beats / (bpm / 60.0);
    return static_cast<SampleCount>(seconds * sr);
}

} // namespace ampl
