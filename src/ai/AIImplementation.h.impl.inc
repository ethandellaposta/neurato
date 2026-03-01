#pragma once

#include "AIComponents.hpp"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ampl
{

class EditPreviewUI;

// Local inference runtime implementation
class LocalInferenceRuntime::RuntimeImpl
{
  public:
    RuntimeImpl();
    ~RuntimeImpl();

    bool loadModel(const ModelConfig &config);
    void unloadModel(ModelType type);
    bool isModelLoaded(ModelType type) const;

    std::string runInference(ModelType type, const std::string &input);
    std::vector<float> runAudioInference(ModelType type, const float *audio, int numSamples);

    std::vector<ModelType> getAvailableModels() const;
    std::string getModelInfo(ModelType type) const;

    InferenceStats getStats(ModelType type) const;
    void resetStats(ModelType type);

  private:
    struct ModelInstance
    {
        ModelType type;
        std::string modelPath;
        std::string architecture;
        bool loaded{false};
        InferenceStats stats;

        // Model-specific data would go here
        void *modelHandle{nullptr};
    };

    std::map<ModelType, ModelInstance> models_;
    mutable std::mutex modelsMutex_;

    // Inference execution
    std::string runLanguageModelInference(const std::string &input);
    std::vector<float> runAudioAnalysisInference(const float *audio, int numSamples);
    std::vector<float> runMixAssistantInference(const std::vector<float> &features);
    std::vector<SampleCount> runBeatDetectionInference(const float *audio, int numSamples);

    // Model loading
    bool loadLlamaModel(const std::string &path);
    bool loadONNXModel(const std::string &path);
    bool loadCustomModel(const std::string &path);

    // Utility functions
    void updateStats(ModelType type, double latencyMs);
    std::vector<float> extractAudioFeatures(const float *audio, int numSamples);
};

// Simple AI Planner implementation
class SimpleAIPlanner : public AIPlanner
{
  public:
    SimpleAIPlanner(std::shared_ptr<LocalInferenceRuntime> inference);
    ~SimpleAIPlanner() override = default;

    PlanningResponse planActions(const PlanningRequest &request) override;
    bool isAvailable() const override;
    std::string getModelInfo() const override;

    void provideFeedback(const PlanningRequest &request, const PlanningResponse &response,
                         bool wasHelpful) override;

  private:
    std::shared_ptr<LocalInferenceRuntime> inference_;

    // Planning strategies
    ActionDSL::ActionSequence planTrackOperations(const std::string &query,
                                                  const SessionSnapshot &snapshot);
    ActionDSL::ActionSequence planMixOperations(const std::string &query,
                                                const SessionSnapshot &snapshot);
    ActionDSL::ActionSequence planAutomationOperations(const std::string &query,
                                                       const SessionSnapshot &snapshot);
    ActionDSL::ActionSequence planPluginOperations(const std::string &query,
                                                   const SessionSnapshot &snapshot);

    // Query analysis
    struct QueryAnalysis
    {
        std::string intent;
        std::vector<std::string> entities;
        std::vector<std::string> parameters;
        float confidence{0.0f};
    };

    QueryAnalysis analyzeQuery(const std::string &query) const;

    // Action generation
    std::unique_ptr<ActionDSL::Action> createActionFromIntent(const QueryAnalysis &analysis,
                                                              const SessionSnapshot &snapshot);

    // Confidence calculation
    float calculateActionConfidence(const ActionDSL::Action &action,
                                    const SessionSnapshot &snapshot) const;
};

// Command Palette UI integration
class CommandPalette
{
  public:
    struct PaletteItem
    {
        std::string id;
        std::string title;
        std::string description;
        std::string category;
        std::vector<std::string> keywords;
        std::function<void()> action;
    };

    CommandPalette();
    ~CommandPalette() = default;

    // Palette management
    void show();
    void hide();
    bool isVisible() const
    {
        return visible_;
    }

    // Item registration
    void registerItem(const PaletteItem &item);
    void removeItem(const std::string &id);
    void clearItems();

    // Search and filtering
    std::vector<PaletteItem> searchItems(const std::string &query) const;
    void setQuery(const std::string &query);

    // Selection
    void selectItem(int index);
    void executeSelected();

    // AI integration
    void setAIPlanner(std::shared_ptr<AIPlanner> planner);
    void setSessionState(std::shared_ptr<SessionStateAPI> sessionState);
    void setEditPreviewUI(std::shared_ptr<EditPreviewUI> editPreviewUI);

    // Events
    using ItemSelectedCallback = std::function<void(const PaletteItem &)>;
    using QueryChangedCallback = std::function<void(const std::string &)>;

    void setItemSelectedCallback(ItemSelectedCallback callback);
    void setQueryChangedCallback(QueryChangedCallback callback);

  private:
    bool visible_{false};
    std::vector<PaletteItem> items_;
    std::string currentQuery_;
    int selectedIndex_{0};

    std::shared_ptr<AIPlanner> aiPlanner_;
    std::shared_ptr<SessionStateAPI> sessionState_;
    std::shared_ptr<EditPreviewUI> editPreviewUI_;

    ItemSelectedCallback itemSelectedCallback_;
    QueryChangedCallback queryChangedCallback_;

    // AI-powered suggestions
    std::vector<PaletteItem> generateAISuggestions(const std::string &query) const;

    // Built-in commands
    void registerBuiltinCommands();

    // UI helpers
    void updateSearchResults();
    void selectNext();
    void selectPrevious();
};

// Preview/Diff UI for AI edits
class EditPreviewUI
{
  public:
    struct EditPreview
    {
        std::string id;
        std::string description;
        ActionDSL::ActionSequence actions;
        SessionSnapshot beforeState;
        SessionSnapshot afterState;
        float confidence{0.0f};
        bool accepted{false};
    };

    EditPreviewUI();
    ~EditPreviewUI() = default;

    // Preview management
    void showPreview(EditPreview preview);
    void hidePreview();
    bool isVisible() const
    {
        return visible_;
    }

    // Preview operations
    void acceptPreview(const std::string &previewId);
    void rejectPreview(const std::string &previewId);
    void applyAllPreviews();
    void rejectAllPreviews();

    // Preview generation
    EditPreview generatePreview(ActionDSL::ActionSequence actions,
                                const SessionSnapshot &currentState);

    // Diff visualization
    struct DiffItem
    {
        std::string type; // "track", "clip", "plugin", "parameter"
        std::string id;
        std::string property;
        std::string oldValue;
        std::string newValue;
        bool isAddition{false};
        bool isDeletion{false};
    };

    std::vector<DiffItem> generateDiff(const SessionSnapshot &before,
                                       const SessionSnapshot &after) const;
    std::vector<DiffItem> getPreviewDiff(const std::string &previewId) const;
    std::string explainPreview(const std::string &previewId) const;
    size_t pendingPreviewCount() const;

    // Events
    using PreviewAcceptedCallback = std::function<void(const EditPreview &)>;
    using PreviewRejectedCallback = std::function<void(const EditPreview &)>;

    void setPreviewAcceptedCallback(PreviewAcceptedCallback callback);
    void setPreviewRejectedCallback(PreviewRejectedCallback callback);

  private:
    bool visible_{false};
    std::map<std::string, EditPreview> previews_;

    PreviewAcceptedCallback previewAcceptedCallback_;
    PreviewRejectedCallback previewRejectedCallback_;

    // Diff formatting
    std::string formatValue(const ActionDSL::ParameterValue &value) const;
    std::string formatTrackChange(const SessionSnapshot::TrackInfo &before,
                                  const SessionSnapshot::TrackInfo &after) const;
    std::string formatClipChange(const SessionSnapshot::ClipInfo &before,
                                 const SessionSnapshot::ClipInfo &after) const;

    // UI helpers
    void updatePreviewList();
    void highlightChanges(const std::string &previewId);
};

// Mix Assistant implementation
class MixAssistantImpl
{
  public:
    MixAssistantImpl(std::shared_ptr<LocalInferenceRuntime> inference);
    ~MixAssistantImpl() = default;

    MixAssistant::MixResponse analyzeMix(const MixAssistant::MixRequest &request);
    std::vector<MixAssistant::MixSuggestion>
    getSuggestionsForTrack(const std::string &trackId, const SessionSnapshot &snapshot);
    void learnFromUserAction(const MixAssistant::MixSuggestion &suggestion, bool accepted);

  private:
    std::shared_ptr<LocalInferenceRuntime> inference_;

    // Analysis methods
    float calculateTrackLUFS(const SessionSnapshot::TrackInfo &track,
                             const SessionSnapshot &snapshot) const;
    std::vector<float> suggestEQCurve(const SessionSnapshot::TrackInfo &track,
                                      const SessionSnapshot::AudioAnalysis &analysis) const;
    float suggestGainReduction(const SessionSnapshot::TrackInfo &track,
                               const SessionSnapshot::AudioAnalysis &analysis) const;

    // Learning system
    struct UserFeedback
    {
        std::string trackId;
        std::string parameterId;
        float suggestedValue;
        float actualValue;
        bool accepted;
        std::chrono::system_clock::time_point timestamp;
    };

    std::vector<UserFeedback> feedbackHistory_;
    mutable std::mutex feedbackMutex_;

    // Machine learning models
    void updateModelsFromFeedback();
    float predictUserPreference(const std::string &trackId, const std::string &parameterId,
                                float suggestedValue) const;
};

// Transient Detector implementation
class TransientDetectorImpl
{
  public:
    TransientDetectorImpl();
    ~TransientDetectorImpl() = default;

    std::vector<TransientDetector::Transient> detectTransients(const float *audio, int numSamples,
                                                               double sampleRate) const;
    TransientDetector::BeatGrid detectBeatGrid(const float *audio, int numSamples,
                                               double sampleRate,
                                               double initialTempo = 120.0) const;

    void processRealTime(const float *audio, int numSamples, double sampleRate,
                         std::vector<TransientDetector::Transient> &transients);

  private:
    // Detection algorithms
    std::vector<float> calculateSpectralFlux(const float *audio, int numSamples, int windowSize,
                                             int hopSize) const;
    std::vector<float> calculateEnergy(const float *audio, int numSamples, int windowSize,
                                       int hopSize) const;
    std::vector<float> calculateHighFrequencyContent(const float *audio, int numSamples,
                                                     int windowSize, int hopSize) const;

    // Peak picking
    std::vector<int> pickPeaks(const std::vector<float> &signal, float threshold,
                               int minDistance) const;

    // Tempo estimation
    double estimateTempo(const std::vector<TransientDetector::Transient> &transients,
                         double sampleRate) const;

    // Beat tracking
    std::vector<SampleCount> trackBeats(const std::vector<TransientDetector::Transient> &transients,
                                        double tempo, double sampleRate) const;

    // Real-time processing state
    std::vector<float> previousFrame_;
    int frameCounter_{0};
    static constexpr int kAnalysisWindowSize = 1024;
    static constexpr int kHopSize = 512;
};

} // namespace ampl
