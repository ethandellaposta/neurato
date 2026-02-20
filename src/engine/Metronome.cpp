#include "engine/Metronome.h"
#include <cmath>

namespace neurato {

Metronome::Metronome() = default;

void Metronome::setEnabled(bool enabled) noexcept
{
    enabled_.store(enabled, std::memory_order_release);
}

bool Metronome::isEnabled() const noexcept
{
    return enabled_.load(std::memory_order_acquire);
}

void Metronome::setGain(float gain) noexcept
{
    gain_.store(gain, std::memory_order_release);
}

void Metronome::setSampleRate(double sampleRate) noexcept
{
    sampleRate_ = sampleRate;
}

void Metronome::process(float* leftChannel, float* rightChannel, int numSamples,
                         const Transport& transport) noexcept
{
    if (!enabled_.load(std::memory_order_acquire))
        return;

    if (transport.getState() != Transport::State::Playing)
    {
        lastBeatPosition_ = -1.0;
        clickSamplesRemaining_ = 0;
        return;
    }

    const float g = gain_.load(std::memory_order_acquire);
    const double bpm = transport.getBpm();
    const int timeSigNum = transport.getTimeSigNumerator();

    if (bpm <= 0.0 || sampleRate_ <= 0.0)
        return;

    const double samplesPerBeat = (60.0 / bpm) * sampleRate_;
    const SampleCount transportPos = transport.getPositionInSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const SampleCount samplePos = transportPos - numSamples + i;
        const double beatPosition = static_cast<double>(samplePos) / samplesPerBeat;

        // Detect beat boundary crossing
        if (lastBeatPosition_ >= 0.0)
        {
            double currentBeatFloor = std::floor(beatPosition);
            double lastBeatFloor = std::floor(lastBeatPosition_);

            if (currentBeatFloor > lastBeatFloor && beatPosition >= 0.0)
            {
                // New beat — trigger click
                clickSamplesRemaining_ = kClickDurationSamples;
                clickPhase_ = 0.0;

                // Determine if this is a downbeat
                int beatInBar = static_cast<int>(currentBeatFloor) % timeSigNum;
                currentClickIsDownbeat_ = (beatInBar == 0);
            }
        }

        lastBeatPosition_ = beatPosition;

        // Render click if active
        if (clickSamplesRemaining_ > 0)
        {
            float sample = synthesizeClick(clickPhase_, currentClickIsDownbeat_) * g;

            // Apply envelope (quick attack, exponential decay)
            float envelope = static_cast<float>(clickSamplesRemaining_) /
                             static_cast<float>(kClickDurationSamples);
            envelope = envelope * envelope; // Quadratic decay
            sample *= envelope;

            if (leftChannel != nullptr)
                leftChannel[i] += sample;
            if (rightChannel != nullptr)
                rightChannel[i] += sample;

            double freq = currentClickIsDownbeat_ ? kDownbeatFreq : kBeatFreq;
            clickPhase_ += freq / sampleRate_;
            if (clickPhase_ >= 1.0)
                clickPhase_ -= 1.0;

            --clickSamplesRemaining_;
        }
    }
}

float Metronome::synthesizeClick(double phase, bool isDownbeat) const noexcept
{
    // Simple sine wave click
    (void)isDownbeat; // Frequency already selected by caller
    return static_cast<float>(std::sin(phase * 2.0 * 3.14159265358979323846));
}

} // namespace neurato
