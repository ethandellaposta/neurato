#pragma once

#include <juce_core/juce_core.h>
#include "util/Types.hpp"
#include <vector>
#include <algorithm>

namespace ampl {

// A single MIDI note event within a MidiClip.
struct MidiNote
{
    juce::String id{juce::Uuid().toString()};
    int noteNumber{60};           // 0-127 (C4 = 60)
    float velocity{0.8f};         // 0.0 - 1.0
    SampleCount startSample{0};   // Position within the clip (relative to clip start)
    SampleCount lengthSamples{0}; // Duration

    SampleCount getEndSample() const { return startSample + lengthSamples; }

    MidiNote clone() const
    {
        MidiNote n;
        n.id = juce::Uuid().toString();
        n.noteNumber = noteNumber;
        n.velocity = velocity;
        n.startSample = startSample;
        n.lengthSamples = lengthSamples;
        return n;
    }
};

// A MIDI clip on the timeline, containing a sequence of MidiNotes.
struct MidiClip
{
    juce::String id{juce::Uuid().toString()};
    juce::String name{"MIDI"};

    SampleCount timelineStartSample{0};
    SampleCount lengthSamples{0};   // Total clip length on timeline

    std::vector<MidiNote> notes;

    SampleCount getTimelineEndSample() const
    {
        return timelineStartSample + lengthSamples;
    }

    MidiClip clone() const
    {
        MidiClip c;
        c.id = juce::Uuid().toString();
        c.name = name;
        c.timelineStartSample = timelineStartSample;
        c.lengthSamples = lengthSamples;
        c.notes.reserve(notes.size());
        for (const auto& n : notes)
            c.notes.push_back(n.clone());
        return c;
    }

    // Factory: create an empty MIDI clip of given length
    static MidiClip createEmpty(SampleCount startSample, SampleCount length,
                                 const juce::String& clipName = "MIDI")
    {
        MidiClip c;
        c.timelineStartSample = startSample;
        c.lengthSamples = length;
        c.name = clipName;
        return c;
    }

    // Find note by ID
    MidiNote* findNote(const juce::String& noteId)
    {
        for (auto& n : notes)
            if (n.id == noteId) return &n;
        return nullptr;
    }

    const MidiNote* findNote(const juce::String& noteId) const
    {
        for (const auto& n : notes)
            if (n.id == noteId) return &n;
        return nullptr;
    }

    // Get the highest note number and lowest for display range
    int getLowestNote() const
    {
        if (notes.empty()) return 60;
        int lo = 127;
        for (const auto& n : notes)
            lo = std::min(lo, n.noteNumber);
        return lo;
    }

    int getHighestNote() const
    {
        if (notes.empty()) return 72;
        int hi = 0;
        for (const auto& n : notes)
            hi = std::max(hi, n.noteNumber);
        return hi;
    }
};

} // namespace ampl
