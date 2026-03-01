#pragma once

#include "engine/core/Transport.hpp"

namespace ampl {

// RT-safe metronome that synthesizes click sounds.
// No allocations, no locks — pure DSP in the audio callback.
class Metronome
{
public:
    Metronome();

    void setEnabled(bool enabled) noexcept;
    bool isEnabled() const noexcept;

    void setGain(float gain) noexcept;

    // Called from audio thread only. Renders click into the buffer,
    // mixing (adding) to existing content.
    void process(float* leftChannel, float* rightChannel, int numSamples,
                 const Transport& transport) noexcept;

    void setSampleRate(double sampleRate) noexcept;

private:
    // Synthesize a single click sample at the given phase
    float synthesizeClick(double phase, bool isDownbeat) const noexcept;

    std::atomic<bool> enabled_{true};
    std::atomic<float> gain_{0.5f};
    double sampleRate_{44100.0};

    // Click state — only touched on audio thread
    double clickPhase_{0.0};
    int clickSamplesRemaining_{0};
    bool currentClickIsDownbeat_{false};

    // Beat tracking — only touched on audio thread
    double lastBeatPosition_{-1.0};

    // Click parameters
    static constexpr int kClickDurationSamples = 2000; // ~45ms at 44.1kHz
    static constexpr double kDownbeatFreq = 1200.0;    // Higher pitch for downbeat
    static constexpr double kBeatFreq = 800.0;         // Lower pitch for other beats
};

} // namespace ampl
