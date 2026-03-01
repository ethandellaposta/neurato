#pragma once

#include <juce_core/juce_core.h>
#include "util/Types.hpp"
#include <vector>
#include <memory>

namespace ampl {

// Immutable audio data loaded from a file. Shared across clips that reference
// the same source file. Never modified after creation — RT-safe to read.
struct AudioAsset
{
    juce::String filePath;
    juce::String fileName;
    std::vector<std::vector<float>> channels; // [channel][sample]
    SampleCount lengthInSamples{0};
    double sampleRate{0.0};
    int numChannels{0};
};

using AudioAssetPtr = std::shared_ptr<const AudioAsset>;

// A clip represents a region of audio on a track's timeline.
// All edits are non-destructive — the source AudioAsset is never modified.
struct Clip
{
    juce::String id;                    // Unique clip ID
    AudioAssetPtr asset;                // Source audio data (immutable, shared)

    // Position on the timeline (in samples, relative to project start)
    SampleCount timelineStartSample{0};

    // Region within the source asset to play
    SampleCount sourceStartSample{0};   // Trim start (offset into asset)
    SampleCount sourceLengthSamples{0}; // How many samples to play from source

    // Non-destructive edit parameters
    float gainDb{0.0f};                 // Gain in dB
    SampleCount fadeInSamples{0};       // Fade-in duration
    SampleCount fadeOutSamples{0};      // Fade-out duration

    // Computed helpers
    SampleCount getTimelineEndSample() const { return timelineStartSample + sourceLengthSamples; }

    // Create a deep copy (for undo snapshots)
    Clip clone() const
    {
        Clip c = *this;
        c.id = juce::Uuid().toString(); // New unique ID
        return c;
    }

    // Factory: create a clip from a full asset
    static Clip fromAsset(AudioAssetPtr asset, SampleCount timelineStart = 0)
    {
        Clip c;
        c.id = juce::Uuid().toString();
        c.asset = asset;
        c.timelineStartSample = timelineStart;
        c.sourceStartSample = 0;
        c.sourceLengthSamples = asset ? asset->lengthInSamples : 0;
        return c;
    }
};

} // namespace ampl
