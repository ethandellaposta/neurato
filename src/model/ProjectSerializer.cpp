#include "model/ProjectSerializer.hpp"

namespace ampl
{

bool ProjectSerializer::save(const Session &session, const juce::File &projectFile)
{
    auto projectDir = projectFile.getParentDirectory();
    auto json = sessionToJson(session, projectDir);

    auto jsonString = juce::JSON::toString(json, true);
    return projectFile.replaceWithText(jsonString);
}

bool ProjectSerializer::load(Session &session, const juce::File &projectFile,
                             juce::AudioFormatManager &formatManager)
{
    if (!projectFile.existsAsFile())
        return false;

    auto jsonString = projectFile.loadFileAsString();
    auto json = juce::JSON::parse(jsonString);

    if (!json.isObject())
        return false;

    auto projectDir = projectFile.getParentDirectory();
    return jsonToSession(json, session, projectDir, formatManager);
}

juce::var ProjectSerializer::sessionToJson(const Session &session, const juce::File &projectDir)
{
    auto *obj = new juce::DynamicObject();

    obj->setProperty("formatVersion", kFormatVersion);
    obj->setProperty("appVersion", "0.1.0");
    obj->setProperty("bpm", session.getBpm());
    obj->setProperty("timeSigNumerator", session.getTimeSigNumerator());
    obj->setProperty("timeSigDenominator", session.getTimeSigDenominator());
    obj->setProperty("sampleRate", session.getSampleRate());

    // Loop region
    auto *loopObj = new juce::DynamicObject();
    const auto &loop = session.getLoopRegion();
    loopObj->setProperty("enabled", loop.enabled);
    loopObj->setProperty("startSample", static_cast<int64_t>(loop.startSample));
    loopObj->setProperty("endSample", static_cast<int64_t>(loop.endSample));
    obj->setProperty("loopRegion", juce::var(loopObj));

    // Master bus
    obj->setProperty("masterGainDb", static_cast<double>(session.getMasterGainDb()));
    obj->setProperty("masterPan", static_cast<double>(session.getMasterPan()));

    // Tracks
    juce::Array<juce::var> tracksArray;
    for (const auto &track : session.getTracks())
        tracksArray.add(trackToJson(track, projectDir));
    obj->setProperty("tracks", tracksArray);

    return juce::var(obj);
}

bool ProjectSerializer::jsonToSession(const juce::var &json, Session &session,
                                      const juce::File &projectDir,
                                      juce::AudioFormatManager &formatManager)
{
    int version = json.getProperty("formatVersion", 0);
    if (version < 1 || version > kFormatVersion)
        return false;
    // Version 1 files have no track type — all tracks are Audio (backward compat)

    session = Session(); // Reset

    session.setBpm(json.getProperty("bpm", 120.0));
    session.setTimeSignature(json.getProperty("timeSigNumerator", 4),
                             json.getProperty("timeSigDenominator", 4));
    session.setSampleRate(json.getProperty("sampleRate", 44100.0));

    // Master bus
    session.setMasterGainDb(static_cast<float>((double)json.getProperty("masterGainDb", 0.0)));
    session.setMasterPan(static_cast<float>((double)json.getProperty("masterPan", 0.0)));

    // Loop region
    auto loopVar = json.getProperty("loopRegion", juce::var());
    if (loopVar.isObject())
    {
        bool enabled = loopVar.getProperty("enabled", false);
        SampleCount start = static_cast<SampleCount>((int64_t)loopVar.getProperty("startSample", 0));
        SampleCount end = static_cast<SampleCount>((int64_t)loopVar.getProperty("endSample", 0));
        session.setLoopRegion(start, end, enabled);
    }

    // Tracks
    auto tracksVar = json.getProperty("tracks", juce::var());
    if (tracksVar.isArray())
    {
        for (int i = 0; i < tracksVar.size(); ++i)
        {
            auto trackVar = tracksVar[i];
            if (!trackVar.isObject())
                continue;

            juce::String name = trackVar.getProperty("name", "Track");
            juce::String typeStr = trackVar.getProperty("type", "audio").toString();
            TrackType trackType = (typeStr == "midi") ? TrackType::Midi : TrackType::Audio;

            int trackIndex = session.addTrack(name, trackType);
            auto *track = session.getTrack(trackIndex);
            if (track == nullptr)
                continue;

            track->id = trackVar.getProperty("id", track->id).toString();
            track->gainDb = static_cast<float>((double)trackVar.getProperty("gainDb", 0.0));
            track->pan = static_cast<float>((double)trackVar.getProperty("pan", 0.0));
            track->muted = trackVar.getProperty("muted", false);
            track->solo = trackVar.getProperty("solo", false);

            // Audio clips
            auto clipsVar = trackVar.getProperty("clips", juce::var());
            if (clipsVar.isArray())
            {
                for (int j = 0; j < clipsVar.size(); ++j)
                {
                    auto clipVar = clipsVar[j];
                    if (!clipVar.isObject())
                        continue;

                    juce::String assetPath = clipVar.getProperty("assetPath", "").toString();
                    juce::String assetData = clipVar.getProperty("assetData", "").toString();
                    juce::String assetFileName = clipVar.getProperty("assetFileName", "").toString();

                    AudioAssetPtr asset;

                    // Prefer embedded audio data when present
                    if (assetData.isNotEmpty())
                    {
                        juce::MemoryBlock data;
                        if (data.fromBase64Encoding(assetData))
                        {
                            auto nameHint = assetFileName.isNotEmpty() ? assetFileName : assetPath;
                            asset = session.loadAudioAssetFromMemory(data.getData(), data.getSize(),
                                                                     nameHint, formatManager);
                        }
                    }

                    // Fallback to disk-based asset if embedding is missing
                    if (!asset && assetPath.isNotEmpty())
                    {
                        auto audioFile = resolveRelativePath(assetPath, projectDir);
                        asset = session.loadAudioAsset(audioFile, formatManager);
                    }

                    if (!asset)
                        continue;

                    Clip clip;
                    clip.id = clipVar.getProperty("id", juce::Uuid().toString()).toString();
                    clip.asset = asset;
                    clip.timelineStartSample = static_cast<SampleCount>(
                        (int64_t)clipVar.getProperty("timelineStartSample", 0));
                    clip.sourceStartSample = static_cast<SampleCount>(
                        (int64_t)clipVar.getProperty("sourceStartSample", 0));
                    clip.sourceLengthSamples = static_cast<SampleCount>(
                        (int64_t)clipVar.getProperty("sourceLengthSamples", asset->lengthInSamples));
                    clip.gainDb = static_cast<float>((double)clipVar.getProperty("gainDb", 0.0));
                    clip.fadeInSamples = static_cast<SampleCount>(
                        (int64_t)clipVar.getProperty("fadeInSamples", 0));
                    clip.fadeOutSamples = static_cast<SampleCount>(
                        (int64_t)clipVar.getProperty("fadeOutSamples", 0));

                    session.addClipToTrack(trackIndex, clip);
                }
            }

            // MIDI clips
            auto midiClipsVar = trackVar.getProperty("midiClips", juce::var());
            if (midiClipsVar.isArray())
            {
                for (int j = 0; j < midiClipsVar.size(); ++j)
                {
                    auto mcVar = midiClipsVar[j];
                    if (!mcVar.isObject())
                        continue;

                    MidiClip mc;
                    mc.id = mcVar.getProperty("id", juce::Uuid().toString()).toString();
                    mc.name = mcVar.getProperty("name", "MIDI").toString();
                    mc.timelineStartSample = static_cast<SampleCount>(
                        (int64_t)mcVar.getProperty("timelineStartSample", 0));
                    mc.lengthSamples = static_cast<SampleCount>(
                        (int64_t)mcVar.getProperty("lengthSamples", 0));

                    auto notesVar = mcVar.getProperty("notes", juce::var());
                    if (notesVar.isArray())
                    {
                        for (int k = 0; k < notesVar.size(); ++k)
                        {
                            auto nVar = notesVar[k];
                            if (!nVar.isObject())
                                continue;

                            MidiNote note;
                            note.id = nVar.getProperty("id", juce::Uuid().toString()).toString();
                            note.noteNumber = nVar.getProperty("noteNumber", 60);
                            note.velocity = static_cast<float>((double)nVar.getProperty("velocity", 0.8));
                            note.startSample = static_cast<SampleCount>(
                                (int64_t)nVar.getProperty("startSample", 0));
                            note.lengthSamples = static_cast<SampleCount>(
                                (int64_t)nVar.getProperty("lengthSamples", 0));
                            mc.notes.push_back(note);
                        }
                    }

                    track->midiClips.push_back(std::move(mc));
                }
            }

            // Plugin chain
            auto pluginChainVar = trackVar.getProperty("pluginChain", juce::var());
            if (pluginChainVar.isArray())
            {
                for (int j = 0; j < pluginChainVar.size(); ++j)
                {
                    auto slotVar = pluginChainVar[j];
                    if (slotVar.isObject())
                        track->pluginChain.push_back(pluginSlotFromJson(slotVar));
                }
            }

            // Instrument plugin (for MIDI tracks)
            auto instrumentVar = trackVar.getProperty("instrumentPlugin", juce::var());
            if (instrumentVar.isObject())
                track->instrumentPlugin = pluginSlotFromJson(instrumentVar);
        }
    }

    return true;
}

juce::var ProjectSerializer::trackToJson(const TrackState &track, const juce::File &projectDir)
{
    auto *obj = new juce::DynamicObject();

    obj->setProperty("id", track.id);
    obj->setProperty("name", track.name);
    obj->setProperty("type", track.isMidi() ? "midi" : "audio");
    obj->setProperty("gainDb", static_cast<double>(track.gainDb));
    obj->setProperty("pan", static_cast<double>(track.pan));
    obj->setProperty("muted", track.muted);
    obj->setProperty("solo", track.solo);

    juce::Array<juce::var> clipsArray;
    for (const auto &clip : track.clips)
        clipsArray.add(clipToJson(clip, projectDir));
    obj->setProperty("clips", clipsArray);

    juce::Array<juce::var> midiClipsArray;
    for (const auto &mc : track.midiClips)
        midiClipsArray.add(midiClipToJson(mc));
    obj->setProperty("midiClips", midiClipsArray);

    // Plugin chain serialization
    juce::Array<juce::var> pluginChainArray;
    for (const auto &slot : track.pluginChain)
        pluginChainArray.add(pluginSlotToJson(slot));
    obj->setProperty("pluginChain", pluginChainArray);

    // Instrument plugin (for MIDI tracks)
    if (track.instrumentPlugin.has_value())
        obj->setProperty("instrumentPlugin", pluginSlotToJson(track.instrumentPlugin.value()));

    return juce::var(obj);
}

juce::var ProjectSerializer::clipToJson(const Clip &clip, const juce::File &projectDir)
{
    auto *obj = new juce::DynamicObject();

    obj->setProperty("id", clip.id);
    obj->setProperty("timelineStartSample", static_cast<int64_t>(clip.timelineStartSample));
    obj->setProperty("sourceStartSample", static_cast<int64_t>(clip.sourceStartSample));
    obj->setProperty("sourceLengthSamples", static_cast<int64_t>(clip.sourceLengthSamples));
    obj->setProperty("gainDb", static_cast<double>(clip.gainDb));
    obj->setProperty("fadeInSamples", static_cast<int64_t>(clip.fadeInSamples));
    obj->setProperty("fadeOutSamples", static_cast<int64_t>(clip.fadeOutSamples));

    if (clip.asset)
    {
        const juce::File sourceFile(clip.asset->filePath);

        obj->setProperty("assetFileName",
                         sourceFile.getFileName().isNotEmpty() ? sourceFile.getFileName()
                                                               : clip.asset->fileName);

        if (sourceFile.existsAsFile())
        {
            obj->setProperty("assetPath", makeRelativePath(sourceFile, projectDir));

            if (auto stream = sourceFile.createInputStream())
            {
                juce::MemoryBlock data;
                if (stream->readIntoMemoryBlock(data))
                    obj->setProperty("assetData", data.toBase64Encoding());
            }
        }
    }

    return juce::var(obj);
}

juce::var ProjectSerializer::midiClipToJson(const MidiClip &clip)
{
    auto *obj = new juce::DynamicObject();
    obj->setProperty("id", clip.id);
    obj->setProperty("name", clip.name);
    obj->setProperty("timelineStartSample", static_cast<int64_t>(clip.timelineStartSample));
    obj->setProperty("lengthSamples", static_cast<int64_t>(clip.lengthSamples));

    juce::Array<juce::var> notesArray;
    for (const auto &note : clip.notes)
        notesArray.add(midiNoteToJson(note));
    obj->setProperty("notes", notesArray);

    return juce::var(obj);
}

juce::var ProjectSerializer::midiNoteToJson(const MidiNote &note)
{
    auto *obj = new juce::DynamicObject();
    obj->setProperty("id", note.id);
    obj->setProperty("noteNumber", note.noteNumber);
    obj->setProperty("velocity", static_cast<double>(note.velocity));
    obj->setProperty("startSample", static_cast<int64_t>(note.startSample));
    obj->setProperty("lengthSamples", static_cast<int64_t>(note.lengthSamples));
    return juce::var(obj);
}

juce::var ProjectSerializer::pluginSlotToJson(const PluginSlot &slot)
{
    auto *obj = new juce::DynamicObject();
    obj->setProperty("pluginId", slot.pluginId);
    obj->setProperty("pluginName", slot.pluginName);
    obj->setProperty("pluginFormat", slot.pluginFormat);
    obj->setProperty("pluginPath", slot.pluginPath);
    obj->setProperty("bypassed", slot.bypassed);
    obj->setProperty("isResolved", slot.isResolved);
    obj->setProperty("originalIdentifier", slot.originalIdentifier);

    // Encode state data as base64
    if (!slot.stateData.empty())
    {
        juce::MemoryBlock mb(slot.stateData.data(), slot.stateData.size());
        obj->setProperty("stateData", mb.toBase64Encoding());
    }

    // Parameter values
    if (!slot.parameterValues.empty())
    {
        auto *paramsObj = new juce::DynamicObject();
        for (const auto &[key, value] : slot.parameterValues)
            paramsObj->setProperty(key, static_cast<double>(value));
        obj->setProperty("parameterValues", juce::var(paramsObj));
    }

    return juce::var(obj);
}

PluginSlot ProjectSerializer::pluginSlotFromJson(const juce::var &json)
{
    PluginSlot slot;

    if (!json.isObject())
        return slot;

    slot.pluginId = json.getProperty("pluginId", "").toString();
    slot.pluginName = json.getProperty("pluginName", "").toString();
    slot.pluginFormat = json.getProperty("pluginFormat", "").toString();
    slot.pluginPath = json.getProperty("pluginPath", "").toString();
    slot.bypassed = json.getProperty("bypassed", false);
    slot.isResolved = json.getProperty("isResolved", false);
    slot.originalIdentifier = json.getProperty("originalIdentifier", "").toString();

    // Decode state data from base64
    juce::String stateDataStr = json.getProperty("stateData", "").toString();
    if (stateDataStr.isNotEmpty())
    {
        juce::MemoryBlock mb;
        if (mb.fromBase64Encoding(stateDataStr))
        {
            const uint8_t *data = static_cast<const uint8_t *>(mb.getData());
            slot.stateData.assign(data, data + mb.getSize());
        }
    }

    // Parameter values
    auto paramsVar = json.getProperty("parameterValues", juce::var());
    if (paramsVar.isObject())
    {
        if (auto *paramsObj = paramsVar.getDynamicObject())
        {
            for (const auto &prop : paramsObj->getProperties())
                slot.parameterValues[prop.name.toString()] = static_cast<float>((double)prop.value);
        }
    }

    return slot;
}

juce::String ProjectSerializer::makeRelativePath(const juce::File &file, const juce::File &projectDir)
{
    return file.getRelativePathFrom(projectDir);
}

juce::File ProjectSerializer::resolveRelativePath(const juce::String &relativePath,
                                                  const juce::File &projectDir)
{
    return projectDir.getChildFile(relativePath);
}

} // namespace ampl
