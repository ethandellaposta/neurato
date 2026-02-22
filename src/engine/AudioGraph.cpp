#include "AudioGraph.h"
#include "Automation.h"
#include <algorithm>
#include <stdexcept>
#include <queue>
#include <set>

namespace neurato {

// AudioGraph implementation details
struct AudioGraph::GraphImpl {
    std::vector<std::shared_ptr<AudioNode>> nodes;
    std::vector<AudioConnection> connections;
    std::unordered_map<std::string, std::vector<AudioConnection>> nodeConnections;
    std::vector<std::string> processingOrder;
    bool needsReorder{true};
};

// AudioNode implementation
AudioNode::AudioNode(Type type, std::string id) 
    : id_(std::move(id)), type_(type) {
}

void AudioNode::setAutomationLane(const std::string& paramId, std::shared_ptr<AutomationLane> lane) {
    std::lock_guard<std::mutex> lock(automationMutex_);
    automationLanes_[paramId] = lane;
}

std::shared_ptr<AutomationLane> AudioNode::getAutomationLane(const std::string& paramId) const {
    std::lock_guard<std::mutex> lock(automationMutex_);
    auto it = automationLanes_.find(paramId);
    return (it != automationLanes_.end()) ? it->second : nullptr;
}

// AudioGraph implementation
AudioGraph::AudioGraph() : impl_(std::make_unique<GraphImpl>()) {
}

AudioGraph::~AudioGraph() = default;

void AudioGraph::addNode(std::shared_ptr<AudioNode> node) {
    if (!node) return;
    
    std::lock_guard<std::mutex> lock(graphMutex_);
    impl_->nodes.push_back(node);
    impl_->needsReorder = true;
}

void AudioGraph::removeNode(const std::string& nodeId) {
    std::lock_guard<std::mutex> lock(graphMutex_);
    
    // Remove connections to/from this node
    impl_->connections.erase(
        std::remove_if(impl_->connections.begin(), impl_->connections.end(),
            [&nodeId](const AudioConnection& conn) {
                return conn.sourceNodeId == nodeId || conn.destNodeId == nodeId;
            }),
        impl_->connections.end());
    
    // Remove the node
    impl_->nodes.erase(
        std::remove_if(impl_->nodes.begin(), impl_->nodes.end(),
            [&nodeId](const std::shared_ptr<AudioNode>& node) {
                return node->getId() == nodeId;
            }),
        impl_->nodes.end());
    
    impl_->needsReorder = true;
}

std::shared_ptr<AudioNode> AudioGraph::getNode(const std::string& nodeId) {
    std::lock_guard<std::mutex> lock(graphMutex_);
    auto it = std::find_if(impl_->nodes.begin(), impl_->nodes.end(),
        [&nodeId](const std::shared_ptr<AudioNode>& node) {
            return node->getId() == nodeId;
        });
    return (it != impl_->nodes.end()) ? *it : nullptr;
}

std::vector<std::shared_ptr<AudioNode>> AudioGraph::getAllNodes() const {
    std::lock_guard<std::mutex> lock(graphMutex_);
    return impl_->nodes;
}

bool AudioGraph::addConnection(const AudioConnection& connection) {
    if (!connection.isValid()) return false;
    
    std::lock_guard<std::mutex> lock(graphMutex_);
    
    // Check if nodes exist
    auto sourceNode = getNode(connection.sourceNodeId);
    auto destNode = getNode(connection.destNodeId);
    if (!sourceNode || !destNode) return false;
    
    // Check for duplicate connection
    for (const auto& conn : impl_->connections) {
        if (conn.sourceNodeId == connection.sourceNodeId && 
            conn.destNodeId == connection.destNodeId &&
            conn.sourceChannel == connection.sourceChannel &&
            conn.destChannel == connection.destChannel) {
            return false; // Already exists
        }
    }
    
    impl_->connections.push_back(connection);
    impl_->needsReorder = true;
    return true;
}

bool AudioGraph::removeConnection(const AudioConnection& connection) {
    std::lock_guard<std::mutex> lock(graphMutex_);
    
    auto it = std::find_if(impl_->connections.begin(), impl_->connections.end(),
        [&connection](const AudioConnection& conn) {
            return conn.sourceNodeId == connection.sourceNodeId && 
                   conn.destNodeId == connection.destNodeId &&
                   conn.sourceChannel == connection.sourceChannel &&
                   conn.destChannel == connection.destChannel;
        });
    
    if (it != impl_->connections.end()) {
        impl_->connections.erase(it);
        impl_->needsReorder = true;
        return true;
    }
    
    return false;
}

std::vector<AudioConnection> AudioGraph::getConnections() const {
    std::lock_guard<std::mutex> lock(graphMutex_);
    return impl_->connections;
}

void AudioGraph::process(AudioBuffer& input, AudioBuffer& output, 
                       int numSamples, SampleCount position) noexcept {
    std::lock_guard<std::mutex> lock(graphMutex_);
    
    if (impl_->needsReorder) {
        topologicalSort();
        impl_->needsReorder = false;
    }
    
    // Clear output buffer
    output.clear();
    
    // Create temporary buffers for intermediate processing
    std::vector<std::vector<float>> tempBuffers(impl_->nodes.size() * 2);
    std::vector<float*> tempChannelPtrs;
    
    for (auto& buffer : tempBuffers) {
        buffer.resize(numSamples, 0.0f);
    }
    
    for (size_t i = 0; i < tempBuffers.size(); ++i) {
        tempChannelPtrs.push_back(tempBuffers[i].data());
    }
    
    // Process nodes in topological order
    for (const auto& nodeId : impl_->processingOrder) {
        auto node = getNode(nodeId);
        if (!node || node->isBypassed()) continue;
        
        // Find input connections
        AudioBuffer nodeInput;
        AudioBuffer nodeOutput;
        
        // Setup input buffer
        if (nodeId == "input") {
            nodeInput = input;
        } else {
            // Sum inputs from connected nodes
            int inputIdx = 0;
            for (const auto& conn : impl_->connections) {
                if (conn.destNodeId == nodeId) {
                    auto sourceNode = getNode(conn.sourceNodeId);
                    if (sourceNode) {
                        // Find source node's buffer index
                        auto sourceIt = std::find(impl_->processingOrder.begin(), 
                                                impl_->processingOrder.end(), 
                                                conn.sourceNodeId);
                        if (sourceIt != impl_->processingOrder.end()) {
                            int sourceIdx = std::distance(impl_->processingOrder.begin(), sourceIt);
                            AudioBuffer sourceBuffer(&tempChannelPtrs[sourceIdx * 2], 
                                                   sourceNode->getOutputChannelCount(), 
                                                   numSamples);
                            
                            if (inputIdx == 0) {
                                nodeInput = sourceBuffer;
                            } else {
                                nodeInput.addFrom(sourceBuffer);
                            }
                        }
                    }
                    inputIdx++;
                }
            }
        }
        
        // Setup output buffer
        auto nodeIt = std::find(impl_->processingOrder.begin(), 
                               impl_->processingOrder.end(), nodeId);
        if (nodeIt != impl_->processingOrder.end()) {
            int nodeIdx = std::distance(impl_->processingOrder.begin(), nodeIt);
            nodeOutput = AudioBuffer(&tempChannelPtrs[nodeIdx * 2], 
                                   node->getOutputChannelCount(), numSamples);
        }
        
        // Process the node
        node->process(nodeInput, nodeOutput, numSamples, position);
    }
    
    // Copy final output (from output node or last processed node)
    if (!impl_->processingOrder.empty()) {
        auto lastNodeId = impl_->processingOrder.back();
        auto lastNode = getNode(lastNodeId);
        if (lastNode) {
            auto nodeIt = std::find(impl_->processingOrder.begin(), 
                                   impl_->processingOrder.end(), lastNodeId);
            if (nodeIt != impl_->processingOrder.end()) {
                int nodeIdx = std::distance(impl_->processingOrder.begin(), nodeIt);
                AudioBuffer finalBuffer(&tempChannelPtrs[nodeIdx * 2], 
                                      lastNode->getOutputChannelCount(), numSamples);
                output.copyFrom(finalBuffer);
            }
        }
    }
}

void AudioGraph::updateLatencyCompensation() {
    std::lock_guard<std::mutex> lock(graphMutex_);
    
    // Calculate total latency through the graph
    totalLatency_ = 0;
    for (const auto& node : impl_->nodes) {
        totalLatency_ = std::max(totalLatency_, node->getLatencySamples());
    }
    
    // Update delay compensation nodes if needed
    // This would involve inserting delay nodes to align all paths
}

bool AudioGraph::isValid() const {
    std::lock_guard<std::mutex> lock(graphMutex_);
    
    // Check for cycles
    if (hasCycles()) return false;
    
    // Check that all connections reference existing nodes
    for (const auto& conn : impl_->connections) {
        bool sourceExists = std::any_of(impl_->nodes.begin(), impl_->nodes.end(),
            [&conn](const std::shared_ptr<AudioNode>& node) {
                return node->getId() == conn.sourceNodeId;
            });
        
        bool destExists = std::any_of(impl_->nodes.begin(), impl_->nodes.end(),
            [&conn](const std::shared_ptr<AudioNode>& node) {
                return node->getId() == conn.destNodeId;
            });
        
        if (!sourceExists || !destExists) return false;
    }
    
    return true;
}

std::vector<std::string> AudioGraph::getValidationErrors() const {
    std::vector<std::string> errors;
    
    if (hasCycles()) {
        errors.push_back("Graph contains cycles");
    }
    
    // Check for disconnected nodes
    for (const auto& node : impl_->nodes) {
        bool hasInput = false;
        bool hasOutput = false;
        
        for (const auto& conn : impl_->connections) {
            if (conn.destNodeId == node->getId()) hasInput = true;
            if (conn.sourceNodeId == node->getId()) hasOutput = true;
        }
        
        if (!hasInput && node->getType() != AudioNode::Type::TrackInput) {
            errors.push_back("Node " + node->getId() + " has no input connections");
        }
        if (!hasOutput && node->getType() != AudioNode::Type::TrackOutput) {
            errors.push_back("Node " + node->getId() + " has no output connections");
        }
    }
    
    return errors;
}

void AudioGraph::prepareToPlay(double sampleRate, int samplesPerBlock) {
    std::lock_guard<std::mutex> lock(graphMutex_);
    
    sampleRate_ = sampleRate;
    samplesPerBlock_ = samplesPerBlock;
    
    for (auto& node : impl_->nodes) {
        node->prepareToPlay(sampleRate, samplesPerBlock);
    }
    
    updateLatencyCompensation();
}

void AudioGraph::reset() {
    std::lock_guard<std::mutex> lock(graphMutex_);
    
    for (auto& node : impl_->nodes) {
        node->reset();
    }
}

AudioGraph::GraphState AudioGraph::getState() const {
    std::lock_guard<std::mutex> lock(graphMutex_);
    
    GraphState state;
    for (const auto& node : impl_->nodes) {
        state.nodeIds.push_back(node->getId());
    }
    state.connections = impl_->connections;
    
    return state;
}

void AudioGraph::setState(const GraphState& state) {
    std::lock_guard<std::mutex> lock(graphMutex_);
    
    // Clear current state
    impl_->nodes.clear();
    impl_->connections.clear();
    
    // Restore connections (nodes would need to be recreated by caller)
    impl_->connections = state.connections;
    impl_->needsReorder = true;
}

void AudioGraph::topologicalSort() {
    // Kahn's algorithm for topological sorting
    std::unordered_map<std::string, int> inDegree;
    std::queue<std::string> queue;
    std::vector<std::string> result;
    
    // Calculate in-degrees
    for (const auto& node : impl_->nodes) {
        inDegree[node->getId()] = 0;
    }
    
    for (const auto& conn : impl_->connections) {
        inDegree[conn.destNodeId]++;
    }
    
    // Find nodes with no incoming edges
    for (const auto& pair : inDegree) {
        if (pair.second == 0) {
            queue.push(pair.first);
        }
    }
    
    // Process nodes
    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();
        result.push_back(current);
        
        // Remove outgoing edges
        for (const auto& conn : impl_->connections) {
            if (conn.sourceNodeId == current) {
                inDegree[conn.destNodeId]--;
                if (inDegree[conn.destNodeId] == 0) {
                    queue.push(conn.destNodeId);
                }
            }
        }
    }
    
    impl_->processingOrder = result;
}

std::vector<std::string> AudioGraph::getProcessingOrder() const {
    return impl_->processingOrder;
}

bool AudioGraph::hasCycles() const {
    // DFS-based cycle detection
    std::unordered_map<std::string, bool> visited;
    std::unordered_map<std::string, bool> recStack;
    
    std::function<bool(const std::string&)> hasCycleDFS = 
        [&](const std::string& nodeId) -> bool {
        if (!visited[nodeId]) {
            visited[nodeId] = true;
            recStack[nodeId] = true;
            
            // Check all neighbors
            for (const auto& conn : impl_->connections) {
                if (conn.sourceNodeId == nodeId) {
                    if (!visited[conn.destNodeId] && hasCycleDFS(conn.destNodeId)) {
                        return true;
                    } else if (recStack[conn.destNodeId]) {
                        return true;
                    }
                }
            }
        }
        
        recStack[nodeId] = false;
        return false;
    };
    
    for (const auto& node : impl_->nodes) {
        if (hasCycleDFS(node->getId())) {
            return true;
        }
    }
    
    return false;
}

} // namespace neurato
