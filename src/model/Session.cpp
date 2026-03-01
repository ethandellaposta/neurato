#include "model/Session.hpp"

#include <string_view>

namespace ampl
{

Session::Session() = default;

void Session::setLoopRegion(SampleCount start, SampleCount end, bool enabled)
{
    loopRegion_.startSample = start;
    loopRegion_.endSample = end;
    loopRegion_.enabled = enabled;
}

int Session::addTrack(const juce::String &name, TrackType type)
{
    TrackState track;
    track.id = juce::Uuid().toString();
    track.type = type;
    if (name.isEmpty())
    {
        juce::String prefix = (type == TrackType::Midi) ? "MIDI " : "Track ";
        track.name = prefix + juce::String(nextTrackNumber_++);
    }
    else
    {
        track.name = name;
    }
    tracks_.push_back(std::move(track));
    return static_cast<int>(tracks_.size()) - 1;
}

int Session::addMidiTrack(const juce::String &name)
{
    return addTrack(name, TrackType::Midi);
}

void Session::removeTrack(int index)
{
    if (index >= 0 && index < static_cast<int>(tracks_.size()))
        tracks_.erase(tracks_.begin() + index);
}

void Session::insertTrack(int index, const TrackState &track)
{
    if (index < 0)
        index = 0;
    if (index > static_cast<int>(tracks_.size()))
        index = static_cast<int>(tracks_.size());
    tracks_.insert(tracks_.begin() + index, track);
}

void Session::moveTrack(int fromIndex, int toIndex)
{
    int count = static_cast<int>(tracks_.size());
    if (fromIndex < 0 || fromIndex >= count || toIndex < 0 || toIndex >= count)
        return;
    if (fromIndex == toIndex)
        return;

    auto track = std::move(tracks_[static_cast<size_t>(fromIndex)]);
    tracks_.erase(tracks_.begin() + fromIndex);
    tracks_.insert(tracks_.begin() + toIndex, std::move(track));
}

TrackState *Session::getTrack(int index)
{
    if (index >= 0 && index < static_cast<int>(tracks_.size()))
        return &tracks_[static_cast<size_t>(index)];
    return nullptr;
}

const TrackState *Session::getTrack(int index) const
{
    if (index >= 0 && index < static_cast<int>(tracks_.size()))
        return &tracks_[static_cast<size_t>(index)];
    return nullptr;
}

TrackState *Session::findTrackById(const juce::String &id)
{
    for (auto &t : tracks_)
        if (t.id == id)
            return &t;
    return nullptr;
}

AudioAssetPtr Session::loadAudioAsset(const juce::File &file,
                                      juce::AudioFormatManager &formatManager)
{
    auto key = file.getFullPathName().toStdString();

    // Check cache first
    auto it = assetCache_.find(key);
    if (it != assetCache_.end())
        return it->second;

    // Load from disk
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr)
        return nullptr;

    auto asset = std::make_shared<AudioAsset>();
    auto *mutableAsset = const_cast<AudioAsset *>(asset.get());
    mutableAsset->filePath = file.getFullPathName();
    mutableAsset->fileName = file.getFileName();
    mutableAsset->lengthInSamples = static_cast<SampleCount>(reader->lengthInSamples);
    mutableAsset->sampleRate = reader->sampleRate;
    mutableAsset->numChannels = static_cast<int>(reader->numChannels);

    // Read entire file into memory
    mutableAsset->channels.resize(static_cast<size_t>(mutableAsset->numChannels));
    for (auto &ch : mutableAsset->channels)
        ch.resize(static_cast<size_t>(mutableAsset->lengthInSamples), 0.0f);

    juce::AudioBuffer<float> tempBuffer(mutableAsset->numChannels,
                                        static_cast<int>(mutableAsset->lengthInSamples));
    reader->read(&tempBuffer, 0, static_cast<int>(mutableAsset->lengthInSamples), 0, true, true);

    for (int ch = 0; ch < mutableAsset->numChannels; ++ch)
    {
        const float *src = tempBuffer.getReadPointer(ch);
        std::copy(src, src + mutableAsset->lengthInSamples,
                  mutableAsset->channels[static_cast<size_t>(ch)].data());
    }

    assetCache_[key] = asset;
    return asset;
}

AudioAssetPtr Session::loadAudioAssetFromMemory(const void *data, size_t size,
                                                const juce::String &nameHint,
                                                juce::AudioFormatManager &formatManager)
{
    if (data == nullptr || size == 0)
        return nullptr;

    const auto hash =
        std::hash<std::string_view>{}(std::string_view(reinterpret_cast<const char *>(data), size));
    auto keyString = juce::String(static_cast<int64_t>(hash));
    auto key = ("embedded:" + keyString).toStdString();

    auto it = assetCache_.find(key);
    if (it != assetCache_.end())
        return it->second;

    std::unique_ptr<juce::InputStream> stream =
        std::make_unique<juce::MemoryInputStream>(data, size, false);
    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(std::move(stream)));
    if (reader == nullptr)
        return nullptr;

    auto asset = std::make_shared<AudioAsset>();
    auto *mutableAsset = const_cast<AudioAsset *>(asset.get());
    mutableAsset->filePath = "embedded:" + keyString;
    mutableAsset->fileName = nameHint.isNotEmpty() ? nameHint : mutableAsset->filePath;
    mutableAsset->lengthInSamples = static_cast<SampleCount>(reader->lengthInSamples);
    mutableAsset->sampleRate = reader->sampleRate;
    mutableAsset->numChannels = static_cast<int>(reader->numChannels);

    mutableAsset->channels.resize(static_cast<size_t>(mutableAsset->numChannels));
    for (auto &ch : mutableAsset->channels)
        ch.resize(static_cast<size_t>(mutableAsset->lengthInSamples), 0.0f);

    juce::AudioBuffer<float> tempBuffer(mutableAsset->numChannels,
                                        static_cast<int>(mutableAsset->lengthInSamples));
    reader->read(&tempBuffer, 0, static_cast<int>(mutableAsset->lengthInSamples), 0, true, true);

    for (int ch = 0; ch < mutableAsset->numChannels; ++ch)
    {
        const float *src = tempBuffer.getReadPointer(ch);
        std::copy(src, src + mutableAsset->lengthInSamples,
                  mutableAsset->channels[static_cast<size_t>(ch)].data());
    }

    assetCache_[key] = asset;
    return asset;
}

AudioAssetPtr Session::getAudioAsset(const juce::String &filePath) const
{
    auto it = assetCache_.find(filePath.toStdString());
    if (it != assetCache_.end())
        return it->second;
    return nullptr;
}

bool Session::addClipToTrack(int trackIndex, const Clip &clip)
{
    auto *track = getTrack(trackIndex);
    if (track == nullptr)
        return false;
    track->clips.push_back(clip);
    return true;
}

bool Session::removeClipFromTrack(int trackIndex, const juce::String &clipId)
{
    auto *track = getTrack(trackIndex);
    if (track == nullptr)
        return false;

    auto &clips = track->clips;
    auto it = std::remove_if(clips.begin(), clips.end(),
                             [&clipId](const Clip &c) { return c.id == clipId; });
    if (it == clips.end())
        return false;

    clips.erase(it, clips.end());
    return true;
}

Clip *Session::findClip(const juce::String &clipId)
{
    for (auto &track : tracks_)
        for (auto &clip : track.clips)
            if (clip.id == clipId)
                return &clip;
    return nullptr;
}

Session::Snapshot Session::takeSnapshot() const
{
    Snapshot snap;
    snap.tracks.reserve(tracks_.size());
    for (const auto &t : tracks_)
        snap.tracks.push_back(t.clone());
    snap.bpm = bpm_;
    snap.timeSigNumerator = timeSigNumerator_;
    snap.timeSigDenominator = timeSigDenominator_;
    snap.loopRegion = loopRegion_;
    snap.masterGainDb = masterGainDb_;
    snap.masterPan = masterPan_;
    return snap;
}

void Session::restoreSnapshot(const Snapshot &snapshot)
{
    tracks_ = snapshot.tracks;
    bpm_ = snapshot.bpm;
    timeSigNumerator_ = snapshot.timeSigNumerator;
    timeSigDenominator_ = snapshot.timeSigDenominator;
    loopRegion_ = snapshot.loopRegion;
    masterGainDb_ = snapshot.masterGainDb;
    masterPan_ = snapshot.masterPan;
}

} // namespace ampl
