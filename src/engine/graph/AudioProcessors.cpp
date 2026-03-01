#include "AudioProcessors.hpp"
#include <algorithm>
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>

namespace ampl {

// GainNode implementation
GainNode::GainNode(const std::string& id) : AudioNode(Type::Gain, id) {
    setOutputChannelCount(2);
}

void GainNode::process(AudioBuffer& input, AudioBuffer& output,
                      int numSamples, SampleCount position) noexcept {
    if (isBypassed() || !input.channels || !output.channels) {
        if (input.channels && output.channels) {
            output.copyFrom(input);
        }
        return;
    }

    // Get automated values
    float targetGain = gain_;
    float targetPan = pan_;

    auto gainLane = getAutomationLane("gain");
    if (gainLane) {
        targetGain = gainLane->getValueAt(position);
    }

    auto panLane = getAutomationLane("pan");
    if (panLane) {
        targetPan = panLane->getValueAt(position);
    }

    // Smooth parameter changes
    gainSmoother_ = gainSmoother_ * kSmoothingCoeff + targetGain * (1.0f - kSmoothingCoeff);
    panSmoother_ = panSmoother_ * kSmoothingCoeff + targetPan * (1.0f - kSmoothingCoeff);

    currentGain_ = gainSmoother_;
    currentPan_ = panSmoother_;

    // Calculate pan coefficients (constant power panning)
    float panAngle = (currentPan_ + 1.0f) * juce::MathConstants<float>::pi / 4.0f;
    float leftGain = std::cos(panAngle);
    float rightGain = std::sin(panAngle);

    // Process samples
    for (int ch = 0; ch < std::min(input.numChannels, output.numChannels); ++ch) {
        if (!input.channels[ch] || !output.channels[ch]) continue;

        float channelGain = currentGain_;
        if (ch == 0) channelGain *= leftGain;
        if (ch == 1) channelGain *= rightGain;

        juce::FloatVectorOperations::multiply(output.channels[ch],
                                            input.channels[ch],
                                            channelGain,
                                            numSamples);
    }
}

std::vector<ParameterInfo> GainNode::getParameters() const {
    return {
        {"gain", "Gain", 0.0f, 2.0f, 1.0f, true, "linear"},
        {"pan", "Pan", -1.0f, 1.0f, 0.0f, true, "center"}
    };
}

float GainNode::getParameterValue(const std::string& paramId) const {
    if (paramId == "gain") return gain_;
    if (paramId == "pan") return pan_;
    return 0.0f;
}

void GainNode::setParameterValue(const std::string& paramId, float value) {
    if (paramId == "gain") gain_ = std::clamp(value, 0.0f, 2.0f);
    if (paramId == "pan") pan_ = std::clamp(value, -1.0f, 1.0f);
}

void GainNode::prepareToPlay(double sampleRate, int samplesPerBlock) {
    gainSmoother_ = gain_;
    panSmoother_ = pan_;
}

void GainNode::reset() {
    gainSmoother_ = gain_;
    panSmoother_ = pan_;
}

// EQNode implementation
EQNode::EQNode(const std::string& id) : AudioNode(Type::EQ, id) {
    setOutputChannelCount(2);

    // Initialize default EQ bands
    bands_[0] = {80.0f, 0.0f, 1.0f, true};   // Low shelf
    bands_[1] = {250.0f, 0.0f, 1.0f, true};  // Low-mid
    bands_[2] = {1000.0f, 0.0f, 1.0f, true}; // Mid
    bands_[3] = {8000.0f, 0.0f, 1.0f, true}; // High shelf
}

void EQNode::process(AudioBuffer& input, AudioBuffer& output,
                    int numSamples, SampleCount position) noexcept {
    if (isBypassed() || !input.channels || !output.channels) {
        if (input.channels && output.channels) {
            output.copyFrom(input);
        }
        return;
    }

    // Process each channel
    for (int ch = 0; ch < std::min(input.numChannels, output.numChannels); ++ch) {
        if (!input.channels[ch] || !output.channels[ch]) continue;

        // Copy input to output
        juce::FloatVectorOperations::copy(output.channels[ch],
                                        input.channels[ch],
                                        numSamples);

        // Apply each EQ band
        for (auto& band : bands_) {
            if (!band.enabled) continue;

            for (int i = 0; i < numSamples; ++i) {
                output.channels[ch][i] = processBiquad(band, output.channels[ch][i]);
            }
        }
    }
}

std::vector<ParameterInfo> EQNode::getParameters() const {
    std::vector<ParameterInfo> params;
    for (int i = 0; i < 4; ++i) {
        params.push_back({"band" + std::to_string(i) + "_freq", "Band " + std::to_string(i) + " Freq",
                         20.0f, 20000.0f, bands_[i].frequency, true, "Hz"});
        params.push_back({"band" + std::to_string(i) + "_gain", "Band " + std::to_string(i) + " Gain",
                         -24.0f, 24.0f, bands_[i].gain, true, "dB"});
        params.push_back({"band" + std::to_string(i) + "_q", "Band " + std::to_string(i) + " Q",
                         0.1f, 10.0f, bands_[i].q, true, ""});
        params.push_back({"band" + std::to_string(i) + "_enabled", "Band " + std::to_string(i) + " Enable",
                         0.0f, 1.0f, bands_[i].enabled ? 1.0f : 0.0f, true, ""});
    }
    return params;
}

float EQNode::getParameterValue(const std::string& paramId) const {
    for (int i = 0; i < 4; ++i) {
        std::string prefix = "band" + std::to_string(i) + "_";
        if (paramId == prefix + "freq") return bands_[i].frequency;
        if (paramId == prefix + "gain") return bands_[i].gain;
        if (paramId == prefix + "q") return bands_[i].q;
        if (paramId == prefix + "enabled") return bands_[i].enabled ? 1.0f : 0.0f;
    }
    return 0.0f;
}

void EQNode::setParameterValue(const std::string& paramId, float value) {
    for (int i = 0; i < 4; ++i) {
        std::string prefix = "band" + std::to_string(i) + "_";
        if (paramId == prefix + "freq") {
            bands_[i].frequency = std::clamp(value, 20.0f, 20000.0f);
            calculateBiquadCoeffs(bands_[i]);
        }
        if (paramId == prefix + "gain") {
            bands_[i].gain = std::clamp(value, -24.0f, 24.0f);
            calculateBiquadCoeffs(bands_[i]);
        }
        if (paramId == prefix + "q") {
            bands_[i].q = std::clamp(value, 0.1f, 10.0f);
            calculateBiquadCoeffs(bands_[i]);
        }
        if (paramId == prefix + "enabled") {
            bands_[i].enabled = value > 0.5f;
        }
    }
}

void EQNode::prepareToPlay(double sampleRate, int samplesPerBlock) {
    sampleRate_ = sampleRate;
    for (auto& band : bands_) {
        calculateBiquadCoeffs(band);
    }
}

void EQNode::reset() {
    for (auto& band : bands_) {
        band.z1 = 0.0f;
        band.z2 = 0.0f;
    }
}

void EQNode::calculateBiquadCoeffs(EQBand& band) {
    double sampleRate = sampleRate_;
    double omega = 2.0 * juce::MathConstants<double>::pi * band.frequency / sampleRate;
    double sinOmega = std::sin(omega);
    double cosOmega = std::cos(omega);

    // Convert gain from dB to linear
    double A = std::pow(10.0, band.gain / 40.0);
    double alpha = sinOmega / (2.0 * band.q);

    // Peaking EQ filter coefficients
    band.b0 = 1.0 + alpha * A;
    band.b1 = -2.0 * cosOmega;
    band.b2 = 1.0 - alpha * A;
    band.a0 = 1.0 + alpha / A;
    band.a1 = -2.0 * cosOmega;
    band.a2 = 1.0 - alpha / A;

    // Normalize
    band.b0 /= band.a0;
    band.b1 /= band.a0;
    band.b2 /= band.a0;
    band.a1 /= band.a0;
    band.a2 /= band.a0;
}

float EQNode::processBiquad(EQBand& band, float input) {
    float output = band.b0 * input + band.b1 * band.z1 + band.b2 * band.z2
                   - band.a1 * band.z1 - band.a2 * band.z2;

    band.z2 = band.z1;
    band.z1 = output;

    return output;
}

// CompressorNode implementation
CompressorNode::CompressorNode(const std::string& id) : AudioNode(Type::Compressor, id) {
    setOutputChannelCount(2);
    rmsBuffer_.fill(0.0f);
}

void CompressorNode::process(AudioBuffer& input, AudioBuffer& output,
                           int numSamples, SampleCount position) noexcept {
    if (isBypassed() || !enabled_ || !input.channels || !output.channels) {
        if (input.channels && output.channels) {
            output.copyFrom(input);
        }
        return;
    }

    // Copy input to output
    for (int ch = 0; ch < std::min(input.numChannels, output.numChannels); ++ch) {
        if (input.channels[ch] && output.channels[ch]) {
            juce::FloatVectorOperations::copy(output.channels[ch],
                                            input.channels[ch],
                                            numSamples);
        }
    }

    // Process left channel (or mono)
    if (input.channels[0]) {
        for (int i = 0; i < numSamples; ++i) {
            float inputSample = input.channels[0][i];

            // RMS detection
            float rmsLevel = processRMS(inputSample);
            float inputDb = juce::Decibels::gainToDecibels(rmsLevel + 0.0001f);

            // Calculate gain reduction
            float gainReductionDb = calculateGainReduction(inputDb);
            float gainReductionLinear = juce::Decibels::decibelsToGain(gainReductionDb);

            // Update envelope
            float targetEnvelope = gainReductionLinear;
            if (targetEnvelope < envelope_) {
                envelope_ = envelope_ * attackCoeff_ + targetEnvelope * (1.0f - attackCoeff_);
            } else {
                envelope_ = envelope_ * releaseCoeff_ + targetEnvelope * (1.0f - releaseCoeff_);
            }

            // Apply gain reduction to all channels
            for (int ch = 0; ch < output.numChannels; ++ch) {
                if (output.channels[ch]) {
                    output.channels[ch][i] *= envelope_;
                }
            }
        }
    }

    // Apply makeup gain
    float makeupGainValue = makeupGain_.load();
    float makeupGainLinear = juce::Decibels::decibelsToGain(makeupGainValue);
    for (int ch = 0; ch < output.numChannels; ++ch) {
        if (output.channels[ch]) {
            juce::FloatVectorOperations::multiply(output.channels[ch],
                                                makeupGainLinear,
                                                numSamples);
        }
    }

    gainReduction_ = juce::Decibels::gainToDecibels(envelope_);
}

std::vector<ParameterInfo> CompressorNode::getParameters() const {
    return {
        {"threshold", "Threshold", -60.0f, 0.0f, -20.0f, true, "dB"},
        {"ratio", "Ratio", 1.0f, 20.0f, 4.0f, true, ":1"},
        {"attack", "Attack", 0.1f, 100.0f, 5.0f, true, "ms"},
        {"release", "Release", 1.0f, 1000.0f, 50.0f, true, "ms"},
        {"knee", "Knee", 0.0f, 10.0f, 2.0f, true, "dB"},
        {"makeup", "Makeup", 0.0f, 24.0f, 0.0f, true, "dB"},
        {"enabled", "Enabled", 0.0f, 1.0f, 1.0f, true, ""}
    };
}

float CompressorNode::getParameterValue(const std::string& paramId) const {
    if (paramId == "threshold") return threshold_;
    if (paramId == "ratio") return ratio_;
    if (paramId == "attack") return attack_;
    if (paramId == "release") return release_;
    if (paramId == "knee") return knee_;
    if (paramId == "makeup") return makeupGain_;
    if (paramId == "enabled") return enabled_ ? 1.0f : 0.0f;
    return 0.0f;
}

void CompressorNode::setParameterValue(const std::string& paramId, float value) {
    if (paramId == "threshold") {
        threshold_ = std::clamp(value, -60.0f, 0.0f);
    } else if (paramId == "ratio") {
        ratio_ = std::clamp(value, 1.0f, 20.0f);
    } else if (paramId == "attack") {
        attack_ = std::clamp(value, 0.1f, 100.0f);
        updateCoefficients();
    } else if (paramId == "release") {
        release_ = std::clamp(value, 1.0f, 1000.0f);
        updateCoefficients();
    } else if (paramId == "knee") {
        knee_ = std::clamp(value, 0.0f, 10.0f);
    } else if (paramId == "makeup") {
        makeupGain_ = std::clamp(value, 0.0f, 24.0f);
    } else if (paramId == "enabled") {
        enabled_ = value > 0.5f;
    }
}

void CompressorNode::prepareToPlay(double sampleRate, int samplesPerBlock) {
    sampleRate_ = sampleRate;
    updateCoefficients();
    reset();
}

void CompressorNode::reset() {
    envelope_ = 1.0f;
    gainReduction_ = 0.0f;
    rmsBuffer_.fill(0.0f);
    rmsBufferPos_ = 0;
    rmsSum_ = 0.0f;
}

void CompressorNode::updateCoefficients() {
    double sampleRate = sampleRate_;
    attackCoeff_ = std::exp(-1.0 / (sampleRate * attack_ * 0.001));
    releaseCoeff_ = std::exp(-1.0 / (sampleRate * release_ * 0.001));
}

float CompressorNode::calculateGainReduction(float inputLevel) const {
    float kneeHalf = knee_ * 0.5f;

    // Soft knee calculation
    if (inputLevel < threshold_ - kneeHalf) {
        return 0.0f; // No compression
    } else if (inputLevel > threshold_ + kneeHalf) {
        // Above knee range
        float slope = 1.0f - (1.0f / ratio_);
        return slope * (inputLevel - threshold_);
    } else {
        // Within knee range
        float kneeRatio = (inputLevel - (threshold_ - kneeHalf)) / knee_;
        float slope = kneeRatio * (1.0f - (1.0f / ratio_));
        return slope * (inputLevel - threshold_);
    }
}

float CompressorNode::processRMS(float input) {
    // Add to RMS buffer
    rmsSum_ -= rmsBuffer_[rmsBufferPos_];
    rmsBuffer_[rmsBufferPos_] = input * input;
    rmsSum_ += rmsBuffer_[rmsBufferPos_];
    rmsBufferPos_ = (rmsBufferPos_ + 1) % kRMSBufferSize;

    return std::sqrt(rmsSum_ / kRMSBufferSize);
}

// TrackInputNode implementation
TrackInputNode::TrackInputNode(const std::string& id) : AudioNode(Type::TrackInput, id) {
    setOutputChannelCount(2);
}

void TrackInputNode::process(AudioBuffer& input, AudioBuffer& output,
                           int numSamples, SampleCount position) noexcept {
    if (isBypassed() || !output.channels) {
        return;
    }

    // Clear output
    output.clear();

    // Check if we're within the clip region
    if (position < clipStart_ || position >= clipStart_ + clipLength_) {
        return;
    }

    // Calculate source position
    SampleCount sourcePos = (position - clipStart_) + sourceStart_;

    // Check bounds
    if (sourcePos < 0 || sourcePos >= assetLength_) {
        return;
    }

    // Copy audio data
    int samplesToCopy = std::min(numSamples,
                               static_cast<int>(assetLength_ - sourcePos));

    for (int ch = 0; ch < std::min(numChannels_, output.numChannels); ++ch) {
        if (!output.channels[ch]) continue;

        const float* sourceChannel = (ch == 0) ? leftChannel_ : rightChannel_;
        if (!sourceChannel) continue;

        juce::FloatVectorOperations::copy(output.channels[ch],
                                        sourceChannel + sourcePos,
                                        samplesToCopy);

        // Apply clip gain
        juce::FloatVectorOperations::multiply(output.channels[ch],
                                            clipGain_,
                                            samplesToCopy);
    }
}

void TrackInputNode::setAudioData(const float* leftChannel, const float* rightChannel,
                                 int numChannels, SampleCount length) {
    leftChannel_ = leftChannel;
    rightChannel_ = rightChannel;
    numChannels_ = numChannels;
    assetLength_ = length;
}

void TrackInputNode::setClipRegion(SampleCount start, SampleCount length,
                                  SampleCount sourceStart, float gain) {
    clipStart_ = start;
    clipLength_ = length;
    sourceStart_ = sourceStart;
    clipGain_ = gain;
}

// TrackOutputNode implementation
TrackOutputNode::TrackOutputNode(const std::string& id) : AudioNode(Type::TrackOutput, id) {
    setOutputChannelCount(2);
}

void TrackOutputNode::process(AudioBuffer& input, AudioBuffer& output,
                            int numSamples, SampleCount position) noexcept {
    if (isBypassed() || muted_ || !input.channels || !output.channels) {
        if (input.channels && output.channels && !muted_) {
            output.copyFrom(input);
        }
        return;
    }

    // Get automated values
    float targetGain = gain_;
    float targetPan = pan_;

    auto gainLane = getAutomationLane("gain");
    if (gainLane) {
        targetGain = gainLane->getValueAt(position);
    }

    auto panLane = getAutomationLane("pan");
    if (panLane) {
        targetPan = panLane->getValueAt(position);
    }

    // Smooth parameter changes
    gainSmoother_ = gainSmoother_ * kSmoothingCoeff + targetGain * (1.0f - kSmoothingCoeff);
    panSmoother_ = panSmoother_ * kSmoothingCoeff + targetPan * (1.0f - kSmoothingCoeff);

    currentGain_ = gainSmoother_;
    currentPan_ = panSmoother_;

    // Calculate pan coefficients
    float panAngle = (currentPan_ + 1.0f) * juce::MathConstants<float>::pi / 4.0f;
    float leftGain = std::cos(panAngle);
    float rightGain = std::sin(panAngle);

    // Process samples
    for (int ch = 0; ch < std::min(input.numChannels, output.numChannels); ++ch) {
        if (!input.channels[ch] || !output.channels[ch]) continue;

        float channelGain = currentGain_;
        if (ch == 0) channelGain *= leftGain;
        if (ch == 1) channelGain *= rightGain;

        juce::FloatVectorOperations::multiply(output.channels[ch],
                                            input.channels[ch],
                                            channelGain,
                                            numSamples);
    }
}

std::vector<ParameterInfo> TrackOutputNode::getParameters() const {
    return {
        {"gain", "Gain", 0.0f, 2.0f, 1.0f, true, "linear"},
        {"pan", "Pan", -1.0f, 1.0f, 0.0f, true, "center"}
    };
}

float TrackOutputNode::getParameterValue(const std::string& paramId) const {
    if (paramId == "gain") return gain_;
    if (paramId == "pan") return pan_;
    return 0.0f;
}

void TrackOutputNode::setParameterValue(const std::string& paramId, float value) {
    if (paramId == "gain") gain_ = std::clamp(value, 0.0f, 2.0f);
    if (paramId == "pan") pan_ = std::clamp(value, -1.0f, 1.0f);
}

// MixerNode implementation
MixerNode::MixerNode(const std::string& id) : AudioNode(Type::Mixer, id) {
    setOutputChannelCount(2);
}

void MixerNode::process(AudioBuffer& input, AudioBuffer& output,
                       int numSamples, SampleCount position) noexcept {
    if (isBypassed() || !input.channels || !output.channels) {
        if (input.channels && output.channels) {
            output.copyFrom(input);
        }
        return;
    }

    // Copy input to output (summing happens in AudioGraph)
    output.copyFrom(input);

    // Apply master gain and pan
    float targetGain = masterGain_;
    float targetPan = masterPan_;

    auto gainLane = getAutomationLane("masterGain");
    if (gainLane) {
        targetGain = gainLane->getValueAt(position);
    }

    auto panLane = getAutomationLane("masterPan");
    if (panLane) {
        targetPan = panLane->getValueAt(position);
    }

    // Smooth parameter changes
    gainSmoother_ = gainSmoother_ * kSmoothingCoeff + targetGain * (1.0f - kSmoothingCoeff);
    panSmoother_ = panSmoother_ * kSmoothingCoeff + targetPan * (1.0f - kSmoothingCoeff);

    currentMasterGain_ = gainSmoother_;
    currentMasterPan_ = panSmoother_;

    // Calculate pan coefficients
    float panAngle = (currentMasterPan_ + 1.0f) * juce::MathConstants<float>::pi / 4.0f;
    float leftGain = std::cos(panAngle);
    float rightGain = std::sin(panAngle);

    // Apply master processing
    for (int ch = 0; ch < output.numChannels; ++ch) {
        if (!output.channels[ch]) continue;

        float channelGain = currentMasterGain_;
        if (ch == 0) channelGain *= leftGain;
        if (ch == 1) channelGain *= rightGain;

        juce::FloatVectorOperations::multiply(output.channels[ch],
                                            channelGain,
                                            numSamples);
    }
}

std::vector<ParameterInfo> MixerNode::getParameters() const {
    return {
        {"masterGain", "Master Gain", 0.0f, 2.0f, 1.0f, true, "linear"},
        {"masterPan", "Master Pan", -1.0f, 1.0f, 0.0f, true, "center"}
    };
}

float MixerNode::getParameterValue(const std::string& paramId) const {
    if (paramId == "masterGain") return masterGain_;
    if (paramId == "masterPan") return masterPan_;
    return 0.0f;
}

void MixerNode::setParameterValue(const std::string& paramId, float value) {
    if (paramId == "masterGain") masterGain_ = std::clamp(value, 0.0f, 2.0f);
    if (paramId == "masterPan") masterPan_ = std::clamp(value, -1.0f, 1.0f);
}

// LatencyCompensatorNode implementation
LatencyCompensatorNode::LatencyCompensatorNode(const std::string& id)
    : AudioNode(Type::LatencyCompensator, id) {
    setOutputChannelCount(2);
}

void LatencyCompensatorNode::process(AudioBuffer& input, AudioBuffer& output,
                                   int numSamples, SampleCount position) noexcept {
    if (isBypassed() || !input.channels || !output.channels || delaySamples_ == 0) {
        if (input.channels && output.channels) {
            output.copyFrom(input);
        }
        return;
    }

    int delay = delaySamples_;

    // Process each channel
    for (int ch = 0; ch < std::min(input.numChannels, output.numChannels); ++ch) {
        if (!input.channels[ch] || !output.channels[ch]) continue;

        float* delayBuffer = delayBuffers_[ch].data();

        // Write input to delay buffer
        for (int i = 0; i < numSamples; ++i) {
            delayBuffer[writePos_] = input.channels[ch][i];

            // Read delayed sample
            int readPos = (writePos_ - delay + bufferSize_) % bufferSize_;
            output.channels[ch][i] = delayBuffer[readPos];

            writePos_ = (writePos_ + 1) % bufferSize_;
        }
    }
}

void LatencyCompensatorNode::setDelaySamples(int delaySamples) {
    delaySamples_ = std::max(0, delaySamples);
}

void LatencyCompensatorNode::prepareToPlay(double sampleRate, int samplesPerBlock) {
    bufferSize_ = samplesPerBlock * 4; // 4x buffer size for safety
    delayBuffers_.resize(2, std::vector<float>(bufferSize_, 0.0f));
    writePos_ = 0;
}

void LatencyCompensatorNode::reset() {
    for (auto& buffer : delayBuffers_) {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
    }
    writePos_ = 0;
}

} // namespace ampl
