#pragma once

#include "commands/Command.hpp"
#include "model/Session.hpp"
#include "model/MidiClip.hpp"

namespace ampl {

// --- Add MIDI clip to track ---
class AddMidiClipCommand : public Command
{
public:
    AddMidiClipCommand(int trackIndex, const MidiClip& clip)
        : trackIndex_(trackIndex), clip_(clip) {}

    void execute(Session& session) override
    {
        if (auto* track = session.getTrack(trackIndex_))
            track->midiClips.push_back(clip_);
    }

    void undo(Session& session) override
    {
        if (auto* track = session.getTrack(trackIndex_))
        {
            auto& clips = track->midiClips;
            for (auto it = clips.begin(); it != clips.end(); ++it)
            {
                if (it->id == clip_.id)
                {
                    clips.erase(it);
                    break;
                }
            }
        }
    }

    juce::String getDescription() const override { return "Add MIDI Clip"; }

private:
    int trackIndex_;
    MidiClip clip_;
};

// --- Remove MIDI clip from track ---
class RemoveMidiClipCommand : public Command
{
public:
    RemoveMidiClipCommand(int trackIndex, const juce::String& clipId)
        : trackIndex_(trackIndex), clipId_(clipId) {}

    void execute(Session& session) override
    {
        if (auto* track = session.getTrack(trackIndex_))
        {
            auto& clips = track->midiClips;
            for (auto it = clips.begin(); it != clips.end(); ++it)
            {
                if (it->id == clipId_)
                {
                    savedClip_ = *it;
                    clips.erase(it);
                    break;
                }
            }
        }
    }

    void undo(Session& session) override
    {
        if (auto* track = session.getTrack(trackIndex_))
            track->midiClips.push_back(savedClip_);
    }

    juce::String getDescription() const override { return "Remove MIDI Clip"; }

private:
    int trackIndex_;
    juce::String clipId_;
    MidiClip savedClip_;
};

// --- Add note to MIDI clip ---
class AddMidiNoteCommand : public Command
{
public:
    AddMidiNoteCommand(int trackIndex, const juce::String& clipId, const MidiNote& note)
        : trackIndex_(trackIndex), clipId_(clipId), note_(note) {}

    void execute(Session& session) override
    {
        if (auto* track = session.getTrack(trackIndex_))
            if (auto* clip = track->findMidiClip(clipId_))
                clip->notes.push_back(note_);
    }

    void undo(Session& session) override
    {
        if (auto* track = session.getTrack(trackIndex_))
        {
            if (auto* clip = track->findMidiClip(clipId_))
            {
                auto& notes = clip->notes;
                for (auto it = notes.begin(); it != notes.end(); ++it)
                {
                    if (it->id == note_.id)
                    {
                        notes.erase(it);
                        break;
                    }
                }
            }
        }
    }

    juce::String getDescription() const override { return "Add MIDI Note"; }

private:
    int trackIndex_;
    juce::String clipId_;
    MidiNote note_;
};

// --- Remove note from MIDI clip ---
class RemoveMidiNoteCommand : public Command
{
public:
    RemoveMidiNoteCommand(int trackIndex, const juce::String& clipId, const juce::String& noteId)
        : trackIndex_(trackIndex), clipId_(clipId), noteId_(noteId) {}

    void execute(Session& session) override
    {
        if (auto* track = session.getTrack(trackIndex_))
        {
            if (auto* clip = track->findMidiClip(clipId_))
            {
                auto& notes = clip->notes;
                for (auto it = notes.begin(); it != notes.end(); ++it)
                {
                    if (it->id == noteId_)
                    {
                        savedNote_ = *it;
                        notes.erase(it);
                        break;
                    }
                }
            }
        }
    }

    void undo(Session& session) override
    {
        if (auto* track = session.getTrack(trackIndex_))
            if (auto* clip = track->findMidiClip(clipId_))
                clip->notes.push_back(savedNote_);
    }

    juce::String getDescription() const override { return "Remove MIDI Note"; }

private:
    int trackIndex_;
    juce::String clipId_;
    juce::String noteId_;
    MidiNote savedNote_;
};

// --- Move MIDI note (change position and/or pitch) ---
class MoveMidiNoteCommand : public Command
{
public:
    MoveMidiNoteCommand(int trackIndex, const juce::String& clipId, const juce::String& noteId,
                        SampleCount newStart, int newNoteNumber)
        : trackIndex_(trackIndex), clipId_(clipId), noteId_(noteId),
          newStart_(newStart), newNoteNumber_(newNoteNumber) {}

    void execute(Session& session) override
    {
        if (auto* track = session.getTrack(trackIndex_))
        {
            if (auto* clip = track->findMidiClip(clipId_))
            {
                if (auto* note = clip->findNote(noteId_))
                {
                    oldStart_ = note->startSample;
                    oldNoteNumber_ = note->noteNumber;
                    note->startSample = newStart_;
                    note->noteNumber = newNoteNumber_;
                }
            }
        }
    }

    void undo(Session& session) override
    {
        if (auto* track = session.getTrack(trackIndex_))
            if (auto* clip = track->findMidiClip(clipId_))
                if (auto* note = clip->findNote(noteId_))
                {
                    note->startSample = oldStart_;
                    note->noteNumber = oldNoteNumber_;
                }
    }

    juce::String getDescription() const override { return "Move MIDI Note"; }

private:
    int trackIndex_;
    juce::String clipId_;
    juce::String noteId_;
    SampleCount newStart_;
    int newNoteNumber_;
    SampleCount oldStart_{0};
    int oldNoteNumber_{60};
};

// --- Resize MIDI note ---
class ResizeMidiNoteCommand : public Command
{
public:
    ResizeMidiNoteCommand(int trackIndex, const juce::String& clipId, const juce::String& noteId,
                          SampleCount newLength)
        : trackIndex_(trackIndex), clipId_(clipId), noteId_(noteId), newLength_(newLength) {}

    void execute(Session& session) override
    {
        if (auto* track = session.getTrack(trackIndex_))
            if (auto* clip = track->findMidiClip(clipId_))
                if (auto* note = clip->findNote(noteId_))
                {
                    oldLength_ = note->lengthSamples;
                    note->lengthSamples = newLength_;
                }
    }

    void undo(Session& session) override
    {
        if (auto* track = session.getTrack(trackIndex_))
            if (auto* clip = track->findMidiClip(clipId_))
                if (auto* note = clip->findNote(noteId_))
                    note->lengthSamples = oldLength_;
    }

    juce::String getDescription() const override { return "Resize MIDI Note"; }

private:
    int trackIndex_;
    juce::String clipId_;
    juce::String noteId_;
    SampleCount newLength_;
    SampleCount oldLength_{0};
};

// --- Set MIDI note velocity ---
class SetMidiNoteVelocityCommand : public Command
{
public:
    SetMidiNoteVelocityCommand(int trackIndex, const juce::String& clipId,
                                const juce::String& noteId, float newVelocity)
        : trackIndex_(trackIndex), clipId_(clipId), noteId_(noteId), newVelocity_(newVelocity) {}

    void execute(Session& session) override
    {
        if (auto* track = session.getTrack(trackIndex_))
            if (auto* clip = track->findMidiClip(clipId_))
                if (auto* note = clip->findNote(noteId_))
                {
                    oldVelocity_ = note->velocity;
                    note->velocity = newVelocity_;
                }
    }

    void undo(Session& session) override
    {
        if (auto* track = session.getTrack(trackIndex_))
            if (auto* clip = track->findMidiClip(clipId_))
                if (auto* note = clip->findNote(noteId_))
                    note->velocity = oldVelocity_;
    }

    juce::String getDescription() const override { return "Set Note Velocity"; }

private:
    int trackIndex_;
    juce::String clipId_;
    juce::String noteId_;
    float newVelocity_;
    float oldVelocity_{0.8f};
};

} // namespace ampl
