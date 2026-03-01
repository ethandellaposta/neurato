#pragma once

#include "commands/Command.hpp"
#include "model/Clip.hpp"
#include "model/Session.hpp"
#include "model/Track.hpp"

namespace ampl
{

// --- Add a clip to a track ---
class AddClipCommand : public Command
{
public:
    AddClipCommand(int trackIndex, Clip clip)
        : trackIndex_(trackIndex), clip_(std::move(clip)) {}

    void execute(Session &session) override
    {
        session.addClipToTrack(trackIndex_, clip_);
    }

    void undo(Session &session) override
    {
        session.removeClipFromTrack(trackIndex_, clip_.id);
    }

    juce::String getDescription() const override { return "Add Clip"; }

private:
    int  trackIndex_;
    Clip clip_;
};

// --- Remove a clip from a track ---
class RemoveClipCommand : public Command
{
public:
    RemoveClipCommand(int trackIndex, const juce::String &clipId)
        : trackIndex_(trackIndex), clipId_(clipId) {}

    void execute(Session &session) override
    {
        if (auto *clip = session.findClip(clipId_))
            savedClip_ = *clip;
        session.removeClipFromTrack(trackIndex_, clipId_);
    }

    void undo(Session &session) override
    {
        session.addClipToTrack(trackIndex_, savedClip_);
    }

    juce::String getDescription() const override { return "Remove Clip"; }

private:
    int          trackIndex_;
    juce::String clipId_;
    Clip         savedClip_;
};

// --- Move a clip on the timeline ---
class MoveClipCommand : public Command
{
public:
    MoveClipCommand(const juce::String &clipId, SampleCount newStartSample)
        : clipId_(clipId), newStart_(newStartSample) {}

    void execute(Session &session) override
    {
        if (auto *clip = session.findClip(clipId_))
        {
            oldStart_ = clip->timelineStartSample;
            clip->timelineStartSample = newStart_;
        }
    }

    void undo(Session &session) override
    {
        if (auto *clip = session.findClip(clipId_))
            clip->timelineStartSample = oldStart_;
    }

    juce::String getDescription() const override { return "Move Clip"; }

private:
    juce::String clipId_;
    SampleCount  newStart_;
    SampleCount  oldStart_{0};
};

// --- Trim clip (change source start/length) ---
class TrimClipCommand : public Command
{
public:
    TrimClipCommand(const juce::String &clipId,
                    SampleCount         newSourceStart,
                    SampleCount         newSourceLength,
                    SampleCount         newTimelineStart)
        : clipId_(clipId),
          newSourceStart_(newSourceStart),
          newSourceLength_(newSourceLength),
          newTimelineStart_(newTimelineStart)
    {}

    void execute(Session &session) override
    {
        if (auto *clip = session.findClip(clipId_))
        {
            oldSourceStart_   = clip->sourceStartSample;
            oldSourceLength_  = clip->sourceLengthSamples;
            oldTimelineStart_ = clip->timelineStartSample;
            clip->sourceStartSample   = newSourceStart_;
            clip->sourceLengthSamples = newSourceLength_;
            clip->timelineStartSample = newTimelineStart_;
        }
    }

    void undo(Session &session) override
    {
        if (auto *clip = session.findClip(clipId_))
        {
            clip->sourceStartSample   = oldSourceStart_;
            clip->sourceLengthSamples = oldSourceLength_;
            clip->timelineStartSample = oldTimelineStart_;
        }
    }

    juce::String getDescription() const override { return "Trim Clip"; }

private:
    juce::String clipId_;
    SampleCount  newSourceStart_, newSourceLength_, newTimelineStart_;
    SampleCount  oldSourceStart_{0}, oldSourceLength_{0}, oldTimelineStart_{0};
};

// --- Set clip gain ---
class SetClipGainCommand : public Command
{
public:
    SetClipGainCommand(const juce::String &clipId, float newGainDb)
        : clipId_(clipId), newGainDb_(newGainDb) {}

    void execute(Session &session) override
    {
        if (auto *clip = session.findClip(clipId_))
        {
            oldGainDb_   = clip->gainDb;
            clip->gainDb = newGainDb_;
        }
    }

    void undo(Session &session) override
    {
        if (auto *clip = session.findClip(clipId_))
            clip->gainDb = oldGainDb_;
    }

    juce::String getDescription() const override { return "Set Clip Gain"; }

private:
    juce::String clipId_;
    float        newGainDb_;
    float        oldGainDb_{0.0f};
};

// --- Set clip fade in/out ---
class SetClipFadeCommand : public Command
{
public:
    SetClipFadeCommand(const juce::String &clipId,
                       SampleCount         fadeIn,
                       SampleCount         fadeOut)
        : clipId_(clipId), newFadeIn_(fadeIn), newFadeOut_(fadeOut) {}

    void execute(Session &session) override
    {
        if (auto *clip = session.findClip(clipId_))
        {
            oldFadeIn_           = clip->fadeInSamples;
            oldFadeOut_          = clip->fadeOutSamples;
            clip->fadeInSamples  = newFadeIn_;
            clip->fadeOutSamples = newFadeOut_;
        }
    }

    void undo(Session &session) override
    {
        if (auto *clip = session.findClip(clipId_))
        {
            clip->fadeInSamples  = oldFadeIn_;
            clip->fadeOutSamples = oldFadeOut_;
        }
    }

    juce::String getDescription() const override { return "Set Clip Fade"; }

private:
    juce::String clipId_;
    SampleCount  newFadeIn_, newFadeOut_;
    SampleCount  oldFadeIn_{0}, oldFadeOut_{0};
};

// --- Set track gain ---
class SetTrackGainCommand : public Command
{
public:
    SetTrackGainCommand(int trackIndex, float newGainDb)
        : trackIndex_(trackIndex), newGainDb_(newGainDb) {}

    void execute(Session &session) override
    {
        if (auto *track = session.getTrack(trackIndex_))
        {
            oldGainDb_    = track->gainDb;
            track->gainDb = newGainDb_;
        }
    }

    void undo(Session &session) override
    {
        if (auto *track = session.getTrack(trackIndex_))
            track->gainDb = oldGainDb_;
    }

    juce::String getDescription() const override { return "Set Track Gain"; }

private:
    int   trackIndex_;
    float newGainDb_;
    float oldGainDb_{0.0f};
};

// --- Set track mute ---
class SetTrackMuteCommand : public Command
{
public:
    SetTrackMuteCommand(int trackIndex, bool muted)
        : trackIndex_(trackIndex), newMuted_(muted) {}

    void execute(Session &session) override
    {
        if (auto *track = session.getTrack(trackIndex_))
        {
            oldMuted_    = track->muted;
            track->muted = newMuted_;
        }
    }

    void undo(Session &session) override
    {
        if (auto *track = session.getTrack(trackIndex_))
            track->muted = oldMuted_;
    }

    juce::String getDescription() const override
    {
        return newMuted_ ? "Mute Track" : "Unmute Track";
    }

private:
    int  trackIndex_;
    bool newMuted_;
    bool oldMuted_{false};
};

// --- Set BPM ---
class SetBpmCommand : public Command
{
public:
    explicit SetBpmCommand(double newBpm) : newBpm_(newBpm) {}

    void execute(Session &session) override
    {
        oldBpm_ = session.getBpm();
        session.setBpm(newBpm_);
    }

    void undo(Session &session) override { session.setBpm(oldBpm_); }

    juce::String getDescription() const override
    {
        return "Set BPM to " + juce::String(newBpm_, 1);
    }

private:
    double newBpm_;
    double oldBpm_{120.0};
};

// --- Set track pan ---
class SetTrackPanCommand : public Command
{
public:
    SetTrackPanCommand(int trackIndex, float newPan)
        : trackIndex_(trackIndex), newPan_(newPan) {}

    void execute(Session &session) override
    {
        if (auto *track = session.getTrack(trackIndex_))
        {
            oldPan_    = track->pan;
            track->pan = newPan_;
        }
    }

    void undo(Session &session) override
    {
        if (auto *track = session.getTrack(trackIndex_))
            track->pan = oldPan_;
    }

    juce::String getDescription() const override { return "Set Track Pan"; }

private:
    int   trackIndex_;
    float newPan_;
    float oldPan_{0.0f};
};

// --- Set track solo ---
class SetTrackSoloCommand : public Command
{
public:
    SetTrackSoloCommand(int trackIndex, bool solo)
        : trackIndex_(trackIndex), newSolo_(solo) {}

    void execute(Session &session) override
    {
        if (auto *track = session.getTrack(trackIndex_))
        {
            oldSolo_    = track->solo;
            track->solo = newSolo_;
        }
    }

    void undo(Session &session) override
    {
        if (auto *track = session.getTrack(trackIndex_))
            track->solo = oldSolo_;
    }

    juce::String getDescription() const override
    {
        return newSolo_ ? "Solo Track" : "Unsolo Track";
    }

private:
    int  trackIndex_;
    bool newSolo_;
    bool oldSolo_{false};
};

// --- Add track (audio or MIDI) ---
class AddTrackCommand : public Command
{
public:
    AddTrackCommand(TrackType    type,
                    juce::String name                = {},
                    bool         withDefaultMidiClip = false)
        : trackType_(type),
          trackName_(std::move(name)),
          withDefaultMidiClip_(withDefaultMidiClip)
    {}

    void execute(Session &session) override
    {
        if (!hasSavedTrack_)
        {
            insertIndex_ = session.addTrack(trackName_, trackType_);
            if (withDefaultMidiClip_ && trackType_ == TrackType::Midi)
            {
                if (auto *track = session.getTrack(insertIndex_))
                {
                    double      bpm = session.getBpm();
                    double      sr  = session.getSampleRate();
                    if (sr  <= 0.0) sr  = 44100.0;
                    if (bpm <= 0.0) bpm = 120.0;
                    SampleCount barLen = static_cast<SampleCount>(
                        (60.0 / bpm) * sr * session.getTimeSigNumerator());
                    track->midiClips.push_back(MidiClip::createEmpty(0, barLen * 4));
                }
            }
            if (auto *track = session.getTrack(insertIndex_))
            {
                savedTrack_    = track->clone();
                hasSavedTrack_ = true;
            }
            return;
        }
        session.insertTrack(insertIndex_, savedTrack_);
    }

    void undo(Session &session) override { session.removeTrack(insertIndex_); }

    juce::String getDescription() const override
    {
        if (trackType_ == TrackType::Midi)
            return "Add MIDI Track";
        return "Add Audio Track";
    }

private:
    TrackType    trackType_{TrackType::Audio};
    juce::String trackName_;
    bool         withDefaultMidiClip_{false};
    int          insertIndex_{-1};
    bool         hasSavedTrack_{false};
    TrackState   savedTrack_;
};

// --- Reorder track (move from one index to another) ---
class ReorderTrackCommand : public Command
{
public:
    ReorderTrackCommand(int fromIndex, int toIndex)
        : fromIndex_(fromIndex), toIndex_(toIndex) {}

    void execute(Session &session) override { session.moveTrack(fromIndex_, toIndex_); }
    void undo(Session &session) override    { session.moveTrack(toIndex_, fromIndex_); }

    juce::String getDescription() const override { return "Reorder Track"; }

private:
    int fromIndex_;
    int toIndex_;
};

// --- Remove track ---
class RemoveTrackCommand : public Command
{
public:
    explicit RemoveTrackCommand(int trackIndex) : trackIndex_(trackIndex) {}

    void execute(Session &session) override
    {
        if (auto *track = session.getTrack(trackIndex_))
            savedTrack_ = track->clone();
        session.removeTrack(trackIndex_);
    }

    void undo(Session &session) override { session.insertTrack(trackIndex_, savedTrack_); }

    juce::String getDescription() const override { return "Remove Track"; }

private:
    int        trackIndex_;
    TrackState savedTrack_;
};

// --- Rename track ---
class RenameTrackCommand : public Command
{
public:
    RenameTrackCommand(int trackIndex, const juce::String &newName)
        : trackIndex_(trackIndex), newName_(newName) {}

    void execute(Session &session) override
    {
        if (auto *track = session.getTrack(trackIndex_))
        {
            oldName_    = track->name;
            track->name = newName_;
        }
    }

    void undo(Session &session) override
    {
        if (auto *track = session.getTrack(trackIndex_))
            track->name = oldName_;
    }

    juce::String getDescription() const override { return "Rename Track"; }

private:
    int          trackIndex_;
    juce::String newName_;
    juce::String oldName_;
};

// --- Set master gain ---
class SetMasterGainCommand : public Command
{
public:
    explicit SetMasterGainCommand(float newGainDb) : newGainDb_(newGainDb) {}

    void execute(Session &session) override
    {
        oldGainDb_ = session.getMasterGainDb();
        session.setMasterGainDb(newGainDb_);
    }

    void undo(Session &session) override { session.setMasterGainDb(oldGainDb_); }

    juce::String getDescription() const override { return "Set Master Gain"; }

private:
    float newGainDb_;
    float oldGainDb_{0.0f};
};

// --- Set master pan ---
class SetMasterPanCommand : public Command
{
public:
    explicit SetMasterPanCommand(float newPan) : newPan_(newPan) {}

    void execute(Session &session) override
    {
        oldPan_ = session.getMasterPan();
        session.setMasterPan(newPan_);
    }

    void undo(Session &session) override { session.setMasterPan(oldPan_); }

    juce::String getDescription() const override { return "Set Master Pan"; }

private:
    float newPan_;
    float oldPan_{0.0f};
};

// --- Set track instrument plugin ---
class SetTrackInstrumentCommand : public Command
{
public:
    SetTrackInstrumentCommand(int trackIndex, std::optional<PluginSlot> slot)
        : trackIndex_(trackIndex), newSlot_(std::move(slot)) {}

    void execute(Session &session) override
    {
        if (auto *track = session.getTrack(trackIndex_))
        {
            oldSlot_                = track->instrumentPlugin;
            track->instrumentPlugin = newSlot_;
        }
    }

    void undo(Session &session) override
    {
        if (auto *track = session.getTrack(trackIndex_))
            track->instrumentPlugin = oldSlot_;
    }

    juce::String getDescription() const override
    {
        return newSlot_.has_value() ? "Set Instrument" : "Clear Instrument";
    }

private:
    int                       trackIndex_;
    std::optional<PluginSlot> newSlot_;
    std::optional<PluginSlot> oldSlot_;
};

// --- Add FX plugin to track chain ---
class AddTrackFxCommand : public Command
{
public:
    AddTrackFxCommand(int trackIndex, PluginSlot slot)
        : trackIndex_(trackIndex), slot_(std::move(slot)) {}

    void execute(Session &session) override
    {
        if (auto *track = session.getTrack(trackIndex_))
            track->pluginChain.push_back(slot_);
    }

    void undo(Session &session) override
    {
        if (auto *track = session.getTrack(trackIndex_))
            if (!track->pluginChain.empty())
                track->pluginChain.pop_back();
    }

    juce::String getDescription() const override { return "Add FX: " + slot_.pluginName; }

private:
    int        trackIndex_;
    PluginSlot slot_;
};

// --- Remove FX plugin from track chain ---
class RemoveTrackFxCommand : public Command
{
public:
    RemoveTrackFxCommand(int trackIndex, int fxIndex)
        : trackIndex_(trackIndex), fxIndex_(fxIndex) {}

    void execute(Session &session) override
    {
        if (auto *track = session.getTrack(trackIndex_))
        {
            if (fxIndex_ >= 0 && fxIndex_ < static_cast<int>(track->pluginChain.size()))
            {
                removedSlot_ = track->pluginChain[static_cast<size_t>(fxIndex_)];
                track->pluginChain.erase(track->pluginChain.begin() + fxIndex_);
            }
        }
    }

    void undo(Session &session) override
    {
        if (auto *track = session.getTrack(trackIndex_))
        {
            auto pos = std::min(fxIndex_, static_cast<int>(track->pluginChain.size()));
            track->pluginChain.insert(track->pluginChain.begin() + pos, removedSlot_);
        }
    }

    juce::String getDescription() const override { return "Remove FX: " + removedSlot_.pluginName; }

private:
    int        trackIndex_;
    int        fxIndex_;
    PluginSlot removedSlot_;
};

} // namespace ampl
