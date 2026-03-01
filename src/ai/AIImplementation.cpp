#include "AIImplementation.hpp"
#include <algorithm>
#include <chrono>
#include <juce_audio_basics/juce_audio_basics.h>
#include <numeric>
#include <random>
#include <regex>
#include <sstream>

namespace ampl
{

namespace
{

template <typename T> const T *getParam(const ActionDSL::Parameters &params, const std::string &key)
{
    const auto it = params.find(key);
    if (it == params.end())
        return nullptr;
    return std::get_if<T>(&it->second);
}

SessionSnapshot::TrackInfo *findTrackById(SessionSnapshot &snapshot, const std::string &trackId)
{
    auto it = std::find_if(snapshot.tracks.begin(), snapshot.tracks.end(),
                           [&trackId](const SessionSnapshot::TrackInfo &track)
                           { return track.id == trackId; });
    return it == snapshot.tracks.end() ? nullptr : &(*it);
}

SessionSnapshot::ClipInfo *findClipById(SessionSnapshot &snapshot, const std::string &clipId)
{
    auto it = std::find_if(snapshot.clips.begin(), snapshot.clips.end(),
                           [&clipId](const SessionSnapshot::ClipInfo &clip)
                           { return clip.id == clipId; });
    return it == snapshot.clips.end() ? nullptr : &(*it);
}

void applyActionToSnapshot(SessionSnapshot &snapshot, const ActionDSL::Action &action)
{
    switch (action.type)
    {
    case ActionDSL::ActionType::CreateTrack:
    {
        SessionSnapshot::TrackInfo track;
        if (const auto *name = getParam<std::string>(action.params, "name"))
            track.name = *name;
        if (track.name.empty())
            track.name = "AI Track";

        if (const auto *isMidi = getParam<bool>(action.params, "isMidi"))
            track.isMidi = *isMidi;

        track.id = "ai_track_" + std::to_string(snapshot.tracks.size() + 1);
        snapshot.tracks.push_back(track);
        break;
    }
    case ActionDSL::ActionType::DeleteTrack:
    {
        if (const auto *trackId = getParam<std::string>(action.params, "trackId"))
        {
            snapshot.tracks.erase(std::remove_if(snapshot.tracks.begin(), snapshot.tracks.end(),
                                                 [trackId](const SessionSnapshot::TrackInfo &track)
                                                 { return track.id == *trackId; }),
                                  snapshot.tracks.end());

            snapshot.clips.erase(std::remove_if(snapshot.clips.begin(), snapshot.clips.end(),
                                                [trackId](const SessionSnapshot::ClipInfo &clip)
                                                { return clip.trackId == *trackId; }),
                                 snapshot.clips.end());
        }
        break;
    }
    case ActionDSL::ActionType::RenameTrack:
    {
        const auto *trackId = getParam<std::string>(action.params, "trackId");
        const auto *newName = getParam<std::string>(action.params, "newName");
        if (trackId != nullptr && newName != nullptr)
            if (auto *track = findTrackById(snapshot, *trackId))
                track->name = *newName;
        break;
    }
    case ActionDSL::ActionType::SetTrackGain:
    {
        const auto *trackId = getParam<std::string>(action.params, "trackId");
        const auto *gain = getParam<float>(action.params, "gain");
        if (trackId != nullptr && gain != nullptr)
            if (auto *track = findTrackById(snapshot, *trackId))
                track->gain = *gain;
        break;
    }
    case ActionDSL::ActionType::SetTrackPan:
    {
        const auto *trackId = getParam<std::string>(action.params, "trackId");
        const auto *pan = getParam<float>(action.params, "pan");
        if (trackId != nullptr && pan != nullptr)
            if (auto *track = findTrackById(snapshot, *trackId))
                track->pan = *pan;
        break;
    }
    case ActionDSL::ActionType::SetTrackMute:
    {
        const auto *trackId = getParam<std::string>(action.params, "trackId");
        const auto *muted = getParam<bool>(action.params, "muted");
        if (trackId != nullptr && muted != nullptr)
            if (auto *track = findTrackById(snapshot, *trackId))
                track->muted = *muted;
        break;
    }
    case ActionDSL::ActionType::SetTrackSolo:
    {
        const auto *trackId = getParam<std::string>(action.params, "trackId");
        const auto *soloed = getParam<bool>(action.params, "soloed");
        if (trackId != nullptr && soloed != nullptr)
            if (auto *track = findTrackById(snapshot, *trackId))
                track->soloed = *soloed;
        break;
    }
    case ActionDSL::ActionType::AddPlugin:
    {
        const auto *trackId = getParam<std::string>(action.params, "trackId");
        const auto *pluginId = getParam<std::string>(action.params, "pluginId");
        if (trackId != nullptr && pluginId != nullptr)
            if (auto *track = findTrackById(snapshot, *trackId))
                track->pluginIds.push_back(*pluginId);
        break;
    }
    case ActionDSL::ActionType::RemovePlugin:
    {
        const auto *trackId = getParam<std::string>(action.params, "trackId");
        const auto *pluginIndex = getParam<int>(action.params, "pluginIndex");
        if (trackId != nullptr && pluginIndex != nullptr)
        {
            if (auto *track = findTrackById(snapshot, *trackId))
            {
                const auto index = static_cast<size_t>(std::max(0, *pluginIndex));
                if (index < track->pluginIds.size())
                    track->pluginIds.erase(track->pluginIds.begin() + static_cast<long>(index));
            }
        }
        break;
    }
    case ActionDSL::ActionType::AddClip:
    {
        SessionSnapshot::ClipInfo clip;
        if (const auto *trackId = getParam<std::string>(action.params, "trackId"))
            clip.trackId = *trackId;
        if (const auto *start = getParam<SampleCount>(action.params, "start"))
            clip.start = *start;
        if (const auto *length = getParam<SampleCount>(action.params, "length"))
            clip.length = *length;
        clip.id = "ai_clip_" + std::to_string(snapshot.clips.size() + 1);
        clip.type = "audio";
        snapshot.clips.push_back(clip);
        if (auto *track = findTrackById(snapshot, clip.trackId))
            track->numClips += 1;
        break;
    }
    case ActionDSL::ActionType::MoveClip:
    {
        const auto *clipId = getParam<std::string>(action.params, "clipId");
        const auto *newStart = getParam<SampleCount>(action.params, "newStart");
        if (clipId != nullptr && newStart != nullptr)
            if (auto *clip = findClipById(snapshot, *clipId))
                clip->start = *newStart;
        break;
    }
    case ActionDSL::ActionType::ResizeClip:
    {
        const auto *clipId = getParam<std::string>(action.params, "clipId");
        const auto *newLength = getParam<SampleCount>(action.params, "newLength");
        if (clipId != nullptr && newLength != nullptr)
            if (auto *clip = findClipById(snapshot, *clipId))
                clip->length = *newLength;
        break;
    }
    default:
        break;
    }
}

} // namespace

// LocalInferenceRuntime implementation
LocalInferenceRuntime::LocalInferenceRuntime() : impl_(std::make_unique<RuntimeImpl>()) {}

LocalInferenceRuntime::~LocalInferenceRuntime() = default;

bool LocalInferenceRuntime::loadModel(const ModelConfig &config)
{
    return impl_->loadModel(config);
}

void LocalInferenceRuntime::unloadModel(ModelType type)
{
    impl_->unloadModel(type);
}

bool LocalInferenceRuntime::isModelLoaded(ModelType type) const
{
    return impl_->isModelLoaded(type);
}

std::string LocalInferenceRuntime::runInference(ModelType type, const std::string &input)
{
    return impl_->runInference(type, input);
}

std::vector<float> LocalInferenceRuntime::runAudioInference(ModelType type, const float *audio,
                                                            int numSamples)
{
    return impl_->runAudioInference(type, audio, numSamples);
}

std::vector<LocalInferenceRuntime::ModelType> LocalInferenceRuntime::getAvailableModels() const
{
    return impl_->getAvailableModels();
}

std::string LocalInferenceRuntime::getModelInfo(ModelType type) const
{
    return impl_->getModelInfo(type);
}

LocalInferenceRuntime::InferenceStats LocalInferenceRuntime::getStats(ModelType type) const
{
    return impl_->getStats(type);
}

void LocalInferenceRuntime::resetStats(ModelType type)
{
    impl_->resetStats(type);
}

// RuntimeImpl implementation
LocalInferenceRuntime::RuntimeImpl::RuntimeImpl() {}

LocalInferenceRuntime::RuntimeImpl::~RuntimeImpl()
{
    for (auto &pair : models_)
    {
        unloadModel(pair.first);
    }
}

bool LocalInferenceRuntime::RuntimeImpl::loadModel(const ModelConfig &config)
{
    std::lock_guard<std::mutex> lock(modelsMutex_);

    ModelInstance instance;
    instance.type = config.type;
    instance.modelPath = config.modelPath;
    instance.architecture = config.architecture;

    // Load model based on architecture
    bool loaded = false;
    if (config.architecture == "llama")
    {
        loaded = loadLlamaModel(config.modelPath);
    }
    else if (config.architecture == "onnx")
    {
        loaded = loadONNXModel(config.modelPath);
    }
    else if (config.architecture == "custom")
    {
        loaded = loadCustomModel(config.modelPath);
    }

    if (loaded)
    {
        instance.loaded = true;
        models_[config.type] = instance;
        return true;
    }

    return false;
}

void LocalInferenceRuntime::RuntimeImpl::unloadModel(ModelType type)
{
    std::lock_guard<std::mutex> lock(modelsMutex_);

    auto it = models_.find(type);
    if (it != models_.end())
    {
        // Free model resources
        if (it->second.modelHandle)
        {
            // In a real implementation, this would properly free the model
            it->second.modelHandle = nullptr;
        }
        it->second.loaded = false;
        models_.erase(it);
    }
}

bool LocalInferenceRuntime::RuntimeImpl::isModelLoaded(ModelType type) const
{
    std::lock_guard<std::mutex> lock(modelsMutex_);

    auto it = models_.find(type);
    return (it != models_.end() && it->second.loaded);
}

std::string LocalInferenceRuntime::RuntimeImpl::runInference(ModelType type,
                                                             const std::string &input)
{
    auto start = std::chrono::high_resolution_clock::now();

    std::string result;

    switch (type)
    {
    case ModelType::LanguageModel:
        result = runLanguageModelInference(input);
        break;
    default:
        result = "Error: Model type not supported for text inference";
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    updateStats(type, duration.count() / 1000.0);

    return result;
}

std::vector<float> LocalInferenceRuntime::RuntimeImpl::runAudioInference(ModelType type,
                                                                         const float *audio,
                                                                         int numSamples)
{
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<float> result;

    switch (type)
    {
    case ModelType::AudioAnalysis:
        result = runAudioAnalysisInference(audio, numSamples);
        break;
    case ModelType::MixAssistant:
    {
        auto features = extractAudioFeatures(audio, numSamples);
        result = runMixAssistantInference(features);
    }
    break;
    case ModelType::BeatDetection:
    {
        auto transients = runBeatDetectionInference(audio, numSamples);
        result.assign(transients.begin(), transients.end());
    }
    break;
    default:
        result = {0.0f}; // Error indicator
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    updateStats(type, duration.count() / 1000.0);

    return result;
}

std::vector<LocalInferenceRuntime::ModelType>
LocalInferenceRuntime::RuntimeImpl::getAvailableModels() const
{
    std::lock_guard<std::mutex> lock(modelsMutex_);

    std::vector<ModelType> available;
    for (const auto &pair : models_)
    {
        if (pair.second.loaded)
        {
            available.push_back(pair.first);
        }
    }

    return available;
}

std::string LocalInferenceRuntime::RuntimeImpl::getModelInfo(ModelType type) const
{
    std::lock_guard<std::mutex> lock(modelsMutex_);

    auto it = models_.find(type);
    if (it != models_.end())
    {
        std::ostringstream oss;
        oss << "Model: " << it->second.modelPath << ", Architecture: " << it->second.architecture
            << ", Loaded: " << (it->second.loaded ? "Yes" : "No");
        return oss.str();
    }

    return "Model not loaded";
}

LocalInferenceRuntime::InferenceStats
LocalInferenceRuntime::RuntimeImpl::getStats(ModelType type) const
{
    std::lock_guard<std::mutex> lock(modelsMutex_);

    auto it = models_.find(type);
    return (it != models_.end()) ? it->second.stats : InferenceStats{};
}

void LocalInferenceRuntime::RuntimeImpl::resetStats(ModelType type)
{
    std::lock_guard<std::mutex> lock(modelsMutex_);

    auto it = models_.find(type);
    if (it != models_.end())
    {
        it->second.stats = InferenceStats{};
    }
}

std::string LocalInferenceRuntime::RuntimeImpl::runLanguageModelInference(const std::string &input)
{
    // Simplified language model inference
    // In production, this would use a real Llama.cpp or similar model

    // For demonstration, we'll provide rule-based responses
    std::string lowerInput = input;
    std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);

    if (lowerInput.find("create track") != std::string::npos)
    {
        return "Action: create_track, name: 'New Track', isMidi: false";
    }
    else if (lowerInput.find("add plugin") != std::string::npos)
    {
        return "Action: add_plugin, trackId: 'track_1', pluginId: 'reverb'";
    }
    else if (lowerInput.find("set gain") != std::string::npos)
    {
        return "Action: set_track_gain, trackId: 'track_1', gain: 0.8";
    }
    else if (lowerInput.find("mix") != std::string::npos)
    {
        return "Action: mix_operation, type: 'balance', targetLUFS: -14.0";
    }
    else
    {
        return "Action: unknown, error: 'Could not understand request'";
    }
}

std::vector<float> LocalInferenceRuntime::RuntimeImpl::runAudioAnalysisInference(const float *audio,
                                                                                 int numSamples)
{
    // Simplified audio analysis
    std::vector<float> features;

    // Calculate RMS
    float rms = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        rms += audio[i] * audio[i];
    }
    rms = std::sqrt(rms / numSamples);
    features.push_back(rms);

    // Calculate peak
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        peak = std::max(peak, std::abs(audio[i]));
    }
    features.push_back(peak);

    // Calculate zero crossing rate
    int zeroCrossings = 0;
    for (int i = 1; i < numSamples; ++i)
    {
        if ((audio[i] >= 0.0f && audio[i - 1] < 0.0f) || (audio[i] < 0.0f && audio[i - 1] >= 0.0f))
        {
            zeroCrossings++;
        }
    }
    features.push_back(static_cast<float>(zeroCrossings) / numSamples);

    return features;
}

std::vector<float>
LocalInferenceRuntime::RuntimeImpl::runMixAssistantInference(const std::vector<float> &features)
{

    // Simplified mix assistant inference
    std::vector<float> suggestions;

    if (features.size() >= 2)
    {
        float rms = features[0];
        float peak = features[1];

        // Suggest gain adjustment based on RMS
        float targetRMS = 0.2f; // Target RMS level
        float gainSuggestion = targetRMS / (rms + 1e-6f);
        suggestions.push_back(gainSuggestion);

        // Suggest compression if peak is too high
        float compressionSuggestion = (peak > 0.9f) ? 0.7f : 0.0f;
        suggestions.push_back(compressionSuggestion);
    }

    return suggestions;
}

std::vector<SampleCount>
LocalInferenceRuntime::RuntimeImpl::runBeatDetectionInference(const float *audio, int numSamples)
{

    // Simplified beat detection
    std::vector<SampleCount> beats;

    const int windowSize = 1024;
    const int hopSize = 512;

    std::vector<float> energy;
    for (int i = 0; i < numSamples - windowSize; i += hopSize)
    {
        float windowEnergy = 0.0f;
        for (int j = 0; j < windowSize; ++j)
        {
            windowEnergy += audio[i + j] * audio[i + j];
        }
        energy.push_back(windowEnergy);
    }

    // Find peaks in energy
    for (size_t i = 1; i < energy.size() - 1; ++i)
    {
        if (energy[i] > energy[i - 1] * 1.5f && energy[i] > energy[i + 1] * 1.5f)
        {
            beats.push_back(i * hopSize);
        }
    }

    return beats;
}

bool LocalInferenceRuntime::RuntimeImpl::loadLlamaModel(const std::string &path)
{
    // In production, this would load a Llama.cpp model
    // For now, we'll simulate successful loading
    return true;
}

bool LocalInferenceRuntime::RuntimeImpl::loadONNXModel(const std::string &path)
{
    // In production, this would load an ONNX model
    return true;
}

bool LocalInferenceRuntime::RuntimeImpl::loadCustomModel(const std::string &path)
{
    // In production, this would load a custom model
    return true;
}

void LocalInferenceRuntime::RuntimeImpl::updateStats(ModelType type, double latencyMs)
{
    std::lock_guard<std::mutex> lock(modelsMutex_);

    auto &stats = models_[type].stats;
    stats.totalInferences++;

    if (stats.totalInferences == 1)
    {
        stats.averageLatencyMs = latencyMs;
        stats.maxLatencyMs = latencyMs;
    }
    else
    {
        stats.averageLatencyMs =
            (stats.averageLatencyMs * (stats.totalInferences - 1) + latencyMs) /
            stats.totalInferences;
        stats.maxLatencyMs = std::max(stats.maxLatencyMs, latencyMs);
    }
}

std::vector<float> LocalInferenceRuntime::RuntimeImpl::extractAudioFeatures(const float *audio,
                                                                            int numSamples)
{
    return runAudioAnalysisInference(audio, numSamples);
}

// SimpleAIPlanner implementation
SimpleAIPlanner::SimpleAIPlanner(std::shared_ptr<LocalInferenceRuntime> inference)
    : inference_(inference)
{
}

AIPlanner::PlanningResponse SimpleAIPlanner::planActions(const PlanningRequest &request)
{
    PlanningResponse response;

    if (!inference_ || !inference_->isModelLoaded(LocalInferenceRuntime::ModelType::LanguageModel))
    {
        response.confidence = 0.0f;
        response.explanation = "AI model not available";
        return response;
    }

    // Analyze the query
    auto analysis = analyzeQuery(request.naturalLanguageQuery);

    // Generate actions based on intent
    switch (analysis.confidence > 0.5f ? 0 : 1)
    {       // Simplified intent detection
    case 0: // Track operations
        response.actions =
            planTrackOperations(request.naturalLanguageQuery, request.currentSnapshot);
        break;
    case 1: // Mix operations
        response.actions = planMixOperations(request.naturalLanguageQuery, request.currentSnapshot);
        break;
    }

    // Calculate overall confidence
    if (!response.actions.empty())
    {
        response.confidence = analysis.confidence;
        response.explanation = "Generated " + std::to_string(response.actions.size()) + " actions";
    }
    else
    {
        response.confidence = 0.0f;
        response.explanation = "Could not generate actions from query";
    }

    return response;
}

bool SimpleAIPlanner::isAvailable() const
{
    return inference_ && inference_->isModelLoaded(LocalInferenceRuntime::ModelType::LanguageModel);
}

std::string SimpleAIPlanner::getModelInfo() const
{
    if (inference_)
    {
        return inference_->getModelInfo(LocalInferenceRuntime::ModelType::LanguageModel);
    }
    return "No model loaded";
}

void SimpleAIPlanner::provideFeedback(const PlanningRequest &request,
                                      const PlanningResponse &response, bool wasHelpful)
{
    // In a real implementation, this would update the model
    // For now, we'll just log the feedback
    juce::Logger::writeToLog("AI Feedback: " + std::string(wasHelpful ? "Helpful" : "Not helpful"));
}

SimpleAIPlanner::QueryAnalysis SimpleAIPlanner::analyzeQuery(const std::string &query) const
{
    QueryAnalysis analysis;

    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    // Simple keyword-based intent detection
    if (lowerQuery.find("track") != std::string::npos ||
        lowerQuery.find("create") != std::string::npos)
    {
        analysis.intent = "track_operations";
        analysis.confidence = 0.8f;
    }
    else if (lowerQuery.find("mix") != std::string::npos ||
             lowerQuery.find("gain") != std::string::npos)
    {
        analysis.intent = "mix_operations";
        analysis.confidence = 0.7f;
    }
    else if (lowerQuery.find("plugin") != std::string::npos ||
             lowerQuery.find("effect") != std::string::npos)
    {
        analysis.intent = "plugin_operations";
        analysis.confidence = 0.6f;
    }
    else
    {
        analysis.intent = "unknown";
        analysis.confidence = 0.1f;
    }

    return analysis;
}

ActionDSL::ActionSequence SimpleAIPlanner::planTrackOperations(const std::string &query,
                                                               const SessionSnapshot &snapshot)
{
    ActionDSL::ActionSequence actions;

    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    if (lowerQuery.find("create") != std::string::npos)
    {
        auto action = ActionDSL::createTrack("New Track");
        actions.push_back(std::move(action));
    }

    return actions;
}

ActionDSL::ActionSequence SimpleAIPlanner::planMixOperations(const std::string &query,
                                                             const SessionSnapshot &snapshot)
{
    ActionDSL::ActionSequence actions;

    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    if (lowerQuery.find("gain") != std::string::npos && !snapshot.tracks.empty())
    {
        auto action = ActionDSL::setTrackGain(snapshot.tracks[0].id, 0.8f);
        actions.push_back(std::move(action));
    }

    return actions;
}

ActionDSL::ActionSequence SimpleAIPlanner::planAutomationOperations(const std::string &query,
                                                                    const SessionSnapshot &snapshot)
{
    ActionDSL::ActionSequence actions;
    // Implementation for automation operations
    return actions;
}

ActionDSL::ActionSequence SimpleAIPlanner::planPluginOperations(const std::string &query,
                                                                const SessionSnapshot &snapshot)
{
    ActionDSL::ActionSequence actions;

    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    if (lowerQuery.find("add plugin") != std::string::npos && !snapshot.tracks.empty())
    {
        auto action = ActionDSL::addPlugin(snapshot.tracks[0].id, "reverb_plugin");
        actions.push_back(std::move(action));
    }

    return actions;
}

// CommandPalette implementation
CommandPalette::CommandPalette()
{
    registerBuiltinCommands();
}

void CommandPalette::show()
{
    visible_ = true;
    selectedIndex_ = 0;
    updateSearchResults();
}

void CommandPalette::hide()
{
    visible_ = false;
    currentQuery_.clear();
}

void CommandPalette::registerItem(const PaletteItem &item)
{
    items_.push_back(item);
}

void CommandPalette::removeItem(const std::string &id)
{
    items_.erase(std::remove_if(items_.begin(), items_.end(),
                                [&id](const PaletteItem &item) { return item.id == id; }),
                 items_.end());
}

void CommandPalette::clearItems()
{
    items_.clear();
}

std::vector<CommandPalette::PaletteItem> CommandPalette::searchItems(const std::string &query) const
{
    std::vector<PaletteItem> results;

    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    for (const auto &item : items_)
    {
        std::string lowerTitle = item.title;
        std::transform(lowerTitle.begin(), lowerTitle.end(), lowerTitle.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        std::string lowerDescription = item.description;
        std::transform(lowerDescription.begin(), lowerDescription.end(), lowerDescription.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        // Check if query matches title or description
        if (lowerTitle.find(lowerQuery) != std::string::npos ||
            lowerDescription.find(lowerQuery) != std::string::npos)
        {
            results.push_back(item);
        }

        // Check keywords
        for (const auto &keyword : item.keywords)
        {
            std::string lowerKeyword = keyword;
            std::transform(lowerKeyword.begin(), lowerKeyword.end(), lowerKeyword.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            if (lowerKeyword.find(lowerQuery) != std::string::npos)
            {
                results.push_back(item);
                break;
            }
        }
    }

    // Add AI suggestions if available
    if (aiPlanner_ && !query.empty())
    {
        auto aiSuggestions = generateAISuggestions(query);
        results.insert(results.end(), aiSuggestions.begin(), aiSuggestions.end());
    }

    return results;
}

void CommandPalette::setQuery(const std::string &query)
{
    currentQuery_ = query;
    selectedIndex_ = 0;
    updateSearchResults();

    if (queryChangedCallback_)
    {
        queryChangedCallback_(query);
    }
}

void CommandPalette::selectItem(int index)
{
    auto results = searchItems(currentQuery_);
    if (index >= 0 && index < static_cast<int>(results.size()))
    {
        selectedIndex_ = index;

        if (itemSelectedCallback_)
        {
            itemSelectedCallback_(results[index]);
        }
    }
}

void CommandPalette::executeSelected()
{
    auto results = searchItems(currentQuery_);
    if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(results.size()))
    {
        if (results[selectedIndex_].action)
        {
            results[selectedIndex_].action();
        }
    }
}

void CommandPalette::setAIPlanner(std::shared_ptr<AIPlanner> planner)
{
    aiPlanner_ = planner;
}

void CommandPalette::setSessionState(std::shared_ptr<SessionStateAPI> sessionState)
{
    sessionState_ = sessionState;
}

void CommandPalette::setEditPreviewUI(std::shared_ptr<EditPreviewUI> editPreviewUI)
{
    editPreviewUI_ = editPreviewUI;

    if (editPreviewUI_)
    {
        editPreviewUI_->setPreviewAcceptedCallback(
            [this](const EditPreviewUI::EditPreview &preview)
            {
                if (sessionState_)
                {
                    sessionState_->applyActionSequence(preview.actions);
                }
            });
    }
}

void CommandPalette::setItemSelectedCallback(ItemSelectedCallback callback)
{
    itemSelectedCallback_ = callback;
}

void CommandPalette::setQueryChangedCallback(QueryChangedCallback callback)
{
    queryChangedCallback_ = callback;
}

std::vector<CommandPalette::PaletteItem>
CommandPalette::generateAISuggestions(const std::string &query) const
{
    std::vector<PaletteItem> suggestions;

    if (!aiPlanner_ || !sessionState_)
    {
        return suggestions;
    }

    AIPlanner::PlanningRequest request;
    request.naturalLanguageQuery = query;
    request.currentSnapshot = sessionState_->generateSnapshot();

    auto response = aiPlanner_->planActions(request);
    if (response.actions.empty())
    {
        return suggestions;
    }

    const auto confidencePercent = static_cast<int>(std::round(response.confidence * 100.0f));

    PaletteItem item;
    item.id = "ai_preview_" + std::to_string(suggestions.size());
    item.title = response.actions.front()->description;
    item.description = "Preview required • Confidence " + std::to_string(confidencePercent) +
                       "% • " + response.explanation;
    item.category = "AI Suggestion";

    item.action = [this, query]()
    {
        if (!aiPlanner_ || !sessionState_ || !editPreviewUI_)
        {
            return;
        }

        AIPlanner::PlanningRequest latestRequest;
        latestRequest.naturalLanguageQuery = query;
        latestRequest.currentSnapshot = sessionState_->generateSnapshot();

        auto latestResponse = aiPlanner_->planActions(latestRequest);
        if (latestResponse.actions.empty())
        {
            return;
        }

        auto preview = editPreviewUI_->generatePreview(std::move(latestResponse.actions),
                                                       latestRequest.currentSnapshot);
        preview.description = latestResponse.explanation;
        preview.confidence = std::max(preview.confidence, latestResponse.confidence);
        editPreviewUI_->showPreview(std::move(preview));
    };

    suggestions.push_back(item);

    return suggestions;
}

void CommandPalette::registerBuiltinCommands()
{
    // File operations
    registerItem({"new_project",
                  "New Project",
                  "Create a new empty project",
                  "File",
                  {"new", "project", "create"},
                  []() { juce::Logger::writeToLog("New Project command"); }});

    registerItem({"open_project",
                  "Open Project",
                  "Open an existing project file",
                  "File",
                  {"open", "project", "load"},
                  []() { juce::Logger::writeToLog("Open Project command"); }});

    registerItem({"save_project",
                  "Save Project",
                  "Save the current project",
                  "File",
                  {"save", "project"},
                  []() { juce::Logger::writeToLog("Save Project command"); }});

    // Track operations
    registerItem({"add_track",
                  "Add Track",
                  "Add a new audio track",
                  "Track",
                  {"add", "track", "create"},
                  []() { juce::Logger::writeToLog("Add Track command"); }});

    registerItem({"add_midi_track",
                  "Add MIDI Track",
                  "Add a new MIDI track",
                  "Track",
                  {"add", "midi", "track", "create"},
                  []() { juce::Logger::writeToLog("Add MIDI Track command"); }});

    // Transport operations
    registerItem({"play_stop",
                  "Play/Stop",
                  "Toggle playback",
                  "Transport",
                  {"play", "stop", "toggle"},
                  []() { juce::Logger::writeToLog("Play/Stop command"); }});

    registerItem({"record",
                  "Record",
                  "Start recording",
                  "Transport",
                  {"record", "rec"},
                  []() { juce::Logger::writeToLog("Record command"); }});
}

void CommandPalette::updateSearchResults()
{
    // This would update the UI with search results
    // Implementation depends on the UI framework
}

// EditPreviewUI implementation
EditPreviewUI::EditPreviewUI() {}

void EditPreviewUI::showPreview(EditPreview preview)
{
    previews_[preview.id] = std::move(preview);
    visible_ = true;
    updatePreviewList();
}

void EditPreviewUI::hidePreview()
{
    visible_ = false;
}

void EditPreviewUI::acceptPreview(const std::string &previewId)
{
    auto it = previews_.find(previewId);
    if (it != previews_.end())
    {
        it->second.accepted = true;

        if (previewAcceptedCallback_)
        {
            previewAcceptedCallback_(it->second);
        }
    }
}

void EditPreviewUI::rejectPreview(const std::string &previewId)
{
    auto it = previews_.find(previewId);
    if (it != previews_.end())
    {
        if (previewRejectedCallback_)
        {
            previewRejectedCallback_(it->second);
        }

        previews_.erase(it);
    }
}

void EditPreviewUI::applyAllPreviews()
{
    for (auto &pair : previews_)
    {
        pair.second.accepted = true;

        if (previewAcceptedCallback_)
        {
            previewAcceptedCallback_(pair.second);
        }
    }
}

void EditPreviewUI::rejectAllPreviews()
{
    previews_.clear();
}

EditPreviewUI::EditPreview EditPreviewUI::generatePreview(ActionDSL::ActionSequence actions,
                                                          const SessionSnapshot &currentState)
{
    EditPreview preview;
    preview.id =
        "preview_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    preview.beforeState = currentState;

    preview.afterState = currentState;
    float confidenceTotal = 0.0f;
    int confidenceCount = 0;

    for (const auto &action : actions)
    {
        if (action)
        {
            applyActionToSnapshot(preview.afterState, *action);
            confidenceTotal += action->confidence;
            ++confidenceCount;
        }
    }

    preview.description = "Apply " + std::to_string(actions.size()) + " AI-assisted edits";
    preview.confidence =
        confidenceCount > 0 ? confidenceTotal / static_cast<float>(confidenceCount) : 0.0f;
    preview.actions = std::move(actions);

    return preview;
}

std::vector<EditPreviewUI::DiffItem> EditPreviewUI::generateDiff(const SessionSnapshot &before,
                                                                 const SessionSnapshot &after) const
{
    std::vector<DiffItem> diffs;

    // Compare tracks
    for (const auto &afterTrack : after.tracks)
    {
        auto beforeIt = std::find_if(before.tracks.begin(), before.tracks.end(),
                                     [&afterTrack](const SessionSnapshot::TrackInfo &track)
                                     { return track.id == afterTrack.id; });

        if (beforeIt == before.tracks.end())
        {
            // Track added
            DiffItem diff;
            diff.type = "track";
            diff.id = afterTrack.id;
            diff.property = "name";
            diff.newValue = afterTrack.name;
            diff.isAddition = true;
            diffs.push_back(diff);
        }
        else
        {
            // Track modified
            if (beforeIt->name != afterTrack.name)
            {
                DiffItem diff;
                diff.type = "track";
                diff.id = afterTrack.id;
                diff.property = "name";
                diff.oldValue = beforeIt->name;
                diff.newValue = afterTrack.name;
                diffs.push_back(diff);
            }

            if (beforeIt->gain != afterTrack.gain)
            {
                DiffItem diff;
                diff.type = "track";
                diff.id = afterTrack.id;
                diff.property = "gain";
                diff.oldValue = std::to_string(beforeIt->gain);
                diff.newValue = std::to_string(afterTrack.gain);
                diffs.push_back(diff);
            }

            if (beforeIt->pan != afterTrack.pan)
            {
                DiffItem diff;
                diff.type = "track";
                diff.id = afterTrack.id;
                diff.property = "pan";
                diff.oldValue = std::to_string(beforeIt->pan);
                diff.newValue = std::to_string(afterTrack.pan);
                diffs.push_back(diff);
            }

            if (beforeIt->muted != afterTrack.muted)
            {
                DiffItem diff;
                diff.type = "track";
                diff.id = afterTrack.id;
                diff.property = "muted";
                diff.oldValue = beforeIt->muted ? "true" : "false";
                diff.newValue = afterTrack.muted ? "true" : "false";
                diffs.push_back(diff);
            }

            if (beforeIt->soloed != afterTrack.soloed)
            {
                DiffItem diff;
                diff.type = "track";
                diff.id = afterTrack.id;
                diff.property = "soloed";
                diff.oldValue = beforeIt->soloed ? "true" : "false";
                diff.newValue = afterTrack.soloed ? "true" : "false";
                diffs.push_back(diff);
            }
        }
    }

    for (const auto &beforeTrack : before.tracks)
    {
        auto afterIt = std::find_if(after.tracks.begin(), after.tracks.end(),
                                    [&beforeTrack](const SessionSnapshot::TrackInfo &track)
                                    { return track.id == beforeTrack.id; });
        if (afterIt == after.tracks.end())
        {
            DiffItem diff;
            diff.type = "track";
            diff.id = beforeTrack.id;
            diff.property = "deleted";
            diff.oldValue = beforeTrack.name;
            diff.isDeletion = true;
            diffs.push_back(diff);
        }
    }

    return diffs;
}

std::vector<EditPreviewUI::DiffItem>
EditPreviewUI::getPreviewDiff(const std::string &previewId) const
{
    const auto it = previews_.find(previewId);
    if (it == previews_.end())
    {
        return {};
    }

    return generateDiff(it->second.beforeState, it->second.afterState);
}

std::string EditPreviewUI::explainPreview(const std::string &previewId) const
{
    const auto it = previews_.find(previewId);
    if (it == previews_.end())
    {
        return "Preview not found";
    }

    const auto &preview = it->second;
    const auto diff = generateDiff(preview.beforeState, preview.afterState);

    std::ostringstream oss;
    oss << "Preview: " << preview.description
        << " | Confidence: " << static_cast<int>(std::round(preview.confidence * 100.0f)) << "%"
        << " | Actions: " << preview.actions.size() << " | Changes: " << diff.size();

    if (!diff.empty())
    {
        oss << " | First change: " << diff.front().type << "." << diff.front().property;
    }

    return oss.str();
}

size_t EditPreviewUI::pendingPreviewCount() const
{
    return previews_.size();
}

void EditPreviewUI::setPreviewAcceptedCallback(PreviewAcceptedCallback callback)
{
    previewAcceptedCallback_ = callback;
}

void EditPreviewUI::setPreviewRejectedCallback(PreviewRejectedCallback callback)
{
    previewRejectedCallback_ = callback;
}

std::string EditPreviewUI::formatValue(const ActionDSL::ParameterValue &value) const
{
    if (std::holds_alternative<bool>(value))
    {
        return std::get<bool>(value) ? "true" : "false";
    }
    else if (std::holds_alternative<int>(value))
    {
        return std::to_string(std::get<int>(value));
    }
    else if (std::holds_alternative<float>(value))
    {
        return std::to_string(std::get<float>(value));
    }
    else if (std::holds_alternative<std::string>(value))
    {
        return std::get<std::string>(value);
    }
    else if (std::holds_alternative<SampleCount>(value))
    {
        return std::to_string(std::get<SampleCount>(value));
    }
    else if (std::holds_alternative<double>(value))
    {
        return std::to_string(std::get<double>(value));
    }
    return "unknown";
}

void EditPreviewUI::updatePreviewList()
{
    // This would update the UI with the preview list
    // Implementation depends on the UI framework
}

void EditPreviewUI::highlightChanges(const std::string &previewId)
{
    // This would highlight changes in the UI
    // Implementation depends on the UI framework
}

} // namespace ampl
