#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_core/juce_core.h>
#include <unordered_map>
#include <memory>

namespace ampl {

class PluginScannerThread : public juce::ThreadWithProgressWindow
{
public:
    PluginScannerThread() : juce::ThreadWithProgressWindow("Plugin Scanner", true, true) {}

    void setScanFunction(std::function<void()> scanFunc) {
        scanFunction = scanFunc;
    }

    void run() override {
        if (scanFunction) {
            scanFunction();
        }
    }

private:
    std::function<void()> scanFunction;
};

/**
 * Manages third-party plugin loading, scanning, and instantiation.
 * Thread-safe for audio thread usage via lock-free structures.
 */
class PluginManager
{
public:
    struct PluginInfo
    {
        juce::PluginDescription description;
        juce::String format; // "VST3", "AU", "VST2", etc.
        bool isInstrument = false;
        bool isEffect = false;
        juce::String category; // "Synth", "Effect", "Analyzer", etc.
    };

    struct LoadedPlugin
    {
        std::unique_ptr<juce::AudioPluginInstance> instance;
        juce::String pluginId;
        bool isActive = false;
    };

public:
    PluginManager();
    ~PluginManager();

    // Plugin discovery and management
    void scanForPlugins(const juce::FileSearchPath& searchPaths);
    void refreshPluginList();
    std::vector<PluginInfo> getAvailablePlugins() const;
    std::vector<PluginInfo> getInstruments() const;
    std::vector<PluginInfo> getEffects() const;

    // Plugin loading (UI thread only)
    std::unique_ptr<LoadedPlugin> loadPlugin(const juce::String& pluginId);
    bool loadPluginByDescription(const juce::PluginDescription& desc,
                                 juce::String& errorOut);
    void unloadPlugin(const juce::String& pluginId);

    // Audio thread access
    LoadedPlugin* getPluginForAudio(const juce::String& pluginId) noexcept;

    // Default instruments
    void loadDefaultInstruments();
    juce::String getDefaultPianoId() const { return "ampl.piano"; }

    // Audio configuration
    void setSampleRate(double sr) { currentSampleRate_ = sr; }
    void setBlockSize(int bs) { currentBlockSize_ = bs; }
    double getSampleRate() const { return currentSampleRate_; }
    int getBlockSize() const { return currentBlockSize_; }

    // Known plugin list access (for displaying scan results)
    const juce::KnownPluginList& getKnownPluginList() const { return knownPlugins; }
    juce::AudioPluginFormatManager& getFormatManager() { return formatManager_; }

private:
    // Plugin formats
    juce::AudioPluginFormatManager formatManager_;

    // Plugin list
    juce::KnownPluginList knownPlugins;
    std::vector<juce::AudioPluginFormat*> allFormats;

    // Loaded plugins (UI thread owns, audio thread reads)
    std::unordered_map<juce::String, std::unique_ptr<LoadedPlugin>> loadedPlugins;

    // Lock-free access for audio thread
    juce::Atomic<LoadedPlugin*> defaultPianoPlugin{nullptr};

    // Audio configuration
    double currentSampleRate_{44100.0};
    int currentBlockSize_{512};

    // Scanning
    bool isScanning = false;
    std::unique_ptr<PluginScannerThread> scannerThread;

    void initializeFormats();
    void createDefaultPiano();
    bool isPluginCompatible(const juce::PluginDescription& desc) const;
};

} // namespace ampl
