#include "engine/PluginManager.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_core/juce_core.h>

namespace neurato {

PluginManager::PluginManager()
    : scannerThread("Plugin Scanner", true)
{
    initializeFormats();
    loadDefaultInstruments();
}

PluginManager::~PluginManager()
{
    // Clean up all loaded plugins
    for (auto& [id, plugin] : loadedPlugins)
    {
        if (plugin->instance)
        {
            plugin->instance->releaseResources();
        }
    }
    loadedPlugins.clear();
}

void PluginManager::initializeFormats()
{
    // Initialize plugin formats based on platform
#if JUCE_PLUGINHOST_VST3
    vst3Format = std::make_unique<juce::VST3PluginFormat>();
    allFormats.push_back(vst3Format.get());
#endif

#if JUCE_PLUGINHOST_AU && JUCE_MAC
    auFormat = std::make_unique<juuce::AudioUnitPluginFormat>();
    allFormats.push_back(auFormat.get());
#endif

#if JUCE_PLUGINHOST_VST2
    vst2Format = std::make_unique<juce::VSTPluginFormat>();
    allFormats.push_back(vst2Format.get());
#endif
}

void PluginManager::scanForPlugins(const juce::FileSearchPath& searchPaths)
{
    if (isScanning)
        return;
    
    isScanning = true;
    
    // Run scanning on background thread
    scannerThread.runTask([this, searchPaths]()
    {
        juce::DeadMansPedal dmp;
        
        for (auto* format : allFormats)
        {
            if (format)
            {
                format->findAllPluginsForFile(searchPaths, knownPlugins, dmp);
            }
        }
        
        // Sort and deduplicate
        knownPlugins.sortAlphabetically([], true);
        
        isScanning = false;
    });
}

void PluginManager::refreshPluginList()
{
    // Clear and rescan default locations
    knownPlugins.clear();
    
    juce::FileSearchPath defaultSearchPaths;
    
#if JUCE_MAC
    defaultSearchPaths.add(juce::File("~/Library/Audio/Plug-Ins/VST3"));
    defaultSearchPaths.add(juce::File("/Library/Audio/Plug-Ins/VST3"));
    defaultSearchPaths.add(juce::File("~/Library/Audio/Plug-Ins/Components"));
    defaultSearchPaths.add(juce::File("/Library/Audio/Plug-Ins/Components"));
#elif JUCE_WINDOWS
    defaultSearchPaths.add(juce::File::getSpecialLocation(juce::File::globalApplicationsDirectory)
                            .getChildFile("Common Files/VST3"));
    defaultSearchPaths.add(juce::File::getSpecialLocation(juce::File::globalApplicationsDirectory)
                            .getChildFile("VST3"));
#elif JUCE_LINUX
    defaultSearchPaths.add(juce::File("~/.vst3"));
    defaultSearchPaths.add(juce::File("/usr/lib/vst3"));
    defaultSearchPaths.add(juce::File("/usr/local/lib/vst3"));
#endif
    
    scanForPlugins(defaultSearchPaths);
}

std::vector<PluginManager::PluginInfo> PluginManager::getAvailablePlugins() const
{
    std::vector<PluginInfo> plugins;
    
    for (int i = 0; i < knownPlugins.getNumTypes(); ++i)
    {
        auto* desc = knownPlugins.getType(i);
        if (desc && isPluginCompatible(*desc))
        {
            PluginInfo info;
            info.description = *desc;
            info.format = desc->pluginFormatName;
            info.isInstrument = desc->isInstrument;
            info.isEffect = desc->isInstrument == false;
            info.category = desc->category;
            
            plugins.push_back(info);
        }
    }
    
    return plugins;
}

std::vector<PluginManager::PluginInfo> PluginManager::getInstruments() const
{
    auto all = getAvailablePlugins();
    std::vector<PluginInfo> instruments;
    
    for (const auto& plugin : all)
    {
        if (plugin.isInstrument)
        {
            instruments.push_back(plugin);
        }
    }
    
    return instruments;
}

std::vector<PluginManager::PluginInfo> PluginManager::getEffects() const
{
    auto all = getAvailablePlugins();
    std::vector<PluginInfo> effects;
    
    for (const auto& plugin : all)
    {
        if (plugin.isEffect)
        {
            effects.push_back(plugin);
        }
    }
    
    return effects;
}

std::unique_ptr<PluginManager::LoadedPlugin> PluginManager::loadPlugin(const juce::String& pluginId)
{
    // Find plugin description
    juce::PluginDescription desc;
    if (!knownPlugins.getTypeForIdentifierString(pluginId, desc))
    {
        juce::Logger::writeToLog("Plugin not found: " + pluginId);
        return nullptr;
    }
    
    if (!isPluginCompatible(desc))
    {
        juce::Logger::writeToLog("Plugin not compatible: " + pluginId);
        return nullptr;
    }
    
    // Find appropriate format
    juce::AudioPluginFormat* format = nullptr;
    for (auto* f : allFormats)
    {
        if (f && f->getName() == desc.pluginFormatName)
        {
            format = f;
            break;
        }
    }
    
    if (!format)
    {
        juce::Logger::writeToLog("No format for plugin: " + pluginId);
        return nullptr;
    }
    
    // Create plugin instance
    auto instance = std::unique_ptr<juce::AudioPluginInstance>(
        format->createInstanceFromDescription(desc, 44100.0, 512));
    
    if (!instance)
    {
        juce::Logger::writeToLog("Failed to create plugin instance: " + pluginId);
        return nullptr;
    }
    
    // Prepare plugin
    instance->prepareToPlay(44100.0, 512);
    instance->setPlayConfigDetails(512, 44100.0, 2, 2);
    
    // Wrap in LoadedPlugin
    auto loaded = std::make_unique<LoadedPlugin>();
    loaded->instance = std::move(instance);
    loaded->pluginId = pluginId;
    loaded->isActive = false;
    
    // Store in map
    loadedPlugins[pluginId] = std::make_unique<LoadedPlugin>(*loaded);
    
    juce::Logger::writeToLog("Loaded plugin: " + desc.name);
    return loaded;
}

void PluginManager::unloadPlugin(const juce::String& pluginId)
{
    auto it = loadedPlugins.find(pluginId);
    if (it != loadedPlugins.end())
    {
        if (it->second->instance)
        {
            it->second->instance->releaseResources();
        }
        
        // Clear from atomic pointer if it's the default piano
        if (pluginId == getDefaultPianoId())
        {
            defaultPianoPlugin.store(nullptr);
        }
        
        loadedPlugins.erase(it);
        juce::Logger::writeToLog("Unloaded plugin: " + pluginId);
    }
}

PluginManager::LoadedPlugin* PluginManager::getPluginForAudio(const juce::String& pluginId) noexcept
{
    auto it = loadedPlugins.find(pluginId);
    if (it != loadedPlugins.end())
    {
        return it->second.get();
    }
    return nullptr;
}

void PluginManager::loadDefaultInstruments()
{
    createDefaultPiano();
}

void PluginManager::createDefaultPiano()
{
    // For now, we'll use the existing sine synth as the default piano
    // Later we can replace this with a sampled piano
    
    // Create a special entry for the default piano
    juce::PluginDescription desc;
    desc.name = "Neurato Piano";
    desc.pluginFormatName = "Internal";
    desc.descriptiveName = "Clean light piano sound";
    desc.numInputChannels = 0;
    desc.numOutputChannels = 2;
    desc.isInstrument = true;
    desc.manufacturerName = "Neurato";
    desc.pluginIdentifierCode = "neurato.piano";
    
    // Add to known plugins
    knownPlugins.addType(desc);
    
    juce::Logger::writeToLog("Created default piano instrument");
}

bool PluginManager::isPluginCompatible(const juce::PluginDescription& desc) const
{
    // Check if plugin has required I/O configuration
    if (desc.isInstrument)
    {
        // Instruments should have no inputs and at least 2 outputs
        return desc.numInputChannels == 0 && desc.numOutputChannels >= 2;
    }
    else
    {
        // Effects should have at least 2 inputs and 2 outputs
        return desc.numInputChannels >= 2 && desc.numOutputChannels >= 2;
    }
}

} // namespace neurato
