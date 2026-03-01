#include "SandboxHost.hpp"
#include "engine/plugins/manager/PluginManager.hpp"
#include <juce_core/juce_core.h>
#include <iostream>
#include <chrono>

namespace ampl {

// SandboxIPC implementation
class SandboxIPC::IPCImpl {
public:
    IPCImpl() = default;
    ~IPCImpl() = default;

    bool initialize(bool isHost) {
        isHost_ = isHost;

        // Initialize shared memory for audio
        // In a real implementation, this would use platform-specific shared memory
        // For now, we'll use a simple in-memory buffer
        sharedAudioBuffer_.resize(2 * 4096); // 2 channels, 4096 samples max
        processAlive_ = true;
        lastHeartbeat_ = getCurrentTimeMs();

        return true;
    }

    void shutdown() {
        processAlive_ = false;
        messageQueue_ = {}; // Clear the queue
    }

    bool sendMessage(const Message& message) {
        std::lock_guard<std::mutex> lock(queueMutex_);

        if (!processAlive_) return false;

        messageQueue_.push(message);
        return true;
    }

    bool receiveMessage(Message& message, int timeoutMs) {
        std::unique_lock<std::mutex> lock(queueMutex_);

        auto timeout = std::chrono::steady_clock::now() +
                      std::chrono::milliseconds(timeoutMs);

        while (messageQueue_.empty() &&
               std::chrono::steady_clock::now() < timeout) {
            queueCondition_.wait_for(lock, std::chrono::milliseconds(10));
        }

        if (messageQueue_.empty()) {
            return false;
        }

        message = messageQueue_.front();
        messageQueue_.pop();
        return true;
    }

    bool setupSharedAudioBuffer(int numChannels, int bufferSize) {
        sharedAudioBuffer_.resize(numChannels * bufferSize);
        return true;
    }

    float* getSharedAudioBuffer() {
        return sharedAudioBuffer_.data();
    }

    const float* getSharedAudioBuffer() const {
        return sharedAudioBuffer_.data();
    }

    bool isProcessAlive() const {
        return processAlive_;
    }

    void sendHeartbeat() {
        lastHeartbeat_ = getCurrentTimeMs();
    }

    bool checkHeartbeat() {
        int64_t currentTime = getCurrentTimeMs();
        return (currentTime - lastHeartbeat_) < 5000; // 5 second timeout
    }

private:
    bool isHost_{false};
    std::atomic<bool> processAlive_{true};
    std::atomic<int64_t> lastHeartbeat_{0};

    std::queue<Message> messageQueue_;
    mutable std::mutex queueMutex_;
    std::condition_variable queueCondition_;

    std::vector<float> sharedAudioBuffer_;

    static int64_t getCurrentTimeMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
};

SandboxIPC::SandboxIPC() : impl_(std::make_unique<IPCImpl>()) {
}

SandboxIPC::~SandboxIPC() = default;

bool SandboxIPC::initialize(bool isHost) {
    return impl_->initialize(isHost);
}

void SandboxIPC::shutdown() {
    impl_->shutdown();
}

bool SandboxIPC::sendMessage(const Message& message) {
    return impl_->sendMessage(message);
}

bool SandboxIPC::receiveMessage(Message& message, int timeoutMs) {
    return impl_->receiveMessage(message, timeoutMs);
}

bool SandboxIPC::setupSharedAudioBuffer(int numChannels, int bufferSize) {
    return impl_->setupSharedAudioBuffer(numChannels, bufferSize);
}

float* SandboxIPC::getSharedAudioBuffer() {
    return impl_->getSharedAudioBuffer();
}

const float* SandboxIPC::getSharedAudioBuffer() const {
    return impl_->getSharedAudioBuffer();
}

bool SandboxIPC::isProcessAlive() const {
    return impl_->isProcessAlive();
}

void SandboxIPC::sendHeartbeat() {
    impl_->sendHeartbeat();
}

bool SandboxIPC::checkHeartbeat() {
    return impl_->checkHeartbeat();
}

// SandboxProcess implementation
class SandboxProcess::ProcessImpl {
public:
    ProcessImpl() = default;
    ~ProcessImpl() = default;

    bool start(const std::string& executablePath) {
        // In a real implementation, this would launch a separate process
        // For now, we'll simulate it
        running_ = true;
        crashed_ = false;
        crashLog_.clear();

        // Start monitoring thread
        monitorThread_ = std::thread([this]() {
            while (running_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                // Simulate random crashes for testing
                if (rand() % 10000 == 0) { // Very rare crash
                    crashed_ = true;
                    running_ = false;
                    crashLog_ = "Simulated plugin crash";
                }
            }
        });

        return true;
    }

    void stop() {
        running_ = false;
        if (monitorThread_.joinable()) {
            monitorThread_.join();
        }
    }

    bool isRunning() const {
        return running_ && !crashed_;
    }

    bool hasCrashed() const {
        return crashed_;
    }

    std::string getCrashLog() const {
        return crashLog_;
    }

private:
    std::atomic<bool> running_{false};
    std::atomic<bool> crashed_{false};
    std::string crashLog_;
    std::thread monitorThread_;
};

SandboxProcess::SandboxProcess() : impl_(std::make_unique<ProcessImpl>()) {
}

SandboxProcess::~SandboxProcess() {
    stop();
}

bool SandboxProcess::start(const std::string& executablePath) {
    return impl_->start(executablePath);
}

void SandboxProcess::stop() {
    impl_->stop();
}

bool SandboxProcess::isRunning() const {
    return impl_->isRunning();
}

bool SandboxProcess::hasCrashed() const {
    return impl_->hasCrashed();
}

std::string SandboxProcess::getCrashLog() const {
    return impl_->getCrashLog();
}

// SandboxPluginHost implementation
SandboxPluginHost::SandboxPluginHost() : impl_(std::make_unique<SandboxImpl>()) {
}

SandboxPluginHost::~SandboxPluginHost() {
    stopSandbox();
}

bool SandboxPluginHost::startSandbox() {
    return impl_->initialize();
}

void SandboxPluginHost::stopSandbox() {
    impl_->shutdown();
}

bool SandboxPluginHost::isSandboxRunning() const {
    return impl_->isRunning();
}

bool SandboxPluginHost::loadPlugin(const std::string& pluginId, const std::string& filePath) {
    return impl_->loadPlugin(pluginId, filePath);
}

void SandboxPluginHost::unloadPlugin() {
    impl_->unloadPlugin();
}

bool SandboxPluginHost::processAudio(const float* input, float* output,
                                   int numSamples, int numChannels) {
    return impl_->processAudio(input, output, numSamples, numChannels);
}

bool SandboxPluginHost::setParameter(const std::string& paramId, float value) {
    return impl_->setParameter(paramId, value);
}

float SandboxPluginHost::getParameter(const std::string& paramId) const {
    return impl_->getParameter(paramId);
}

bool SandboxPluginHost::saveState(std::vector<uint8_t>& stateData) {
    return impl_->saveState(stateData);
}

bool SandboxPluginHost::loadState(const std::vector<uint8_t>& stateData) {
    return impl_->loadState(stateData);
}

bool SandboxPluginHost::hasCrashed() const {
    return impl_->hasCrashed();
}

std::string SandboxPluginHost::getCrashLog() const {
    return impl_->getCrashLog();
}

void SandboxPluginHost::restartSandbox() {
    impl_->restart();
}

// SandboxImpl implementation
SandboxPluginHost::SandboxImpl::SandboxImpl() {
    tempInputBuffer_.resize(maxChannels_ * maxBufferSize_);
    tempOutputBuffer_.resize(maxChannels_ * maxBufferSize_);
}

SandboxPluginHost::SandboxImpl::~SandboxImpl() {
    shutdown();
}

bool SandboxPluginHost::SandboxImpl::initialize() {
    process_ = std::make_unique<SandboxProcess>();
    ipc_ = std::make_unique<SandboxIPC>();

    // Initialize IPC
    if (!ipc_->initialize(true)) {
        return false;
    }

    // Start sandbox process
    std::string sandboxPath = SandboxUtils::getSandboxExecutablePath();
    if (!process_->start(sandboxPath)) {
        return false;
    }

    // Initialize IPC in process
    ipc_->setupSharedAudioBuffer(maxChannels_, maxBufferSize_);

    // Start heartbeat monitoring
    heartbeatRunning_ = true;
    heartbeatThread_ = std::thread([this]() {
        monitorHeartbeat();
    });

    return true;
}

void SandboxPluginHost::SandboxImpl::shutdown() {
    heartbeatRunning_ = false;
    if (heartbeatThread_.joinable()) {
        heartbeatThread_.join();
    }

    if (process_) {
        SandboxIPC::Message shutdownMsg(SandboxIPC::MessageType::Shutdown);
        ipc_->sendMessage(shutdownMsg);

        process_->stop();
    }

    if (ipc_) {
        ipc_->shutdown();
    }

    pluginLoaded_ = false;
    currentPluginId_.clear();
}

bool SandboxPluginHost::SandboxImpl::loadPlugin(const std::string& pluginId,
                                               const std::string& filePath) {
    SandboxIPC::Message request(SandboxIPC::MessageType::LoadPlugin);
    request.data = pluginId + "|" + filePath;

    SandboxIPC::Message response;
    if (!sendAndWaitForResponse(request, response,
                               SandboxIPC::MessageType::Heartbeat, 10000)) {
        return false;
    }

    pluginLoaded_ = true;
    currentPluginId_ = pluginId;
    return true;
}

void SandboxPluginHost::SandboxImpl::unloadPlugin() {
    if (!pluginLoaded_) return;

    SandboxIPC::Message request(SandboxIPC::MessageType::UnloadPlugin);

    SandboxIPC::Message response;
    sendAndWaitForResponse(request, response,
                          SandboxIPC::MessageType::Heartbeat, 5000);

    pluginLoaded_ = false;
    currentPluginId_.clear();
}

bool SandboxPluginHost::SandboxImpl::processAudio(const float* input, float* output,
                                                 int numSamples, int numChannels) {
    if (!pluginLoaded_) return false;

    // Copy input to shared buffer
    float* sharedBuffer = ipc_->getSharedAudioBuffer();
    std::copy(input, input + numSamples * numChannels, sharedBuffer);

    SandboxIPC::Message request(SandboxIPC::MessageType::ProcessAudio);
    request.numSamples = numSamples;
    request.numChannels = numChannels;

    SandboxIPC::Message response;
    if (!sendAndWaitForResponse(request, response,
                               SandboxIPC::MessageType::Heartbeat, 1000)) {
        return false;
    }

    // Copy output from shared buffer
    std::copy(sharedBuffer, sharedBuffer + numSamples * numChannels, output);

    return true;
}

bool SandboxPluginHost::SandboxImpl::setParameter(const std::string& paramId, float value) {
    if (!pluginLoaded_) return false;

    SandboxIPC::Message request(SandboxIPC::MessageType::SetParameter);
    request.parameterId = paramId;
    request.parameterValue = value;

    SandboxIPC::Message response;
    return sendAndWaitForResponse(request, response,
                                 SandboxIPC::MessageType::Heartbeat, 1000);
}

float SandboxPluginHost::SandboxImpl::getParameter(const std::string& paramId) const {
    if (!pluginLoaded_) return 0.0f;

    SandboxIPC::Message request(SandboxIPC::MessageType::GetParameter);
    request.parameterId = paramId;

    SandboxIPC::Message response;
    if (const_cast<SandboxImpl*>(this)->sendAndWaitForResponse(
            request, response, SandboxIPC::MessageType::Heartbeat, 1000)) {
        return response.parameterValue;
    }

    return 0.0f;
}

bool SandboxPluginHost::SandboxImpl::saveState(std::vector<uint8_t>& stateData) {
    if (!pluginLoaded_) return false;

    SandboxIPC::Message request(SandboxIPC::MessageType::SaveState);

    SandboxIPC::Message response;
    if (!sendAndWaitForResponse(request, response,
                               SandboxIPC::MessageType::Heartbeat, 5000)) {
        return false;
    }

    // Convert response data to byte vector
    stateData.clear();
    for (char c : response.data) {
        stateData.push_back(static_cast<uint8_t>(c));
    }

    return true;
}

bool SandboxPluginHost::SandboxImpl::loadState(const std::vector<uint8_t>& stateData) {
    if (!pluginLoaded_) return false;

    SandboxIPC::Message request(SandboxIPC::MessageType::LoadState);

    // Convert byte vector to string
    request.data.clear();
    for (uint8_t byte : stateData) {
        request.data.push_back(static_cast<char>(byte));
    }

    SandboxIPC::Message response;
    return sendAndWaitForResponse(request, response,
                                 SandboxIPC::MessageType::Heartbeat, 5000);
}

bool SandboxPluginHost::SandboxImpl::isRunning() const {
    return process_ && process_->isRunning();
}

bool SandboxPluginHost::SandboxImpl::hasCrashed() const {
    return process_ && process_->hasCrashed();
}

std::string SandboxPluginHost::SandboxImpl::getCrashLog() const {
    return process_ ? process_->getCrashLog() : "";
}

void SandboxPluginHost::SandboxImpl::restart() {
    shutdown();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    initialize();
}

void SandboxPluginHost::SandboxImpl::monitorHeartbeat() {
    while (heartbeatRunning_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        if (ipc_ && !ipc_->checkHeartbeat()) {
            // Heartbeat failed - process likely crashed
            if (process_) {
                // Access parent class members through a proper interface
                // For now, we'll just log the issue
                juce::Logger::writeToLog("Sandbox process heartbeat timeout");
            }
        }
    }
}

bool SandboxPluginHost::SandboxImpl::sendAndWaitForResponse(
    const SandboxIPC::Message& request,
    SandboxIPC::Message& response,
    SandboxIPC::MessageType expectedResponseType,
    int timeoutMs) {

    if (!ipc_ || !process_ || !process_->isRunning()) {
        return false;
    }

    // Send request
    if (!ipc_->sendMessage(request)) {
        return false;
    }

    // Wait for response
    return ipc_->receiveMessage(response, timeoutMs);
}

// SandboxServer implementation
SandboxServer::SandboxServer() {
    ipc_ = std::make_unique<SandboxIPC>();
}

int SandboxServer::run(int argc, char* argv[]) {
    // Initialize IPC as server
    if (!ipc_->initialize(false)) {
        return -1;
    }

    // Main message loop
    while (running_) {
        SandboxIPC::Message message;
        if (ipc_->receiveMessage(message, 1000)) {
            switch (message.type) {
                case SandboxIPC::MessageType::LoadPlugin:
                    handleLoadPlugin(message);
                    break;
                case SandboxIPC::MessageType::UnloadPlugin:
                    handleUnloadPlugin(message);
                    break;
                case SandboxIPC::MessageType::ProcessAudio:
                    handleProcessAudio(message);
                    break;
                case SandboxIPC::MessageType::SetParameter:
                    handleSetParameter(message);
                    break;
                case SandboxIPC::MessageType::GetParameter:
                    handleGetParameter(message);
                    break;
                case SandboxIPC::MessageType::SaveState:
                    handleSaveState(message);
                    break;
                case SandboxIPC::MessageType::LoadState:
                    handleLoadState(message);
                    break;
                case SandboxIPC::MessageType::Shutdown:
                    handleShutdown(message);
                    running_ = false;
                    break;
                case SandboxIPC::MessageType::Heartbeat:
                    sendResponse(message);
                    break;
                default:
                    sendError("Unknown message type");
                    break;
            }
        }

        // Send heartbeat periodically
        ipc_->sendHeartbeat();
    }

    return 0;
}

void SandboxServer::handleLoadPlugin(const SandboxIPC::Message& message) {
    try {
        // Parse plugin ID and file path
        size_t separator = message.data.find('|');
        if (separator == std::string::npos) {
            sendError("Invalid load plugin message format");
            return;
        }

        std::string pluginId = message.data.substr(0, separator);
        std::string filePath = message.data.substr(separator + 1);

        // Create plugin manager and load plugin
        PluginManager manager;
        manager.scanForPlugins(juce::FileSearchPath());

        auto loadedPlugin = manager.loadPlugin(juce::String(pluginId));
        if (!loadedPlugin) {
            sendError("Failed to load plugin");
            return;
        }

        // TODO: Initialize the loaded plugin
        // For now, just send success response
        SandboxIPC::Message response(SandboxIPC::MessageType::Heartbeat);
        sendResponse(response);

    } catch (const std::exception& e) {
        sendError(std::string("Load plugin error: ") + e.what());
    }
}

void SandboxServer::handleUnloadPlugin(const SandboxIPC::Message& message) {
    currentPlugin_.reset();

    SandboxIPC::Message response(SandboxIPC::MessageType::Heartbeat);
    sendResponse(response);
}

void SandboxServer::handleProcessAudio(SandboxIPC::Message& message) {
    if (!currentPlugin_) {
        sendError("No plugin loaded");
        return;
    }

    try {
        float* sharedBuffer = ipc_->getSharedAudioBuffer();

        // Create JUCE buffer from shared memory
        juce::AudioBuffer<float> buffer(message.numChannels, message.numSamples);
        for (int ch = 0; ch < message.numChannels; ++ch) {
            buffer.copyFrom(ch, 0, sharedBuffer + ch * message.numSamples,
                           message.numSamples);
        }

        // Process audio
        juce::MidiBuffer midiBuffer;
        currentPlugin_->processAudio(buffer, midiBuffer);

        // Copy result back to shared buffer
        for (int ch = 0; ch < message.numChannels; ++ch) {
            std::copy(buffer.getReadPointer(ch),
                     buffer.getReadPointer(ch) + message.numSamples,
                     sharedBuffer + ch * message.numSamples);
        }

        SandboxIPC::Message response(SandboxIPC::MessageType::Heartbeat);
        sendResponse(response);

    } catch (const std::exception& e) {
        sendError(std::string("Process audio error: ") + e.what());
    }
}

void SandboxServer::handleSetParameter(const SandboxIPC::Message& message) {
    if (!currentPlugin_) {
        sendError("No plugin loaded");
        return;
    }

    try {
        currentPlugin_->setParameterValue(message.parameterId, message.parameterValue);

        SandboxIPC::Message response(SandboxIPC::MessageType::Heartbeat);
        sendResponse(response);

    } catch (const std::exception& e) {
        sendError(std::string("Set parameter error: ") + e.what());
    }
}

void SandboxServer::handleShutdown(const SandboxIPC::Message& message) {
    currentPlugin_.reset();

    SandboxIPC::Message response(SandboxIPC::MessageType::Heartbeat);
    sendResponse(response);
}

void SandboxServer::handleGetParameter(SandboxIPC::Message& message) {
    sendError("Not implemented");
}

void SandboxServer::handleSaveState(SandboxIPC::Message& message) {
    sendError("Not implemented");
}

void SandboxServer::handleLoadState(SandboxIPC::Message& message) {
    sendError("Not implemented");
}

void SandboxServer::sendResponse(const SandboxIPC::Message& response) {
    ipc_->sendMessage(response);
}

void SandboxServer::sendError(const std::string& error) {
    SandboxIPC::Message response(SandboxIPC::MessageType::Error);
    response.data = error;
    ipc_->sendMessage(response);
}

// SandboxUtils implementation
namespace SandboxUtils {
    std::string getSandboxExecutablePath() {
        // In a real implementation, this would return the path to the sandbox executable
        // For now, return a placeholder
        return "ampl_sandbox";
    }

    bool createSandboxEnvironment() {
        // Create sandbox environment with restricted permissions
        return true;
    }

    void cleanupSandboxEnvironment() {
        // Clean up sandbox resources
    }

    bool validateSandboxSecurity() {
        // Validate sandbox security settings
        return true;
    }
}

} // namespace ampl
