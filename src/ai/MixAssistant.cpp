#include "AIImplementation.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <numeric>
#include <cmath>

namespace neurato {

// MixAssistant implementation
MixAssistant::MixAssistant(std::shared_ptr<LocalInferenceRuntime> inference) 
    : impl_(std::make_unique<MixAssistantImpl>(inference)) {
}

MixAssistant::MixResponse MixAssistant::analyzeMix(const MixRequest& request) {
    return impl_->analyzeMix(request);
}

std::vector<MixAssistant::MixSuggestion> MixAssistant::getSuggestionsForTrack(
    const std::string& trackId, const SessionSnapshot& snapshot) {
    
    return impl_->getSuggestionsForTrack(trackId, snapshot);
}

void MixAssistant::learnFromUserAction(const MixSuggestion& suggestion, bool accepted) {
    impl_->learnFromUserAction(suggestion, accepted);
}

// MixAssistantImpl implementation
MixAssistantImpl::MixAssistantImpl(std::shared_ptr<LocalInferenceRuntime> inference) 
    : inference_(inference) {
}

MixAssistant::MixResponse MixAssistantImpl::analyzeMix(const MixAssistant::MixRequest& request) {
    MixAssistant::MixResponse response;
    
    // Analyze current mix state
    float totalLUFS = 0.0f;
    float peakLevel = 0.0f;
    std::vector<float> trackLUFS;
    
    for (const auto& track : request.snapshot.tracks) {
        float trackLUFS = calculateTrackLUFS(track, request.snapshot);
        trackLUFS.push_back(trackLUFS);
        totalLUFS += trackLUFS;
        
        // Get peak level from audio analysis
        auto audioIt = request.snapshot.audioAnalysis.find(track.id);
        if (audioIt != request.snapshot.audioAnalysis.end() && !audioIt->second.peakLevels.empty()) {
            float trackPeak = *std::max_element(audioIt->second.peakLevels.begin(),
                                              audioIt->second.peakLevels.end());
            peakLevel = std::max(peakLevel, trackPeak);
        }
    }
    
    response.overallLufs = trackLUFS.empty() ? -14.0f : totalLUFS / trackLUFS.size();
    response.peakLevel = peakLevel;
    
    // Generate suggestions
    for (const auto& track : request.snapshot.tracks) {
        auto audioIt = request.snapshot.audioAnalysis.find(track.id);
        if (audioIt == request.snapshot.audioAnalysis.end()) continue;
        
        const auto& analysis = audioIt->second;
        
        // Gain suggestion
        float currentLUFS = calculateTrackLUFS(track, request.snapshot);
        float targetLUFS = request.targetLUFS - 6.0f; // 6dB headroom
        
        if (std::abs(currentLUFS - targetLUFS) > 2.0f) {
            MixAssistant::MixSuggestion suggestion;
            suggestion.trackId = track.id;
            suggestion.parameterId = "gain";
            suggestion.currentValue = track.gain;
            suggestion.suggestedValue = track.gain * std::pow(10.0f, (targetLUFS - currentLUFS) / 20.0f);
            suggestion.reason = "Adjust track level to target LUFS";
            suggestion.confidence = 0.8f;
            
            response.suggestions.push_back(suggestion);
        }
        
        // EQ suggestions based on spectral analysis
        if (analysis.spectralCentroid > 0.0f) {
            if (analysis.spectralCentroid < 1000.0f) {
                // Low-frequency content - suggest high shelf boost
                MixAssistant::MixSuggestion suggestion;
                suggestion.trackId = track.id;
                suggestion.parameterId = "eq_band_3_gain"; // High shelf
                suggestion.currentValue = 0.0f;
                suggestion.suggestedValue = 2.0f; // +2dB boost
                suggestion.reason = "Add brightness to low-frequency content";
                suggestion.confidence = 0.6f;
                
                response.suggestions.push_back(suggestion);
            } else if (analysis.spectralCentroid > 4000.0f) {
                // High-frequency content - suggest low shelf cut
                MixAssistant::MixSuggestion suggestion;
                suggestion.trackId = track.id;
                suggestion.parameterId = "eq_band_0_gain"; // Low shelf
                suggestion.currentValue = 0.0f;
                suggestion.suggestedValue = -1.0f; // -1dB cut
                suggestion.reason = "Reduce low frequencies in bright content";
                suggestion.confidence = 0.5f;
                
                response.suggestions.push_back(suggestion);
            }
        }
        
        // Compression suggestion
        if (!analysis.peakLevels.empty()) {
            float peakRMS = analysis.peakLevels[0]; // Simplified
            float rmsLevel = !analysis.rmsLevels.empty() ? analysis.rmsLevels[0] : 0.1f;
            
            if (peakRMS > rmsLevel * 4.0f) { // High dynamic range
                MixAssistant::MixSuggestion suggestion;
                suggestion.trackId = track.id;
                suggestion.parameterId = "compressor_threshold";
                suggestion.currentValue = -20.0f;
                suggestion.suggestedValue = -12.0f;
                suggestion.reason = "Reduce dynamic range";
                suggestion.confidence = 0.7f;
                
                response.suggestions.push_back(suggestion);
            }
        }
    }
    
    // Generate summary
    response.summary = "Generated " + std::to_string(response.suggestions.size()) + 
                      " mix suggestions. Current LUFS: " + 
                      std::to_string(response.overallLufs) + 
                      ", Peak: " + std::to_string(response.peakLevel);
    
    // Add warnings
    if (response.peakLevel > -1.0f) {
        response.warnings.push_back("Peak levels are too high - risk of clipping");
    }
    
    if (response.overallLufs > -10.0f) {
        response.warnings.push_back("Mix is too loud - may cause streaming platform normalization");
    }
    
    return response;
}

std::vector<MixAssistant::MixSuggestion> MixAssistantImpl::getSuggestionsForTrack(
    const std::string& trackId, const SessionSnapshot& snapshot) {
    
    std::vector<MixAssistant::MixSuggestion> suggestions;
    
    // Find the track
    auto trackIt = std::find_if(snapshot.tracks.begin(), snapshot.tracks.end(),
        [&trackId](const SessionSnapshot::TrackInfo& track) {
            return track.id == trackId;
        });
    
    if (trackIt == snapshot.tracks.end()) return suggestions;
    
    const auto& track = *trackIt;
    auto audioIt = snapshot.audioAnalysis.find(trackId);
    if (audioIt == snapshot.audioAnalysis.end()) return suggestions;
    
    const auto& analysis = audioIt->second;
    
    // Generate track-specific suggestions
    // (Similar to analyzeMix but focused on one track)
    
    return suggestions;
}

void MixAssistantImpl::learnFromUserAction(const MixAssistant::MixSuggestion& suggestion, 
                                          bool accepted) {
    std::lock_guard<std::mutex> lock(feedbackMutex_);
    
    UserFeedback feedback;
    feedback.trackId = suggestion.trackId;
    feedback.parameterId = suggestion.parameterId;
    feedback.suggestedValue = suggestion.suggestedValue;
    feedback.accepted = accepted;
    feedback.timestamp = std::chrono::system_clock::now();
    
    feedbackHistory_.push_back(feedback);
    
    // Update models based on feedback
    updateModelsFromFeedback();
}

float MixAssistantImpl::calculateTrackLUFS(const SessionSnapshot::TrackInfo& track,
                                           const SessionSnapshot& snapshot) const {
    // Simplified LUFS calculation
    // In production, would use proper LUFS algorithm (EBU R128)
    
    auto audioIt = snapshot.audioAnalysis.find(track.id);
    if (audioIt == snapshot.audioAnalysis.end() || audioIt->second.rmsLevels.empty()) {
        return -20.0f; // Default value
    }
    
    const auto& rmsLevels = audioIt->second.rmsLevels;
    float avgRMS = std::accumulate(rmsLevels.begin(), rmsLevels.end(), 0.0f) / rmsLevels.size();
    
    // Convert RMS to LUFS (simplified)
    float lufs = -20.0f * std::log10(avgRMS + 1e-6f);
    
    // Apply track gain
    lufs -= 20.0f * std::log10(track.gain + 1e-6f);
    
    return lufs;
}

std::vector<float> MixAssistantImpl::suggestEQCurve(const SessionSnapshot::TrackInfo& track,
                                                   const SessionSnapshot::AudioAnalysis& analysis) const {
    std::vector<float> eqCurve(4, 0.0f); // 4-band EQ
    
    // Analyze spectral characteristics and suggest EQ
    if (analysis.spectralCentroid < 800.0f) {
        eqCurve[3] = 2.0f; // Boost high frequencies
    } else if (analysis.spectralCentroid > 5000.0f) {
        eqCurve[0] = -1.0f; // Cut low frequencies
    }
    
    return eqCurve;
}

float MixAssistantImpl::suggestGainReduction(const SessionSnapshot::TrackInfo& track,
                                            const SessionSnapshot::AudioAnalysis& analysis) const {
    if (analysis.peakLevels.empty() || analysis.rmsLevels.empty()) {
        return 0.0f;
    }
    
    float peakRatio = analysis.peakLevels[0] / (analysis.rmsLevels[0] + 1e-6f);
    
    if (peakRatio > 6.0f) {
        return 6.0f; // 6:1 ratio
    } else if (peakRatio > 4.0f) {
        return 4.0f; // 4:1 ratio
    } else if (peakRatio > 2.0f) {
        return 2.0f; // 2:1 ratio
    }
    
    return 1.0f; // No compression
}

void MixAssistantImpl::updateModelsFromFeedback() {
    // In production, this would update machine learning models
    // For now, we'll just limit feedback history size
    if (feedbackHistory_.size() > 1000) {
        feedbackHistory_.erase(feedbackHistory_.begin(), feedbackHistory_.begin() + 100);
    }
}

float MixAssistantImpl::predictUserPreference(const std::string& trackId,
                                              const std::string& parameterId,
                                              float suggestedValue) const {
    // In production, this would use a trained model
    // For now, return the suggested value with some noise
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> noise(0.0f, 0.1f);
    
    return suggestedValue * (1.0f + noise(gen));
}

// TransientDetector implementation
TransientDetector::TransientDetector() : impl_(std::make_unique<TransientDetectorImpl>()) {
}

std::vector<TransientDetector::Transient> TransientDetector::detectTransients(
    const float* audio, int numSamples, double sampleRate) const {
    
    return impl_->detectTransients(audio, numSamples, sampleRate);
}

TransientDetector::BeatGrid TransientDetector::detectBeatGrid(const float* audio, int numSamples,
                                                             double sampleRate, 
                                                             double initialTempo) const {
    return impl_->detectBeatGrid(audio, numSamples, sampleRate, initialTempo);
}

void TransientDetector::processRealTime(const float* audio, int numSamples, double sampleRate,
                                        std::vector<Transient>& transients) {
    impl_->processRealTime(audio, numSamples, sampleRate, transients);
}

// TransientDetectorImpl implementation
TransientDetectorImpl::TransientDetectorImpl() {
    previousFrame_.resize(kAnalysisWindowSize, 0.0f);
}

std::vector<TransientDetector::Transient> TransientDetectorImpl::detectTransients(
    const float* audio, int numSamples, double sampleRate) const {
    
    std::vector<TransientDetector::Transient> transients;
    
    // Calculate multiple detection functions
    auto spectralFlux = calculateSpectralFlux(audio, numSamples, kAnalysisWindowSize, kHopSize);
    auto energy = calculateEnergy(audio, numSamples, kAnalysisWindowSize, kHopSize);
    auto hfc = calculateHighFrequencyContent(audio, numSamples, kAnalysisWindowSize, kHopSize);
    
    // Combine detection functions
    std::vector<float> combinedFunction;
    for (size_t i = 0; i < spectralFlux.size(); ++i) {
        float combined = spectralFlux[i] * 0.5f + energy[i] * 0.3f + hfc[i] * 0.2f;
        combinedFunction.push_back(combined);
    }
    
    // Pick peaks
    auto peakIndices = pickPeaks(combinedFunction, 0.1f, kHopSize / sampleRate * 1000); // 10ms min distance
    
    // Convert to transients
    for (int peakIndex : peakIndices) {
        TransientDetector::Transient transient;
        transient.position = peakIndex * kHopSize;
        transient.strength = combinedFunction[peakIndex];
        transient.frequency = 1000.0f; // Placeholder - would calculate actual frequency
        
        transients.push_back(transient);
    }
    
    return transients;
}

TransientDetector::BeatGrid TransientDetectorImpl::detectBeatGrid(const float* audio, int numSamples,
                                                                 double sampleRate, 
                                                                 double initialTempo) const {
    TransientDetector::BeatGrid beatGrid;
    
    // Detect transients first
    auto transients = detectTransients(audio, numSamples, sampleRate);
    
    if (transients.empty()) {
        return beatGrid;
    }
    
    // Estimate tempo from transients
    double estimatedTempo = estimateTempo(transients, sampleRate);
    if (estimatedTempo <= 0) {
        estimatedTempo = initialTempo;
    }
    
    beatGrid.detectedTempo = estimatedTempo;
    beatGrid.confidence = 0.8f; // Simplified confidence
    
    // Generate beat grid
    beatGrid.beats = trackBeats(transients, estimatedTempo, sampleRate);
    
    // Identify downbeats (simplified - assume first beat is downbeat)
    if (!beatGrid.beats.empty()) {
        beatGrid.downbeats.push_back(beatGrid.beats[0]);
        
        // Add downbeats every 4 beats (4/4 time)
        for (size_t i = 4; i < beatGrid.beats.size(); i += 4) {
            beatGrid.downbeats.push_back(beatGrid.beats[i]);
        }
    }
    
    return beatGrid;
}

void TransientDetectorImpl::processRealTime(const float* audio, int numSamples, double sampleRate,
                                            std::vector<TransientDetector::Transient>& transients) {
    // Process in chunks
    int samplesProcessed = 0;
    
    while (samplesProcessed < numSamples) {
        int chunkSize = std::min(kHopSize, numSamples - samplesProcessed);
        
        // Calculate detection function for current frame
        std::vector<float> currentFrame(kAnalysisWindowSize, 0.0f);
        
        // Copy previous frame data
        std::copy(previousFrame_.begin() + chunkSize, previousFrame_.end(), currentFrame.begin());
        
        // Add new audio data
        std::copy(audio + samplesProcessed, audio + samplesProcessed + chunkSize,
                 currentFrame.begin() + (kAnalysisWindowSize - chunkSize));
        
        // Calculate spectral flux
        float spectralFlux = 0.0f;
        for (int i = 0; i < kAnalysisWindowSize / 2; ++i) {
            float currentMag = std::abs(currentFrame[i]);
            float prevMag = std::abs(previousFrame_[i]);
            
            if (currentMag > prevMag) {
                spectralFlux += currentMag - prevMag;
            }
        }
        
        // Detect transient
        if (spectralFlux > 0.5f) { // Threshold
            TransientDetector::Transient transient;
            transient.position = frameCounter_ * kHopSize;
            transient.strength = spectralFlux;
            transient.frequency = 1000.0f; // Placeholder
            
            transients.push_back(transient);
        }
        
        // Update previous frame
        previousFrame_ = currentFrame;
        
        samplesProcessed += chunkSize;
        frameCounter_++;
    }
}

std::vector<float> TransientDetectorImpl::calculateSpectralFlux(const float* audio, int numSamples,
                                                               int windowSize, int hopSize) const {
    std::vector<float> spectralFlux;
    
    for (int i = 0; i < numSamples - windowSize; i += hopSize) {
        float flux = 0.0f;
        
        // Calculate FFT magnitudes for current and previous windows
        std::vector<float> currentMag(windowSize / 2);
        std::vector<float> prevMag(windowSize / 2);
        
        for (int j = 0; j < windowSize / 2; ++j) {
            currentMag[j] = std::abs(audio[i + j]);
            prevMag[j] = std::abs(audio[i + j - hopSize]);
            
            if (currentMag[j] > prevMag[j]) {
                flux += currentMag[j] - prevMag[j];
            }
        }
        
        spectralFlux.push_back(flux);
    }
    
    return spectralFlux;
}

std::vector<float> TransientDetectorImpl::calculateEnergy(const float* audio, int numSamples,
                                                          int windowSize, int hopSize) const {
    std::vector<float> energy;
    
    for (int i = 0; i < numSamples - windowSize; i += hopSize) {
        float windowEnergy = 0.0f;
        
        for (int j = 0; j < windowSize; ++j) {
            windowEnergy += audio[i + j] * audio[i + j];
        }
        
        energy.push_back(std::sqrt(windowEnergy / windowSize));
    }
    
    return energy;
}

std::vector<float> TransientDetectorImpl::calculateHighFrequencyContent(const float* audio, 
                                                                       int numSamples,
                                                                       int windowSize, 
                                                                       int hopSize) const {
    std::vector<float> hfc;
    
    for (int i = 0; i < numSamples - windowSize; i += hopSize) {
        float highFreqEnergy = 0.0f;
        
        // Focus on high frequencies (upper half of spectrum)
        for (int j = windowSize / 2; j < windowSize; ++j) {
            highFreqEnergy += audio[i + j] * audio[i + j] * j;
        }
        
        hfc.push_back(highFreqEnergy);
    }
    
    return hfc;
}

std::vector<int> TransientDetectorImpl::pickPeaks(const std::vector<float>& signal, 
                                                  float threshold, int minDistance) const {
    std::vector<int> peaks;
    
    for (size_t i = 1; i < signal.size() - 1; ++i) {
        // Check if current sample is a local maximum
        if (signal[i] > signal[i-1] && signal[i] > signal[i+1] && signal[i] > threshold) {
            // Check minimum distance from previous peaks
            bool validPeak = true;
            
            for (int peak : peaks) {
                if (std::abs(static_cast<int>(i) - peak) < minDistance) {
                    validPeak = false;
                    break;
                }
            }
            
            if (validPeak) {
                peaks.push_back(static_cast<int>(i));
            }
        }
    }
    
    return peaks;
}

double TransientDetectorImpl::estimateTempo(const std::vector<TransientDetector::Transient>& transients,
                                            double sampleRate) const {
    if (transients.size() < 4) {
        return 0.0; // Not enough transients
    }
    
    // Calculate inter-onset intervals
    std::vector<double> intervals;
    for (size_t i = 1; i < transients.size(); ++i) {
        double interval = (transients[i].position - transients[i-1].position) / sampleRate;
        intervals.push_back(interval);
    }
    
    // Create histogram of intervals
    std::map<int, int> histogram; // Map interval (in ms) to count
    
    for (double interval : intervals) {
        int intervalMs = static_cast<int>(interval * 1000);
        
        // Quantize to nearest 10ms
        intervalMs = (intervalMs / 10) * 10;
        
        histogram[intervalMs]++;
    }
    
    // Find most common interval
    int mostCommonInterval = 0;
    int maxCount = 0;
    
    for (const auto& pair : histogram) {
        if (pair.second > maxCount) {
            maxCount = pair.second;
            mostCommonInterval = pair.first;
        }
    }
    
    if (mostCommonInterval == 0) {
        return 0.0;
    }
    
    // Convert interval to tempo
    double beatDuration = mostCommonInterval / 1000.0; // Convert to seconds
    double tempo = 60.0 / beatDuration;
    
    // Clamp to reasonable tempo range
    if (tempo < 60.0) tempo *= 2.0; // Double if too slow
    if (tempo > 200.0) tempo /= 2.0; // Halve if too fast
    
    return tempo;
}

std::vector<SampleCount> TransientDetectorImpl::trackBeats(const std::vector<TransientDetector::Transient>& transients,
                                                           double tempo, double sampleRate) const {
    std::vector<SampleCount> beats;
    
    if (transients.empty()) {
        return beats;
    }
    
    double beatDuration = 60.0 / tempo;
    double beatDurationSamples = beatDuration * sampleRate;
    
    // Start from the first transient
    SampleCount firstBeat = transients[0].position;
    
    // Generate beats at regular intervals
    SampleCount currentBeat = firstBeat;
    
    while (currentBeat < transients.back().position + beatDurationSamples) {
        beats.push_back(currentBeat);
        currentBeat += static_cast<SampleCount>(beatDurationSamples);
    }
    
    return beats;
}

} // namespace neurato
