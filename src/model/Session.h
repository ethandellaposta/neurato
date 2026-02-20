#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "model/Track.h"
#include "model/Clip.h"
#include <vector>
#include <unordered_map>
#include <memory>

namespace neurato {

// The Session is the top-level model object representing the entire project.
// It owns tracks, audio assets, tempo, time signature, and loop region.
// Modified on the UI thread only. Audio thread reads snapshots.
class Session
{
public:
    Session();

    // --- Tempo & Time Signature ---
    double getBpm() const { return bpm_; }
    void setBpm(double bpm) { bpm_ = bpm; }

    int getTimeSigNumerator() const { return timeSigNumerator_; }
    int getTimeSigDenominator() const { return timeSigDenominator_; }
    void setTimeSignature(int num, int den) { timeSigNumerator_ = num; timeSigDenominator_ = den; }

    double getSampleRate() const { return sampleRate_; }
    void setSampleRate(double sr) { sampleRate_ = sr; }

    // --- Loop Region ---
    struct LoopRegion
    {
        bool enabled{false};
        SampleCount startSample{0};
        SampleCount endSample{0};
    };

    const LoopRegion& getLoopRegion() const { return loopRegion_; }
    void setLoopRegion(SampleCount start, SampleCount end, bool enabled = true);
    void setLoopEnabled(bool enabled) { loopRegion_.enabled = enabled; }

    // --- Tracks ---
    const std::vector<TrackState>& getTracks() const { return tracks_; }
    std::vector<TrackState>& getTracks() { return tracks_; }

    int addTrack(const juce::String& name = "", TrackType type = TrackType::Audio);
    int addMidiTrack(const juce::String& name = "");
    void removeTrack(int index);
    void insertTrack(int index, const TrackState& track);
    void moveTrack(int fromIndex, int toIndex);
    TrackState* getTrack(int index);
    const TrackState* getTrack(int index) const;
    TrackState* findTrackById(const juce::String& id);

    // --- Master Bus ---
    float getMasterGainDb() const { return masterGainDb_; }
    void setMasterGainDb(float db) { masterGainDb_ = db; }
    float getMasterPan() const { return masterPan_; }
    void setMasterPan(float pan) { masterPan_ = pan; }

    // --- Audio Assets ---
    AudioAssetPtr loadAudioAsset(const juce::File& file, juce::AudioFormatManager& formatManager);
    AudioAssetPtr getAudioAsset(const juce::String& filePath) const;

    // --- Clip Operations ---
    bool addClipToTrack(int trackIndex, const Clip& clip);
    bool removeClipFromTrack(int trackIndex, const juce::String& clipId);
    Clip* findClip(const juce::String& clipId);

    // --- Snapshot for undo ---
    struct Snapshot
    {
        std::vector<TrackState> tracks;
        double bpm;
        int timeSigNumerator;
        int timeSigDenominator;
        LoopRegion loopRegion;
        float masterGainDb;
        float masterPan;
    };

    Snapshot takeSnapshot() const;
    void restoreSnapshot(const Snapshot& snapshot);

private:
    double bpm_{120.0};
    int timeSigNumerator_{4};
    int timeSigDenominator_{4};
    double sampleRate_{44100.0};
    float masterGainDb_{0.0f};
    float masterPan_{0.0f};
    LoopRegion loopRegion_;

    std::vector<TrackState> tracks_;
    std::unordered_map<std::string, AudioAssetPtr> assetCache_;

    int nextTrackNumber_{1};
};

} // namespace neurato
