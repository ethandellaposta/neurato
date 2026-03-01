#include "import/LogicImporter.hpp"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <optional>

namespace
{
bool isValidBpm(double bpm)
{
    return bpm >= 20.0 && bpm <= 400.0;
}

bool isTempoHint(const juce::String &text)
{
    auto s = text.toLowerCase();
    return s.contains("tempo") || s.contains("bpm");
}

std::optional<double> extractTempoCandidate(const juce::String &rawText)
{
    auto text = rawText.toLowerCase().replaceCharacter(',', '.').trim();
    if (text.isEmpty())
        return std::nullopt;

    // Fast path for simple numeric values (e.g. "128" / "128.5")
    double direct = text.getDoubleValue();
    if (isValidBpm(direct))
        return direct;

    // Tokenized fallback for strings like "tempo=128 bpm"
    juce::StringArray tokens;
    tokens.addTokens(text, " \t\n\r,;:|()[]{}<>/=\\\"'", "");
    for (auto token : tokens)
    {
        token = token.trim();
        if (token.isEmpty())
            continue;

        // Handle suffix forms such as "120bpm"
        if (token.endsWith("bpm"))
            token = token.dropLastCharacters(3).trim();

        double value = token.getDoubleValue();
        if (isValidBpm(value))
            return value;
    }

    return std::nullopt;
}

std::optional<double> parseAudioRateToSampleRate(const juce::String &audioRate)
{
    auto s = audioRate.toLowerCase().replaceCharacter(',', '.').trim();
    if (s.isEmpty())
        return std::nullopt;

    if (s.endsWith("khz"))
        s = s.dropLastCharacters(3).trim();
    else if (s.endsWith("k"))
        s = s.dropLastCharacters(1).trim();

    double value = s.getDoubleValue();
    if (value <= 0.0)
        return std::nullopt;

    // Values like 44.1/48 usually mean kHz
    if (value < 1000.0)
        value *= 1000.0;

    if (value >= 8000.0 && value <= 384000.0)
        return value;

    return std::nullopt;
}
} // namespace

namespace ampl
{

LogicImporter::LogicImporter() {}

LogicImportResult LogicImporter::importFromFCPXML(const juce::File &fcpxmlFile, Session &session,
                                                  juce::AudioFormatManager &formatManager)
{
    LogicImportResult result;

    if (!fcpxmlFile.existsAsFile())
    {
        result.errorMessage = "File does not exist: " + fcpxmlFile.getFullPathName();
        return result;
    }

    // Parse the FCPXML file
    auto projectData = parseFCPXML(fcpxmlFile);
    if (projectData.tracks.empty() && projectData.projectName.isEmpty())
    {
        result.errorMessage = "Failed to parse FCPXML file or file is empty";
        return result;
    }

    // Convert to Ampl session
    return convertToSession(projectData, session, formatManager, fcpxmlFile.getParentDirectory());
}

LogicImportResult LogicImporter::importFromLogicBundle(const juce::File &logicxBundle,
                                                       Session &session,
                                                       juce::AudioFormatManager &formatManager)
{
    LogicImportResult result;

    if (!logicxBundle.isDirectory())
    {
        result.errorMessage = "Not a valid Logic Pro bundle: " + logicxBundle.getFullPathName();
        return result;
    }

    // Logic bundles use a proprietary binary format
    // We can only extract audio/MIDI files, not arrangement data
    auto projectData = parseLogicBundle(logicxBundle);

    if (projectData.tracks.empty())
    {
        result.errorMessage = "Could not extract project data from Logic bundle.\n\n"
                              "Note: .logicx files use Apple's proprietary format.\n"
                              "For full arrangement import, export from Logic Pro as:\n"
                              "  File -> Export -> Final Cut Pro XML\n\n"
                              "This preserves track positions, clip timing, and regions.";
        return result;
    }

    // Add warning about limitations
    result.warnings.push_back("Logic bundles use a proprietary format. "
                              "Audio files were extracted but arrangement data is unavailable.\n"
                              "For proper timing, export as FCPXML from Logic Pro: "
                              "File -> Export -> Final Cut Pro XML");

    return convertToSession(projectData, session, formatManager, logicxBundle.getParentDirectory());
}

LogicProjectData LogicImporter::parseFCPXML(const juce::File &file)
{
    LogicProjectData project;

    auto xml = juce::XmlDocument::parse(file);
    if (!xml)
    {
        juce::Logger::writeToLog("Failed to parse XML: " + file.getFullPathName());
        return project;
    }

    // FCPXML root element
    if (xml->getTagName() != "fcpxml")
    {
        // Try alternate Logic XML format
        if (xml->getTagName() == "LogicProject" || xml->getTagName() == "project")
        {
            // Handle alternate format
            project.projectName =
                xml->getStringAttribute("name", file.getFileNameWithoutExtension());
        }
        else
        {
            juce::Logger::writeToLog("Not a valid FCPXML file");
            return project;
        }
    }

    // Parse resources (media files)
    if (auto *resources = xml->getChildByName("resources"))
    {
        for (auto *asset : resources->getChildWithTagNameIterator("asset"))
        {
            juce::String id = asset->getStringAttribute("id");
            juce::String src = asset->getStringAttribute("src");
            if (id.isNotEmpty() && src.isNotEmpty())
            {
                // Remove file:// prefix if present
                if (src.startsWith("file://"))
                    src = src.substring(7);
                src = juce::URL::removeEscapeChars(src);
                project.mediaFiles[id] = juce::File(src);
            }
        }

        // Also check for format elements with media references
        for (auto *format : resources->getChildWithTagNameIterator("format"))
        {
            // Sample-rate hint may appear on format entries
            auto sampleRateAttr = format->getStringAttribute("sampleRate");
            if (sampleRateAttr.isNotEmpty())
            {
                auto sr = parseAudioRateToSampleRate(sampleRateAttr);
                if (sr.has_value())
                    project.sampleRate = sr.value();
            }
        }

        // Logic Pro often exports sequence data under resources/media
        for (auto *media : resources->getChildWithTagNameIterator("media"))
        {
            if (project.projectName.isEmpty())
                project.projectName = media->getStringAttribute("name", project.projectName);

            if (auto *sequence = media->getChildByName("sequence"))
                parseSequence(sequence, project);
        }
    }

    // Parse library/event/project structure
    if (auto *library = xml->getChildByName("library"))
    {
        for (auto *event : library->getChildWithTagNameIterator("event"))
        {
            for (auto *proj : event->getChildWithTagNameIterator("project"))
            {
                project.projectName = proj->getStringAttribute("name", "Imported Project");

                if (auto *sequence = proj->getChildByName("sequence"))
                {
                    parseSequence(sequence, project);
                }
            }
        }
    }

    // Direct project element (simpler format)
    if (auto *proj = xml->getChildByName("project"))
    {
        project.projectName = proj->getStringAttribute("name", "Imported Project");
        if (auto *sequence = proj->getChildByName("sequence"))
        {
            parseSequence(sequence, project);
        }
    }

    if (project.projectName.isEmpty() && !project.tracks.empty())
        project.projectName = file.getFileNameWithoutExtension();

    return project;
}

void LogicImporter::parseSequence(juce::XmlElement *sequence, LogicProjectData &project)
{
    if (!sequence)
        return;

    auto applyBpm = [&](double bpm) -> bool
    {
        if (!isValidBpm(bpm))
            return false;
        project.bpm = bpm;
        project.bpmDetected = true;
        return true;
    };

    // Parse direct sequence attributes first
    for (const auto &attrName : {"tempo", "bpm", "audioBpm", "musicTempo", "projectTempo"})
    {
        auto attr = sequence->getStringAttribute(attrName);
        if (attr.isNotEmpty())
        {
            auto bpmCandidate = extractTempoCandidate(attr);
            if (bpmCandidate.has_value() && applyBpm(bpmCandidate.value()))
                break;
        }
    }

    // Parse sample-rate hint from sequence
    auto audioRateAttr = sequence->getStringAttribute("audioRate");
    if (audioRateAttr.isNotEmpty())
    {
        auto sr = parseAudioRateToSampleRate(audioRateAttr);
        if (sr.has_value())
            project.sampleRate = sr.value();
    }

    // Look for tempo in metadata
    if (auto *metadata = sequence->getChildByName("metadata"))
    {
        std::function<void(juce::XmlElement *)> parseTempoInTree;
        parseTempoInTree = [&](juce::XmlElement *element)
        {
            if (!element || project.bpmDetected)
                return;

            auto key = element->getStringAttribute("key");
            if (key.isEmpty())
                key = element->getStringAttribute("name");

            auto value = element->getStringAttribute("value");
            if (value.isEmpty())
                value = element->getStringAttribute("tempo");
            if (value.isEmpty())
                value = element->getAllSubText().trim();

            if (isTempoHint(key) || isTempoHint(value))
            {
                auto bpmCandidate = extractTempoCandidate(value);
                if (!bpmCandidate.has_value() && value != key)
                    bpmCandidate = extractTempoCandidate(key);

                if (bpmCandidate.has_value())
                    applyBpm(bpmCandidate.value());
            }

            for (auto *child : element->getChildIterator())
                parseTempoInTree(child);
        };

        parseTempoInTree(metadata);
    }

    // Parse spine (main timeline)
    if (auto *spine = sequence->getChildByName("spine"))
    {
        parseSpine(spine, project);
    }

    // Parse audio roles / tracks
    for (auto *role : sequence->getChildWithTagNameIterator("audio-role"))
    {
        LogicProjectData::TrackData track;
        track.name = role->getStringAttribute("name", "Track");
        track.type = "audio";
        parseAudioRole(role, track);
        project.tracks.push_back(track);
    }
}

void LogicImporter::parseSpine(juce::XmlElement *spine, LogicProjectData &project)
{
    if (!spine)
        return;

    // Spine contains clips sequentially - track current timeline position
    double currentTimelinePosition = 0.0;

    // Helper to parse FCPXML time values
    // Format: "numerator/denominatorFrameRates" or "seconds" or rational like "123/456s"
    auto parseTime = [](const juce::String &timeStr, double defaultFps = 24.0) -> double
    {
        if (timeStr.isEmpty())
            return -1.0; // Indicates "not set"

        // Handle rational seconds format: "123/456s"
        if (timeStr.endsWith("s"))
        {
            juce::String numPart = timeStr.dropLastCharacters(1);
            if (numPart.contains("/"))
            {
                int slashPos = numPart.indexOf("/");
                double num = numPart.substring(0, slashPos).getDoubleValue();
                double den = numPart.substring(slashPos + 1).getDoubleValue();
                return den > 0 ? num / den : 0.0;
            }
            return numPart.getDoubleValue();
        }

        // Handle frame-based format: "frames/fpsFrameRates" e.g., "3600/24000s" means 3600 samples
        // at 24000fps Or FCPXML frame format: "14400/2400s" meaning 14400 frames at 2400/100 =
        // 24fps
        if (timeStr.contains("/"))
        {
            int slashPos = timeStr.indexOf("/");
            juce::String numStr = timeStr.substring(0, slashPos);
            juce::String denStr = timeStr.substring(slashPos + 1);

            // Remove trailing 's' if present
            if (denStr.endsWith("s"))
                denStr = denStr.dropLastCharacters(1);

            double num = numStr.getDoubleValue();
            double den = denStr.getDoubleValue();

            if (den > 0)
                return num / den;
        }

        // Plain number - assume seconds
        return timeStr.getDoubleValue();
    };

    auto parseClipElement = [&](juce::XmlElement *element, double fallbackTimelineStart)
    {
        juce::String offsetStr = element->getStringAttribute("offset", "");
        juce::String durationStr = element->getStringAttribute("duration", "0s");
        juce::String startStr = element->getStringAttribute("start", "0s");

        juce::String ref = element->getStringAttribute("ref");
        if (ref.isEmpty())
        {
            if (auto *audio = element->getChildByName("audio"))
            {
                ref = audio->getStringAttribute("ref");
                if (durationStr == "0s")
                    durationStr = audio->getStringAttribute("duration", durationStr);
                if (startStr == "0s")
                    startStr = audio->getStringAttribute("start", startStr);
            }
            else if (auto *video = element->getChildByName("video"))
            {
                ref = video->getStringAttribute("ref");
                if (durationStr == "0s")
                    durationStr = video->getStringAttribute("duration", durationStr);
                if (startStr == "0s")
                    startStr = video->getStringAttribute("start", startStr);
            }
        }

        double offset = parseTime(offsetStr);
        double duration = parseTime(durationStr);
        double sourceStart = parseTime(startStr);

        double clipTimelineStart = (offset < 0) ? fallbackTimelineStart : offset;

        juce::String role = element->getStringAttribute(
            "audioRole", element->getStringAttribute("role", "dialogue"));

        int lane = element->getIntAttribute("lane", 0);

        juce::String trackName = role;
        if (lane != 0)
            trackName += " (Lane " + juce::String(lane) + ")";

        LogicProjectData::TrackData *track = nullptr;
        for (auto &t : project.tracks)
        {
            if (t.name == trackName)
            {
                track = &t;
                break;
            }
        }

        if (!track)
        {
            project.tracks.push_back({});
            track = &project.tracks.back();
            track->name = trackName;
            track->type = "audio";
        }

        LogicProjectData::TrackData::AudioRegion region;
        region.assetPath = ref;
        region.startTime = clipTimelineStart;
        region.duration = duration > 0 ? duration : 0.0;
        region.sourceOffset = sourceStart > 0 ? sourceStart : 0.0;

        if (auto *adjustVolume = element->getChildByName("adjust-volume"))
        {
            juce::String amount = adjustVolume->getStringAttribute("amount", "0dB");
            if (amount.endsWith("dB"))
                region.gain = amount.dropLastCharacters(2).getFloatValue();
        }

        if (auto *volume = element->getChildByName("volume"))
        {
            juce::String amount = volume->getStringAttribute("amount", "0dB");
            if (amount.endsWith("dB"))
                region.gain = amount.dropLastCharacters(2).getFloatValue();
        }

        track->audioRegions.push_back(region);

        juce::Logger::writeToLog("Imported clip: " + ref + " @ " +
                                 juce::String(clipTimelineStart, 3) + "s" +
                                 " dur=" + juce::String(duration, 3) + "s" +
                                 " srcOff=" + juce::String(sourceStart, 3) + "s");

        if (offset < 0 && duration > 0)
            currentTimelinePosition = clipTimelineStart + duration;
        else if (offset >= 0 && duration > 0)
            currentTimelinePosition = offset + duration;
    };

    for (auto *child : spine->getChildIterator())
    {
        juce::String tagName = child->getTagName();

        if (tagName == "asset-clip" || tagName == "clip" || tagName == "audio" ||
            tagName == "video")
        {
            parseClipElement(child, currentTimelinePosition);
        }
        else if (tagName == "gap")
        {
            // Logic Pro may wrap layered clips inside a gap container
            bool hadClipChildren = false;
            double gapBaseStart = currentTimelinePosition;
            juce::String gapOffsetStr = child->getStringAttribute("offset", "");
            double gapOffset = parseTime(gapOffsetStr);
            if (gapOffset >= 0)
                gapBaseStart = gapOffset;

            for (auto *gapChild : child->getChildIterator())
            {
                juce::String gapChildTag = gapChild->getTagName();
                if (gapChildTag == "asset-clip" || gapChildTag == "clip" || gapChildTag == "audio" ||
                    gapChildTag == "video")
                {
                    hadClipChildren = true;
                    parseClipElement(gapChild, gapBaseStart);
                }
            }

            if (!hadClipChildren)
            {
                // Gap advances timeline position when used as silence spacer
                juce::String durationStr = child->getStringAttribute("duration", "0s");
                double gapDuration = parseTime(durationStr);
                if (gapDuration > 0)
                    currentTimelinePosition += gapDuration;

                juce::Logger::writeToLog("Gap: " + juce::String(gapDuration, 3) +
                                         "s, timeline now at " +
                                         juce::String(currentTimelinePosition, 3) + "s");
            }
        }
        else if (tagName == "ref-clip")
        {
            // Reference to another clip - need to resolve from resources
            juce::String refId = child->getStringAttribute("ref");
            juce::String durationStr = child->getStringAttribute("duration", "0s");
            double duration = parseTime(durationStr);

            // For now, handle similarly to asset-clip
            juce::String offsetStr = child->getStringAttribute("offset", "");
            double offset = parseTime(offsetStr);
            double clipStart = (offset < 0) ? currentTimelinePosition : offset;

            if (duration > 0)
                currentTimelinePosition = clipStart + duration;
        }
        else if (tagName == "sync-clip" || tagName == "mc-clip")
        {
            // Synchronized or multicam clip - parse contained elements
            for (auto *innerChild : child->getChildIterator())
            {
                // Recursively handle audio/video within sync-clip
            }

            juce::String durationStr = child->getStringAttribute("duration", "0s");
            double duration = parseTime(durationStr);
            if (duration > 0)
                currentTimelinePosition += duration;
        }
    }
}

void LogicImporter::parseAssetClip(juce::XmlElement *assetClip, LogicProjectData::TrackData &track)
{
    if (!assetClip)
        return;

    LogicProjectData::TrackData::AudioRegion region;

    // Get asset reference
    juce::String ref = assetClip->getStringAttribute("ref");
    region.assetPath = ref;

    // Parse timing
    juce::String offset = assetClip->getStringAttribute("offset", "0s");
    juce::String duration = assetClip->getStringAttribute("duration", "0s");
    juce::String start = assetClip->getStringAttribute("start", "0s");

    // Parse time values (format: "123/456s" or "123s" or "00:01:23:12")
    auto parseTime = [](const juce::String &timeStr) -> double
    {
        if (timeStr.endsWith("s"))
        {
            juce::String numPart = timeStr.dropLastCharacters(1);
            if (numPart.contains("/"))
            {
                // Rational format: "123/456s"
                int slashPos = numPart.indexOf("/");
                double num = numPart.substring(0, slashPos).getDoubleValue();
                double den = numPart.substring(slashPos + 1).getDoubleValue();
                return den > 0 ? num / den : 0.0;
            }
            return numPart.getDoubleValue();
        }
        // Timecode format - would need frame rate
        return 0.0;
    };

    region.startTime = parseTime(offset);
    region.duration = parseTime(duration);
    region.sourceOffset = parseTime(start);

    // Parse gain
    if (auto *volume = assetClip->getChildByName("volume"))
    {
        juce::String amount = volume->getStringAttribute("amount", "0dB");
        if (amount.endsWith("dB"))
            region.gain = amount.dropLastCharacters(2).getFloatValue();
    }

    track.audioRegions.push_back(region);
}

void LogicImporter::parseAudioRole(juce::XmlElement *role, LogicProjectData::TrackData &track)
{
    // Parse audio role elements
}

LogicProjectData LogicImporter::parseLogicBundle(const juce::File &bundle)
{
    LogicProjectData project;
    project.projectName = bundle.getFileNameWithoutExtension();

    // Logic bundles have a specific internal structure
    // The main project data is in a proprietary binary format (stored in binary plist)
    // But we can extract audio and MIDI files from the Media folder

    // Look for Media folder for audio files
    auto mediaFolder = bundle.getChildFile("Media");
    if (mediaFolder.isDirectory())
    {
        for (const auto &file : juce::RangedDirectoryIterator(
                 mediaFolder, true, "*.aif;*.aiff;*.wav;*.mp3;*.m4a;*.caf"))
        {
            juce::String id = file.getFile().getFileNameWithoutExtension();
            project.mediaFiles[id] = file.getFile();
        }
    }

    // Also look in Resources folder (some bundles store media here)
    auto resourcesFolder = bundle.getChildFile("Resources");
    if (resourcesFolder.isDirectory())
    {
        for (const auto &file : juce::RangedDirectoryIterator(
                 resourcesFolder, true, "*.aif;*.aiff;*.wav;*.mp3;*.m4a;*.caf"))
        {
            juce::String id = file.getFile().getFileNameWithoutExtension();
            if (project.mediaFiles.count(id) == 0)
                project.mediaFiles[id] = file.getFile();
        }
    }

    // Look for MIDI files in the bundle
    std::vector<juce::File> midiFiles;
    for (const auto &file : juce::RangedDirectoryIterator(bundle, true, "*.mid;*.midi"))
    {
        midiFiles.push_back(file.getFile());
    }

    // Look for Alternatives (different project versions)
    auto alternativesFolder = bundle.getChildFile("Alternatives");
    if (alternativesFolder.isDirectory())
    {
        // Each alternative has its own Project Data folder
        for (const auto &altEntry : juce::RangedDirectoryIterator(alternativesFolder, false))
        {
            if (!altEntry.isDirectory())
                continue;

            // Look for audio in this alternative
            auto altMediaFolder = altEntry.getFile().getChildFile("Media");
            if (altMediaFolder.isDirectory())
            {
                for (const auto &file : juce::RangedDirectoryIterator(
                         altMediaFolder, true, "*.aif;*.aiff;*.wav;*.mp3;*.m4a;*.caf"))
                {
                    juce::String id = file.getFile().getFileNameWithoutExtension();
                    if (project.mediaFiles.count(id) == 0)
                        project.mediaFiles[id] = file.getFile();
                }
            }
        }
    }

    // Try to parse any XML files that might contain metadata
    for (const auto &file : juce::RangedDirectoryIterator(bundle, true, "*.xml"))
    {
        auto xml = juce::XmlDocument::parse(file.getFile());
        if (xml)
        {
            // Check if this is an FCPXML or similar timeline format
            if (xml->getTagName() == "fcpxml" || xml->getTagName() == "project")
            {
                // Found embedded timeline data!
                return parseFCPXML(file.getFile());
            }
        }
    }

    // Since .logicx is a binary format, we create tracks for each found file
    // Audio files get placed at position 0 (user will need to arrange manually)
    if (!project.mediaFiles.empty())
    {
        for (const auto &[id, file] : project.mediaFiles)
        {
            LogicProjectData::TrackData track;
            track.name = file.getFileNameWithoutExtension();
            track.type = "audio";

            LogicProjectData::TrackData::AudioRegion region;
            region.assetPath = file.getFullPathName();
            region.startTime = 0.0;
            region.duration = 0.0; // Will be determined when loading
            track.audioRegions.push_back(region);

            project.tracks.push_back(track);
        }
    }

    // Also import any MIDI files found
    for (const auto &midiFile : midiFiles)
    {
        // Parse MIDI file and add tracks
        juce::FileInputStream stream(midiFile);
        if (stream.openedOk())
        {
            juce::MidiFile midi;
            if (midi.readFrom(stream))
            {
                midi.convertTimestampTicksToSeconds();

                for (int trackIdx = 0; trackIdx < midi.getNumTracks(); ++trackIdx)
                {
                    const auto *midiTrack = midi.getTrack(trackIdx);
                    if (!midiTrack || midiTrack->getNumEvents() == 0)
                        continue;

                    LogicProjectData::TrackData track;
                    track.name = midiFile.getFileNameWithoutExtension();
                    if (midi.getNumTracks() > 1)
                        track.name += " (Track " + juce::String(trackIdx + 1) + ")";
                    track.type = "midi";

                    // Parse MIDI events into a region
                    LogicProjectData::TrackData::MidiRegion midiRegion;
                    midiRegion.name = track.name;
                    midiRegion.startTime = 0.0;

                    double maxTime = 0.0;
                    std::map<int, double>
                        noteOnTimes; // Track note-on times by note number + channel
                    std::map<int, float> noteOnVelocities;

                    for (int i = 0; i < midiTrack->getNumEvents(); ++i)
                    {
                        const auto &event = midiTrack->getEventPointer(i)->message;
                        double timestamp = event.getTimeStamp();

                        if (event.isNoteOn())
                        {
                            int key = event.getNoteNumber() * 16 + event.getChannel();
                            noteOnTimes[key] = timestamp;
                            noteOnVelocities[key] = event.getFloatVelocity();
                        }
                        else if (event.isNoteOff())
                        {
                            int key = event.getNoteNumber() * 16 + event.getChannel();
                            if (noteOnTimes.count(key))
                            {
                                LogicProjectData::TrackData::MidiRegion::Note note;
                                note.noteNumber = event.getNoteNumber();
                                note.velocity = noteOnVelocities[key];
                                note.startTime = noteOnTimes[key];
                                note.duration = timestamp - noteOnTimes[key];
                                midiRegion.notes.push_back(note);

                                maxTime = std::max(maxTime, timestamp);
                                noteOnTimes.erase(key);
                            }
                        }
                    }

                    midiRegion.duration = maxTime;
                    if (!midiRegion.notes.empty())
                    {
                        track.midiRegions.push_back(midiRegion);
                        project.tracks.push_back(track);
                    }
                }
            }
        }
    }

    return project;
}

LogicImportResult LogicImporter::convertToSession(const LogicProjectData &projectData,
                                                  Session &session,
                                                  juce::AudioFormatManager &formatManager,
                                                  const juce::File &projectDir)
{
    LogicImportResult result;
    result.success = true;

    // Reset session
    session = Session();

    // Set project-level properties
    session.setBpm(projectData.bpm);
    session.setTimeSignature(projectData.timeSigNumerator, projectData.timeSigDenominator);
    session.setSampleRate(projectData.sampleRate);

    if (!projectData.bpmDetected)
    {
        result.warnings.push_back(
            "No tempo metadata found in import. Using default BPM " +
            juce::String(projectData.bpm, 1));
    }

    double sampleRate = projectData.sampleRate > 0 ? projectData.sampleRate : 44100.0;

    // Import each track
    for (const auto &trackData : projectData.tracks)
    {
        // Determine track type
        TrackType trackType = TrackType::Audio;
        if (trackData.type == "midi" || trackData.type == "instrument")
            trackType = TrackType::Midi;

        int trackIndex = session.addTrack(trackData.name, trackType);
        auto *track = session.getTrack(trackIndex);
        if (!track)
            continue;

        result.tracksImported++;

        // Set track properties
        track->gainDb = trackData.volume;
        track->pan = trackData.pan;
        track->muted = trackData.muted;
        track->solo = trackData.solo;

        // Import audio regions
        for (const auto &region : trackData.audioRegions)
        {
            // Resolve audio file
            juce::File audioFile = resolveMediaFile(region.assetPath, projectDir);

            if (!audioFile.existsAsFile())
            {
                // Try looking in mediaFiles map
                if (projectData.mediaFiles.count(region.assetPath) > 0)
                    audioFile = projectData.mediaFiles.at(region.assetPath);
            }

            if (!audioFile.existsAsFile())
            {
                result.warnings.push_back("Audio file not found: " + region.assetPath);
                continue;
            }

            // Load audio asset
            auto asset = session.loadAudioAsset(audioFile, formatManager);
            if (!asset)
            {
                result.warnings.push_back("Could not load audio file: " +
                                          audioFile.getFullPathName());
                continue;
            }

            // Get asset's native sample rate for source-relative calculations
            double assetSampleRate = asset->sampleRate > 0 ? asset->sampleRate : sampleRate;

            // Create clip
            Clip clip;
            clip.id = juce::Uuid().toString();
            clip.asset = asset;

            // Timeline position uses project sample rate
            clip.timelineStartSample = secondsToSamples(region.startTime, sampleRate);

            // Source offset uses ASSET sample rate (position within original file)
            clip.sourceStartSample = secondsToSamples(region.sourceOffset, assetSampleRate);

            // Clip duration on timeline (in project sample rate)
            // But we need to calculate source length in asset sample rate
            if (region.duration > 0)
            {
                // Duration should match clip length in asset samples
                clip.sourceLengthSamples = secondsToSamples(region.duration, assetSampleRate);
            }
            else
            {
                clip.sourceLengthSamples = asset->lengthInSamples - clip.sourceStartSample;
            }

            // Validate bounds
            if (clip.sourceStartSample >= asset->lengthInSamples)
            {
                result.warnings.push_back("Source offset exceeds file length for: " +
                                          audioFile.getFileName());
                clip.sourceStartSample = 0;
            }
            if (clip.sourceStartSample + clip.sourceLengthSamples > asset->lengthInSamples)
            {
                clip.sourceLengthSamples = asset->lengthInSamples - clip.sourceStartSample;
            }

            juce::Logger::writeToLog(
                "  Clip created: timeline=" + juce::String(clip.timelineStartSample) +
                " samples (" + juce::String(region.startTime, 3) + "s), source=" +
                juce::String(clip.sourceStartSample) + "/" + juce::String(asset->lengthInSamples) +
                " samples, len=" + juce::String(clip.sourceLengthSamples) + " samples");

            clip.gainDb = region.gain;

            session.addClipToTrack(trackIndex, clip);
            result.audioClipsImported++;
        }

        // Import MIDI regions
        for (const auto &midiRegion : trackData.midiRegions)
        {
            MidiClip mc;
            mc.id = juce::Uuid().toString();
            mc.name = midiRegion.name;
            mc.timelineStartSample = secondsToSamples(midiRegion.startTime, sampleRate);
            mc.lengthSamples = secondsToSamples(midiRegion.duration, sampleRate);

            for (const auto &note : midiRegion.notes)
            {
                MidiNote mn;
                mn.id = juce::Uuid().toString();
                mn.noteNumber = note.noteNumber;
                mn.velocity = note.velocity;
                mn.startSample = secondsToSamples(note.startTime, sampleRate);
                mn.lengthSamples = secondsToSamples(note.duration, sampleRate);
                mc.notes.push_back(mn);
            }

            track->midiClips.push_back(std::move(mc));
            result.midiClipsImported++;
        }

        // Import plugins
        for (const auto &pluginData : trackData.plugins)
        {
            PluginSlot slot;
            slot.pluginName = pluginData.name;
            slot.originalIdentifier = pluginData.identifier;
            slot.bypassed = pluginData.bypassed;

            // Try to resolve plugin
            auto match = resolvePlugin(pluginData);
            if (match.found)
            {
                slot.pluginId = match.amplPluginId;
                slot.pluginPath = match.pluginPath;
                slot.pluginFormat = match.format;
                slot.isResolved = true;
                result.pluginsResolved++;
            }
            else
            {
                slot.isResolved = false;
                result.pluginsUnresolved++;
                result.warnings.push_back("Plugin not found: " + pluginData.name + " (" +
                                          pluginData.identifier + ")");
            }

            // Store parameter values
            for (const auto &[paramName, paramValue] : pluginData.parameters)
                slot.parameterValues[paramName] = paramValue;

            track->pluginChain.push_back(slot);
        }

        // Import instrument plugin
        if (trackData.instrument.has_value())
        {
            const auto &instrumentData = trackData.instrument.value();
            PluginSlot slot;
            slot.pluginName = instrumentData.name;
            slot.originalIdentifier = instrumentData.identifier;
            slot.bypassed = instrumentData.bypassed;

            auto match = resolvePlugin(instrumentData);
            if (match.found)
            {
                slot.pluginId = match.amplPluginId;
                slot.pluginPath = match.pluginPath;
                slot.pluginFormat = match.format;
                slot.isResolved = true;
                result.pluginsResolved++;
            }
            else
            {
                slot.isResolved = false;
                result.pluginsUnresolved++;
                result.warnings.push_back("Instrument not found: " + instrumentData.name + " (" +
                                          instrumentData.identifier + ")");
            }

            track->instrumentPlugin = slot;
        }
    }

    return result;
}

LogicImporter::PluginMatch
LogicImporter::resolvePlugin(const LogicProjectData::TrackData::PluginData &plugin)
{
    PluginMatch match;

    if (!pluginManager_)
        return match;

    // Get available plugins
    auto availablePlugins = pluginManager_->getAvailablePlugins();

    // Try exact identifier match first
    for (const auto &available : availablePlugins)
    {
        if (available.description.fileOrIdentifier == plugin.identifier ||
            available.description.createIdentifierString() == plugin.identifier)
        {
            match.found = true;
            match.amplPluginId = available.description.createIdentifierString();
            match.pluginPath = available.description.fileOrIdentifier;
            match.format = available.format.toStdString();
            return match;
        }
    }

    // Try matching by name and manufacturer
    for (const auto &available : availablePlugins)
    {
        bool nameMatch = available.description.name.containsIgnoreCase(plugin.name) ||
                         plugin.name.containsIgnoreCase(available.description.name);
        bool mfrMatch =
            plugin.manufacturer.isEmpty() ||
            available.description.manufacturerName.containsIgnoreCase(plugin.manufacturer);

        if (nameMatch && mfrMatch)
        {
            match.found = true;
            match.amplPluginId = available.description.createIdentifierString();
            match.pluginPath = available.description.fileOrIdentifier;
            match.format = available.format.toStdString();
            return match;
        }
    }

    // Try partial name match
    for (const auto &available : availablePlugins)
    {
        // Remove common suffixes and compare
        juce::String pluginNameClean = plugin.name.toLowerCase().replace(" ", "").replace("_", "");
        juce::String availableNameClean =
            available.description.name.toLowerCase().replace(" ", "").replace("_", "");

        if (pluginNameClean == availableNameClean || pluginNameClean.contains(availableNameClean) ||
            availableNameClean.contains(pluginNameClean))
        {
            match.found = true;
            match.amplPluginId = available.description.createIdentifierString();
            match.pluginPath = available.description.fileOrIdentifier;
            match.format = available.format.toStdString();
            return match;
        }
    }

    return match;
}

std::vector<juce::String> LogicImporter::getAvailablePluginIds() const
{
    std::vector<juce::String> ids;

    if (!pluginManager_)
        return ids;

    auto plugins = pluginManager_->getAvailablePlugins();
    for (const auto &plugin : plugins)
        ids.push_back(plugin.description.createIdentifierString());

    return ids;
}

SampleCount LogicImporter::secondsToSamples(double seconds, double sampleRate) const
{
    return static_cast<SampleCount>(seconds * sampleRate);
}

double LogicImporter::parseTimecode(const juce::String &timecode, double fps) const
{
    // Parse HH:MM:SS:FF format
    auto parts = juce::StringArray::fromTokens(timecode, ":", "");
    if (parts.size() != 4)
        return 0.0;

    double hours = parts[0].getDoubleValue();
    double minutes = parts[1].getDoubleValue();
    double seconds = parts[2].getDoubleValue();
    double frames = parts[3].getDoubleValue();

    return hours * 3600.0 + minutes * 60.0 + seconds + frames / fps;
}

juce::File LogicImporter::resolveMediaFile(const juce::String &ref, const juce::File &projectDir)
{
    // Try various resolution strategies

    // 1. Absolute path
    juce::File absoluteFile(ref);
    if (absoluteFile.existsAsFile())
        return absoluteFile;

    // 2. Relative to project directory
    auto relativeFile = projectDir.getChildFile(ref);
    if (relativeFile.existsAsFile())
        return relativeFile;

    // 3. Check in Media subfolder
    auto mediaFile = projectDir.getChildFile("Media").getChildFile(ref);
    if (mediaFile.existsAsFile())
        return mediaFile;

    // 4. Check just the filename in project directory
    auto justFilename = juce::File(ref).getFileName();
    auto filenameFile = projectDir.getChildFile(justFilename);
    if (filenameFile.existsAsFile())
        return filenameFile;

    // 5. Search recursively in project directory
    for (const auto &entry : juce::RangedDirectoryIterator(projectDir, true, ref))
    {
        return entry.getFile();
    }

    return juce::File();
}

} // namespace ampl
