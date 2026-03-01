#pragma once

#include "util/Types.hpp"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace ampl {

// Forward declarations
class AudioGraph;
struct AutomationLane;
struct AudioBuffer;

// Parameter definition for automation
struct ParameterInfo {
    std::string id;
    std::string name;
    float minValue{0.0f};
    float maxValue{1.0f};
    float defaultValue{0.0f};
    bool isAutomatable{true};
    std::string unit;
};

// Audio processing node base class
class AudioNode {
public:
    enum class Type {
        TrackInput,
        TrackOutput,
        Gain,
        EQ,
        Compressor,
        Plugin,
        Mixer,
        Automation,
        LatencyCompensator
    };

    AudioNode(Type type, std::string id);
    virtual ~AudioNode() = default;

    // Core processing interface
    virtual void process(AudioBuffer& input, AudioBuffer& output,
                        int numSamples, SampleCount position) noexcept = 0;

    // Node management
    const std::string& getId() const { return id_; }
    Type getType() const { return type_; }

    // Audio I/O configuration
    void setInputChannelCount(int count) { inputChannels_ = count; }
    void setOutputChannelCount(int count) { outputChannels_ = count; }
    int getInputChannelCount() const { return inputChannels_; }
    int getOutputChannelCount() const { return outputChannels_; }

    // Latency management
    virtual int getLatencySamples() const { return latencySamples_; }
    void setLatencySamples(int latency) { latencySamples_ = latency; }

    // Parameter interface
    virtual std::vector<ParameterInfo> getParameters() const { return {}; }
    virtual float getParameterValue(const std::string& paramId) const { (void)paramId; return 0.0f; }
    virtual void setParameterValue(const std::string& paramId, float value) { (void)paramId; (void)value; }

    // Automation
    void setAutomationLane(const std::string& paramId, std::shared_ptr<AutomationLane> lane);
    std::shared_ptr<AutomationLane> getAutomationLane(const std::string& paramId) const;

    // State management
    virtual void prepareToPlay(double sampleRate, int samplesPerBlock) { (void)sampleRate; (void)samplesPerBlock; }
    virtual void reset() {}

    // Bypass
    void setBypassed(bool bypassed) { bypassed_ = bypassed; }
    bool isBypassed() const { return bypassed_; }

protected:
    std::string id_;
    Type type_;
    int inputChannels_{2};
    int outputChannels_{2};
    int latencySamples_{0};
    std::atomic<bool> bypassed_{false};

    std::unordered_map<std::string, std::shared_ptr<AutomationLane>> automationLanes_;
    mutable std::mutex automationMutex_;
};

// Audio buffer wrapper for graph processing
struct AudioBuffer {
    float** channels{nullptr};
    int numChannels{0};
    int numSamples{0};

    AudioBuffer() = default;
    AudioBuffer(float** ch, int numCh, int numSamps)
        : channels(ch), numChannels(numCh), numSamples(numSamps) {}

    void clear() {
        if (channels) {
            for (int ch = 0; ch < numChannels; ++ch) {
                if (channels[ch]) {
                    juce::FloatVectorOperations::fill(channels[ch], 0.0f, numSamples);
                }
            }
        }
    }

    void copyFrom(const AudioBuffer& other) {
        if (numChannels != other.numChannels || numSamples != other.numSamples)
            return;

        for (int ch = 0; ch < numChannels; ++ch) {
            if (channels[ch] && other.channels[ch]) {
                juce::FloatVectorOperations::copy(channels[ch], other.channels[ch], numSamples);
            }
        }
    }

    void addFrom(const AudioBuffer& other) {
        if (numChannels != other.numChannels || numSamples != other.numSamples)
            return;

        for (int ch = 0; ch < numChannels; ++ch) {
            if (channels[ch] && other.channels[ch]) {
                juce::FloatVectorOperations::add(channels[ch], other.channels[ch], numSamples);
            }
        }
    }
};

// Connection between nodes
struct AudioConnection {
    std::string sourceNodeId;
    std::string destNodeId;
    int sourceChannel{-1};  // -1 for all channels
    int destChannel{-1};    // -1 for all channels

    bool isValid() const {
        return !sourceNodeId.empty() && !destNodeId.empty();
    }
};

// Audio processing graph
class AudioGraph {
public:
    AudioGraph();
    ~AudioGraph();

    // Node management
    void addNode(std::shared_ptr<AudioNode> node);
    void removeNode(const std::string& nodeId);
    std::shared_ptr<AudioNode> getNode(const std::string& nodeId);
    std::vector<std::shared_ptr<AudioNode>> getAllNodes() const;

    // Connection management
    bool addConnection(const AudioConnection& connection);
    bool removeConnection(const AudioConnection& connection);
    std::vector<AudioConnection> getConnections() const;

    // Processing
    void process(AudioBuffer& input, AudioBuffer& output,
                int numSamples, SampleCount position) noexcept;

    // Latency compensation
    void updateLatencyCompensation();
    int getTotalLatency() const { return totalLatency_; }

    // Graph validation
    bool isValid() const;
    std::vector<std::string> getValidationErrors() const;

    // State management
    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void reset();

    // Serialization
    struct GraphState {
        std::vector<std::string> nodeIds;
        std::vector<AudioConnection> connections;
        // Node states would be serialized separately
    };

    GraphState getState() const;
    void setState(const GraphState& state);

private:
    struct GraphImpl;
    std::unique_ptr<GraphImpl> impl_;

    std::vector<std::shared_ptr<AudioNode>> nodes_;
    std::vector<AudioConnection> connections_;
    std::unordered_map<std::string, std::vector<AudioConnection>> nodeConnections_;

    int totalLatency_{0};
    double sampleRate_{44100.0};
    int samplesPerBlock_{512};

    mutable std::mutex graphMutex_;

    void topologicalSort();
    std::vector<std::string> getProcessingOrder() const;
    bool hasCycles() const;
};

} // namespace ampl
