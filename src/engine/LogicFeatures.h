#pragma once

#include "AudioGraph.h"
#include "Automation.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <memory>
#include <string>
#include <map>
#include <set>

namespace neurato {

// Logic Pro X-style track types and features
enum class LogicTrackType {
    Audio,
    Instrument, 
    DrumMachine,
    External,
    Output,
    Master,
    Bus,
    Input,
    Aux,
    VCA,
    FolderStack
};

// Logic Pro X-style mixer with advanced routing
class LogicMixerChannel {
public:
    struct ChannelStrip {
        std::string id;
        std::string name;
        LogicTrackType type;
        
        // Core controls
        float volume{0.0f};        // dB scale
        float pan{0.0f};           // -1 to 1
        bool mute{false};
        bool solo{false};
        bool recordArm{false};
        
        // Logic-style features
        float sendLevel[8]{0.0f}; // 8 sends per channel
        bool sendPreFader[8]{false};
        std::string sendTargets[8];
        
        // Advanced routing
        std::string inputSource;
        std::string outputDestination;
        
        // VCA grouping
        std::string vcaAssignment;
        
        // Plugin chain (Logic-style slots)
        static constexpr int kPluginSlots = 15;
        std::array<std::string, kPluginSlots> pluginChain;
        bool pluginBypass[kPluginSlots]{false};
        
        // Automation
        std::map<std::string, std::shared_ptr<AutomationLane>> automationLanes;
        
        // Logic-style features
        float phaseInvert{0.0f};     // 0 or 180 degrees
        float gain{0.0f};            // Trim gain in dB
        bool polarityInverted{false};
        
        // Display options
        bool showAutomation{true};
        bool showPlugins{true};
        bool showSends{true};
    };
    
    LogicMixerChannel(const ChannelStrip& strip);
    ~LogicMixerChannel() = default;
    
    // Processing
    void processAudio(juce::AudioBuffer<float>& buffer, int numSamples);
    void applyVolumeAndPan(juce::AudioBuffer<float>& buffer);
    void applySends(juce::AudioBuffer<float>& buffer, std::map<std::string, juce::AudioBuffer<float>>& sendBuffers);
    
    // Channel strip controls
    void setVolume(float dB);
    void setPan(float pan);
    void setMute(bool muted);
    void setSolo(bool soloed);
    void setRecordArm(bool armed);
    
    // Send controls
    void setSendLevel(int sendIndex, float level);
    void setSendPreFader(int sendIndex, bool preFader);
    void setSendTarget(int sendIndex, const std::string& target);
    
    // Plugin management
    void insertPlugin(int slot, const std::string& pluginId);
    void removePlugin(int slot);
    void bypassPlugin(int slot, bool bypassed);
    void swapPlugins(int slot1, int slot2);
    
    // VCA assignment
    void assignToVCA(const std::string& vcaId);
    void unassignFromVCA();
    
    // Automation
    void addAutomationLane(const std::string& parameter, std::shared_ptr<AutomationLane> lane);
    void removeAutomationLane(const std::string& parameter);
    
    const ChannelStrip& getChannelStrip() const { return channelStrip_; }

private:
    ChannelStrip channelStrip_;
    
    // Processing state
    float currentVolume_{0.0f};
    float currentPan_{0.0f};
    float volumeSmoother_{0.0f};
    float panSmoother_{0.0f};
    
    static constexpr float kSmoothingCoefficient = 0.999f;
    
    void updateSmoothers();
    float dbToLinear(float dB) const;
    float linearToDb(float linear) const;
};

// Logic Pro X-style environment with advanced routing
class LogicEnvironment {
public:
    struct Bus {
        std::string id;
        std::string name;
        int busNumber;
        float volume{0.0f};
        float pan{0.0f};
        bool mute{false};
        std::vector<std::string> inputTracks;
    };
    
    struct VCA {
        std::string id;
        std::string name;
        float volume{0.0f};
        bool mute{false};
        std::vector<std::string> assignedTracks;
    };
    
    LogicEnvironment();
    ~LogicEnvironment() = default;
    
    // Bus management
    std::string createBus(const std::string& name);
    void removeBus(const std::string& busId);
    void assignTrackToBus(const std::string& trackId, const std::string& busId);
    void removeTrackFromBus(const std::string& trackId, const std::string& busId);
    
    // VCA management
    std::string createVCA(const std::string& name);
    void removeVCA(const std::string& vcaId);
    void assignTrackToVCA(const std::string& trackId, const std::string& vcaId);
    void removeTrackFromVCA(const std::string& trackId, const std::string& vcaId);
    
    // Routing
    void setTrackOutput(const std::string& trackId, const std::string& destination);
    std::string getTrackOutput(const std::string& trackId) const;
    
    // Processing
    void processEnvironment(std::map<std::string, juce::AudioBuffer<float>>& trackBuffers,
                           std::map<std::string, juce::AudioBuffer<float>>& busBuffers,
                           int numSamples);
    
    // Accessors
    const std::vector<Bus>& getBuses() const { return buses_; }
    const std::vector<VCA>& getVCAs() const { return vcas_; }
    const std::map<std::string, std::string>& getTrackOutputs() const { return trackOutputs_; }

private:
    std::vector<Bus> buses_;
    std::vector<VCA> vcas_;
    std::map<std::string, std::string> trackOutputs_; // trackId -> outputId
    std::map<std::string, std::vector<std::string>> busInputs_; // busId -> trackIds
    std::map<std::string, std::vector<std::string>> vcaAssignments_; // vcaId -> trackIds
    
    int nextBusNumber_{1};
    int nextVCAId_{1};
};

// Logic Pro X-style smart controls
class LogicSmartControls {
public:
    struct SmartControl {
        std::string id;
        std::string name;
        std::string type; // "fader", "knob", "button", "menu"
        float minValue{0.0f};
        float maxValue{1.0f};
        float currentValue{0.0f};
        
        // Mapped parameters
        struct ParameterMapping {
            std::string trackId;
            std::string parameterId;
            float inputRangeMin{0.0f};
            float inputRangeMax{1.0f};
            float outputRangeMin{0.0f};
            float outputRangeMax{1.0f};
            float curve{1.0f}; // 1.0 = linear
        };
        
        std::vector<ParameterMapping> mappings;
    };
    
    LogicSmartControls();
    ~LogicSmartControls() = default;
    
    // Smart control management
    std::string createSmartControl(const std::string& name, const std::string& type);
    void removeSmartControl(const std::string& controlId);
    void updateSmartControl(const std::string& controlId, float value);
    
    // Parameter mapping
    void addMapping(const std::string& controlId, const std::string& trackId, 
                   const std::string& parameterId);
    void removeMapping(const std::string& controlId, const std::string& trackId, 
                      const std::string& parameterId);
    void setMappingRange(const std::string& controlId, const std::string& trackId,
                        float inputMin, float inputMax, float outputMin, float outputMax);
    
    // Processing
    void processSmartControls(std::map<std::string, std::shared_ptr<LogicMixerChannel>>& channels);
    
    // Accessors
    const std::vector<SmartControl>& getSmartControls() const { return smartControls_; }

private:
    std::vector<SmartControl> smartControls_;
    int nextControlId_{1};
    
    float applyMapping(float inputValue, const SmartControl::ParameterMapping& mapping) const;
};

// Logic Pro X-style track alternatives and takes
class LogicTrackAlternatives {
public:
    struct Take {
        std::string id;
        std::string name;
        SampleCount start{0};
        SampleCount length{0};
        std::string audioFilePath;
        float volume{0.0f};
        bool muted{false};
    };
    
    struct TrackAlternative {
        std::string id;
        std::string name;
        std::vector<Take> takes;
        int currentTakeIndex{0};
        bool isComped{false};
        std::vector<SampleCount> compEditPoints;
    };
    
    LogicTrackAlternatives();
    ~LogicTrackAlternatives() = default;
    
    // Track alternative management
    std::string createTrackAlternative(const std::string& trackId, const std::string& name);
    void removeTrackAlternative(const std::string& alternativeId);
    
    // Take management
    void addTake(const std::string& alternativeId, const Take& take);
    void removeTake(const std::string& alternativeId, const std::string& takeId);
    void switchToTake(const std::string& alternativeId, int takeIndex);
    
    // Comping
    void startComping(const std::string& alternativeId);
    void addCompEditPoint(const std::string& alternativeId, SampleCount position);
    void finishComping(const std::string& alternativeId);
    
    // Accessors
    const std::map<std::string, TrackAlternative>& getTrackAlternatives() const { return trackAlternatives_; }
    const TrackAlternative* getTrackAlternative(const std::string& id) const;

private:
    std::map<std::string, TrackAlternative> trackAlternatives_; // alternativeId -> TrackAlternative
    std::map<std::string, std::string> trackToAlternative_; // trackId -> alternativeId
    int nextAlternativeId_{1};
};

// Logic Pro X-style flex time editing
class LogicFlexTime {
public:
    enum class FlexMode {
        Monophonic,
        Polyphonic,
        Rhythmic,
        Slicing,
        Speed
    };
    
    struct FlexMarker {
        SampleCount position{0};
        float originalTempo{120.0f};
        float targetTempo{120.0f};
        bool isAnchor{false};
    };
    
    struct FlexRegion {
        SampleCount start{0};
        SampleCount end{0};
        FlexMode mode{FlexMode::Rhythmic};
        std::vector<FlexMarker> markers;
        float strength{1.0f};
    };
    
    LogicFlexTime();
    ~LogicFlexTime() = default;
    
    // Flex time regions
    void addFlexRegion(const FlexRegion& region);
    void removeFlexRegion(SampleCount position);
    void setFlexMode(SampleCount position, FlexMode mode);
    
    // Marker management
    void addFlexMarker(SampleCount position, float tempo, bool isAnchor = false);
    void removeFlexMarker(SampleCount position);
    void moveFlexMarker(SampleCount oldPos, SampleCount newPos);
    
    // Processing
    void processFlexTime(juce::AudioBuffer<float>& buffer, SampleCount regionStart, 
                        double originalSampleRate, double currentSampleRate);
    
    // Analysis
    std::vector<FlexMarker> analyzeTransients(const float* audio, int numSamples, 
                                             double sampleRate) const;
    
    // Accessors
    const std::vector<FlexRegion>& getFlexRegions() const { return flexRegions_; }

private:
    std::vector<FlexRegion> flexRegions_;
    
    // Time stretching algorithms
    void stretchAudioRhythmic(juce::AudioBuffer<float>& buffer, float ratio);
    void stretchAudioMonophonic(juce::AudioBuffer<float>& buffer, float ratio);
    void stretchAudioSlicing(juce::AudioBuffer<float>& buffer, float ratio);
    
    // Utility
    std::vector<FlexRegion>::iterator findFlexRegion(SampleCount position);
    std::vector<FlexMarker>::iterator findFlexMarker(std::vector<FlexMarker>& markers, SampleCount position);
};

// Logic Pro X-style step sequencer
class LogicStepSequencer {
public:
    struct StepPattern {
        std::string id;
        std::string name;
        int steps{16};
        int resolution{16}; // 16th notes
        std::vector<std::vector<float>> noteVelocities; // [note][step]
        std::vector<bool> noteMutes; // per note
        std::vector<float> noteGains; // per note
    };
    
    struct DrumKit {
        std::string id;
        std::string name;
        std::vector<std::string> drumNames;
        std::vector<std::string> samplePaths;
    };
    
    LogicStepSequencer();
    ~LogicStepSequencer() = default;
    
    // Pattern management
    std::string createPattern(const std::string& name, int steps = 16);
    void removePattern(const std::string& patternId);
    void duplicatePattern(const std::string& patternId);
    
    // Step editing
    void setStepVelocity(const std::string& patternId, int noteIndex, int step, float velocity);
    void clearStep(const std::string& patternId, int noteIndex, int step);
    void clearPattern(const std::string& patternId);
    
    // Drum kit management
    void loadDrumKit(const DrumKit& kit);
    std::vector<std::string> getAvailableDrumKits() const;
    
    // Processing
    void processPattern(const std::string& patternId, juce::AudioBuffer<float>& output,
                       SampleCount position, double sampleRate, double bpm);
    
    // Real-time recording
    void startRecording();
    void stopRecording();
    void recordNote(int noteIndex, float velocity, SampleCount position);
    
    // Accessors
    const std::vector<StepPattern>& getPatterns() const { return patterns_; }
    const DrumKit* getCurrentDrumKit() const { return currentDrumKit_; }

private:
    std::vector<StepPattern> patterns_;
    std::vector<DrumKit> drumKits_;
    const DrumKit* currentDrumKit_{nullptr};
    int nextPatternId_{1};
    
    bool isRecording_{false};
    std::vector<std::pair<int, float>> recordedNotes_; // (noteIndex, velocity)
    SampleCount recordingStart_{0};
    
    void generateDrumSound(int noteIndex, float velocity, float* output, int numSamples);
};

// Logic Pro X-style score editor integration
class LogicScoreEditor {
public:
    struct Note {
        int pitch{60};        // MIDI note number
        int velocity{80};     // 0-127
        SampleCount start{0};
        SampleCount length{44100}; // In samples
        bool isMuted{false};
    };
    
    struct ScoreTrack {
        std::string id;
        std::string name;
        std::vector<Note> notes;
        int timeSignatureNumerator{4};
        int timeSignatureDenominator{4};
        double key{0.0}; // C=0, C#=1, etc.
        bool isMinor{false};
    };
    
    LogicScoreEditor();
    ~LogicScoreEditor() = default;
    
    // Score track management
    std::string createScoreTrack(const std::string& name);
    void removeScoreTrack(const std::string& trackId);
    
    // Note editing
    void addNote(const std::string& trackId, const Note& note);
    void removeNote(const std::string& trackId, SampleCount position, int pitch);
    void moveNote(const std::string& trackId, SampleCount oldPos, int oldPitch, 
                  SampleCount newPos, int newPitch);
    void resizeNote(const std::string& trackId, SampleCount position, int pitch, 
                    SampleCount newLength);
    
    // Quantization
    void quantizeTrack(const std::string& trackId, int division); // 1=whole, 4=quarter, 16=16th
    void quantizeNote(const std::string& trackId, SampleCount position, int pitch, int division);
    
    // Transposition
    void transposeTrack(const std::string& trackId, int semitones);
    void transposeNote(const std::string& trackId, SampleCount position, int pitch, int semitones);
    
    // MIDI export
    std::vector<uint8_t> exportToMIDI(const std::string& trackId) const;
    void importFromMIDI(const std::string& trackId, const std::vector<uint8_t>& midiData);
    
    // Accessors
    const std::map<std::string, ScoreTrack>& getScoreTracks() const { return scoreTracks_; }

private:
    std::map<std::string, ScoreTrack> scoreTracks_;
    int nextTrackId_{1};
    
    SampleCount quantizePosition(SampleCount position, int division, double bpm) const;
    int pitchToStaffLine(int pitch, const ScoreTrack& track) const;
};

} // namespace neurato
