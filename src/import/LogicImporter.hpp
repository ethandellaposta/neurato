#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "model/Session.hpp"
#include "model/Track.hpp"
#include "engine/plugins/manager/PluginManager.hpp"
#include <vector>
#include <map>

namespace ampl {

// Import result with warnings for unresolved items
struct LogicImportResult
{
    bool success{false};
    juce::String errorMessage;

    // Warnings for items that couldn't be fully imported
    std::vector<juce::String> warnings;

    // Statistics
    int tracksImported{0};
    int audioClipsImported{0};
    int midiClipsImported{0};
    int pluginsResolved{0};
    int pluginsUnresolved{0};
};

// Represents a Logic Pro project structure extracted from FCPXML or .logicx
struct LogicProjectData
{
    // Project metadata
    juce::String projectName;
    double bpm{120.0};
    bool bpmDetected{false};
    int timeSigNumerator{4};
    int timeSigDenominator{4};
    double sampleRate{44100.0};

    // Track data
    struct TrackData
    {
        juce::String name;
        juce::String type; // "audio", "midi", "instrument", "aux", "bus"
        float volume{0.0f}; // dB
        float pan{0.0f};
        bool muted{false};
        bool solo{false};

        // Audio regions
        struct AudioRegion
        {
            juce::String assetPath;
            double startTime{0.0};      // Seconds on timeline
            double duration{0.0};       // Seconds
            double sourceOffset{0.0};   // Offset into source file
            float gain{0.0f};           // dB
        };
        std::vector<AudioRegion> audioRegions;

        // MIDI regions
        struct MidiRegion
        {
            juce::String name;
            double startTime{0.0};
            double duration{0.0};

            struct Note
            {
                int noteNumber{60};
                float velocity{0.8f};
                double startTime{0.0}; // Relative to region start
                double duration{0.0};
            };
            std::vector<Note> notes;
        };
        std::vector<MidiRegion> midiRegions;

        // Plugin chain
        struct PluginData
        {
            juce::String name;
            juce::String manufacturer;
            juce::String identifier;    // Original AU/VST identifier
            juce::String format;        // "AU", "VST3", etc.
            bool bypassed{false};
            std::map<juce::String, float> parameters;
        };
        std::vector<PluginData> plugins;

        // Instrument (for MIDI tracks)
        std::optional<PluginData> instrument;
    };
    std::vector<TrackData> tracks;

    // Media references
    std::map<juce::String, juce::File> mediaFiles;
};

// Imports Logic Pro projects (FCPXML format) into Ampl sessions
class LogicImporter
{
public:
    LogicImporter();
    ~LogicImporter() = default;

    // Import a Logic Pro export file (.fcpxml or .xml)
    LogicImportResult importFromFCPXML(const juce::File& fcpxmlFile,
                                        Session& session,
                                        juce::AudioFormatManager& formatManager);

    // Import directly from a .logicx bundle (macOS only)
    LogicImportResult importFromLogicBundle(const juce::File& logicxBundle,
                                             Session& session,
                                             juce::AudioFormatManager& formatManager);

    // Set the plugin manager for AU/VST resolution
    void setPluginManager(PluginManager* manager) { pluginManager_ = manager; }

    // Get available plugin identifiers for matching
    std::vector<juce::String> getAvailablePluginIds() const;

private:
    PluginManager* pluginManager_{nullptr};

    // FCPXML parsing
    LogicProjectData parseFCPXML(const juce::File& file);
    void parseSequence(juce::XmlElement* sequence, LogicProjectData& project);
    void parseSpine(juce::XmlElement* spine, LogicProjectData& project);
    void parseAssetClip(juce::XmlElement* assetClip, LogicProjectData::TrackData& track);
    void parseAudioRole(juce::XmlElement* role, LogicProjectData::TrackData& track);

    // Logic bundle parsing (alternate format)
    LogicProjectData parseLogicBundle(const juce::File& bundle);

    // Convert parsed data to Ampl session
    LogicImportResult convertToSession(const LogicProjectData& projectData,
                                        Session& session,
                                        juce::AudioFormatManager& formatManager,
                                        const juce::File& projectDir);

    // Plugin resolution
    struct PluginMatch
    {
        bool found{false};
        juce::String amplPluginId;
        juce::String pluginPath;
        juce::String format;
    };
    PluginMatch resolvePlugin(const LogicProjectData::TrackData::PluginData& plugin);

    // Time conversion helpers
    SampleCount secondsToSamples(double seconds, double sampleRate) const;
    double parseTimecode(const juce::String& timecode, double fps = 24.0) const;

    // Media file resolution
    juce::File resolveMediaFile(const juce::String& ref, const juce::File& projectDir);
};

} // namespace ampl
