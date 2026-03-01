#include "engine/plugins/manager/PluginManager.hpp"
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_core/juce_core.h>

namespace ampl
{

PluginManager::PluginManager() : scannerThread(std::make_unique<PluginScannerThread>())
{
    initializeFormats();
    loadDefaultInstruments();
}

PluginManager::~PluginManager()
{
    // Clean up all loaded plugins
    for (auto &[id, plugin] : loadedPlugins)
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
    // Use AudioPluginFormatManager which handles all registered formats
#if JUCE_PLUGINHOST_VST3
    formatManager_.addFormat(new juce::VST3PluginFormat());
#endif

#if JUCE_PLUGINHOST_AU && JUCE_MAC
    formatManager_.addFormat(new juce::AudioUnitPluginFormat());
#endif

#if JUCE_PLUGINHOST_VST2
    formatManager_.addFormat(new juce::VSTPluginFormat());
#endif

    // Collect raw pointers for convenience
    for (int i = 0; i < formatManager_.getNumFormats(); ++i)
        allFormats.push_back(formatManager_.getFormat(i));
}

void PluginManager::scanForPlugins(const juce::FileSearchPath &searchPaths)
{
    if (isScanning)
        return;

    isScanning = true;

    // ----------------------------------------------------------------
    // Step 1 — collect all unique candidate plugin files across every
    //          format and search path.
    // ----------------------------------------------------------------
    juce::StringArray pluginFiles;

    for (auto *format : allFormats)
    {
        if (!format)
            continue;

        juce::FileSearchPath paths = searchPaths;
        auto defaultPaths = format->getDefaultLocationsToSearch();
        for (int i = 0; i < defaultPaths.getNumPaths(); ++i)
            paths.addIfNotAlreadyThere(defaultPaths[i]);

        for (int i = 0; i < paths.getNumPaths(); ++i)
        {
            auto dir = paths[i];
            if (!dir.isDirectory())
                continue;

            auto files = dir.findChildFiles(juce::File::findFilesAndDirectories, false);
            for (const auto &file : files)
            {
                if (format->fileMightContainThisPluginType(file.getFullPathName()))
                    pluginFiles.addIfNotAlreadyThere(file.getFullPathName());
            }
        }
    }

    // ----------------------------------------------------------------
    // Step 2 — scan each file in a fresh subprocess (fork+exec).
    //
    // We launch a new copy of the Ampl binary with --scan-plugin <path>.
    // juce::ChildProcess uses posix_spawn on macOS, so the child starts
    // as a clean single-threaded process.  None of the CoreFoundation /
    // CarbonCore fork-safety restrictions apply, and plugins that call
    // fork() internally (e.g. Melodyne) work without crashing the host.
    // ----------------------------------------------------------------
    auto execFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);

    for (const auto &filePath : pluginFiles)
    {
        juce::StringArray childArgs;
        childArgs.add(execFile.getFullPathName());
        childArgs.add("--scan-plugin");
        childArgs.add(filePath);

        juce::ChildProcess child;
        if (!child.start(childArgs, juce::ChildProcess::wantStdOut))
        {
            DBG("PluginManager: failed to start scan subprocess for: " + filePath);
            continue;
        }

        if (!child.waitForProcessToFinish(15'000))
        {
            child.kill();
            DBG("PluginManager: scan subprocess timed out for: " + filePath);
            continue;
        }

        if (child.getExitCode() != 0)
        {
            DBG("PluginManager: scan subprocess crashed for: " + filePath + " (skipping)");
            continue;
        }

        // Parse output: XML blocks separated by "---END_PLUGIN---" sentinels.
        auto output = child.readAllProcessOutput();
        for (auto &block : juce::StringArray::fromTokens(output, "---END_PLUGIN---", ""))
        {
            auto trimmed = block.trim();
            if (trimmed.isEmpty())
                continue;

            if (auto xml = juce::parseXML(trimmed))
            {
                juce::PluginDescription desc;
                if (desc.loadFromXml(*xml))
                {
                    knownPlugins.addType(desc);
                    DBG("PluginManager: Found plugin: " + desc.name + " (" + desc.pluginFormatName +
                        ")");
                }
            }
        }
    }

    // Sort and deduplicate
    knownPlugins.sort(juce::KnownPluginList::sortAlphabetically, true);

    isScanning = false;
    DBG("PluginManager: Scan complete. Found " + juce::String(knownPlugins.getNumTypes()) +
        " plugins.");
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
        auto desc = knownPlugins.getType(i);
        if (desc)
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

    for (const auto &plugin : all)
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

    for (const auto &plugin : all)
    {
        if (plugin.isEffect)
        {
            effects.push_back(plugin);
        }
    }

    return effects;
}

std::unique_ptr<PluginManager::LoadedPlugin> PluginManager::loadPlugin(const juce::String &pluginId)
{
    // Find plugin description
    auto desc = knownPlugins.getTypeForIdentifierString(pluginId);
    if (!desc)
    {
        juce::Logger::writeToLog("Plugin not found: " + pluginId);
        return nullptr;
    }

    juce::String error;
    return loadPluginByDescription(*desc, error) ? nullptr : nullptr;
}

bool PluginManager::loadPluginByDescription(const juce::PluginDescription &desc,
                                            juce::String &errorOut)
{
    auto pluginId = desc.createIdentifierString();

    // Already loaded?
    if (loadedPlugins.count(pluginId))
        return true;

    // Find appropriate format
    juce::AudioPluginFormat *format = nullptr;
    for (auto *f : allFormats)
    {
        if (f && f->getName() == desc.pluginFormatName)
        {
            format = f;
            break;
        }
    }

    if (!format)
    {
        errorOut = "No format handler for: " + desc.pluginFormatName;
        juce::Logger::writeToLog(errorOut);
        return false;
    }

    // Create plugin instance using current audio settings
    juce::String error;
    auto instance = std::unique_ptr<juce::AudioPluginInstance>(
        format->createInstanceFromDescription(desc, currentSampleRate_, currentBlockSize_, error));

    if (!instance)
    {
        errorOut = "Failed to create plugin: " + desc.name + " - " + error;
        juce::Logger::writeToLog(errorOut);
        return false;
    }

    // Prepare plugin for playback
    instance->prepareToPlay(currentSampleRate_, currentBlockSize_);

    DBG("PluginManager: Loaded plugin: " + desc.name + " [" + desc.pluginFormatName + "]" +
        " ins=" + juce::String(desc.numInputChannels) +
        " outs=" + juce::String(desc.numOutputChannels));

    // Wrap in LoadedPlugin
    auto loaded = std::make_unique<LoadedPlugin>();
    loaded->instance = std::move(instance);
    loaded->pluginId = pluginId;
    loaded->isActive = true;

    loadedPlugins[pluginId] = std::move(loaded);
    return true;
}

void PluginManager::unloadPlugin(const juce::String &pluginId)
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
            defaultPianoPlugin = nullptr;
        }

        loadedPlugins.erase(it);
        juce::Logger::writeToLog("Unloaded plugin: " + pluginId);
    }
}

PluginManager::LoadedPlugin *PluginManager::getPluginForAudio(const juce::String &pluginId) noexcept
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
    juce::PluginDescription desc;
    desc.name = "Ampl Piano";
    desc.pluginFormatName = "Internal";
    desc.descriptiveName = "Clean light piano sound";
    desc.numInputChannels = 0;
    desc.numOutputChannels = 2;
    desc.isInstrument = true;
    desc.manufacturerName = "Ampl";
    desc.fileOrIdentifier = "ampl.piano";
    desc.uniqueId = 0x4e50494f;

    knownPlugins.addType(desc);
    juce::Logger::writeToLog("Created default piano instrument");
}

bool PluginManager::isPluginCompatible(const juce::PluginDescription &desc) const
{
    // Accept all plugins — the DAW should be able to host anything
    // The user picks what goes where
    return true;
}

} // namespace ampl
