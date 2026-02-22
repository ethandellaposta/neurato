#include "LogicFeatures.h"
#include <algorithm>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>

namespace neurato {

// LogicMixerChannel implementation
LogicMixerChannel::LogicMixerChannel(const ChannelStrip& strip) : channelStrip_(strip) {
    currentVolume_ = channelStrip_.volume;
    currentPan_ = channelStrip_.pan;
    volumeSmoother_ = currentVolume_;
    panSmoother_ = currentPan_;
}

void LogicMixerChannel::processAudio(juce::AudioBuffer<float>& buffer, int numSamples) {
    if (channelStrip_.mute || buffer.getNumSamples() == 0) {
        buffer.clear();
        return;
    }
    
    // Apply volume and pan with smoothing
    applyVolumeAndPan(buffer);
    
    // Update smoothing targets
    updateSmoothers();
}

void LogicMixerChannel::applyVolumeAndPan(juce::AudioBuffer<float>& buffer) {
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    if (numChannels == 0 || numSamples == 0) return;
    
    // Calculate linear volume from dB
    float targetVolume = dbToLinear(channelStrip_.volume);
    float targetPan = channelStrip_.pan;
    
    // Smooth volume changes
    volumeSmoother_ = volumeSmoother_ * kSmoothingCoefficient + targetVolume * (1.0f - kSmoothingCoefficient);
    panSmoother_ = panSmoother_ * kSmoothingCoefficient + targetPan * (1.0f - kSmoothingCoefficient);
    
    // Apply gain
    for (int ch = 0; ch < numChannels; ++ch) {
        if (buffer.getWritePointer(ch)) {
            juce::FloatVectorOperations::multiply(buffer.getWritePointer(ch), 
                                                volumeSmoother_, numSamples);
        }
    }
    
    // Apply pan (stereo only)
    if (numChannels >= 2) {
        float leftGain, rightGain;
        
        // Constant power panning
        if (panSmoother_ == 0.0f) {
            leftGain = rightGain = 1.0f / std::sqrt(2.0f);
        } else {
            float panAngle = (panSmoother_ + 1.0f) * juce::MathConstants<float>::pi / 4.0f;
            leftGain = std::cos(panAngle);
            rightGain = std::sin(panAngle);
        }
        
        // Apply pan to left and right channels
        juce::FloatVectorOperations::multiply(buffer.getWritePointer(0), leftGain, numSamples);
        juce::FloatVectorOperations::multiply(buffer.getWritePointer(1), rightGain, numSamples);
    }
    
    // Apply polarity inversion if enabled
    if (channelStrip_.polarityInverted) {
        for (int ch = 0; ch < numChannels; ++ch) {
            if (buffer.getWritePointer(ch)) {
                juce::FloatVectorOperations::multiply(buffer.getWritePointer(ch), -1.0f, numSamples);
            }
        }
    }
    
    // Apply trim gain
    float trimGain = dbToLinear(channelStrip_.gain);
    if (trimGain != 1.0f) {
        for (int ch = 0; ch < numChannels; ++ch) {
            if (buffer.getWritePointer(ch)) {
                juce::FloatVectorOperations::multiply(buffer.getWritePointer(ch), trimGain, numSamples);
            }
        }
    }
}

void LogicMixerChannel::applySends(juce::AudioBuffer<float>& buffer, 
                                  std::map<std::string, juce::AudioBuffer<float>>& sendBuffers) {
    if (buffer.getNumSamples() == 0) return;
    
    for (int i = 0; i < 8; ++i) {
        if (channelStrip_.sendLevel[i] > -60.0f && !channelStrip_.sendTargets[i].empty()) {
            float sendGain = dbToLinear(channelStrip_.sendLevel[i]);
            
            // Get or create send buffer
            auto& sendBuffer = sendBuffers[channelStrip_.sendTargets[i]];
            if (sendBuffer.getNumSamples() != buffer.getNumSamples() || 
                sendBuffer.getNumChannels() != buffer.getNumChannels()) {
                sendBuffer.setSize(buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);
            }
            
            // Add to send buffer (pre or post fader)
            if (channelStrip_.sendPreFader[i]) {
                // Pre-fader: use original buffer (before volume/pan)
                sendBuffer.addFrom(0, 0, buffer, 0, 0, buffer.getNumSamples(), sendGain);
                if (buffer.getNumChannels() > 1) {
                    sendBuffer.addFrom(1, 0, buffer, 1, 0, buffer.getNumSamples(), sendGain);
                }
            } else {
                // Post-fader: use processed buffer
                sendBuffer.addFrom(0, 0, buffer, 0, 0, buffer.getNumSamples(), sendGain);
                if (buffer.getNumChannels() > 1) {
                    sendBuffer.addFrom(1, 0, buffer, 1, 0, buffer.getNumSamples(), sendGain);
                }
            }
        }
    }
}

void LogicMixerChannel::setVolume(float dB) {
    channelStrip_.volume = std::clamp(dB, -60.0f, 12.0f);
}

void LogicMixerChannel::setPan(float pan) {
    channelStrip_.pan = std::clamp(pan, -1.0f, 1.0f);
}

void LogicMixerChannel::setMute(bool muted) {
    channelStrip_.mute = muted;
}

void LogicMixerChannel::setSolo(bool soloed) {
    channelStrip_.solo = soloed;
}

void LogicMixerChannel::setRecordArm(bool armed) {
    channelStrip_.recordArm = armed;
}

void LogicMixerChannel::setSendLevel(int sendIndex, float level) {
    if (sendIndex >= 0 && sendIndex < 8) {
        channelStrip_.sendLevel[sendIndex] = std::clamp(level, -60.0f, 12.0f);
    }
}

void LogicMixerChannel::setSendPreFader(int sendIndex, bool preFader) {
    if (sendIndex >= 0 && sendIndex < 8) {
        channelStrip_.sendPreFader[sendIndex] = preFader;
    }
}

void LogicMixerChannel::setSendTarget(int sendIndex, const std::string& target) {
    if (sendIndex >= 0 && sendIndex < 8) {
        channelStrip_.sendTargets[sendIndex] = target;
    }
}

void LogicMixerChannel::insertPlugin(int slot, const std::string& pluginId) {
    if (slot >= 0 && slot < kPluginSlots) {
        channelStrip_.pluginChain[slot] = pluginId;
    }
}

void LogicMixerChannel::removePlugin(int slot) {
    if (slot >= 0 && slot < kPluginSlots) {
        channelStrip_.pluginChain[slot].clear();
        channelStrip_.pluginBypass[slot] = false;
    }
}

void LogicMixerChannel::bypassPlugin(int slot, bool bypassed) {
    if (slot >= 0 && slot < kPluginSlots) {
        channelStrip_.pluginBypass[slot] = bypassed;
    }
}

void LogicMixerChannel::swapPlugins(int slot1, int slot2) {
    if (slot1 >= 0 && slot1 < kPluginSlots && slot2 >= 0 && slot2 < kPluginSlots) {
        std::swap(channelStrip_.pluginChain[slot1], channelStrip_.pluginChain[slot2]);
        std::swap(channelStrip_.pluginBypass[slot1], channelStrip_.pluginBypass[slot2]);
    }
}

void LogicMixerChannel::assignToVCA(const std::string& vcaId) {
    channelStrip_.vcaAssignment = vcaId;
}

void LogicMixerChannel::unassignFromVCA() {
    channelStrip_.vcaAssignment.clear();
}

void LogicMixerChannel::addAutomationLane(const std::string& parameter, std::shared_ptr<AutomationLane> lane) {
    channelStrip_.automationLanes[parameter] = lane;
}

void LogicMixerChannel::removeAutomationLane(const std::string& parameter) {
    channelStrip_.automationLanes.erase(parameter);
}

void LogicMixerChannel::updateSmoothers() {
    currentVolume_ = volumeSmoother_;
    currentPan_ = panSmoother_;
}

float LogicMixerChannel::dbToLinear(float dB) const {
    if (dB <= -60.0f) return 0.0f;
    return std::pow(10.0f, dB / 20.0f);
}

float LogicMixerChannel::linearToDb(float linear) const {
    if (linear <= 0.0f) return -60.0f;
    return 20.0f * std::log10(linear);
}

// LogicEnvironment implementation
LogicEnvironment::LogicEnvironment() {
    // Create default stereo output bus
    Bus masterBus;
    masterBus.id = "master";
    masterBus.name = "Stereo Out";
    masterBus.busNumber = 1;
    masterBus.volume = 0.0f;
    masterBus.pan = 0.0f;
    masterBus.mute = false;
    buses_.push_back(masterBus);
}

std::string LogicEnvironment::createBus(const std::string& name) {
    Bus bus;
    bus.id = "bus_" + std::to_string(nextBusNumber_);
    bus.name = name;
    bus.busNumber = nextBusNumber_++;
    bus.volume = 0.0f;
    bus.pan = 0.0f;
    bus.mute = false;
    
    buses_.push_back(bus);
    return bus.id;
}

void LogicEnvironment::removeBus(const std::string& busId) {
    auto it = std::find_if(buses_.begin(), buses_.end(),
        [&busId](const Bus& bus) { return bus.id == busId; });
    
    if (it != buses_.end()) {
        // Remove all track assignments to this bus
        for (const auto& trackId : it->inputTracks) {
            auto trackIt = busInputs_.find(it->id);
            if (trackIt != busInputs_.end()) {
                auto& tracks = trackIt->second;
                tracks.erase(std::remove(tracks.begin(), tracks.end(), trackId), tracks.end());
            }
        }
        
        buses_.erase(it);
    }
}

void LogicEnvironment::assignTrackToBus(const std::string& trackId, const std::string& busId) {
    // Remove from current bus assignment
    for (auto& bus : buses_) {
        auto it = std::find(bus.inputTracks.begin(), bus.inputTracks.end(), trackId);
        if (it != bus.inputTracks.end()) {
            bus.inputTracks.erase(it);
        }
    }
    
    // Add to new bus
    auto it = std::find_if(buses_.begin(), buses_.end(),
        [&busId](const Bus& bus) { return bus.id == busId; });
    
    if (it != buses_.end()) {
        it->inputTracks.push_back(trackId);
        busInputs_[trackId] = {busId};
    }
}

void LogicEnvironment::removeTrackFromBus(const std::string& trackId, const std::string& busId) {
    auto it = std::find_if(buses_.begin(), buses_.end(),
        [&busId](const Bus& bus) { return bus.id == busId; });
    
    if (it != buses_.end()) {
        auto trackIt = std::find(it->inputTracks.begin(), it->inputTracks.end(), trackId);
        if (trackIt != it->inputTracks.end()) {
            it->inputTracks.erase(trackIt);
        }
    }
    
    busInputs_.erase(trackId);
}

std::string LogicEnvironment::createVCA(const std::string& name) {
    VCA vca;
    vca.id = "vca_" + std::to_string(nextVCAId_);
    vca.name = name;
    vca.volume = 0.0f;
    vca.mute = false;
    
    vcas_.push_back(vca);
    return vca.id;
}

void LogicEnvironment::removeVCA(const std::string& vcaId) {
    auto it = std::find_if(vcas_.begin(), vcas_.end(),
        [&vcaId](const VCA& vca) { return vca.id == vcaId; });
    
    if (it != vcas_.end()) {
        // Remove all track assignments to this VCA
        for (const auto& trackId : it->assignedTracks) {
            auto trackIt = vcaAssignments_.find(it->id);
            if (trackIt != vcaAssignments_.end()) {
                auto& tracks = trackIt->second;
                tracks.erase(std::remove(tracks.begin(), tracks.end(), trackId), tracks.end());
            }
        }
        
        vcas_.erase(it);
    }
}

void LogicEnvironment::assignTrackToVCA(const std::string& trackId, const std::string& vcaId) {
    // Remove from current VCA assignment
    for (auto& vca : vcas_) {
        auto it = std::find(vca.assignedTracks.begin(), vca.assignedTracks.end(), trackId);
        if (it != vca.assignedTracks.end()) {
            vca.assignedTracks.erase(it);
        }
    }
    
    // Add to new VCA
    auto it = std::find_if(vcas_.begin(), vcas_.end(),
        [&vcaId](const VCA& vca) { return vca.id == vcaId; });
    
    if (it != vcas_.end()) {
        it->assignedTracks.push_back(trackId);
        vcaAssignments_[trackId] = {vcaId};
    }
}

void LogicEnvironment::removeTrackFromVCA(const std::string& trackId, const std::string& vcaId) {
    auto it = std::find_if(vcas_.begin(), vcas_.end(),
        [&vcaId](const VCA& vca) { return vca.id == vcaId; });
    
    if (it != vcas_.end()) {
        auto trackIt = std::find(it->assignedTracks.begin(), it->assignedTracks.end(), trackId);
        if (trackIt != it->assignedTracks.end()) {
            it->assignedTracks.erase(trackIt);
        }
    }
    
    vcaAssignments_.erase(trackId);
}

void LogicEnvironment::setTrackOutput(const std::string& trackId, const std::string& destination) {
    trackOutputs_[trackId] = destination;
}

std::string LogicEnvironment::getTrackOutput(const std::string& trackId) const {
    auto it = trackOutputs_.find(trackId);
    return (it != trackOutputs_.end()) ? it->second : "master";
}

void LogicEnvironment::processEnvironment(std::map<std::string, juce::AudioBuffer<float>>& trackBuffers,
                                         std::map<std::string, juce::AudioBuffer<float>>& busBuffers,
                                         int numSamples) {
    // Process buses by summing assigned tracks
    for (auto& bus : buses_) {
        if (bus.id == "master") continue; // Handle master separately
        
        auto& busBuffer = busBuffers[bus.id];
        busBuffer.setSize(2, numSamples, false, false, true);
        busBuffer.clear();
        
        // Sum all tracks assigned to this bus
        for (const auto& trackId : bus.inputTracks) {
            auto trackIt = trackBuffers.find(trackId);
            if (trackIt != trackBuffers.end()) {
                busBuffer.addFrom(0, 0, trackIt->second, 0, 0, numSamples);
                if (trackIt->second.getNumChannels() > 1) {
                    busBuffer.addFrom(1, 0, trackIt->second, 1, 0, numSamples);
                }
            }
        }
        
        // Apply bus processing (volume, pan, etc.)
        // This would be done by a LogicMixerChannel for the bus
    }
    
    // Process VCA groups
    for (auto& vca : vcas_) {
        if (!vca.mute && !vca.assignedTracks.empty()) {
            float vcaGain = std::pow(10.0f, vca.volume / 20.0f);
            
            // Apply VCA gain to all assigned tracks
            for (const auto& trackId : vca.assignedTracks) {
                auto trackIt = trackBuffers.find(trackId);
                if (trackIt != trackBuffers.end()) {
                    juce::FloatVectorOperations::multiply(trackIt->second.getWritePointer(0), 
                                                        vcaGain, numSamples);
                    if (trackIt->second.getNumChannels() > 1) {
                        juce::FloatVectorOperations::multiply(trackIt->second.getWritePointer(1), 
                                                            vcaGain, numSamples);
                    }
                }
            }
        }
    }
    
    // Route tracks to their outputs
    std::map<std::string, juce::AudioBuffer<float>> outputBuffers;
    
    for (const auto& [trackId, buffer] : trackBuffers) {
        std::string outputId = getTrackOutput(trackId);
        
        auto& outputBuffer = outputBuffers[outputId];
        if (outputBuffer.getNumSamples() != numSamples || outputBuffer.getNumChannels() != buffer.getNumChannels()) {
            outputBuffer.setSize(buffer.getNumChannels(), numSamples, false, false, true);
            outputBuffer.clear();
        }
        
        outputBuffer.addFrom(0, 0, buffer, 0, 0, numSamples);
        if (buffer.getNumChannels() > 1) {
            outputBuffer.addFrom(1, 0, buffer, 1, 0, numSamples);
        }
    }
    
    // Add bus outputs to their destinations
    for (const auto& [busId, buffer] : busBuffers) {
        auto busIt = std::find_if(buses_.begin(), buses_.end(),
            [&busId](const Bus& bus) { return bus.id == busId; });
        
        if (busIt != buses_.end()) {
            std::string outputId = busIt->busNumber == 1 ? "master" : "bus_" + std::to_string(busIt->busNumber);
            
            auto& outputBuffer = outputBuffers[outputId];
            if (outputBuffer.getNumSamples() != numSamples || outputBuffer.getNumChannels() != buffer.getNumChannels()) {
                outputBuffer.setSize(buffer.getNumChannels(), numSamples, false, false, true);
                outputBuffer.clear();
            }
            
            outputBuffer.addFrom(0, 0, buffer, 0, 0, numSamples);
            if (buffer.getNumChannels() > 1) {
                outputBuffer.addFrom(1, 0, buffer, 1, 0, numSamples);
            }
        }
    }
    
    // Update trackBuffers with routed outputs
    trackBuffers = std::move(outputBuffers);
}

} // namespace neurato
