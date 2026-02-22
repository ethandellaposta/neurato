#include "TestSuite.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <filesystem>
#include <fstream>
#include <random>
#include <cmath>

namespace neurato {
namespace tests {

// TestUtilities implementation
std::vector<float> TestUtilities::generateSineWave(float frequency, double sampleRate, int numSamples) {
    std::vector<float> audio(numSamples);
    const double twoPi = 2.0 * juce::MathConstants<double>::pi;
    
    for (int i = 0; i < numSamples; ++i) {
        double time = static_cast<double>(i) / sampleRate;
        audio[i] = static_cast<float>(std::sin(twoPi * frequency * time));
    }
    
    return audio;
}

std::vector<float> TestUtilities::generateWhiteNoise(int numSamples) {
    std::vector<float> audio(numSamples);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
    
    for (int i = 0; i < numSamples; ++i) {
        audio[i] = dis(gen);
    }
    
    return audio;
}

std::vector<float> TestUtilities::generateImpulseTrain(int numSamples, int period) {
    std::vector<float> audio(numSamples, 0.0f);
    
    for (int i = 0; i < numSamples; i += period) {
        audio[i] = 1.0f;
    }
    
    return audio;
}

std::shared_ptr<Session> TestUtilities::createTestSession() {
    auto session = std::make_shared<Session>();
    session->setName("Test Session");
    session->setBPM(120.0);
    session->setTimeSignature({4, 4});
    
    // Add a test track
    auto track = std::make_shared<Track>("test_track", TrackType::Audio);
    track->setName("Test Track");
    track->setGain(1.0f);
    track->setPan(0.0f);
    
    session->addTrack(track);
    
    return session;
}

std::shared_ptr<Session> TestUtilities::createComplexTestSession() {
    auto session = createTestSession();
    
    // Add more tracks
    for (int i = 0; i < 5; ++i) {
        auto track = std::make_shared<Track>("track_" + std::to_string(i), 
                                            i % 2 == 0 ? TrackType::Audio : TrackType::MIDI);
        track->setName("Track " + std::to_string(i + 1));
        track->setGain(0.8f + i * 0.1f);
        track->setPan((i % 3 - 1) * 0.5f);
        
        session->addTrack(track);
    }
    
    return session;
}

float TestUtilities::calculateRMS(const float* audio, int numSamples) {
    float sum = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        sum += audio[i] * audio[i];
    }
    return std::sqrt(sum / numSamples);
}

float TestUtilities::calculatePeak(const float* audio, int numSamples) {
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        peak = std::max(peak, std::abs(audio[i]));
    }
    return peak;
}

std::vector<float> TestUtilities::calculateFFT(const float* audio, int numSamples) {
    // Simplified FFT - in production would use proper FFT library
    std::vector<float> spectrum(numSamples / 2);
    
    for (int k = 0; k < numSamples / 2; ++k) {
        float real = 0.0f;
        float imag = 0.0f;
        
        for (int n = 0; n < numSamples; ++n) {
            float angle = -2.0f * juce::MathConstants<float>::pi * k * n / numSamples;
            real += audio[n] * std::cos(angle);
            imag += audio[n] * std::sin(angle);
        }
        
        spectrum[k] = std::sqrt(real * real + imag * imag);
    }
    
    return spectrum;
}

std::string TestUtilities::getTestDataPath() {
    return std::filesystem::temp_directory_path().string() + "/neurato_test_data";
}

void TestUtilities::createTestAudioFile(const std::string& path, const std::vector<float>& audio, 
                                       double sampleRate) {
    // Create test data directory
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    
    // Write simple WAV file header and data
    std::ofstream file(path, std::ios::binary);
    
    if (!file.is_open()) return;
    
    // WAV header
    int32_t sampleRateInt = static_cast<int32_t>(sampleRate);
    int16_t numChannels = 1;
    int16_t bitsPerSample = 32;
    int32_t byteRate = sampleRateInt * numChannels * (bitsPerSample / 8);
    int16_t blockAlign = numChannels * (bitsPerSample / 8);
    int32_t dataSize = static_cast<int32_t>(audio.size() * sizeof(float));
    int32_t fileSize = 36 + dataSize;
    
    file.write("RIFF", 4);
    file.write(reinterpret_cast<const char*>(&fileSize), 4);
    file.write("WAVE", 4);
    file.write("fmt ", 4);
    int32_t fmtChunkSize = 16;
    file.write(reinterpret_cast<const char*>(&fmtChunkSize), 4);
    int16_t audioFormat = 3; // IEEE float
    file.write(reinterpret_cast<const char*>(&audioFormat), 2);
    file.write(reinterpret_cast<const char*>(&numChannels), 2);
    file.write(reinterpret_cast<const char*>(&sampleRateInt), 4);
    file.write(reinterpret_cast<const char*>(&byteRate), 4);
    file.write(reinterpret_cast<const char*>(&blockAlign), 2);
    file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&dataSize), 4);
    
    // Audio data
    file.write(reinterpret_cast<const char*>(audio.data()), dataSize);
}

void TestUtilities::cleanupTestData() {
    std::filesystem::remove_all(getTestDataPath());
}

// AudioGraphTest implementation
void AudioGraphTest::SetUp() {
    graph_ = std::make_unique<AudioGraph>();
    
    // Create test nodes
    auto inputNode = std::make_shared<TrackInputNode>("input");
    auto gainNode = std::make_shared<GainNode>("gain");
    auto outputNode = std::make_shared<TrackOutputNode>("output");
    
    nodes_.push_back(inputNode);
    nodes_.push_back(gainNode);
    nodes_.push_back(outputNode);
    
    // Add nodes to graph
    for (auto& node : nodes_) {
        graph_->addNode(node);
    }
    
    // Create connections
    AudioConnection conn1{"input", "gain"};
    AudioConnection conn2{"gain", "output"};
    
    graph_->addConnection(conn1);
    graph_->addConnection(conn2);
    
    graph_->prepareToPlay(44100.0, 512);
}

void AudioGraphTest::TearDown() {
    graph_.reset();
    nodes_.clear();
}

// AutomationTest implementation
void AutomationTest::SetUp() {
    lane_ = std::make_unique<AutomationLane>();
    manager_ = std::make_unique<AutomationManager>();
}

void AutomationTest::TearDown() {
    lane_.reset();
    manager_.reset();
}

// AudioProcessorTest implementation
void AudioProcessorTest::SetUp() {
    gainNode_ = std::make_unique<GainNode>("test_gain");
    eqNode_ = std::make_unique<EQNode>("test_eq");
    compressorNode_ = std::make_unique<CompressorNode>("test_compressor");
    
    // Generate test audio
    testAudio_ = TestUtilities::generateSineWave(440.0f, kTestSampleRate, kTestBufferSize);
    
    // Prepare processors
    gainNode_->prepareToPlay(kTestSampleRate, kTestBufferSize);
    eqNode_->prepareToPlay(kTestSampleRate, kTestBufferSize);
    compressorNode_->prepareToPlay(kTestSampleRate, kTestBufferSize);
}

void AudioProcessorTest::TearDown() {
    gainNode_.reset();
    eqNode_.reset();
    compressorNode_.reset();
}

// PluginHostTest implementation
void PluginHostTest::SetUp() {
    pluginManager_ = std::make_unique<PluginManager>();
    pluginScanner_ = std::make_unique<PluginScanner>();
    
    pluginManager_->initialize();
}

void PluginHostTest::TearDown() {
    pluginManager_.reset();
    pluginScanner_.reset();
}

// SandboxHostTest implementation
void SandboxHostTest::SetUp() {
    sandboxHost_ = std::make_unique<SandboxPluginHost>();
}

void SandboxHostTest::TearDown() {
    if (sandboxHost_ && sandboxHost_->isSandboxRunning()) {
        sandboxHost_->stopSandbox();
    }
    sandboxHost_.reset();
}

// AIComponentsTest implementation
void AIComponentsTest::SetUp() {
    sessionState_ = std::make_unique<SessionStateAPI>();
    testSession_ = TestUtilities::createComplexTestSession();
    inference_ = std::make_unique<LocalInferenceRuntime>();
    
    sessionState_->setSession(testSession_);
    
    // Load a simple model for testing
    LocalInferenceRuntime::ModelConfig config;
    config.type = LocalInferenceRuntime::ModelType::LanguageModel;
    config.modelPath = "test_model";
    config.architecture = "custom";
    
    inference_->loadModel(config);
}

void AIComponentsTest::TearDown() {
    sessionState_.reset();
    testSession_.reset();
    inference_.reset();
}

// AIPlannerTest implementation
void AIPlannerTest::SetUp() {
    sessionState_ = std::make_unique<SessionStateAPI>();
    testSession_ = TestUtilities::createTestSession();
    inference_ = std::make_unique<LocalInferenceRuntime>();
    planner_ = std::make_unique<SimpleAIPlanner>(inference_);
    
    sessionState_->setSession(testSession_);
    
    // Load test model
    LocalInferenceRuntime::ModelConfig config;
    config.type = LocalInferenceRuntime::ModelType::LanguageModel;
    config.modelPath = "test_model";
    config.architecture = "custom";
    
    inference_->loadModel(config);
}

void AIPlannerTest::TearDown() {
    planner_.reset();
    sessionState_.reset();
    testSession_.reset();
    inference_.reset();
}

// MixAssistantTest implementation
void MixAssistantTest::SetUp() {
    inference_ = std::make_unique<LocalInferenceRuntime>();
    mixAssistant_ = std::make_unique<MixAssistant>(inference_);
    
    // Create test snapshot
    testSnapshot_.sessionId = "test";
    testSnapshot_.name = "Test Mix";
    testSnapshot_.bpm = 120.0;
    
    // Add test tracks
    for (int i = 0; i < 3; ++i) {
        SessionSnapshot::TrackInfo track;
        track.id = "track_" + std::to_string(i);
        track.name = "Track " + std::to_string(i);
        track.gain = 0.5f + i * 0.2f;
        track.pan = (i - 1) * 0.5f;
        testSnapshot_.tracks.push_back(track);
        
        // Add audio analysis
        SessionSnapshot::AudioAnalysis analysis;
        analysis.rmsLevels = {0.1f, 0.15f, 0.12f};
        analysis.peakLevels = {0.8f, 0.9f, 0.7f};
        analysis.spectralCentroid = 1000.0f + i * 500.0f;
        testSnapshot_.audioAnalysis[track.id] = analysis;
    }
}

void MixAssistantTest::TearDown() {
    mixAssistant_.reset();
    inference_.reset();
}

// TransientDetectorTest implementation
void TransientDetectorTest::SetUp() {
    detector_ = std::make_unique<TransientDetector>();
    
    // Generate test audio with transients
    testAudio_ = TestUtilities::generateImpulseTrain(kTestSampleRate * 2, kTestSampleRate / 4); // Quarter-note impulses
}

void TransientDetectorTest::TearDown() {
    detector_.reset();
}

// CommandPaletteTest implementation
void CommandPaletteTest::SetUp() {
    commandPalette_ = std::make_unique<CommandPalette>();
    sessionState_ = std::make_unique<SessionStateAPI>();
    
    auto testSession = TestUtilities::createTestSession();
    sessionState_->setSession(testSession);
}

void CommandPaletteTest::TearDown() {
    commandPalette_.reset();
    sessionState_.reset();
}

// EditPreviewTest implementation
void EditPreviewTest::SetUp() {
    previewUI_ = std::make_unique<EditPreviewUI>();
    sessionState_ = std::make_unique<SessionStateAPI>();
    
    auto testSession = TestUtilities::createTestSession();
    sessionState_->setSession(testSession);
}

void EditPreviewTest::TearDown() {
    previewUI_.reset();
    sessionState_.reset();
}

// IntegrationTest implementation
void IntegrationTest::SetUp() {
    // Create all system components
    audioGraph_ = std::make_unique<AudioGraph>();
    sessionState_ = std::make_unique<SessionStateAPI>();
    pluginManager_ = std::make_unique<PluginManager>();
    inference_ = std::make_unique<LocalInferenceRuntime>();
    aiPlanner_ = std::make_unique<SimpleAIPlanner>(inference_);
    mixAssistant_ = std::make_unique<MixAssistant>(inference_);
    commandPalette_ = std::make_unique<CommandPalette>();
    
    testSession_ = TestUtilities::createComplexTestSession();
    
    // Initialize components
    pluginManager_->initialize();
    sessionState_->setSession(testSession_);
    
    commandPalette_->setAIPlanner(aiPlanner_);
    commandPalette_->setSessionState(sessionState_);
    
    // Load test model
    LocalInferenceRuntime::ModelConfig config;
    config.type = LocalInferenceRuntime::ModelType::LanguageModel;
    config.modelPath = "test_model";
    config.architecture = "custom";
    
    inference_->loadModel(config);
}

void IntegrationTest::TearDown() {
    audioGraph_.reset();
    sessionState_.reset();
    pluginManager_.reset();
    inference_.reset();
    aiPlanner_.reset();
    mixAssistant_.reset();
    commandPalette_.reset();
    testSession_.reset();
}

// PerformanceTest implementation
void PerformanceTest::SetUp() {
    graph_ = std::make_unique<AudioGraph>();
    
    // Create a complex graph for performance testing
    for (int i = 0; i < 50; ++i) {
        auto gainNode = std::make_shared<GainNode("gain_" + std::to_string(i));
        nodes_.push_back(gainNode);
        graph_->addNode(gainNode);
    }
    
    // Create connections in a chain
    for (size_t i = 0; i < nodes_.size() - 1; ++i) {
        AudioConnection conn{nodes_[i]->getId(), nodes_[i + 1]->getId()};
        graph_->addConnection(conn);
    }
    
    graph_->prepareToPlay(44100.0, kLargeBufferSize);
}

void PerformanceTest::TearDown() {
    graph_.reset();
    nodes_.clear();
}

// StressTest implementation
void StressTest::SetUp() {
    graph_ = std::make_unique<AudioGraph>();
    sessionState_ = std::make_unique<SessionStateAPI>();
    
    // Create stress test session
    auto stressSession = std::make_shared<Session>();
    stressSession->setName("Stress Test Session");
    stressSession->setBPM(120.0);
    
    // Add many tracks
    for (int i = 0; i < kStressTestTracks; ++i) {
        auto track = std::make_shared<Track>("stress_track_" + std::to_string(i), TrackType::Audio);
        track->setName("Stress Track " + std::to_string(i));
        stressSession->addTrack(track);
    }
    
    sessionState_->setSession(stressSession);
}

void StressTest::TearDown() {
    graph_.reset();
    sessionState_.reset();
}

// RegressionTest implementation
void RegressionTest::SetUp() {
    sessionState_ = std::make_unique<SessionStateAPI>();
    testSession_ = TestUtilities::createComplexTestSession();
    
    // Create known good snapshot
    knownGoodSnapshot_ = sessionState_->generateSnapshot();
    knownGoodProjectPath_ = TestUtilities::getTestDataPath() + "/regression_test.neurato";
    
    sessionState_->setSession(testSession_);
}

void RegressionTest::TearDown() {
    sessionState_.reset();
    testSession_.reset();
}

// RealWorldScenarioTest implementation
void RealWorldScenarioTest::SetUp() {
    audioGraph_ = std::make_unique<AudioGraph>();
    sessionState_ = std::make_unique<SessionStateAPI>();
    mixAssistant_ = std::make_unique<MixAssistant>(nullptr);
}

void RealWorldScenarioTest::TearDown() {
    audioGraph_.reset();
    sessionState_.reset();
    mixAssistant_.reset();
}

void RealWorldScenarioTest::simulatePodcastEditing() {
    // Simulate podcast editing scenario
    auto session = TestDataGenerator::generatePodcastSession();
    sessionState_->setSession(session);
    
    // Add voice processing plugins
    // Add automation for volume ducking
    // Test mix suggestions for spoken word
}

void RealWorldScenarioTest::simulateMusicProduction() {
    // Simulate music production scenario
    auto session = TestDataGenerator::generateRockBandSession();
    sessionState_->setSession(session);
    
    // Add instrument plugins
    // Create complex routing
    // Test AI suggestions for music mixing
}

void RealWorldScenarioTest::simulateLiveRecording() {
    // Simulate live recording scenario
    auto session = TestDataGenerator::generateElectronicSession();
    sessionState_->setSession(session);
    
    // Test real-time processing
    // Test automation recording
    // Test plugin performance
}

void RealWorldScenarioTest::simulateMixingSession() {
    // Simulate mixing session scenario
    auto session = TestDataGenerator::generateOrchestralSession();
    sessionState_->setSession(session);
    
    // Test mix assistant
    // Test automation
    // Test plugin chaining
}

// WorkflowTest implementation
void WorkflowTest::SetUp() {
    audioGraph_ = std::make_unique<AudioGraph>();
    sessionState_ = std::make_unique<SessionStateAPI>();
    aiPlanner_ = std::make_unique<SimpleAIPlanner>(nullptr);
    commandPalette_ = std::make_unique<CommandPalette>();
    
    commandPalette_->setAIPlanner(aiPlanner_);
    commandPalette_->setSessionState(sessionState_);
}

void WorkflowTest::TearDown() {
    audioGraph_.reset();
    sessionState_.reset();
    aiPlanner_.reset();
    commandPalette_.reset();
}

void WorkflowTest::testCompleteAudioProductionWorkflow() {
    // Test complete workflow from recording to final mix
}

void WorkflowTest::testAIAssistedMixingWorkflow() {
    // Test AI-assisted mixing workflow
}

void WorkflowTest::testPluginChainingWorkflow() {
    // Test plugin chaining workflow
}

void WorkflowTest::testAutomationWorkflow() {
    // Test automation workflow
}

void WorkflowTest::testCollaborativeWorkflow() {
    // Test collaborative workflow
}

// TestDataGenerator implementation
std::vector<float> TestDataGenerator::generateDrumLoop() {
    std::vector<float> audio(44100 * 2); // 2 seconds at 44.1kHz
    
    // Generate simple drum pattern (kick, snare, hi-hat)
    const int samplesPerBeat = 44100 * 60 / 120; // 120 BPM
    
    for (int i = 0; i < 8; ++i) { // 2 bars, 4 beats per bar
        int beatPos = i * samplesPerBeat;
        
        // Kick on beats 1 and 3
        if (i % 4 == 0 || i % 4 == 2) {
            for (int j = 0; j < 1000 && beatPos + j < audio.size(); ++j) {
                audio[beatPos + j] = std::sin(2.0f * juce::MathConstants<float>::pi * 60.0f * j / 44100.0f) * 
                                   std::exp(-j / 100.0f);
            }
        }
        
        // Snare on beats 2 and 4
        if (i % 4 == 1 || i % 4 == 3) {
            for (int j = 0; j < 500 && beatPos + j < audio.size(); ++j) {
                audio[beatPos + j] = (std::rand() / static_cast<float>(RAND_MAX) - 0.5f) * 
                                   std::exp(-j / 50.0f) * 0.5f;
            }
        }
        
        // Hi-hat on every beat
        for (int j = 0; j < 200 && beatPos + j < audio.size(); ++j) {
            audio[beatPos + j] += (std::rand() / static_cast<float>(RAND_MAX) - 0.5f) * 
                                  std::exp(-j / 20.0f) * 0.1f;
        }
    }
    
    return audio;
}

std::vector<float> TestDataGenerator::generateBassLine() {
    std::vector<float> audio(44100 * 2);
    const int samplesPerBeat = 44100 * 60 / 120;
    
    // Simple bass line (C, G, Am, F)
    std::vector<float> notes = {65.41f, 98.00f, 55.00f, 43.65f}; // C3, G2, A1, F1
    
    for (int i = 0; i < 8; ++i) {
        int noteIndex = i % 4;
        float frequency = notes[noteIndex];
        int beatPos = i * samplesPerBeat;
        
        for (int j = 0; j < samplesPerBeat && beatPos + j < audio.size(); ++j) {
            float envelope = std::exp(-j / 10000.0f);
            audio[beatPos + j] = std::sin(2.0f * juce::MathConstants<float>::pi * frequency * j / 44100.0f) * 
                               envelope * 0.3f;
        }
    }
    
    return audio;
}

std::vector<float> TestDataGenerator::generateMelody() {
    std::vector<float> audio(44100 * 2);
    
    // Simple melody using pentatonic scale
    std::vector<float> notes = {261.63f, 293.66f, 329.63f, 392.00f, 440.00f, 523.25f}; // C4-E5 pentatonic
    const int samplesPerNote = 44100 * 60 / 120 / 4; // 16th notes
    
    for (int i = 0; i < 16; ++i) {
        int noteIndex = std::rand() % notes.size();
        float frequency = notes[noteIndex];
        int notePos = i * samplesPerNote;
        
        for (int j = 0; j < samplesPerNote && notePos + j < audio.size(); ++j) {
            float envelope = j < 100 ? j / 100.0f : std::exp(-(j - 100) / 5000.0f);
            audio[notePos + j] = std::sin(2.0f * juce::MathConstants<float>::pi * frequency * j / 44100.0f) * 
                               envelope * 0.2f;
        }
    }
    
    return audio;
}

std::vector<float> TestDataGenerator::generateSpeech() {
    std::vector<float> audio(44100 * 3); // 3 seconds
    
    // Simulate speech with formants
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-0.3f, 0.3f);
    
    for (int i = 0; i < audio.size(); ++i) {
        // Combine multiple frequencies to simulate formants
        float sample = dis(gen); // Noise component
        
        // Add formant frequencies (simplified)
        sample += std::sin(2.0f * juce::MathConstants<float>::pi * 200.0f * i / 44100.0f) * 0.2f;
        sample += std::sin(2.0f * juce::MathConstants<float>::pi * 800.0f * i / 44100.0f) * 0.15f;
        sample += std::sin(2.0f * juce::MathConstants<float>::pi * 1500.0f * i / 44100.0f) * 0.1f;
        
        // Add amplitude modulation
        float modulation = std::sin(2.0f * juce::MathConstants<float>::pi * 3.0f * i / 44100.0f) * 0.5f + 0.5f;
        sample *= modulation;
        
        audio[i] = sample;
    }
    
    return audio;
}

std::vector<float> TestDataGenerator::generateOrchestral() {
    std::vector<float> audio(44100 * 4); // 4 seconds
    
    // Simulate orchestral texture with multiple instruments
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-0.1f, 0.1f);
    
    for (int i = 0; i < audio.size(); ++i) {
        float sample = 0.0f;
        
        // Strings (low frequency content)
        sample += std::sin(2.0f * juce::MathConstants<float>::pi * 110.0f * i / 44100.0f) * 0.3f;
        sample += std::sin(2.0f * juce::MathConstants<float>::pi * 220.0f * i / 44100.0f) * 0.2f;
        
        // Woodwinds (mid frequency content)
        sample += std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f * i / 44100.0f) * 0.15f;
        sample += std::sin(2.0f * juce::MathConstants<float>::pi * 880.0f * i / 44100.0f) * 0.1f;
        
        // Brass (mid-high frequency content)
        sample += std::sin(2.0f * juce::MathConstants<float>::pi * 330.0f * i / 44100.0f) * 0.2f;
        sample += std::sin(2.0f * juce::MathConstants<float>::pi * 660.0f * i / 44100.0f) * 0.15f;
        
        // Add some noise for texture
        sample += dis(gen) * 0.05f;
        
        // Slow amplitude envelope
        float envelope = std::sin(2.0f * juce::MathConstants<float>::pi * 0.25f * i / 44100.0f) * 0.5f + 0.5f;
        sample *= envelope;
        
        audio[i] = sample;
    }
    
    return audio;
}

std::shared_ptr<Session> TestDataGenerator::generateRockBandSession() {
    auto session = std::make_shared<Session>();
    session->setName("Rock Band Session");
    session->setBPM(140.0);
    
    // Create rock band tracks
    std::vector<std::pair<std::string, TrackType>> tracks = {
        {"Kick", TrackType::Audio},
        {"Snare", TrackType::Audio},
        {"Hi-Hat", TrackType::Audio},
        {"Bass", TrackType::Audio},
        {"Rhythm Guitar", TrackType::Audio},
        {"Lead Guitar", TrackType::Audio},
        {"Vocals", TrackType::Audio},
        {"Backing Vocals", TrackType::Audio}
    };
    
    for (const auto& trackInfo : tracks) {
        auto track = std::make_shared<Track>(trackInfo.first, trackInfo.second);
        track->setName(trackInfo.first);
        session->addTrack(track);
    }
    
    return session;
}

std::shared_ptr<Session> TestDataGenerator::generateElectronicSession() {
    auto session = std::make_shared<Session>();
    session->setName("Electronic Session");
    session->setBPM(128.0);
    
    // Create electronic music tracks
    std::vector<std::pair<std::string, TrackType>> tracks = {
        {"Kick Drum", TrackType::Audio},
        {"Snare Drum", TrackType::Audio},
        {"Hi-Hats", TrackType::Audio},
        {"Percussion", TrackType::Audio},
        {"Bass Synth", TrackType::MIDI},
        {"Lead Synth", TrackType::MIDI},
        {"Pad Synth", TrackType::MIDI},
        {"Arpeggio", TrackType::MIDI},
        {"FX", TrackType::Audio}
    };
    
    for (const auto& trackInfo : tracks) {
        auto track = std::make_shared<Track>(trackInfo.first, trackInfo.second);
        track->setName(trackInfo.first);
        session->addTrack(track);
    }
    
    return session;
}

std::shared_ptr<Session> TestDataGenerator::generatePodcastSession() {
    auto session = std::make_shared<Session>();
    session->setName("Podcast Session");
    session->setBPM(120.0);
    
    // Create podcast tracks
    std::vector<std::pair<std::string, TrackType>> tracks = {
        {"Host Voice", TrackType::Audio},
        {"Guest Voice", TrackType::Audio},
        {"Interview", TrackType::Audio},
        {"Music Bed", TrackType::Audio},
        {"Sound Effects", TrackType::Audio},
        {"Ad Breaks", TrackType::Audio}
    };
    
    for (const auto& trackInfo : tracks) {
        auto track = std::make_shared<Track>(trackInfo.first, trackInfo.second);
        track->setName(trackInfo.first);
        session->addTrack(track);
    }
    
    return session;
}

std::shared_ptr<Session> TestDataGenerator::generateOrchestralSession() {
    auto session = std::make_shared<Session>();
    session->setName("Orchestral Session");
    session->setBPM(80.0);
    
    // Create orchestral sections
    std::vector<std::pair<std::string, TrackType>> tracks = {
        {"Violins I", TrackType::Audio},
        {"Violins II", TrackType::Audio},
        {"Violas", TrackType::Audio},
        {"Cellos", TrackType::Audio},
        {"Double Basses", TrackType::Audio},
        {"Flutes", TrackType::Audio},
        {"Oboes", TrackType::Audio},
        {"Clarinets", TrackType::Audio},
        {"Bassoons", TrackType::Audio},
        {"French Horns", TrackType::Audio},
        {"Trumpets", TrackType::Audio},
        {"Trombones", TrackType::Audio},
        {"Tuba", TrackType::Audio},
        {"Timpani", TrackType::Audio},
        {"Percussion", TrackType::Audio},
        {"Harp", TrackType::Audio}
    };
    
    for (const auto& trackInfo : tracks) {
        auto track = std::make_shared<Track>(trackInfo.first, trackInfo.second);
        track->setName(trackInfo.first);
        session->addTrack(track);
    }
    
    return session;
}

std::vector<AutomationPoint> TestDataGenerator::generateVolumeAutomation() {
    std::vector<AutomationPoint> points;
    
    // Generate fade in/out automation
    points.push_back({0, 0.0f, 0.0f});      // Start silent
    points.push_back({44100, 1.0f, 0.0f});  // Fade in over 1 second
    points.push_back({44100 * 3, 1.0f, 0.0f}); // Hold for 2 seconds
    points.push_back({44100 * 4, 0.0f, 0.0f}); // Fade out over 1 second
    
    return points;
}

std::vector<AutomationPoint> TestDataGenerator::generatePanAutomation() {
    std::vector<AutomationPoint> points;
    
    // Generate left-to-right pan sweep
    points.push_back({0, -1.0f, 0.0f});      // Start hard left
    points.push_back({44100 * 2, 1.0f, 0.0f}); // Sweep to hard right over 2 seconds
    
    return points;
}

std::vector<AutomationPoint> TestDataGenerator::generateEQAutomation() {
    std::vector<AutomationPoint> points;
    
    // Generate EQ automation for high shelf boost
    points.push_back({0, 0.0f, 0.0f});        // Start flat
    points.push_back({44100, 3.0f, 0.0f});    // Boost by 3dB after 1 second
    points.push_back({44100 * 3, 3.0f, 0.0f}); // Hold
    points.push_back({44100 * 4, 0.0f, 0.0f}); // Return to flat
    
    return points;
}

std::vector<AutomationPoint> TestDataGenerator::generateReverbAutomation() {
    std::vector<AutomationPoint> points;
    
    // Generate reverb send automation
    points.push_back({0, 0.0f, 0.0f});        // No reverb at start
    points.push_back({44100, 0.3f, 0.0f});    // Add reverb after 1 second
    points.push_back({44100 * 3, 0.3f, 0.0f}); // Hold
    points.push_back({44100 * 4, 0.0f, 0.0f}); // Remove reverb at end
    
    return points;
}

// TestEnvironment implementation
void TestEnvironment::SetUp() {
    testDataDirectory_ = TestUtilities::getTestDataPath();
    std::filesystem::create_directories(testDataDirectory_);
    
    juce::Logger::writeToLog("Test environment set up: " + testDataDirectory_);
}

void TestEnvironment::TearDown() {
    if (cleanupAfterTests_) {
        TestUtilities::cleanupTestData();
        juce::Logger::writeToLog("Test environment cleaned up");
    }
}

} // namespace tests
} // namespace neurato
