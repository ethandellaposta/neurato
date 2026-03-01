#pragma once

#include <juce_core/juce_core.h>
#include "model/Clip.hpp"
#include "model/MidiClip.hpp"
#include <vector>
#include <atomic>
#include <map>
#include <optional>

namespace ampl {

enum class TrackType { Audio, Midi };

// Plugin slot in a track's plugin chain
struct PluginSlot
{
    juce::String pluginId;           // Unique identifier (e.g., "VST3:Fabfilter Pro-Q 3")
    juce::String pluginName;         // Human-readable name
    juce::String pluginFormat;       // "VST3", "AU", "Internal"
    juce::String pluginPath;         // File path to plugin (for VST3/AU)
    bool bypassed{false};
    bool isResolved{false};          // True if plugin was found on this system

    // Plugin state (chunk data for save/restore)
    std::vector<uint8_t> stateData;

    // Parameter values (fallback if chunk not available)
    std::map<juce::String, float> parameterValues;

    // Original identifier from import (for matching)
    juce::String originalIdentifier; // e.g., Logic AU identifier

    PluginSlot clone() const
    {
        PluginSlot s;
        s.pluginId = pluginId;
        s.pluginName = pluginName;
        s.pluginFormat = pluginFormat;
        s.pluginPath = pluginPath;
        s.bypassed = bypassed;
        s.isResolved = isResolved;
        s.stateData = stateData;
        s.parameterValues = parameterValues;
        s.originalIdentifier = originalIdentifier;
        return s;
    }
};

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

    // Plugin chain (insert effects, instruments)
    std::vector<PluginSlot> pluginChain;

    // Instrument plugin (for MIDI tracks)
    std::optional<PluginSlot> instrumentPlugin;

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
        for (const auto& ps : pluginChain)
            t.pluginChain.push_back(ps.clone());
        if (instrumentPlugin.has_value())
            t.instrumentPlugin = instrumentPlugin->clone();
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

} // namespace ampl
