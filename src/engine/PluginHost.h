#pragma once

#include "AudioGraph.h"
#include "Automation.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <atomic>
#include <mutex>
#include <future>

namespace neurato {

// Plugin format types
enum class PluginFormat {
    VST3,
    AU,
    Standalone
};

// Plugin metadata
struct PluginInfo {
    std::string id;
    std::string name;
    std::string manufacturer;
    std::string version;
    PluginFormat format;
    std::string filePath;
    bool isInstrument{false};
    bool isEffect{true};
    std::vector<std::string> categories;
    
    // Plugin capabilities
    bool supportsMidi{false};
    bool hasEditor{false};
    bool isSynth{false};
    int numInputChannels{0};
    int numOutputChannels{0};
    
    juce::PluginDescription description;
};

// Plugin parameter information
struct PluginParameter {
    std::string id;
    std::string name;
    float minValue{0.0f};
    float maxValue{1.0f};
    float defaultValue{0.0f};
    bool isAutomatable{true};
    bool isDiscrete{false};
    std::vector<std::string> valueStrings;  // For discrete parameters
    std::string unit;
    std::string category;
};

// Plugin state for serialization
struct PluginState {
    std::string pluginId;
    std::map<std::string, float> parameterValues;
    std::vector<uint8_t> chunkData;  // For plugins that use chunk state
    bool usesChunk{false};
    
    // Program/bank information
    int currentProgram{0};
    std::string programName;
};

// Plugin instance wrapper
class PluginInstance {
public:
    PluginInstance(const PluginInfo& info);
    ~PluginInstance();
    
    // Instance management
    bool initialize(double sampleRate, int bufferSize);
    void shutdown();
    bool isInitialized() const { return initialized_; }
    
    // Audio processing
    void processAudio(juce::AudioBuffer<float>& buffer, 
                     juce::MidiBuffer& midiMessages);
    void processBlock(juce::AudioBuffer<float>& audio, 
                     juce::MidiBuffer& midi);
    
    // Parameter access
    std::vector<PluginParameter> getParameters() const;
    float getParameterValue(const std::string& paramId) const;
    void setParameterValue(const std::string& paramId, float value);
    void setParameterValueNotifyingHost(const std::string& paramId, float value);
    
    // State management
    PluginState getState() const;
    void setState(const PluginState& state);
    void resetToDefaults();
    
    // Program management
    int getNumPrograms() const;
    std::string getProgramName(int index) const;
    void setCurrentProgram(int index);
    
    // MIDI support
    bool acceptsMidi() const;
    bool producesMidi() const;
    
    // Editor support
    bool hasEditor() const;
    void* getEditorHandle();  // Platform-specific editor handle
    
    // Plugin info access
    const PluginInfo& getPluginInfo() const { return info_; }
    
    // Bypass
    void setBypassed(bool bypassed);
    bool isBypassed() const;

private:
    PluginInfo info_;
    std::unique_ptr<juce::AudioPluginInstance> plugin_;
    bool initialized_{false};
    std::atomic<bool> bypassed_{false};
    
    // Parameter mapping
    std::map<std::string, int> parameterIndexMap_;
    mutable std::mutex parameterMutex_;
    
    void buildParameterMap();
    int getParameterIndex(const std::string& paramId) const;
};

// Plugin scanner for discovering plugins
class PluginScanner {
public:
    PluginScanner();
    ~PluginScanner() = default;
    
    // Scanning
    std::future<std::vector<PluginInfo>> scanForPlugins(
        const std::vector<std::string>& searchPaths,
        PluginFormat format);
    
    std::future<std::vector<PluginInfo>> scanVST3(
        const std::vector<std::string>& searchPaths);
    
    std::future<std::vector<PluginInfo>> scanAU(
        const std::vector<std::string>& searchPaths);
    
    // Progress reporting
    void setProgressCallback(std::function<void(int, int, const std::string&)> callback);
    void cancelScan();
    
    // Plugin validation
    bool validatePlugin(const std::string& filePath);
    bool testPluginLoad(const std::string& filePath);

private:
    std::atomic<bool> scanCancelled_{false};
    std::function<void(int, int, const std::string&)> progressCallback_;
    
    std::vector<PluginInfo> scanVST3Sync(const std::vector<std::string>& paths);
    std::vector<PluginInfo> scanAUSync(const std::vector<std::string>& paths);
    
    PluginInfo createPluginInfo(const juce::PluginDescription& desc);
};

// Plugin manager for handling plugin lifecycle
class PluginManager {
public:
    PluginManager();
    ~PluginManager();
    
    // Plugin database
    void initialize();
    void shutdown();
    
    // Scanning
    void scanPlugins(const std::vector<std::string>& searchPaths);
    void addPlugin(const PluginInfo& info);
    void removePlugin(const std::string& pluginId);
    
    // Plugin lookup
    std::vector<PluginInfo> getAllPlugins() const;
    std::vector<PluginInfo> getPluginsByFormat(PluginFormat format) const;
    std::vector<PluginInfo> getInstruments() const;
    std::vector<PluginInfo> getEffects() const;
    PluginInfo getPluginInfo(const std::string& pluginId) const;
    
    // Instance creation
    std::unique_ptr<PluginInstance> createInstance(const std::string& pluginId);
    
    // Database management
    void loadPluginDatabase();
    void savePluginDatabase();
    void clearDatabase();

private:
    std::map<std::string, PluginInfo> pluginDatabase_;
    std::unique_ptr<PluginScanner> scanner_;
    mutable std::mutex databaseMutex_;
    
    std::string getDatabasePath() const;
    void serializeDatabase();
    void deserializeDatabase();
};

// Plugin node for audio graph integration
class PluginNode : public AudioNode {
public:
    PluginNode(const std::string& id, std::unique_ptr<PluginInstance> instance);
    ~PluginNode() override;
    
    void process(AudioBuffer& input, AudioBuffer& output, 
                int numSamples, SampleCount position) noexcept override;
    
    std::vector<ParameterInfo> getParameters() const override;
    float getParameterValue(const std::string& paramId) const override;
    void setParameterValue(const std::string& paramId, float value) override;
    
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void reset() override;
    
    // Plugin-specific access
    PluginInstance* getPluginInstance() const { return plugin_.get(); }
    const PluginInfo& getPluginInfo() const;
    
    // State management
    void setState(const PluginState& state);
    PluginState getState() const;

private:
    std::unique_ptr<PluginInstance> plugin_;
    juce::AudioBuffer<float> juceBuffer_;
    juce::MidiBuffer midiBuffer_;
    
    std::vector<float*> channelPointers_;
    
    void convertAudioBuffer(const AudioBuffer& input, juce::AudioBuffer<float>& output);
    void convertAudioBuffer(const juce::AudioBuffer<float>& input, AudioBuffer& output);
};

// Sandbox plugin host for crash isolation
class SandboxPluginHost {
public:
    SandboxPluginHost();
    ~SandboxPluginHost();
    
    // Process management
    bool startSandbox();
    void stopSandbox();
    bool isSandboxRunning() const;
    
    // Plugin loading in sandbox
    bool loadPlugin(const std::string& pluginId, const std::string& filePath);
    void unloadPlugin();
    
    // Communication with sandbox
    bool processAudio(const float* input, float* output, int numSamples, int numChannels);
    bool setParameter(const std::string& paramId, float value);
    float getParameter(const std::string& paramId) const;
    
    // State management
    bool saveState(std::vector<uint8_t>& stateData);
    bool loadState(const std::vector<uint8_t>& stateData);
    
    // Crash detection
    bool hasCrashed() const;
    std::string getCrashLog() const;
    void restartSandbox();

private:
    class SandboxImpl;
    std::unique_ptr<SandboxImpl> impl_;
    
    std::atomic<bool> sandboxRunning_{false};
    std::atomic<bool> hasCrashed_{false};
    std::string crashLog_;
    
    // IPC communication
    mutable std::mutex ipcMutex_;
    bool sendMessage(const std::string& message);
    std::string receiveMessage();
};

// Plugin automation integration
class PluginAutomationController {
public:
    PluginAutomationController(PluginNode* node);
    ~PluginAutomationController() = default;
    
    // Automation lane management
    void addAutomationLane(const std::string& paramId, std::shared_ptr<AutomationLane> lane);
    void removeAutomationLane(const std::string& paramId);
    
    // Processing with automation
    void processWithAutomation(AudioBuffer& input, AudioBuffer& output, 
                              int numSamples, SampleCount position);
    
    // Parameter mapping
    std::string mapParameterId(const std::string& pluginParamId) const;
    std::string unmapParameterId(const std::string& automationParamId) const;

private:
    PluginNode* pluginNode_;
    std::map<std::string, std::shared_ptr<AutomationLane>> automationLanes_;
    std::map<std::string, std::string> parameterIdMap_;
    
    void updateParameterFromAutomation(const std::string& paramId, SampleCount position);
};

} // namespace neurato
