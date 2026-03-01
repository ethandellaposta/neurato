#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include "model/Session.hpp"
#include "util/Types.hpp"

namespace ampl {

// Renders the session to an audio file faster-than-realtime.
// Processes the same audio graph as the real-time engine but without
// device I/O constraints. Runs on a background thread.
class OfflineRenderer
{
public:
    struct Settings
    {
        double sampleRate{44100.0};
        int bitsPerSample{24};
        int numChannels{2};
        int blockSize{512};
        SampleCount startSample{0};
        SampleCount endSample{0}; // 0 = auto-detect from session content
    };

    struct Progress
    {
        double fraction{0.0};   // 0.0 to 1.0
        bool complete{false};
        bool cancelled{false};
        juce::String error;
    };

    // Render the session to a WAV file. Blocking call — run on a background thread.
    // Progress callback is called periodically (from the render thread).
    static bool render(const Session& session,
                       const juce::File& outputFile,
                       const Settings& settings,
                       std::function<void(const Progress&)> progressCallback = nullptr,
                       std::atomic<bool>* cancelFlag = nullptr);

private:
    // Process a single block of audio from the session
    static void processBlock(const Session& session,
                             juce::AudioBuffer<float>& buffer,
                             SampleCount position,
                             int numSamples);
};

} // namespace ampl
