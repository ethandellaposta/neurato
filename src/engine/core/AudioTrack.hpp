#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include "util/Types.hpp"

namespace ampl {

// A single audio track that can play one audio clip.
// The audio data is pre-loaded into memory for RT-safe playback.
// The process() method is called from the audio thread — no allocations.
class AudioTrack
{
public:
    AudioTrack();
    ~AudioTrack();

    // UI thread: load an audio file into memory
    bool loadFile(const juce::File& file, juce::AudioFormatManager& formatManager);

    // UI thread: unload audio data
    void unloadFile();

    // Audio thread: render samples into the output buffer (additive)
    void process(float* leftChannel, float* rightChannel, int numSamples,
                 SampleCount transportPosition) noexcept;

    // Thread-safe accessors
    bool hasAudio() const noexcept;
    SampleCount getLengthInSamples() const noexcept;
    double getSampleRate() const noexcept;
    int getNumChannels() const noexcept;

    void setGain(float gain) noexcept;
    float getGain() const noexcept;

    void setMute(bool mute) noexcept;
    bool isMuted() const noexcept;

    const juce::String& getFileName() const { return fileName_; }

private:
    // Audio data — written on UI thread, read on audio thread via atomic pointer swap
    struct AudioData
    {
        std::vector<std::vector<float>> channels; // [channel][sample]
        SampleCount lengthInSamples{0};
        double sampleRate{0.0};
        int numChannels{0};
    };

    std::atomic<AudioData*> audioData_{nullptr};

    std::atomic<float> gain_{1.0f};
    std::atomic<bool> muted_{false};

    juce::String fileName_;
};

} // namespace ampl
