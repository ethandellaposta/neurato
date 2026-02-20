#pragma once

#include "util/Types.h"
#include "model/Session.h"
#include "model/MidiClip.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <vector>
#include <memory>
#include "PluginManager.h"
#include "PianoSynth.h"
#include <array>

namespace neurato {

// RT-safe snapshot of the session for audio rendering.
// All data is resolved at publish time on the UI thread.
// The audio thread only reads plain values and raw pointers
// to immutable AudioAsset sample data — zero allocations, zero locks.
struct RenderClip
{
    // Raw pointers into immutable AudioAsset channel data.
    // Max 2 channels for now; easily extensible.
    const float* ch0{nullptr};
    const float* ch1{nullptr};
    int numChannels{0};
    SampleCount assetLength{0};

    SampleCount timelineStart{0};
    SampleCount sourceStart{0};
    SampleCount sourceLength{0};

    float gainLinear{1.0f};
    SampleCount fadeInSamples{0};
    SampleCount fadeOutSamples{0};
};

struct RenderMidiNote
{
    int noteNumber{60};
    float velocity{0.8f};
    SampleCount absoluteStart{0};  // Timeline-absolute start
    SampleCount absoluteEnd{0};    // Timeline-absolute end
};

struct RenderMidiClip
{
    std::vector<RenderMidiNote> notes;
};

struct RenderTrack
{
    float gainLinear{1.0f};
    float panL{1.0f};     // Pre-computed constant-power pan coefficients
    float panR{1.0f};
    bool muted{false};
    bool solo{false};
    bool isMidi{false};

    std::vector<RenderClip> clips;       // Audio clips
    std::vector<RenderMidiClip> midiClips; // MIDI clips
};

struct RenderSnapshot
{
    std::vector<RenderTrack> tracks;
    bool hasSoloedTrack{false};

    // Master bus
    float masterGainLinear{1.0f};
    float masterPanL{1.0f};
    float masterPanR{1.0f};

    // Keep AudioAssets alive while this snapshot is in use.
    // Only touched by the UI thread during publish/delete — never by audio thread.
    std::vector<AudioAssetPtr> assetRefs;
};

// Manages publishing session state to the audio thread via atomic pointer swap.
// UI thread calls publishSession() whenever the session changes.
// Audio thread calls process() to render audio from the current snapshot.
class SessionRenderer
{
public:
    SessionRenderer();
    ~SessionRenderer();

    // UI thread: publish a new snapshot from the current session state.
    void publishSession(const Session& session);

    // Audio thread: render all tracks/clips into the output buffer.
    // Completely RT-safe — no allocations, no locks, no shared_ptr ops.
    void process(float* leftOut, float* rightOut, int numSamples,
                 SampleCount position) noexcept;

    void setSampleRate(double sr) noexcept { sampleRate_ = sr; }

private:
    // Simple sine synth for MIDI playback
    void renderMidiTrack(const RenderTrack& track, float* leftOut, float* rightOut,
                         int numSamples, SampleCount position) noexcept;

    static constexpr int kMaxPolyVoices = 32;
    struct SynthVoice
    {
        double phase{0.0};
        double phaseInc{0.0};
        int noteNumber{-1};
        float velocity{0.0f};
        bool active{false};
    };
    std::array<SynthVoice, kMaxPolyVoices> voices_{};
    double sampleRate_{44100.0};

    std::unique_ptr<PianoSynth> pianoSynth_;
    std::unique_ptr<PluginManager> pluginManager_;

    std::atomic<RenderSnapshot*> pending_{nullptr};
    RenderSnapshot* active_{nullptr};
    RenderSnapshot* retired_{nullptr};
};

} // namespace neurato
