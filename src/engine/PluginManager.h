#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>
#include <unordered_map>

namespace neurato {

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
    void unloadPlugin(const juce::String& pluginId);

    // Audio thread access
    LoadedPlugin* getPluginForAudio(const juce::String& pluginId) noexcept;

    // Default instruments
    void loadDefaultInstruments();
    juce::String getDefaultPianoId() const { return "neurato.piano"; }

private:
    // Plugin formats
    std::unique_ptr<juce::AudioPluginFormat> vst3Format;
    std::unique_ptr<juce::AudioPluginFormat> auFormat;
    std::unique_ptr<juce::AudioPluginFormat> vst2Format;

    // Plugin list
    juce::KnownPluginList knownPlugins;
    std::vector<juce::AudioPluginFormat*> allFormats;

    // Loaded plugins (UI thread owns, audio thread reads)
    std::unordered_map<juce::String, std::unique_ptr<LoadedPlugin>> loadedPlugins;

    // Lock-free access for audio thread
    juce::Atomic<LoadedPlugin*> defaultPianoPlugin{nullptr};

    // Scanning
    bool isScanning = false;
    std::unique_ptr<juce::ThreadWithProgressWindow> scannerThread;

    void initializeFormats();
    void createDefaultPiano();
    bool isPluginCompatible(const juce::PluginDescription& desc) const;
};

} // namespace neurato
