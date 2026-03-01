#pragma once

#include "util/Types.hpp"
#include <atomic>

namespace ampl
{

// RT-safe transport state. All members are atomic for lock-free access
// from both the audio thread and the UI thread.
class Transport
{
  public:
    enum class State
    {
        Stopped,
        Playing,
        Paused
    };

    Transport();

    // UI thread
    void play();
    void pause();
    void stop();
    void togglePlayStop();
    void setBpm(double bpm);
    void setTimeSignature(int numerator, int denominator);
    void setPositionInSamples(SampleCount position);
    void setSampleRate(SampleRate sr);

    // Audio thread — called per buffer
    void advance(int numSamples) noexcept;

    // Thread-safe reads
    State getState() const noexcept;
    double getBpm() const noexcept;
    SampleCount getPositionInSamples() const noexcept;
    double getPositionInBeats() const noexcept;
    double getPositionInSeconds() const noexcept;
    SampleRate getSampleRate() const noexcept;
    int getTimeSigNumerator() const noexcept;
    int getTimeSigDenominator() const noexcept;

    // Utility
    double samplesToBeats(SampleCount samples) const noexcept;
    SampleCount beatsToSamples(double beats) const noexcept;

  private:
    std::atomic<State> state_{State::Stopped};
    std::atomic<double> bpm_{120.0};
    std::atomic<SampleCount> positionInSamples_{0};
    std::atomic<SampleRate> sampleRate_{44100.0};
    std::atomic<int> timeSigNumerator_{4};
    std::atomic<int> timeSigDenominator_{4};
};

} // namespace ampl
