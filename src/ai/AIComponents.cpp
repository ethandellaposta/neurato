#include "AIComponents.hpp"
#include "engine/graph/AudioGraph.hpp"
#include "model/Session.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <juce_audio_basics/juce_audio_basics.h>
#include <sstream>

namespace ampl
{

// SessionStateAPI implementation
SessionStateAPI::SessionStateAPI() {}

void SessionStateAPI::setSession(std::shared_ptr<Session> session)
{
    session_ = session;

    if (stateChangeCallback_)
    {
        auto snapshot = generateSnapshot();
        stateChangeCallback_(snapshot);
    }
}

SessionSnapshot SessionStateAPI::generateSnapshot() const
{
    SessionSnapshot snapshot;

    if (!session_)
        return snapshot;

    // Basic session info
    snapshot.sessionId = "session_001"; // Generate or get from session
    snapshot.name = "Ampl Session";     // Use session name if available
    snapshot.bpm = session_->getBpm();
    snapshot.timeSignatureNumerator = session_->getTimeSigNumerator();
    snapshot.timeSignatureDenominator = session_->getTimeSigDenominator();
    snapshot.length = 0; // Calculate from clips or loop region
    snapshot.sampleRate = session_->getSampleRate();

    // Track information
    for (const auto &track : session_->getTracks())
    {
        auto trackInfo = createTrackInfo(track);
        snapshot.tracks.push_back(trackInfo);

        // Clip information for this track
        for (const auto &clip : track.clips)
        {
            auto clipInfo = createClipInfo(clip);
            clipInfo.trackId = track.id.toStdString();
            snapshot.clips.push_back(clipInfo);
        }
    }

    return snapshot;
}

SessionSnapshot
SessionStateAPI::generatePartialSnapshot(const std::vector<std::string> &trackIds) const
{
    SessionSnapshot snapshot = generateSnapshot();

    // Filter tracks and related data
    snapshot.tracks.erase(std::remove_if(snapshot.tracks.begin(), snapshot.tracks.end(),
                                         [&trackIds](const SessionSnapshot::TrackInfo &track)
                                         {
                                             return std::find(trackIds.begin(), trackIds.end(),
                                                              track.id) == trackIds.end();
                                         }),
                          snapshot.tracks.end());

    // Filter clips
    snapshot.clips.erase(std::remove_if(snapshot.clips.begin(), snapshot.clips.end(),
                                        [&trackIds](const SessionSnapshot::ClipInfo &clip)
                                        {
                                            return std::find(trackIds.begin(), trackIds.end(),
                                                             clip.trackId) == trackIds.end();
                                        }),
                         snapshot.clips.end());

    // Filter automation
    snapshot.automation.erase(
        std::remove_if(snapshot.automation.begin(), snapshot.automation.end(),
                       [&trackIds](const SessionSnapshot::AutomationInfo &autoInfo)
                       {
                           return std::find(trackIds.begin(), trackIds.end(), autoInfo.trackId) ==
                                  trackIds.end();
                       }),
        snapshot.automation.end());

    return snapshot;
}

SessionStateAPI::StateDiff SessionStateAPI::compareSnapshots(const SessionSnapshot &before,
                                                             const SessionSnapshot &after) const
{
    StateDiff diff;

    // Compare tracks
    for (const auto &track : after.tracks)
    {
        auto it = std::find_if(before.tracks.begin(), before.tracks.end(),
                               [&track](const SessionSnapshot::TrackInfo &t)
                               { return t.id == track.id; });

        if (it == before.tracks.end())
        {
            diff.addedTracks.push_back(track.id);
        }
        else if (it->name != track.name || it->gain != track.gain || it->pan != track.pan ||
                 it->muted != track.muted || it->soloed != track.soloed ||
                 it->numClips != track.numClips)
        {
            diff.modifiedTracks.push_back(track.id);
        }
    }

    for (const auto &track : before.tracks)
    {
        auto it = std::find_if(after.tracks.begin(), after.tracks.end(),
                               [&track](const SessionSnapshot::TrackInfo &t)
                               { return t.id == track.id; });

        if (it == after.tracks.end())
        {
            diff.removedTracks.push_back(track.id);
        }
    }

    // Compare clips
    for (const auto &clip : after.clips)
    {
        auto it =
            std::find_if(before.clips.begin(), before.clips.end(),
                         [&clip](const SessionSnapshot::ClipInfo &c) { return c.id == clip.id; });

        if (it == before.clips.end())
        {
            diff.addedClips.push_back(clip.id);
        }
        else if (it->trackId != clip.trackId || it->start != clip.start ||
                 it->length != clip.length || it->gain != clip.gain)
        {
            diff.modifiedClips.push_back(clip.id);
        }
    }

    for (const auto &clip : before.clips)
    {
        auto it =
            std::find_if(after.clips.begin(), after.clips.end(),
                         [&clip](const SessionSnapshot::ClipInfo &c) { return c.id == clip.id; });

        if (it == after.clips.end())
        {
            diff.removedClips.push_back(clip.id);
        }
    }

    return diff;
}

void SessionStateAPI::analyzeAudio(SessionSnapshot &snapshot, const AnalysisOptions &options) const
{
    for (auto &track : snapshot.tracks)
    {
        SessionSnapshot::AudioAnalysis analysis;

        // This would analyze the actual audio data from the clips
        // For now, we'll provide placeholder values

        if (options.analyzeRMS)
        {
            analysis.rmsLevels = {0.1f, 0.15f, 0.12f, 0.08f}; // Placeholder
        }

        if (options.analyzePeaks)
        {
            analysis.peakLevels = {0.8f, 0.9f, 0.7f, 0.6f}; // Placeholder
        }

        if (options.analyzeTransients)
        {
            analysis.transients = {44100, 88200, 132300}; // Placeholder positions
        }

        if (options.analyzeBeats)
        {
            analysis.beatGrid = {0, 22050, 44100, 66150, 88200}; // Placeholder beat positions
        }

        if (options.analyzeSpectrum)
        {
            analysis.spectralCentroid = 2000.0f; // Placeholder
            analysis.spectralRolloff = 4000.0f;  // Placeholder
            analysis.zeroCrossingRate = 0.1f;    // Placeholder
        }

        snapshot.audioAnalysis[track.id] = analysis;
    }
}

void SessionStateAPI::analyzeMix(SessionSnapshot &snapshot) const
{
    auto &mixAnalysis = snapshot.mixAnalysis;

    std::vector<float> trackLevels;
    std::vector<std::string> loudTracks;
    std::vector<std::string> quietTracks;

    float totalLevel = 0.0f;
    float maxLevel = 0.0f;

    for (const auto &track : snapshot.tracks)
    {
        float trackLevel = track.gain;

        // Get RMS level from audio analysis if available
        auto audioIt = snapshot.audioAnalysis.find(track.id);
        if (audioIt != snapshot.audioAnalysis.end() && !audioIt->second.rmsLevels.empty())
        {
            float avgRMS = std::accumulate(audioIt->second.rmsLevels.begin(),
                                           audioIt->second.rmsLevels.end(), 0.0f) /
                           audioIt->second.rmsLevels.size();
            trackLevel *= avgRMS;
        }

        trackLevels.push_back(trackLevel);
        totalLevel += trackLevel;
        maxLevel = std::max(maxLevel, trackLevel);

        // Categorize tracks
        if (trackLevel > 0.7f)
        {
            loudTracks.push_back(track.id);
        }
        else if (trackLevel < 0.3f)
        {
            quietTracks.push_back(track.id);
        }
    }

    mixAnalysis.trackLevels = trackLevels;
    mixAnalysis.loudTracks = loudTracks;
    mixAnalysis.quietTracks = quietTracks;
    mixAnalysis.averageTrackLevel = trackLevels.empty() ? 0.0f : totalLevel / trackLevels.size();
    mixAnalysis.peakTrackLevel = maxLevel;
    mixAnalysis.headroomDb = juce::Decibels::gainToDecibels(1.0f - maxLevel);
}

SessionStateAPI::ValidationResult
SessionStateAPI::validateSnapshot(const SessionSnapshot &snapshot) const
{
    ValidationResult result;

    // Validate basic session properties
    if (snapshot.bpm <= 0 || snapshot.bpm > 300)
    {
        result.errors.push_back("Invalid BPM value");
        result.isValid = false;
    }

    if (snapshot.timeSignatureNumerator <= 0 || snapshot.timeSignatureDenominator <= 0)
    {
        result.errors.push_back("Invalid time signature");
        result.isValid = false;
    }

    // Validate tracks
    std::set<std::string> trackIds;
    for (const auto &track : snapshot.tracks)
    {
        if (trackIds.find(track.id) != trackIds.end())
        {
            result.errors.push_back("Duplicate track ID: " + track.id);
            result.isValid = false;
        }
        trackIds.insert(track.id);

        if (track.gain < 0.0f)
        {
            result.errors.push_back("Negative gain on track: " + track.id);
            result.isValid = false;
        }

        if (track.pan < -1.0f || track.pan > 1.0f)
        {
            result.errors.push_back("Invalid pan value on track: " + track.id);
            result.isValid = false;
        }
    }

    // Validate clips
    std::set<std::string> clipIds;
    for (const auto &clip : snapshot.clips)
    {
        if (clipIds.find(clip.id) != clipIds.end())
        {
            result.errors.push_back("Duplicate clip ID: " + clip.id);
            result.isValid = false;
        }
        clipIds.insert(clip.id);

        if (trackIds.find(clip.trackId) == trackIds.end())
        {
            result.errors.push_back("Clip references non-existent track: " + clip.trackId);
            result.isValid = false;
        }

        if (clip.length <= 0)
        {
            result.errors.push_back("Invalid clip length: " + clip.id);
            result.isValid = false;
        }
    }

    // Warnings
    if (snapshot.tracks.empty())
    {
        result.warnings.push_back("No tracks in session");
    }

    if (snapshot.mixAnalysis.headroomDb < -3.0f)
    {
        result.warnings.push_back("Low headroom detected");
    }

    return result;
}

bool SessionStateAPI::applyAction(const ActionDSL::Action &action)
{
    if (!session_)
        return false;

    auto findTrackIndexById = [this](const std::string &trackId) -> int
    {
        const auto &tracks = session_->getTracks();
        for (int i = 0; i < static_cast<int>(tracks.size()); ++i)
        {
            if (tracks[static_cast<size_t>(i)].id.toStdString() == trackId)
            {
                return i;
            }
        }
        return -1;
    };

    auto boolParam = [&action](const std::string &key, bool fallback = false)
    {
        const auto it = action.params.find(key);
        if (it == action.params.end())
            return fallback;
        if (const auto *value = std::get_if<bool>(&it->second))
            return *value;
        return fallback;
    };

    auto floatParam = [&action](const std::string &key, float fallback = 0.0f)
    {
        const auto it = action.params.find(key);
        if (it == action.params.end())
            return fallback;
        if (const auto *value = std::get_if<float>(&it->second))
            return *value;
        return fallback;
    };

    auto stringParam =
        [&action](const std::string &key, const std::string &fallback = std::string{})
    {
        const auto it = action.params.find(key);
        if (it == action.params.end())
            return fallback;
        if (const auto *value = std::get_if<std::string>(&it->second))
            return *value;
        return fallback;
    };

    switch (action.type)
    {
    case ActionDSL::ActionType::CreateTrack:
    {
        const auto name = stringParam("name", "AI Track");
        const auto isMidi = boolParam("isMidi", false);
        session_->addTrack(name, isMidi ? TrackType::Midi : TrackType::Audio);
        break;
    }
    case ActionDSL::ActionType::DeleteTrack:
    {
        const auto trackId = stringParam("trackId");
        const auto trackIndex = findTrackIndexById(trackId);
        if (trackIndex < 0)
            return false;
        session_->removeTrack(trackIndex);
        break;
    }
    case ActionDSL::ActionType::RenameTrack:
    {
        const auto trackId = stringParam("trackId");
        const auto newName = stringParam("newName");
        const auto trackIndex = findTrackIndexById(trackId);
        if (trackIndex < 0)
            return false;
        if (auto *track = session_->getTrack(trackIndex))
        {
            track->name = newName;
        }
        break;
    }
    case ActionDSL::ActionType::SetTrackGain:
    {
        const auto trackId = stringParam("trackId");
        const auto trackIndex = findTrackIndexById(trackId);
        if (trackIndex < 0)
            return false;
        if (auto *track = session_->getTrack(trackIndex))
        {
            track->gainDb = floatParam("gain", track->gainDb);
        }
        break;
    }
    case ActionDSL::ActionType::SetTrackPan:
    {
        const auto trackId = stringParam("trackId");
        const auto trackIndex = findTrackIndexById(trackId);
        if (trackIndex < 0)
            return false;
        if (auto *track = session_->getTrack(trackIndex))
        {
            track->pan = floatParam("pan", track->pan);
        }
        break;
    }
    case ActionDSL::ActionType::SetTrackMute:
    {
        const auto trackId = stringParam("trackId");
        const auto trackIndex = findTrackIndexById(trackId);
        if (trackIndex < 0)
            return false;
        if (auto *track = session_->getTrack(trackIndex))
        {
            track->muted = boolParam("muted", track->muted);
        }
        break;
    }
    case ActionDSL::ActionType::SetTrackSolo:
    {
        const auto trackId = stringParam("trackId");
        const auto trackIndex = findTrackIndexById(trackId);
        if (trackIndex < 0)
            return false;
        if (auto *track = session_->getTrack(trackIndex))
        {
            track->solo = boolParam("soloed", track->solo);
        }
        break;
    }
    default:
        return false;
    }

    if (stateChangeCallback_)
    {
        auto snapshot = generateSnapshot();
        stateChangeCallback_(snapshot);
    }

    return true;
}

bool SessionStateAPI::applyActionSequence(const ActionDSL::ActionSequence &actions)
{
    bool allSucceeded = true;

    for (const auto &action : actions)
    {
        if (!applyAction(*action))
        {
            allSucceeded = false;
            break;
        }
    }

    return allSucceeded;
}

void SessionStateAPI::setStateChangeCallback(StateChangeCallback callback)
{
    stateChangeCallback_ = callback;
}

SessionSnapshot::TrackInfo SessionStateAPI::createTrackInfo(const TrackState &track) const
{
    SessionSnapshot::TrackInfo info;
    info.id = track.id.toStdString();
    info.name = track.name.toStdString();
    info.isMidi = track.isMidi();
    info.muted = track.muted;
    info.soloed = track.solo;
    info.gain = track.gainDb;
    info.pan = track.pan;
    info.numClips = track.clips.size();

    return info;
}

SessionSnapshot::ClipInfo SessionStateAPI::createClipInfo(const Clip &clip) const
{
    SessionSnapshot::ClipInfo info;
    info.id = clip.id.toStdString();
    info.start = clip.timelineStartSample;
    info.length = clip.sourceLengthSamples;
    info.type = "audio"; // All clips are audio for now
    info.gain = clip.gainDb;
    info.fadeIn = clip.fadeInSamples;
    info.fadeOut = clip.fadeOutSamples;

    return info;
}

SessionSnapshot::PluginInfo SessionStateAPI::createPluginInfo(const PluginInstance &plugin) const
{
    SessionSnapshot::PluginInfo info;
    const auto &pluginInfo = plugin.getPluginInfo();
    info.id = pluginInfo.id;
    info.name = pluginInfo.name;
    info.manufacturer = pluginInfo.manufacturer;
    info.type = pluginInfo.isInstrument ? "instrument" : "effect";
    info.bypassed = false;

    // Parameter extraction is intentionally omitted here to keep snapshot generation
    // independent of host plugin runtime state.

    return info;
}

SessionSnapshot::AutomationInfo
SessionStateAPI::createAutomationInfo(const AutomationLane &lane, const std::string &trackId,
                                      const std::string &paramId) const
{
    SessionSnapshot::AutomationInfo info;
    info.trackId = trackId;
    info.parameterId = paramId;
    info.parameterName = paramId; // Would be resolved to actual name

    for (const auto &point : lane.getPoints())
    {
        info.points.emplace_back(point.position, point.value);
    }

    return info;
}

std::vector<float> SessionStateAPI::calculateRMS(const float *audio, int numSamples) const
{
    std::vector<float> rmsLevels;

    const int windowSize = 1024;
    for (int i = 0; i < numSamples; i += windowSize)
    {
        int end = std::min(i + windowSize, numSamples);
        float sum = 0.0f;

        for (int j = i; j < end; ++j)
        {
            sum += audio[j] * audio[j];
        }

        float rms = std::sqrt(sum / (end - i));
        rmsLevels.push_back(rms);
    }

    return rmsLevels;
}

std::vector<float> SessionStateAPI::calculatePeaks(const float *audio, int numSamples) const
{
    std::vector<float> peakLevels;

    const int windowSize = 1024;
    for (int i = 0; i < numSamples; i += windowSize)
    {
        int end = std::min(i + windowSize, numSamples);
        float peak = 0.0f;

        for (int j = i; j < end; ++j)
        {
            peak = std::max(peak, std::abs(audio[j]));
        }

        peakLevels.push_back(peak);
    }

    return peakLevels;
}

std::vector<SampleCount> SessionStateAPI::detectTransients(const float *audio, int numSamples,
                                                           double sampleRate) const
{
    std::vector<SampleCount> transients;

    // Simple spectral flux-based transient detection
    const int windowSize = 512;
    const int hopSize = 256;

    std::vector<float> prevSpectrum(windowSize / 2);

    for (int i = 0; i < numSamples - windowSize; i += hopSize)
    {
        // Calculate spectrum
        std::vector<float> spectrum(windowSize / 2);

        // Apply FFT (simplified - would use actual FFT in production)
        for (int j = 0; j < windowSize / 2; ++j)
        {
            float real = audio[i + j];
            float imag = 0.0f;
            spectrum[j] = std::sqrt(real * real + imag * imag);
        }

        // Calculate spectral flux
        float flux = 0.0f;
        for (int j = 0; j < windowSize / 2; ++j)
        {
            float diff = spectrum[j] - prevSpectrum[j];
            if (diff > 0)
            {
                flux += diff;
            }
        }

        // Detect transient
        if (flux > 0.5f)
        { // Threshold
            transients.push_back(i);
        }

        prevSpectrum = spectrum;
    }

    return transients;
}

std::vector<SampleCount> SessionStateAPI::detectBeats(const float *audio, int numSamples,
                                                      double sampleRate, double bpm) const
{
    std::vector<SampleCount> beats;

    // Generate beat grid based on BPM
    double samplesPerBeat = (sampleRate * 60.0) / bpm;

    for (SampleCount i = 0; i < numSamples; i += static_cast<SampleCount>(samplesPerBeat))
    {
        beats.push_back(i);
    }

    return beats;
}

std::pair<float, float> SessionStateAPI::analyzeSpectrum(const float *audio, int numSamples,
                                                         int fftSize) const
{
    // Simplified spectral analysis
    float spectralCentroid = 1000.0f; // Placeholder
    float spectralRolloff = 4000.0f;  // Placeholder

    // In production, would use actual FFT and calculate these properly

    return {spectralCentroid, spectralRolloff};
}

// Action DSL implementation
namespace ActionDSL
{

std::unique_ptr<Action> createTrack(const std::string &name, bool isMidi)
{
    Parameters params;
    params["name"] = name;
    params["isMidi"] = isMidi;

    return std::make_unique<Action>(ActionType::CreateTrack, params, "Create track: " + name);
}

std::unique_ptr<Action> deleteTrack(const std::string &trackId)
{
    Parameters params;
    params["trackId"] = trackId;

    return std::make_unique<Action>(ActionType::DeleteTrack, params, "Delete track: " + trackId);
}

std::unique_ptr<Action> renameTrack(const std::string &trackId, const std::string &newName)
{
    Parameters params;
    params["trackId"] = trackId;
    params["newName"] = newName;

    return std::make_unique<Action>(ActionType::RenameTrack, params,
                                    "Rename track " + trackId + " to " + newName);
}

std::unique_ptr<Action> addClip(const std::string &trackId, SampleCount start, SampleCount length,
                                const std::string &filePath)
{
    Parameters params;
    params["trackId"] = trackId;
    params["start"] = start;
    params["length"] = length;
    if (!filePath.empty())
    {
        params["filePath"] = filePath;
    }

    return std::make_unique<Action>(ActionType::AddClip, params, "Add clip to track " + trackId);
}

std::unique_ptr<Action> removeClip(const std::string &clipId)
{
    Parameters params;
    params["clipId"] = clipId;

    return std::make_unique<Action>(ActionType::RemoveClip, params, "Remove clip: " + clipId);
}

std::unique_ptr<Action> moveClip(const std::string &clipId, SampleCount newStart)
{
    Parameters params;
    params["clipId"] = clipId;
    params["newStart"] = newStart;

    return std::make_unique<Action>(ActionType::MoveClip, params, "Move clip " + clipId);
}

std::unique_ptr<Action> resizeClip(const std::string &clipId, SampleCount newLength)
{
    Parameters params;
    params["clipId"] = clipId;
    params["newLength"] = newLength;

    return std::make_unique<Action>(ActionType::ResizeClip, params, "Resize clip " + clipId);
}

std::unique_ptr<Action> addPlugin(const std::string &trackId, const std::string &pluginId)
{
    Parameters params;
    params["trackId"] = trackId;
    params["pluginId"] = pluginId;

    return std::make_unique<Action>(ActionType::AddPlugin, params,
                                    "Add plugin to track " + trackId);
}

std::unique_ptr<Action> removePlugin(const std::string &trackId, int pluginIndex)
{
    Parameters params;
    params["trackId"] = trackId;
    params["pluginIndex"] = pluginIndex;

    return std::make_unique<Action>(ActionType::RemovePlugin, params,
                                    "Remove plugin from track " + trackId);
}

std::unique_ptr<Action> setPluginParameter(const std::string &trackId, int pluginIndex,
                                           const std::string &paramId, float value)
{
    Parameters params;
    params["trackId"] = trackId;
    params["pluginIndex"] = pluginIndex;
    params["paramId"] = paramId;
    params["value"] = value;

    return std::make_unique<Action>(ActionType::SetPluginParameter, params,
                                    "Set plugin parameter " + paramId);
}

std::unique_ptr<Action> setTrackGain(const std::string &trackId, float gain)
{
    Parameters params;
    params["trackId"] = trackId;
    params["gain"] = gain;

    return std::make_unique<Action>(ActionType::SetTrackGain, params,
                                    "Set track gain for " + trackId);
}

std::unique_ptr<Action> setTrackPan(const std::string &trackId, float pan)
{
    Parameters params;
    params["trackId"] = trackId;
    params["pan"] = pan;

    return std::make_unique<Action>(ActionType::SetTrackPan, params,
                                    "Set track pan for " + trackId);
}

std::unique_ptr<Action> setTrackMute(const std::string &trackId, bool muted)
{
    Parameters params;
    params["trackId"] = trackId;
    params["muted"] = muted;

    return std::make_unique<Action>(ActionType::SetTrackMute, params,
                                    muted ? ("Mute track " + trackId)
                                          : ("Unmute track " + trackId));
}

std::unique_ptr<Action> setTrackSolo(const std::string &trackId, bool soloed)
{
    Parameters params;
    params["trackId"] = trackId;
    params["soloed"] = soloed;

    return std::make_unique<Action>(ActionType::SetTrackSolo, params,
                                    soloed ? ("Solo track " + trackId)
                                           : ("Unsolo track " + trackId));
}

std::unique_ptr<Action> addAutomationPoint(const std::string &trackId, const std::string &paramId,
                                           SampleCount position, float value)
{
    Parameters params;
    params["trackId"] = trackId;
    params["paramId"] = paramId;
    params["position"] = position;
    params["value"] = value;

    return std::make_unique<Action>(ActionType::AddAutomationPoint, params,
                                    "Add automation point for " + paramId);
}

std::unique_ptr<Action> removeAutomationPoint(const std::string &trackId,
                                              const std::string &paramId, SampleCount position)
{
    Parameters params;
    params["trackId"] = trackId;
    params["paramId"] = paramId;
    params["position"] = position;

    return std::make_unique<Action>(ActionType::RemoveAutomationPoint, params,
                                    "Remove automation point for " + paramId);
}

std::unique_ptr<Action> createInverse(const Action &action, const SessionSnapshot &beforeState)
{
    // This would create the inverse action based on the before state
    // Implementation depends on the action type

    switch (action.type)
    {
    case ActionType::CreateTrack:
    {
        std::string trackId = "generated_id"; // Would be actual ID from before state
        return deleteTrack(trackId);
    }
    case ActionType::DeleteTrack:
    {
        // Find track info in before state and recreate it
        return nullptr; // Placeholder
    }
    case ActionType::SetTrackGain:
    {
        std::string trackId = std::get<std::string>(action.params.at("trackId"));
        float oldGain = 1.0f; // Would get from before state
        return setTrackGain(trackId, oldGain);
    }
        // ... other action types

    default:
        return nullptr;
    }
}

std::string serializeAction(const Action &action)
{
    std::ostringstream oss;
    oss << "Action{type=" << static_cast<int>(action.type);

    for (const auto &param : action.params)
    {
        oss << "," << param.first << "=";

        if (std::holds_alternative<bool>(param.second))
        {
            oss << std::get<bool>(param.second);
        }
        else if (std::holds_alternative<int>(param.second))
        {
            oss << std::get<int>(param.second);
        }
        else if (std::holds_alternative<float>(param.second))
        {
            oss << std::fixed << std::setprecision(6) << std::get<float>(param.second);
        }
        else if (std::holds_alternative<std::string>(param.second))
        {
            oss << "\"" << std::get<std::string>(param.second) << "\"";
        }
        else if (std::holds_alternative<SampleCount>(param.second))
        {
            oss << std::get<SampleCount>(param.second);
        }
        else if (std::holds_alternative<double>(param.second))
        {
            oss << std::fixed << std::setprecision(2) << std::get<double>(param.second);
        }
    }

    oss << "}";
    return oss.str();
}

std::unique_ptr<Action> deserializeAction(const std::string &serialized)
{
    // This would parse the serialized string back into an Action
    // Implementation would be more complex in production
    return nullptr;
}

} // namespace ActionDSL

} // namespace ampl
