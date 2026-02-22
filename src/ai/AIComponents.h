#pragma once

#include "util/Types.h"
#include "model/Session.h"
#include "engine/AudioGraph.h"
#include "engine/Automation.h"
#include "engine/PluginHost.h"
#include <vector>
#include <memory>
#include <string>
#include <map>
#include <functional>
#include <variant>

namespace neurato {

// Forward declarations
class Session;
class AudioGraph;
class AutomationManager;

// Session state representation for AI analysis
struct SessionSnapshot {
    // Basic session info
    std::string sessionId;
    std::string name;
    double bpm{120.0};
    int timeSignatureNumerator{4};
    int timeSignatureDenominator{4};
    SampleCount length{0};
    double sampleRate{44100.0};
    
    // Track information
    struct TrackInfo {
        std::string id;
        std::string name;
        bool isMidi{false};
        bool muted{false};
        bool soloed{false};
        float gain{1.0f};
        float pan{0.0f};
        int numClips{0};
        
        // Plugin chain
        std::vector<std::string> pluginIds;
        
        // Automation lanes
        std::vector<std::string> automatedParameters;
    };
    
    std::vector<TrackInfo> tracks;
    
    // Clip information
    struct ClipInfo {
        std::string trackId;
        std::string id;
        SampleCount start{0};
        SampleCount length{0};
        std::string type; // "audio" or "midi"
        
        // Audio clip specific
        std::string audioFilePath;
        float gain{1.0f};
        SampleCount fadeIn{0};
        SampleCount fadeOut{0};
        
        // MIDI clip specific
        int noteCount{0};
        std::vector<int> usedNotes;
    };
    
    std::vector<ClipInfo> clips;
    
    // Plugin information
    struct PluginInfo {
        std::string id;
        std::string name;
        std::string manufacturer;
        std::string type; // "instrument" or "effect"
        std::map<std::string, float> parameters;
        bool bypassed{false};
    };
    
    std::vector<PluginInfo> plugins;
    
    // Automation data
    struct AutomationInfo {
        std::string trackId;
        std::string parameterId;
        std::string parameterName;
        std::vector<std::pair<SampleCount, float>> points;
    };
    
    std::vector<AutomationInfo> automation;
    
    // Mix analysis
    struct MixAnalysis {
        float averageTrackLevel{0.0f};
        float peakTrackLevel{0.0f};
        float headroomDb{-6.0f};
        std::vector<float> trackLevels;
        std::vector<std::string> loudTracks;
        std::vector<std::string> quietTracks;
    };
    
    MixAnalysis mixAnalysis;
    
    // Audio analysis
    struct AudioAnalysis {
        std::vector<float> rmsLevels;
        std::vector<float> peakLevels;
        std::vector<SampleCount> transients;
        std::vector<SampleCount> beatGrid;
        float spectralCentroid{0.0f};
        float spectralRolloff{0.0f};
        float zeroCrossingRate{0.0f};
    };
    
    std::map<std::string, AudioAnalysis> audioAnalysis; // Per track
};

// Action DSL types
namespace ActionDSL {
    
    // Action types
    enum class ActionType {
        CreateTrack,
        DeleteTrack,
        RenameTrack,
        MoveTrack,
        
        AddClip,
        RemoveClip,
        MoveClip,
        ResizeClip,
        TrimClip,
        
        AddPlugin,
        RemovePlugin,
        BypassPlugin,
        SetPluginParameter,
        
        AddAutomationPoint,
        RemoveAutomationPoint,
        MoveAutomationPoint,
        
        SetTrackGain,
        SetTrackPan,
        SetTrackMute,
        SetTrackSolo,
        
        SetTempo,
        SetTimeSignature,
        
        CreateSelection,
        DeleteSelection,
        CopySelection,
        PasteSelection,
        
        MixOperation,
        AnalysisOperation
    };
    
    // Parameter types
    using ParameterValue = std::variant<
        bool,           // mute, solo, bypass
        int,            // track index, plugin index
        float,          // gain, pan, parameter values
        std::string,    // names, paths
        SampleCount,    // timeline positions
        double          // tempo
    >;
    
    using Parameters = std::map<std::string, ParameterValue>;
    
    // Action definition
    struct Action {
        ActionType type;
        Parameters params;
        std::string description;
        float confidence{1.0f};
        bool isInverse{false};
        std::unique_ptr<Action> inverse; // Inverse action for undo
        
        Action(ActionType t, Parameters p = {}, std::string desc = "")
            : type(t), params(std::move(p)), description(std::move(desc)) {}
    };
    
    // Action sequence
    using ActionSequence = std::vector<std::unique_ptr<Action>>;
    
    // Action factory functions
    std::unique_ptr<Action> createTrack(const std::string& name, bool isMidi = false);
    std::unique_ptr<Action> deleteTrack(const std::string& trackId);
    std::unique_ptr<Action> renameTrack(const std::string& trackId, const std::string& newName);
    
    std::unique_ptr<Action> addClip(const std::string& trackId, SampleCount start, 
                                    SampleCount length, const std::string& filePath = "");
    std::unique_ptr<Action> removeClip(const std::string& clipId);
    std::unique_ptr<Action> moveClip(const std::string& clipId, SampleCount newStart);
    std::unique_ptr<Action> resizeClip(const std::string& clipId, SampleCount newLength);
    
    std::unique_ptr<Action> addPlugin(const std::string& trackId, const std::string& pluginId);
    std::unique_ptr<Action> removePlugin(const std::string& trackId, int pluginIndex);
    std::unique_ptr<Action> setPluginParameter(const std::string& trackId, int pluginIndex,
                                              const std::string& paramId, float value);
    
    std::unique_ptr<Action> setTrackGain(const std::string& trackId, float gain);
    std::unique_ptr<Action> setTrackPan(const std::string& trackId, float pan);
    std::unique_ptr<Action> setTrackMute(const std::string& trackId, bool muted);
    std::unique_ptr<Action> setTrackSolo(const std::string& trackId, bool soloed);
    
    std::unique_ptr<Action> addAutomationPoint(const std::string& trackId,
                                               const std::string& paramId,
                                               SampleCount position, float value);
    std::unique_ptr<Action> removeAutomationPoint(const std::string& trackId,
                                                  const std::string& paramId,
                                                  SampleCount position);
    
    // Inverse action generation
    std::unique_ptr<Action> createInverse(const Action& action, const SessionSnapshot& beforeState);
    
    // Action serialization
    std::string serializeAction(const Action& action);
    std::unique_ptr<Action> deserializeAction(const std::string& serialized);
}

// Session State API
class SessionStateAPI {
public:
    SessionStateAPI();
    ~SessionStateAPI() = default;
    
    // Session access
    void setSession(std::shared_ptr<Session> session);
    std::shared_ptr<Session> getSession() const { return session_; }
    
    // Snapshot generation
    SessionSnapshot generateSnapshot() const;
    SessionSnapshot generatePartialSnapshot(const std::vector<std::string>& trackIds) const;
    
    // State comparison
    struct StateDiff {
        std::vector<std::string> addedTracks;
        std::vector<std::string> removedTracks;
        std::vector<std::string> modifiedTracks;
        std::vector<std::string> addedClips;
        std::vector<std::string> removedClips;
        std::vector<std::string> modifiedClips;
        std::vector<std::string> addedPlugins;
        std::vector<std::string> removedPlugins;
        std::vector<std::string> modifiedParameters;
    };
    
    StateDiff compareSnapshots(const SessionSnapshot& before, const SessionSnapshot& after) const;
    
    // Audio analysis
    struct AnalysisOptions {
        bool analyzeRMS{true};
        bool analyzePeaks{true};
        bool analyzeTransients{true};
        bool analyzeBeats{true};
        bool analyzeSpectrum{true};
        int fftSize{2048};
        int hopSize{512};
    };
    
    void analyzeAudio(SessionSnapshot& snapshot, const AnalysisOptions& options = {}) const;
    
    // Mix analysis
    void analyzeMix(SessionSnapshot& snapshot) const;
    
    // State validation
    struct ValidationResult {
        bool isValid{true};
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
    };
    
    ValidationResult validateSnapshot(const SessionSnapshot& snapshot) const;
    
    // State reconstruction (for applying AI suggestions)
    bool applyAction(const ActionDSL::Action& action);
    bool applyActionSequence(const ActionDSL::ActionSequence& actions);
    
    // Event callbacks
    using StateChangeCallback = std::function<void(const SessionSnapshot&)>;
    void setStateChangeCallback(StateChangeCallback callback);

private:
    std::shared_ptr<Session> session_;
    StateChangeCallback stateChangeCallback_;
    
    // Helper methods
    SessionSnapshot::TrackInfo createTrackInfo(const Track& track) const;
    SessionSnapshot::ClipInfo createClipInfo(const Clip& clip) const;
    SessionSnapshot::PluginInfo createPluginInfo(const PluginInstance& plugin) const;
    SessionSnapshot::AutomationInfo createAutomationInfo(const AutomationLane& lane,
                                                          const std::string& trackId,
                                                          const std::string& paramId) const;
    
    // Audio analysis helpers
    std::vector<float> calculateRMS(const float* audio, int numSamples) const;
    std::vector<float> calculatePeaks(const float* audio, int numSamples) const;
    std::vector<SampleCount> detectTransients(const float* audio, int numSamples, 
                                               double sampleRate) const;
    std::vector<SampleCount> detectBeats(const float* audio, int numSamples, 
                                          double sampleRate, double bpm) const;
    std::pair<float, float> analyzeSpectrum(const float* audio, int numSamples, 
                                            int fftSize) const;
};

// AI Planner interface
class AIPlanner {
public:
    struct PlanningRequest {
        std::string naturalLanguageQuery;
        SessionSnapshot currentSnapshot;
        std::vector<std::string> context;
        std::vector<std::string> constraints;
        float confidenceThreshold{0.5f};
    };
    
    struct PlanningResponse {
        ActionDSL::ActionSequence actions;
        std::string explanation;
        float confidence{0.0f};
        std::vector<std::string> alternativeSuggestions;
        std::vector<std::string> warnings;
    };
    
    virtual ~AIPlanner() = default;
    
    // Planning interface
    virtual PlanningResponse planActions(const PlanningRequest& request) = 0;
    virtual bool isAvailable() const = 0;
    virtual std::string getModelInfo() const = 0;
    
    // Training/feedback
    virtual void provideFeedback(const PlanningRequest& request, 
                                const PlanningResponse& response,
                                bool wasHelpful) = 0;
};

// Local inference runtime
class LocalInferenceRuntime {
public:
    enum class ModelType {
        LanguageModel,    // For NL → DSL translation
        AudioAnalysis,    // For audio analysis tasks
        MixAssistant,     // For mix suggestions
        BeatDetection     // For transient/beat detection
    };
    
    struct ModelConfig {
        ModelType type;
        std::string modelPath;
        std::string architecture; // "llama", "onnx", "custom"
        std::map<std::string, std::string> parameters;
    };
    
    LocalInferenceRuntime();
    ~LocalInferenceRuntime();
    
    // Model management
    bool loadModel(const ModelConfig& config);
    void unloadModel(ModelType type);
    bool isModelLoaded(ModelType type) const;
    
    // Inference interface
    std::string runInference(ModelType type, const std::string& input);
    std::vector<float> runAudioInference(ModelType type, const float* audio, 
                                        int numSamples);
    
    // Model information
    std::vector<ModelType> getAvailableModels() const;
    std::string getModelInfo(ModelType type) const;
    
    // Performance
    struct InferenceStats {
        double averageLatencyMs{0.0};
        double maxLatencyMs{0.0};
        int totalInferences{0};
        double memoryUsageMB{0.0};
    };
    
    InferenceStats getStats(ModelType type) const;
    void resetStats(ModelType type);

private:
    class RuntimeImpl;
    std::unique_ptr<RuntimeImpl> impl_;
};

// Mix Assistant
class MixAssistant {
public:
    struct MixRequest {
        SessionSnapshot snapshot;
        std::string targetStyle; // "balanced", "loud", "dynamic", "warm"
        std::vector<std::string> priorityTracks;
        std::map<std::string, float> trackWeights;
        float targetLUFS{-14.0f};
        float maxPeakDb{-1.0f};
    };
    
    struct MixSuggestion {
        std::string trackId;
        std::string parameterId; // "gain", "pan", "eq_band_0_gain", etc.
        float currentValue{0.0f};
        float suggestedValue{0.0f};
        std::string reason;
        float confidence{0.0f};
    };
    
    struct MixResponse {
        std::vector<MixSuggestion> suggestions;
        std::string summary;
        float overallLufs{-14.0f};
        float peakLevel{-1.0f};
        std::vector<std::string> warnings;
    };
    
    MixAssistant(std::shared_ptr<LocalInferenceRuntime> inference);
    ~MixAssistant() = default;
    
    // Analysis interface
    MixResponse analyzeMix(const MixRequest& request);
    
    // Real-time suggestions
    std::vector<MixSuggestion> getSuggestionsForTrack(const std::string& trackId,
                                                       const SessionSnapshot& snapshot);
    
    // Learning
    void learnFromUserAction(const MixSuggestion& suggestion, bool accepted);

private:
    std::shared_ptr<LocalInferenceRuntime> inference_;
    
    // Analysis helpers
    float calculateTrackLUFS(const SessionSnapshot::TrackInfo& track,
                            const SessionSnapshot& snapshot) const;
    std::vector<float> suggestEQCurve(const SessionSnapshot::TrackInfo& track,
                                     const SessionSnapshot::AudioAnalysis& analysis) const;
    float suggestGainReduction(const SessionSnapshot::TrackInfo& track,
                              const SessionSnapshot::AudioAnalysis& analysis) const;
};

// Transient detection and beat grid
class TransientDetector {
public:
    struct Transient {
        SampleCount position;
        float strength;
        float frequency;
    };
    
    struct BeatGrid {
        std::vector<SampleCount> beats;
        std::vector<SampleCount> downbeats;
        double detectedTempo{120.0};
        int timeSignatureNumerator{4};
        int timeSignatureDenominator{4};
        float confidence{0.0f};
    };
    
    TransientDetector();
    ~TransientDetector() = default;
    
    // Detection methods
    std::vector<Transient> detectTransients(const float* audio, int numSamples,
                                           double sampleRate) const;
    BeatGrid detectBeatGrid(const float* audio, int numSamples,
                           double sampleRate, double initialTempo = 120.0) const;
    
    // Real-time detection
    void processRealTime(const float* audio, int numSamples, double sampleRate,
                        std::vector<Transient>& transients);

private:
    class DetectorImpl;
    std::unique_ptr<DetectorImpl> impl_;
};

} // namespace neurato
