#pragma once

#include <juce_core/juce_core.h>
#include "model/Clip.h"
#include "model/MidiClip.h"
#include <vector>
#include <atomic>

namespace neurato {

enum class TrackType { Audio, Midi };

// A track holds an ordered list of non-overlapping clips on the timeline.
// Track state is modified on the UI thread; the audio thread reads a
// snapshot via atomic pointer swap.
struct TrackState
{
    juce::String id;
    juce::String name;
    TrackType type{TrackType::Audio};

    // Audio clips (used when type == Audio)
    std::vector<Clip> clips;

    // MIDI clips (used when type == Midi)
    std::vector<MidiClip> midiClips;

    float gainDb{0.0f};
    float pan{0.0f};       // -1.0 (left) to 1.0 (right)
    bool muted{false};
    bool solo{false};

    bool isAudio() const { return type == TrackType::Audio; }
    bool isMidi()  const { return type == TrackType::Midi; }

    // Deep copy for undo snapshots
    TrackState clone() const
    {
        TrackState t;
        t.id = id;
        t.name = name;
        t.type = type;
        t.clips = clips; // Clips share AudioAssetPtr (cheap copy)
        for (const auto& mc : midiClips)
            t.midiClips.push_back(mc.clone());
        t.gainDb = gainDb;
        t.pan = pan;
        t.muted = muted;
        t.solo = solo;
        return t;
    }

    // Find a MIDI clip by ID
    MidiClip* findMidiClip(const juce::String& clipId)
    {
        for (auto& mc : midiClips)
            if (mc.id == clipId) return &mc;
        return nullptr;
    }

    const MidiClip* findMidiClip(const juce::String& clipId) const
    {
        for (const auto& mc : midiClips)
            if (mc.id == clipId) return &mc;
        return nullptr;
    }
};

} // namespace neurato
