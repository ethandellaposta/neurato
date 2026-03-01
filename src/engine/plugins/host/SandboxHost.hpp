#pragma once

#include "PluginHost.hpp"
#include <juce_core/juce_core.h>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>

namespace ampl {

// Inter-process communication for sandbox
class SandboxIPC {
public:
    enum class MessageType {
        LoadPlugin,
        UnloadPlugin,
        ProcessAudio,
        SetParameter,
        GetParameter,
        SaveState,
        LoadState,
        Shutdown,
        Heartbeat,
        Error
    };
    
    struct Message {
        MessageType type;
        std::string data;
        std::vector<float> audioData;
        int numSamples{0};
        int numChannels{0};
        std::string parameterId;
        float parameterValue{0.0f};
        
        Message() = default;
        Message(MessageType t) : type(t) {}
    };
    
    SandboxIPC();
    ~SandboxIPC();
    
    bool initialize(bool isHost);
    void shutdown();
    
    // Communication
    bool sendMessage(const Message& message);
    bool receiveMessage(Message& message, int timeoutMs = 1000);
    
    // Shared memory for audio data
    bool setupSharedAudioBuffer(int numChannels, int bufferSize);
    float* getSharedAudioBuffer();
    const float* getSharedAudioBuffer() const;
    
    // Process monitoring
    bool isProcessAlive() const;
    void sendHeartbeat();
    bool checkHeartbeat();

private:
    class IPCImpl;
    std::unique_ptr<IPCImpl> impl_;
    
    bool isHost_{false};
    std::atomic<bool> processAlive_{false};
    std::atomic<int64_t> lastHeartbeat_{0};
};

// Sandbox process implementation
class SandboxProcess {
public:
    SandboxProcess();
    ~SandboxProcess();
    
    bool start(const std::string& executablePath);
    void stop();
    bool isRunning() const;
    
    // Communication
    bool sendMessage(const SandboxIPC::Message& message);
    bool receiveMessage(SandboxIPC::Message& message, int timeoutMs = 1000);
    
    // Crash detection
    bool hasCrashed() const;
    std::string getCrashLog() const;

private:
    class ProcessImpl;
    std::unique_ptr<ProcessImpl> impl_;
    
    std::atomic<bool> running_{false};
    std::atomic<bool> crashed_{false};
    std::string crashLog_;
    
    std::thread monitorThread_;
    void monitorProcess();
};

// Sandbox plugin host implementation
class SandboxPluginHost::SandboxImpl {
public:
    SandboxImpl();
    ~SandboxImpl();
    
    bool initialize();
    void shutdown();
    
    // Plugin management
    bool loadPlugin(const std::string& pluginId, const std::string& filePath);
    void unloadPlugin();
    
    // Audio processing
    bool processAudio(const float* input, float* output, int numSamples, int numChannels);
    
    // Parameter control
    bool setParameter(const std::string& paramId, float value);
    float getParameter(const std::string& paramId) const;
    
    // State management
    bool saveState(std::vector<uint8_t>& stateData);
    bool loadState(const std::vector<uint8_t>& stateData);
    
    // Process monitoring
    bool isRunning() const;
    bool hasCrashed() const;
    std::string getCrashLog() const;
    void restart();

private:
    std::unique_ptr<SandboxProcess> process_;
    std::unique_ptr<SandboxIPC> ipc_;
    
    std::atomic<bool> pluginLoaded_{false};
    std::string currentPluginId_;
    
    // Audio buffer management
    int maxChannels_{2};
    int maxBufferSize_{4096};
    std::vector<float> tempInputBuffer_;
    std::vector<float> tempOutputBuffer_;
    
    // Heartbeat monitoring
    std::thread heartbeatThread_;
    std::atomic<bool> heartbeatRunning_{false};
    void monitorHeartbeat();
    
    // Message helpers
    bool sendAndWaitForResponse(const SandboxIPC::Message& request, 
                               SandboxIPC::Message& response, 
                               SandboxIPC::MessageType expectedResponseType,
                               int timeoutMs = 5000);
};

// Sandbox executable entry point (separate process)
class SandboxServer {
public:
    SandboxServer();
    ~SandboxServer() = default;
    
    int run(int argc, char* argv[]);
    
private:
    std::unique_ptr<SandboxIPC> ipc_;
    std::unique_ptr<PluginInstance> currentPlugin_;
    std::atomic<bool> running_{true};
    
    // Message handlers
    void handleLoadPlugin(const SandboxIPC::Message& message);
    void handleUnloadPlugin(const SandboxIPC::Message& message);
    void handleProcessAudio(SandboxIPC::Message& message);
    void handleSetParameter(const SandboxIPC::Message& message);
    void handleGetParameter(SandboxIPC::Message& message);
    void handleSaveState(SandboxIPC::Message& message);
    void handleLoadState(SandboxIPC::Message& message);
    void handleShutdown(const SandboxIPC::Message& message);
    
    // Response helpers
    void sendResponse(const SandboxIPC::Message& response);
    void sendError(const std::string& error);
};

// Utility functions for sandbox management
namespace SandboxUtils {
    std::string getSandboxExecutablePath();
    bool createSandboxEnvironment();
    void cleanupSandboxEnvironment();
    bool validateSandboxSecurity();
}

} // namespace ampl
