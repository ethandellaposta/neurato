#pragma once

#include "AudioGraph.h"
#include "Automation.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include <array>

namespace neurato {

// Gain processor with automation support
class GainNode : public AudioNode {
public:
    GainNode(const std::string& id);
    ~GainNode() override = default;
    
    void process(AudioBuffer& input, AudioBuffer& output, 
                int numSamples, SampleCount position) noexcept override;
    
    std::vector<ParameterInfo> getParameters() const override;
    float getParameterValue(const std::string& paramId) const override;
    void setParameterValue(const std::string& paramId, float value) override;
    
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void reset() override;

private:
    std::atomic<float> gain_{1.0f};
    std::atomic<float> pan_{0.0f};  // -1.0 to 1.0
    float currentGain_{1.0f};
    float currentPan_{0.0f};
    
    // Smoothing for parameter changes
    float gainSmoother_{1.0f};
    float panSmoother_{0.0f};
    static constexpr float kSmoothingCoeff = 0.999f;
};

// 4-band parametric EQ
class EQNode : public AudioNode {
public:
    struct EQBand {
        float frequency{1000.0f};
        float gain{0.0f};        // dB
        float q{1.0f};           // Quality factor
        bool enabled{true};
        
        // Biquad filter state
        float z1{0.0f}, z2{0.0f};
        float a0{1.0f}, a1{0.0f}, a2{0.0f};
        float b1{0.0f}, b2{0.0f};
    };
    
    EQNode(const std::string& id);
    ~EQNode() override = default;
    
    void process(AudioBuffer& input, AudioBuffer& output, 
                int numSamples, SampleCount position) noexcept override;
    
    std::vector<ParameterInfo> getParameters() const override;
    float getParameterValue(const std::string& paramId) const override;
    void setParameterValue(const std::string& paramId, float value) override;
    
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void reset() override;
    
    // Band access
    EQBand& getBand(int bandIndex) { return bands_[bandIndex]; }
    const EQBand& getBand(int bandIndex) const { return bands_[bandIndex]; }

private:
    std::array<EQBand, 4> bands_;
    double sampleRate_{44100.0};
    
    void calculateBiquadCoeffs(EQBand& band);
    float processBiquad(EQBand& band, float input);
};

// Compressor with RMS detection, soft knee, and full parameter control
class CompressorNode : public AudioNode {
public:
    CompressorNode(const std::string& id);
    ~CompressorNode() override = default;
    
    void process(AudioBuffer& input, AudioBuffer& output, 
                int numSamples, SampleCount position) noexcept override;
    
    std::vector<ParameterInfo> getParameters() const override;
    float getParameterValue(const std::string& paramId) const override;
    void setParameterValue(const std::string& paramId, float value) override;
    
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void reset() override;

private:
    // Parameters
    std::atomic<float> threshold_{-20.0f};    // dB
    std::atomic<float> ratio_{4.0f};           // 1:infinity
    std::atomic<float> attack_{5.0f};          // ms
    std::atomic<float> release_{50.0f};        // ms
    std::atomic<float> knee_{2.0f};            // dB
    std::atomic<float> makeupGain_{0.0f};     // dB
    std::atomic<bool> enabled_{true};
    
    // Runtime state
    double sampleRate_{44100.0};
    float envelope_{0.0f};
    float gainReduction_{0.0f};
    
    // Coefficients
    float attackCoeff_{0.0f};
    float releaseCoeff_{0.0f};
    
    // RMS detection buffer
    static constexpr int kRMSBufferSize = 256;
    std::array<float, kRMSBufferSize> rmsBuffer_;
    int rmsBufferPos_{0};
    float rmsSum_{0.0f};
    
    void updateCoefficients();
    float calculateGainReduction(float inputLevel) const;
    float processRMS(float input);
};

// Track input node - receives audio from clips
class TrackInputNode : public AudioNode {
public:
    TrackInputNode(const std::string& id);
    ~TrackInputNode() override = default;
    
    void process(AudioBuffer& input, AudioBuffer& output, 
                int numSamples, SampleCount position) noexcept override;
    
    // Set audio data for this track
    void setAudioData(const float* leftChannel, const float* rightChannel, 
                     int numChannels, SampleCount length);
    
    void setClipRegion(SampleCount start, SampleCount length, 
                      SampleCount sourceStart, float gain);

private:
    const float* leftChannel_{nullptr};
    const float* rightChannel_{nullptr};
    int numChannels_{0};
    SampleCount assetLength_{0};
    
    SampleCount clipStart_{0};
    SampleCount clipLength_{0};
    SampleCount sourceStart_{0};
    float clipGain_{1.0f};
};

// Track output node - applies gain/pan/mute/solo
class TrackOutputNode : public AudioNode {
public:
    TrackOutputNode(const std::string& id);
    ~TrackOutputNode() override = default;
    
    void process(AudioBuffer& input, AudioBuffer& output, 
                int numSamples, SampleCount position) noexcept override;
    
    std::vector<ParameterInfo> getParameters() const override;
    float getParameterValue(const std::string& paramId) const override;
    void setParameterValue(const std::string& paramId, float value) override;
    
    void setMuted(bool muted) { muted_ = muted; }
    void setSoloed(bool soloed) { soloed_ = soloed; }
    bool isMuted() const { return muted_; }
    bool isSoloed() const { return soloed_; }

private:
    std::atomic<float> gain_{1.0f};
    std::atomic<float> pan_{0.0f};  // -1.0 to 1.0
    std::atomic<bool> muted_{false};
    std::atomic<bool> soloed_{false};
    
    float currentGain_{1.0f};
    float currentPan_{0.0f};
    float gainSmoother_{1.0f};
    float panSmoother_{0.0f};
    
    static constexpr float kSmoothingCoeff = 0.999f;
};

// Mixer node - sums multiple track outputs
class MixerNode : public AudioNode {
public:
    MixerNode(const std::string& id);
    ~MixerNode() override = default;
    
    void process(AudioBuffer& input, AudioBuffer& output, 
                int numSamples, SampleCount position) noexcept override;
    
    std::vector<ParameterInfo> getParameters() const override;
    float getParameterValue(const std::string& paramId) const override;
    void setParameterValue(const std::string& paramId, float value) override;

private:
    std::atomic<float> masterGain_{1.0f};
    std::atomic<float> masterPan_{0.0f};
    
    float currentMasterGain_{1.0f};
    float currentMasterPan_{0.0f};
    float gainSmoother_{1.0f};
    float panSmoother_{0.0f};
    
    static constexpr float kSmoothingCoeff = 0.999f;
};

// Latency compensator node - adds delay to align tracks
class LatencyCompensatorNode : public AudioNode {
public:
    LatencyCompensatorNode(const std::string& id);
    ~LatencyCompensatorNode() override = default;
    
    void process(AudioBuffer& input, AudioBuffer& output, 
                int numSamples, SampleCount position) noexcept override;
    
    void setDelaySamples(int delaySamples);
    int getDelaySamples() const { return delaySamples_; }
    
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void reset() override;
    
    int getLatencySamples() const override { return delaySamples_; }

private:
    std::atomic<int> delaySamples_{0};
    std::vector<std::vector<float>> delayBuffers_;
    int writePos_{0};
    int bufferSize_{0};
};

} // namespace neurato
