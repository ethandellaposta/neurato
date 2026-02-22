#pragma once

#include "LogicFeatures.h"
#include "AudioGraph.h"
#include "SessionRenderer.h"
#include <memory>
#include <map>
#include <atomic>

namespace neurato {

// Logic Pro X-style audio engine integration
class LogicAudioEngine {
public:
    LogicAudioEngine();
    ~LogicAudioEngine();
    
    // Initialization
    bool initialize(double sampleRate, int bufferSize);
    void shutdown();
    
    // Track management
    std::string createTrack(const std::string& name, LogicTrackType type);
    void removeTrack(const std::string& trackId);
    void renameTrack(const std::string& trackId, const std::string& newName);
    
    // Channel strip management
    std::shared_ptr<LogicMixerChannel> getChannel(const std::string& trackId);
    void updateChannel(const std::string& trackId, const LogicMixerChannel::ChannelStrip& strip);
    
    // Environment management
    std::shared_ptr<LogicEnvironment> getEnvironment() { return environment_; }
    
    // Audio processing
    void processAudio(float** outputBuffers, int numChannels, int numSamples, SampleCount position);
    
    // Real-time parameter updates
    void setTrackVolume(const std::string& trackId, float dB);
    void setTrackPan(const std::string& trackId, float pan);
    void setTrackMute(const std::string& trackId, bool muted);
    void setTrackSolo(const std::string& trackId, bool soloed);
    void setSendLevel(const std::string& trackId, int sendIndex, float level);
    
    // Plugin management
    void insertPlugin(const std::string& trackId, int slot, const std::string& pluginId);
    void bypassPlugin(const std::string& trackId, int slot, bool bypassed);
    void removePlugin(const std::string& trackId, int slot);
    
    // Automation
    void addAutomationLane(const std::string& trackId, const std::string& parameter, 
                          std::shared_ptr<AutomationLane> lane);
    void removeAutomationLane(const std::string& trackId, const std::string& parameter);
    
    // State management
    struct EngineState {
        std::map<std::string, LogicMixerChannel::ChannelStrip> channelStrips;
        std::vector<LogicEnvironment::Bus> buses;
        std::vector<LogicEnvironment::VCA> vcas;
        std::map<std::string, std::string> trackOutputs;
    };
    
    EngineState getEngineState() const;
    void setEngineState(const EngineState& state);

private:
    // Audio graph for processing
    std::unique_ptr<AudioGraph> audioGraph_;
    
    // Logic-style components
    std::shared_ptr<LogicEnvironment> environment_;
    std::map<std::string, std::shared_ptr<LogicMixerChannel>> channels_;
    
    // Audio processing state
    double sampleRate_{44100.0};
    int bufferSize_{512};
    std::atomic<bool> processing_{false};
    
    // Track ID generation
    int nextTrackId_{1};
    
    // Processing buffers
    std::map<std::string, juce::AudioBuffer<float>> trackBuffers_;
    std::map<std::string, juce::AudioBuffer<float>> busBuffers_;
    std::map<std::string, juce::AudioBuffer<float>> sendBuffers_;
    juce::AudioBuffer<float> masterBuffer_;
    
    // Solo/mute state
    std::string soloedTrack_;
    std::set<std::string> mutedTracks_;
    
    // Helper methods
    void updateAudioGraph();
    void createAudioNodes(const std::string& trackId, LogicTrackType type);
    void updateAudioNodeConnections(const std::string& trackId);
    void handleSoloLogic(const std::string& trackId, bool soloed);
    void updateMuteStates();
    
    // Node creation helpers
    std::shared_ptr<AudioNode> createInputNode(LogicTrackType type);
    std::shared_ptr<AudioNode> createOutputNode();
    std::shared_ptr<AudioNode> createGainNode(const std::string& trackId);
    std::shared_ptr<AudioNode> createPluginNode(const std::string& trackId, int slot);
    std::shared_ptr<AudioNode> createSendNode(const std::string& trackId, int sendIndex);
    std::shared_ptr<AudioNode> createBusNode(const std::string& busId);
    std::shared_ptr<AudioNode> createVCANode(const std::string& vcaId);
};

// Logic Pro X-style session manager
class LogicSessionManager {
public:
    LogicSessionManager();
    ~LogicSessionManager() = default;
    
    // Session management
    void createNewSession();
    void loadSession(const std::string& filePath);
    void saveSession(const std::string& filePath);
    
    // Track operations
    std::string addTrack(const std::string& name, LogicTrackType type);
    void deleteTrack(const std::string& trackId);
    void duplicateTrack(const std::string& trackId);
    void moveTrack(const std::string& trackId, int newPosition);
    
    // Mixer operations
    void setTrackVolume(const std::string& trackId, float dB);
    void setTrackPan(const std::string& trackId, float pan);
    void setTrackMute(const std::string& trackId, bool muted);
    void setTrackSolo(const std::string& trackId, bool soloed);
    
    // Plugin operations
    void insertPlugin(const std::string& trackId, int slot, const std::string& pluginId);
    void bypassPlugin(const std::string& trackId, int slot, bool bypassed);
    void removePlugin(const std::string& trackId, int slot);
    
    // Send operations
    void setSendLevel(const std::string& trackId, int sendIndex, float level);
    void setSendTarget(const std::string& trackId, int sendIndex, const std::string& target);
    void setSendPreFader(const std::string& trackId, int sendIndex, bool preFader);
    
    // VCA operations
    std::string createVCA(const std::string& name);
    void deleteVCA(const std::string& vcaId);
    void assignTrackToVCA(const std::string& trackId, const std::string& vcaId);
    void unassignTrackFromVCA(const std::string& trackId);
    
    // Bus operations
    std::string createBus(const std::string& name);
    void deleteBus(const std::string& busId);
    void assignTrackToBus(const std::string& trackId, const std::string& busId);
    void setBusVolume(const std::string& busId, float dB);
    void setBusPan(const std::string& busId, float pan);
    
    // Accessors
    std::shared_ptr<LogicAudioEngine> getAudioEngine() { return audioEngine_; }
    std::shared_ptr<LogicEnvironment> getEnvironment() { return environment_; }
    std::shared_ptr<LogicSmartControls> getSmartControls() { return smartControls_; }
    std::shared_ptr<LogicTrackAlternatives> getTrackAlternatives() { return trackAlternatives_; }
    std::shared_ptr<LogicFlexTime> getFlexTime() { return flexTime_; }
    std::shared_ptr<LogicStepSequencer> getStepSequencer() { return stepSequencer_; }
    std::shared_ptr<LogicScoreEditor> getScoreEditor() { return scoreEditor_; }

private:
    std::shared_ptr<LogicAudioEngine> audioEngine_;
    std::shared_ptr<LogicEnvironment> environment_;
    std::shared_ptr<LogicSmartControls> smartControls_;
    std::shared_ptr<LogicTrackAlternatives> trackAlternatives_;
    std::shared_ptr<LogicFlexTime> flexTime_;
    std::shared_ptr<LogicStepSequencer> stepSequencer_;
    std::shared_ptr<LogicScoreEditor> scoreEditor_;
    
    // Session state
    std::string currentSessionPath_;
    bool hasUnsavedChanges_{false};
    
    // Track management
    int nextTrackId_{1};
    
    // Helper methods
    void markSessionModified();
    void updateAudioEngine();
};

// Logic Pro X-style transport with advanced features
class LogicTransport {
public:
    enum class PlayMode {
        Stop,
        Play,
        Record,
        Loop,
        PunchIn,
        PunchOut
    };
    
    struct TransportState {
        PlayMode playMode{PlayMode::Stop};
        double currentPosition{0.0}; // In samples
        double loopStart{0.0};
        double loopEnd{0.0};
        bool isLooping{false};
        bool isPunching{false};
        double punchIn{0.0};
        double punchOut{0.0};
        
        // Logic-style features
        bool cycleMode{false};
        bool replaceMode{false};
        bool autoPunch{false};
        bool countIn{false};
        int preRollBars{2};
        int postRollBars{2};
    };
    
    LogicTransport();
    ~LogicTransport() = default;
    
    // Transport control
    void play();
    void stop();
    void record();
    void toggleLoop();
    void setLoopRange(double start, double end);
    void setPosition(double position);
    
    // Logic-style features
    void setCycleMode(bool enabled);
    void setReplaceMode(bool enabled);
    void setAutoPunch(bool enabled);
    void setCountIn(bool enabled);
    void setPreRollBars(int bars);
    void setPostRollBars(int bars);
    
    // State access
    const TransportState& getState() const { return state_; }
    void setState(const TransportState& state);
    
    // Position calculation
    double getCurrentPosition() const { return state_.currentPosition; }
    double getLoopLength() const { return state_.loopEnd - state_.loopStart; }
    bool isInLoop() const;
    bool isInPunchRange() const;
    
    // Tempo and time signature
    void setTempo(double bpm);
    void setTimeSignature(int numerator, int denominator);
    double getTempo() const { return tempo_; }
    int getTimeSignatureNumerator() const { return timeSignatureNumerator_; }
    int getTimeSignatureDenominator() const { return timeSignatureDenominator_; }

private:
    TransportState state_;
    
    // Tempo and timing
    double tempo_{120.0};
    int timeSignatureNumerator_{4};
    int timeSignatureDenominator_{4};
    double sampleRate_{44100.0};
    
    // Position tracking
    std::atomic<double> samplePosition_{0.0};
    
    // Helper methods
    void updatePosition();
    bool shouldLoop() const;
    bool shouldPunch() const;
    double samplesToBeats(double samples) const;
    double beatsToSamples(double beats) const;
};

// Logic Pro X-style main application controller
class LogicController {
public:
    LogicController();
    ~LogicController() = default;
    
    // Initialization
    bool initialize(double sampleRate = 44100.0, int bufferSize = 512);
    void shutdown();
    
    // Component access
    std::shared_ptr<LogicSessionManager> getSessionManager() { return sessionManager_; }
    std::shared_ptr<LogicTransport> getTransport() { return transport_; }
    std::shared_ptr<LogicAudioEngine> getAudioEngine() { return audioEngine_; }
    
    // High-level operations
    void createNewProject();
    void loadProject(const std::string& filePath);
    void saveProject(const std::string& filePath);
    
    // Track operations
    std::string addAudioTrack(const std::string& name = "Audio Track");
    std::string addInstrumentTrack(const std::string& name = "Instrument Track");
    std::string addDrumTrack(const std::string& name = "Drums");
    std::string addBus(const std::string& name = "Bus");
    std::string addVCA(const std::string& name = "VCA");
    
    // Mixer operations
    void setTrackVolume(const std::string& trackId, float dB);
    void setTrackPan(const std::string& trackId, float pan);
    void setTrackMute(const std::string& trackId, bool muted);
    void setTrackSolo(const std::string& trackId, bool soloed);
    
    // Plugin operations
    void loadPlugin(const std::string& trackId, const std::string& pluginPath);
    void bypassPlugin(const std::string& trackId, int slot, bool bypassed);
    void removePlugin(const std::string& trackId, int slot);
    
    // Transport operations
    void play();
    void stop();
    void record();
    void setTempo(double bpm);
    
    // Audio processing
    void processAudio(float** outputBuffers, int numChannels, int numSamples);

private:
    std::shared_ptr<LogicSessionManager> sessionManager_;
    std::shared_ptr<LogicTransport> transport_;
    std::shared_ptr<LogicAudioEngine> audioEngine_;
    
    // Audio processing state
    double sampleRate_{44100.0};
    int bufferSize_{512};
    bool initialized_{false};
    
    // Helper methods
    void setupDefaultSession();
    void connectComponents();
};

} // namespace neurato
