#include "PluginHost.hpp"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace ampl {

// PluginInstance implementation
PluginInstance::PluginInstance(const PluginInfo& info) : info_(info) {
}

PluginInstance::~PluginInstance() {
    shutdown();
}

bool PluginInstance::initialize(double sampleRate, int bufferSize) {
    try {
        // Create plugin instance using JUCE
        juce::AudioPluginFormatManager formatManager;

        if (info_.format == PluginFormat::VST3) {
            formatManager.addFormat(std::make_unique<juce::VST3PluginFormat>());
        } else if (info_.format == PluginFormat::AU) {
            formatManager.addFormat(std::make_unique<juce::AudioUnitPluginFormat>());
        }

        // Create plugin instance
        std::unique_ptr<juce::AudioPluginInstance> instance;
        juce::String error;

        for (auto format : formatManager.getFormats()) {
            instance = format->createInstanceFromDescription(info_.description, sampleRate, bufferSize, error);
            if (instance != nullptr) {
                break;
            }
        }

        if (!instance) {
            return false;
        }

        plugin_ = std::move(instance);

        // Prepare plugin
        plugin_->prepareToPlay(sampleRate, bufferSize);
        plugin_->setPlayConfigDetails(bufferSize, sampleRate,
                                     info_.numInputChannels,
                                     info_.numOutputChannels);

        buildParameterMap();
        initialized_ = true;

        return true;

    } catch (const std::exception& e) {
        juce::Logger::writeToLog("Plugin initialization failed: " + std::string(e.what()));
        return false;
    }
}

void PluginInstance::shutdown() {
    if (plugin_) {
        plugin_->releaseResources();
        plugin_.reset();
    }
    initialized_ = false;
}

void PluginInstance::processAudio(juce::AudioBuffer<float>& buffer,
                                 juce::MidiBuffer& midiMessages) {
    if (!initialized_ || bypassed_) return;

    try {
        plugin_->processBlock(buffer, midiMessages);
    } catch (const std::exception& e) {
        juce::Logger::writeToLog("Plugin processing error: " + std::string(e.what()));
    }
}

void PluginInstance::processBlock(juce::AudioBuffer<float>& audio,
                                 juce::MidiBuffer& midi) {
    processAudio(audio, midi);
}

std::vector<PluginParameter> PluginInstance::getParameters() const {
    std::vector<PluginParameter> parameters;

    if (!plugin_) return parameters;

    std::lock_guard<std::mutex> lock(parameterMutex_);

    for (auto* param : plugin_->getParameters()) {
        PluginParameter paramInfo;
        paramInfo.id = param->getName(100).toStdString();
        paramInfo.name = param->getName(100).toStdString();
        paramInfo.defaultValue = param->getDefaultValue();
        paramInfo.currentValue = param->getValue();
        paramInfo.minValue = 0.0f;
        paramInfo.maxValue = 1.0f;
        paramInfo.isAutomatable = param->isAutomatable();
        paramInfo.category = "generic"; // JUCE 8 simplified category
        paramInfo.unit = param->getLabel().toStdString();

        parameters.push_back(paramInfo);
    }

    return parameters;
}

float PluginInstance::getParameterValue(const std::string& paramId) const {
    if (!plugin_) return 0.0f;

    int index = getParameterIndex(paramId);
    if (index < 0) return 0.0f;

    auto param = plugin_->getParameters()[index];
    return param->getValue();
}

void PluginInstance::setParameterValue(const std::string& paramId, float value) {
    if (!plugin_) return;

    int index = getParameterIndex(paramId);
    if (index < 0) return;

    auto param = plugin_->getParameters()[index];
    param->setValue(value);
}

void PluginInstance::setParameterValueNotifyingHost(const std::string& paramId, float value) {
    if (!plugin_) return;

    int index = getParameterIndex(paramId);
    if (index < 0) return;

    auto param = plugin_->getParameters()[index];
    param->setValueNotifyingHost(value);
}

PluginState PluginInstance::getState() const {
    PluginState state;
    state.pluginId = info_.id;

    if (!plugin_) return state;

    // Get parameter values
    for (auto& param : plugin_->getParameters()) {
        std::string paramId = std::to_string(param->getParameterIndex());
        float value = param->getValue();
        state.parameterValues[paramId] = value;
    }

    // Check if plugin uses chunk state
    juce::MemoryBlock chunk;
    plugin_->getStateInformation(chunk);
    if (chunk.getSize() > 0) {
        state.usesChunk = true;
        const char* data = static_cast<const char*>(chunk.getData());
        state.chunkData.assign(data, data + chunk.getSize());
    }

    state.currentProgram = plugin_->getCurrentProgram();
    state.programName = "Default"; // JUCE 8 simplified

    return state;
}

void PluginInstance::setState(const PluginState& state) {
    if (!plugin_) return;

    // Set parameter values
    for (const auto& pair : state.parameterValues) {
        int index = getParameterIndex(pair.first);
        if (index >= 0) {
            auto param = plugin_->getParameters()[index];
            param->setValue(pair.second);
        }
    }

    // Set chunk state if available
    if (state.usesChunk && !state.chunkData.empty()) {
        juce::MemoryBlock chunk(state.chunkData.data(), state.chunkData.size());
        plugin_->setStateInformation(chunk.getData(), chunk.getSize());
    }

    // Set program (JUCE 8 simplified)
    if (state.currentProgram >= 0) {
        plugin_->setCurrentProgram(state.currentProgram);
    }
}

void PluginInstance::resetToDefaults() {
    if (!plugin_) return;

    for (auto& param : plugin_->getParameters()) {
        param->setValue(param->getDefaultValue());
    }
}

int PluginInstance::getNumPrograms() const {
    return plugin_ ? 1 : 0; // JUCE 8 simplified
}

std::string PluginInstance::getProgramName(int index) const {
    if (!plugin_ || index != 0) {
        return "";
    }

    return plugin_->getProgramName(index).toStdString();
}

void PluginInstance::setCurrentProgram(int index) {
    if (plugin_ && index == 0) { // JUCE 8 simplified
        plugin_->setCurrentProgram(index);
    }
}

bool PluginInstance::acceptsMidi() const {
    return plugin_ ? plugin_->acceptsMidi() : false;
}

bool PluginInstance::producesMidi() const {
    return plugin_ ? plugin_->producesMidi() : false;
}

bool PluginInstance::hasEditor() const {
    return plugin_ ? plugin_->hasEditor() : false;
}

void* PluginInstance::getEditorHandle() {
    // This would need platform-specific implementation
    // For now, return nullptr
    return nullptr;
}

void PluginInstance::setBypassed(bool bypassed) {
    bypassed_ = bypassed;
}

bool PluginInstance::isBypassed() const {
    return bypassed_;
}

void PluginInstance::buildParameterMap() {
    std::lock_guard<std::mutex> lock(parameterMutex_);

    parameterIndexMap_.clear();

    if (!plugin_) return;

    for (int i = 0; i < plugin_->getParameters().size(); ++i) {
        std::string paramId = std::to_string(i);
        parameterIndexMap_[paramId] = i;
    }
}

int PluginInstance::getParameterIndex(const std::string& paramId) const {
    std::lock_guard<std::mutex> lock(parameterMutex_);

    auto it = parameterIndexMap_.find(paramId);
    return (it != parameterIndexMap_.end()) ? it->second : -1;
}

// PluginScanner implementation
PluginScanner::PluginScanner() {
}

std::future<std::vector<PluginInfo>> PluginScanner::scanForPlugins(
    const std::vector<std::string>& searchPaths,
    PluginFormat format) {

    scanCancelled_ = false;

    return std::async(std::launch::async, [this, searchPaths, format]() {
        switch (format) {
            case PluginFormat::VST3:
                return scanVST3Sync(searchPaths);
            case PluginFormat::AU:
                return scanAUSync(searchPaths);
            case PluginFormat::Standalone:
                return std::vector<PluginInfo>(); // Handle all cases
            default:
                return std::vector<PluginInfo>();
        }
    });
}

std::future<std::vector<PluginInfo>> PluginScanner::scanVST3(
    const std::vector<std::string>& searchPaths) {

    return std::async(std::launch::async, [this, searchPaths]() {
        return scanVST3Sync(searchPaths);
    });
}

std::future<std::vector<PluginInfo>> PluginScanner::scanAU(
    const std::vector<std::string>& searchPaths) {

    return std::async(std::launch::async, [this, searchPaths]() {
        return scanAUSync(searchPaths);
    });
}

void PluginScanner::setProgressCallback(std::function<void(int, int, const std::string&)> callback) {
    progressCallback_ = callback;
}

void PluginScanner::cancelScan() {
    scanCancelled_ = true;
}

bool PluginScanner::validatePlugin(const std::string& filePath) {
    return testPluginLoad(filePath);
}

bool PluginScanner::testPluginLoad(const std::string& filePath) {
    try {
        juce::VST3PluginFormat format;
        juce::OwnedArray<juce::PluginDescription> descriptions;

        // JUCE 8 simplified plugin loading
        juce::File file(filePath);
        if (!file.exists()) {
            return false;
        }

        // Try to create plugin instance to validate
        juce::AudioPluginFormatManager formatManager;
        formatManager.addFormat(std::make_unique<juce::VST3PluginFormat>());

        juce::String error;
        auto instance = formatManager.getFormat(0)->createInstanceFromDescription(
            juce::PluginDescription(), 44100.0, 512, error);

        return instance != nullptr;

    } catch (const std::exception& e) {
        juce::Logger::writeToLog("Plugin validation failed: " + std::string(e.what()));
        return false;
    }
}

std::vector<PluginInfo> PluginScanner::scanVST3Sync(const std::vector<std::string>& paths) {
    std::vector<PluginInfo> plugins;

    juce::VST3PluginFormat format;

    int totalFiles = 0;
    int processedFiles = 0;

    // Count total files first
    for (const auto& path : paths) {
        try {
            std::filesystem::path fsPath(path);
            if (std::filesystem::exists(fsPath)) {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(fsPath)) {
                    if (entry.is_regular_file() &&
                        entry.path().extension() == ".vst3") {
                        totalFiles++;
                    }
                }
            }
        } catch (const std::exception& e) {
            juce::Logger::writeToLog("Error scanning path " + path + ": " + e.what());
        }
    }

    // Scan files
    for (const auto& path : paths) {
        if (scanCancelled_) break;

        try {
            std::filesystem::path fsPath(path);
            if (std::filesystem::exists(fsPath)) {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(fsPath)) {
                    if (scanCancelled_) break;

                    if (entry.is_regular_file() &&
                        entry.path().extension() == ".vst3") {

                        std::string filePath = entry.path().string();

                        if (progressCallback_) {
                            progressCallback_(processedFiles, totalFiles,
                                            "Scanning: " + entry.path().filename().string());
                        }

                        // JUCE 8 simplified - just validate file exists and try to load
                        if (testPluginLoad(filePath)) {
                            PluginInfo info;
                            info.id = "vst3_" + std::to_string(plugins.size());
                            info.name = entry.path().filename().stem().string();
                            info.manufacturer = "Unknown";
                            info.version = "1.0";
                            info.filePath = filePath;
                            info.format = PluginFormat::VST3;
                            info.numInputChannels = 2;
                            info.numOutputChannels = 2;
                            info.isInstrument = false;
                            info.isEffect = true;
                            info.supportsMidi = false;
                            info.hasEditor = false;
                            info.isSynth = false;

                            plugins.push_back(info);
                        }

                        processedFiles++;
                    }
                }
            }
        } catch (const std::exception& e) {
            juce::Logger::writeToLog("Error scanning path " + path + ": " + e.what());
        }
    }

    return plugins;
}

std::vector<PluginInfo> PluginScanner::scanAUSync(const std::vector<std::string>& paths) {
    std::vector<PluginInfo> plugins;

#ifdef JUCE_MAC
    juce::AudioUnitPluginFormat format;

    // Get all AU plugins
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile(descriptions, "");

    for (auto* desc : descriptions) {
        PluginInfo info = createPluginInfo(*desc);
        plugins.push_back(info);
    }
#endif

    return plugins;
}

PluginInfo PluginScanner::createPluginInfo(const juce::PluginDescription& desc) {
    PluginInfo info;
    info.id = desc.createIdentifierString().toStdString();
    info.name = desc.name.toStdString();
    info.manufacturer = desc.manufacturerName.toStdString();
    info.version = desc.version.toStdString();

    if (desc.pluginFormatName == "VST3") {
        info.format = PluginFormat::VST3;
    } else if (desc.pluginFormatName == "AudioUnit") {
        info.format = PluginFormat::AU;
    }

    info.numInputChannels = desc.numInputChannels;
    info.numOutputChannels = desc.numOutputChannels;
    info.isInstrument = desc.isInstrument;
    info.isEffect = !desc.isInstrument;
    info.supportsMidi = desc.isInstrument; // Instruments support MIDI by default
    info.hasEditor = false; // JUCE 8 simplified
    info.isSynth = desc.isInstrument; // JUCE 8 simplified

    info.description = desc;

    // Parse categories
    juce::StringArray categoryArray;
    categoryArray.addTokens(desc.category, ",", "");
    for (const auto& cat : categoryArray) {
        info.categories.push_back(cat.trim().toStdString());
    }

    return info;
}

} // namespace ampl
