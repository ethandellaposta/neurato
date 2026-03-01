#pragma once

#include "engine/plugins/instruments/PianoSynth.hpp"
#include "engine/plugins/manager/PluginManager.hpp"
#include "model/MidiClip.hpp"
#include "model/Session.hpp"
#include "util/Types.hpp"
#include <array>
#include <atomic>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>

namespace ampl
{

// RT-safe snapshot of the session for audio rendering.
// All data is resolved at publish time on the UI thread.
// The audio thread only reads plain values and raw pointers
// to immutable AudioAsset sample data — zero allocations, zero locks.
struct RenderClip
{
    // Raw pointers into immutable AudioAsset channel data.
    // Max 2 channels for now; easily extensible.
    const float *ch0{nullptr};
    const float *ch1{nullptr};
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
    SampleCount absoluteStart{0}; // Timeline-absolute start
    SampleCount absoluteEnd{0};   // Timeline-absolute end
};

struct RenderMidiClip
{
    std::vector<RenderMidiNote> notes;
};

struct RenderTrack
{
    float gainLinear{1.0f};
    float panL{1.0f}; // Pre-computed constant-power pan coefficients
    float panR{1.0f};
    bool muted{false};
    bool solo{false};
    bool isMidi{false};
    bool isRecordArmed{false}; // Track is armed for audio input recording

    std::vector<RenderClip> clips;         // Audio clips
    std::vector<RenderMidiClip> midiClips; // MIDI clips

    // Plugin chain — loaded plugin instances for this track.
    // Instrument (index 0 for MIDI tracks) + insert effects.
    // Owned by UI thread; audio thread reads via snapshot.
    struct PluginSlotInstance
    {
        juce::AudioPluginInstance *instance{nullptr}; // raw ptr, owned by pluginInstances_
        bool bypassed{false};
        bool isInstrument{false};
    };
    std::vector<PluginSlotInstance> pluginSlots;
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
    void publishSession(const Session &session);

    // Audio thread: render all tracks/clips into the output buffer.
    // Completely RT-safe — no allocations, no locks, no shared_ptr ops.
    void process(float *leftOut, float *rightOut, int numSamples, SampleCount position) noexcept;

    // Audio thread: render with external MIDI input and audio input.
    void processWithExternalIO(float *leftOut, float *rightOut, int numSamples,
                               SampleCount position,
                               const float *audioInLeft, const float *audioInRight,
                               juce::MidiBuffer &externalMidi) noexcept;

    void setSampleRate(double sr) noexcept
    {
        sampleRate_ = sr;
    }

    void setBlockSize(int blockSize) noexcept
    {
        blockSize_ = blockSize;
    }

    // Access to plugin manager for plugin resolution
    PluginManager *getPluginManager()
    {
        return pluginManager_.get();
    }

  private:
    // Simple sine synth for MIDI playback
    void renderMidiTrack(const RenderTrack &track, float *leftOut, float *rightOut, int numSamples,
                         SampleCount position) noexcept;

    // Process a track's plugin chain (instruments + effects)
    void processPluginChain(const RenderTrack &track,
                           juce::AudioBuffer<float> &buffer,
                           juce::MidiBuffer &midi) noexcept;

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
    int blockSize_{512};

    std::unique_ptr<PianoSynth> pianoSynth_;
    std::unique_ptr<PluginManager> pluginManager_;

    // Plugin instances owned here, raw ptrs go into RenderSnapshot
    std::vector<std::unique_ptr<juce::AudioPluginInstance>> pluginInstances_;
    std::mutex pluginInstancesMutex_; // protects pluginInstances_ on UI thread

    std::atomic<RenderSnapshot *> pending_{nullptr};
    RenderSnapshot *active_{nullptr};
    RenderSnapshot *retired_{nullptr};
};

} // namespace ampl
