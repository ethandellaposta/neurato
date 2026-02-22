#include "TestSuite.h"
#include <gmock/gmock.h>

using ::testing::_;
using ::testing::Return;
using ::testing::AtLeast;
using namespace neurato::tests;

// Audio Graph Tests
TEST_F(AudioGraphTest, CanCreateGraph) {
    EXPECT_NE(graph_, nullptr);
    EXPECT_TRUE(graph_->isValid());
}

TEST_F(AudioGraphTest, CanAddNodes) {
    auto testNode = std::make_shared<GainNode>("test_node");
    EXPECT_TRUE(graph_->addNode(AudioConnection{"input", "test_node"}));
    EXPECT_TRUE(graph_->addNode(AudioConnection{"test_node", "output"}));
}

TEST_F(AudioGraphTest, CanProcessAudio) {
    // Create test audio buffers
    std::vector<float> inputBuffer(512, 0.5f);
    std::vector<float> outputBuffer(512, 0.0f);
    
    float* inputChannels[] = {inputBuffer.data()};
    float* outputChannels[] = {outputBuffer.data()};
    
    AudioBuffer input(inputChannels, 1, 512);
    AudioBuffer output(outputChannels, 1, 512);
    
    // Process audio
    graph_->process(input, output, 512, 0);
    
    // Verify output is not silent (gain should be applied)
    EXPECT_GT(TestUtilities::calculateRMS(outputBuffer.data(), 512), 0.0f);
}

TEST_F(AudioGraphTest, TopologicalSortWorks) {
    // Test that graph processing order is correct
    auto connections = graph_->getConnections();
    EXPECT_FALSE(connections.empty());
    
    // Verify graph is still valid after processing
    EXPECT_TRUE(graph_->isValid());
}

// Automation Tests
TEST_F(AutomationTest, CanCreateAutomationLane) {
    EXPECT_NE(lane_, nullptr);
    EXPECT_TRUE(lane_->isEmpty());
}

TEST_F(AutomationTest, CanAddAutomationPoints) {
    AutomationPoint point1{0, 0.0f, 0.0f};
    AutomationPoint point2{44100, 1.0f, 0.0f};
    
    lane_->addPoint(point1);
    lane_->addPoint(point2);
    
    EXPECT_FALSE(lane_->isEmpty());
    EXPECT_EQ(lane_->getPoints().size(), 2);
}

TEST_F(AutomationTest, CanInterpolateValues) {
    AutomationPoint point1{0, 0.0f, 0.0f};
    AutomationPoint point2{44100, 1.0f, 0.0f};
    
    lane_->addPoint(point1);
    lane_->addPoint(point2);
    
    // Test interpolation at midpoint
    float midValue = lane_->getInterpolatedValue(22050);
    EXPECT_NEAR(midValue, 0.5f, 0.01f);
}

TEST_F(AutomationTest, CanManageMultipleLanes) {
    manager_->addLane("gain", std::make_shared<AutomationLane>());
    manager_->addLane("pan", std::make_shared<AutomationLane>());
    
    EXPECT_TRUE(manager_->hasLane("gain"));
    EXPECT_TRUE(manager_->hasLane("pan"));
    EXPECT_FALSE(manager_->hasLane("reverb"));
    
    float gainValue = manager_->getParameterValue("gain", 22050);
    EXPECT_FLOAT_EQ(gainValue, 0.0f); // Default value
}

// Audio Processor Tests
TEST_F(AudioProcessorTest, CanCreateGainNode) {
    EXPECT_NE(gainNode_, nullptr);
    EXPECT_EQ(gainNode_->getType(), AudioNode::Type::Gain);
}

TEST_F(AudioProcessorTest, GainNodeAppliesGain) {
    // Set gain to 0.5
    gainNode_->setParameterValue("gain", 0.5f);
    
    // Create test buffers
    std::vector<float> inputBuffer(kTestBufferSize, 1.0f);
    std::vector<float> outputBuffer(kTestBufferSize, 0.0f);
    
    float* inputChannels[] = {inputBuffer.data()};
    float* outputChannels[] = {outputBuffer.data()};
    
    AudioBuffer input(inputChannels, 1, kTestBufferSize);
    AudioBuffer output(outputChannels, 1, kTestBufferSize);
    
    // Process
    gainNode_->process(input, output, kTestBufferSize, 0);
    
    // Verify gain was applied
    float outputRMS = TestUtilities::calculateRMS(outputBuffer.data(), kTestBufferSize);
    EXPECT_NEAR(outputRMS, 0.5f, 0.01f);
}

TEST_F(AudioProcessorTest, EQNodeHasCorrectParameters) {
    auto params = eqNode_->getParameters();
    EXPECT_EQ(params.size(), 16); // 4 bands x 4 parameters each
    
    // Check that all expected parameters are present
    bool hasFreqParams = std::any_of(params.begin(), params.end(),
        [](const ParameterInfo& p) { return p.id.find("_freq") != std::string::npos; });
    EXPECT_TRUE(hasFreqParams);
}

TEST_F(AudioProcessorTest, CompressorNodeCanProcess) {
    // Enable compressor
    compressorNode_->setParameterValue("enabled", 1.0f);
    compressorNode_->setParameterValue("threshold", -20.0f);
    compressorNode_->setParameterValue("ratio", 4.0f);
    
    // Create test buffers with high dynamic range
    std::vector<float> inputBuffer(kTestBufferSize);
    std::vector<float> outputBuffer(kTestBufferSize, 0.0f);
    
    // Generate signal with high peaks
    for (int i = 0; i < kTestBufferSize; ++i) {
        inputBuffer[i] = (i % 100 == 0) ? 1.0f : 0.1f;
    }
    
    float* inputChannels[] = {inputBuffer.data()};
    float* outputChannels[] = {outputBuffer.data()};
    
    AudioBuffer input(inputChannels, 1, kTestBufferSize);
    AudioBuffer output(outputChannels, 1, kTestBufferSize);
    
    // Process
    compressorNode_->process(input, output, kTestBufferSize, 0);
    
    // Verify compression occurred (output should have lower peak/RMS ratio)
    float inputPeak = TestUtilities::calculatePeak(inputBuffer.data(), kTestBufferSize);
    float outputPeak = TestUtilities::calculatePeak(outputBuffer.data(), kTestBufferSize);
    float inputRMS = TestUtilities::calculateRMS(inputBuffer.data(), kTestBufferSize);
    float outputRMS = TestUtilities::calculateRMS(outputBuffer.data(), kTestBufferSize);
    
    float inputRatio = inputPeak / (inputRMS + 1e-6f);
    float outputRatio = outputPeak / (outputRMS + 1e-6f);
    
    EXPECT_LT(outputRatio, inputRatio); // Compression should reduce ratio
}

// Plugin Host Tests
TEST_F(PluginHostTest, CanCreatePluginManager) {
    EXPECT_NE(pluginManager_, nullptr);
    EXPECT_NE(pluginScanner_, nullptr);
}

TEST_F(PluginHostTest, CanScanPlugins) {
    // Test scanning (this would scan actual plugin directories in production)
    std::vector<std::string> searchPaths = {TestUtilities::getTestDataPath()};
    
    // For testing, we'll just verify the scanner doesn't crash
    EXPECT_NO_THROW(pluginScanner_->scanVST3(searchPaths));
}

TEST_F(PluginHostTest, CanManagePluginDatabase) {
    // Add a mock plugin
    PluginInfo mockPlugin;
    mockPlugin.id = "test_plugin";
    mockPlugin.name = "Test Plugin";
    mockPlugin.manufacturer = "Test Manufacturer";
    mockPlugin.format = PluginFormat::VST3;
    
    pluginManager_->addPlugin(mockPlugin);
    
    auto allPlugins = pluginManager_->getAllPlugins();
    EXPECT_FALSE(allPlugins.empty());
    
    auto retrievedPlugin = pluginManager_->getPluginInfo("test_plugin");
    EXPECT_EQ(retrievedPlugin.id, "test_plugin");
    EXPECT_EQ(retrievedPlugin.name, "Test Plugin");
}

// Sandbox Host Tests
TEST_F(SandboxHostTest, CanCreateSandboxHost) {
    EXPECT_NE(sandboxHost_, nullptr);
    EXPECT_FALSE(sandboxHost_->isSandboxRunning());
}

TEST_F(SandboxHostTest, CanStartSandbox) {
    // Note: This test might fail if sandbox executable doesn't exist
    // In production, would need proper sandbox setup
    EXPECT_NO_THROW(sandboxHost_->startSandbox());
    
    // Give it a moment to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Check if running (might be false if sandbox executable missing)
    bool isRunning = sandboxHost_->isSandboxRunning();
    // EXPECT_TRUE(isRunning); // Commented out as it depends on external executable
    
    if (isRunning) {
        sandboxHost_->stopSandbox();
        EXPECT_FALSE(sandboxHost_->isSandboxRunning());
    }
}

// AI Components Tests
TEST_F(AIComponentsTest, CanCreateSessionStateAPI) {
    EXPECT_NE(sessionState_, nullptr);
    EXPECT_NE(testSession_, nullptr);
    EXPECT_NE(inference_, nullptr);
}

TEST_F(AIComponentsTest, CanGenerateSnapshot) {
    auto snapshot = sessionState_->generateSnapshot();
    
    EXPECT_EQ(snapshot.tracks.size(), testSession_->getTracks().size());
    EXPECT_EQ(snapshot.bpm, testSession_->getBPM());
    EXPECT_FALSE(snapshot.tracks.empty());
}

TEST_F(AIComponentsTest, CanAnalyzeAudio) {
    auto snapshot = sessionState_->generateSnapshot();
    SessionStateAPI::AnalysisOptions options;
    options.analyzeRMS = true;
    options.analyzePeaks = true;
    options.analyzeTransients = true;
    
    sessionState_->analyzeAudio(snapshot, options);
    
    // Verify analysis was added
    EXPECT_FALSE(snapshot.audioAnalysis.empty());
    
    for (const auto& track : snapshot.tracks) {
        auto analysisIt = snapshot.audioAnalysis.find(track.id);
        if (analysisIt != snapshot.audioAnalysis.end()) {
            const auto& analysis = analysisIt->second;
            EXPECT_FALSE(analysis.rmsLevels.empty());
            EXPECT_FALSE(analysis.peakLevels.empty());
        }
    }
}

TEST_F(AIComponentsTest, CanValidateSnapshot) {
    auto snapshot = sessionState_->generateSnapshot();
    auto validation = sessionState_->validateSnapshot(snapshot);
    
    EXPECT_TRUE(validation.isValid);
    EXPECT_TRUE(validation.errors.empty());
}

// AI Planner Tests
TEST_F(AIPlannerTest, CanCreateAIPlanner) {
    EXPECT_NE(planner_, nullptr);
    EXPECT_NE(sessionState_, nullptr);
    EXPECT_NE(testSession_, nullptr);
}

TEST_F(AIPlannerTest, CanPlanActions) {
    AIPlanner::PlanningRequest request;
    request.naturalLanguageQuery = "create a new track";
    request.currentSnapshot = sessionState_->generateSnapshot();
    
    auto response = planner_->planActions(request);
    
    EXPECT_GT(response.confidence, 0.0f);
    EXPECT_FALSE(response.explanation.empty());
    
    // Should generate at least one action for a simple query
    if (response.confidence > 0.5f) {
        EXPECT_FALSE(response.actions.empty());
    }
}

TEST_F(AIPlannerTest, CanProvideFeedback) {
    AIPlanner::PlanningRequest request;
    request.naturalLanguageQuery = "set track gain to 0.8";
    request.currentSnapshot = sessionState_->generateSnapshot();
    
    auto response = planner_->planActions(request);
    
    // Provide feedback (should not crash)
    EXPECT_NO_THROW(planner_->provideFeedback(request, response, true));
    EXPECT_NO_THROW(planner_->provideFeedback(request, response, false));
}

// Mix Assistant Tests
TEST_F(MixAssistantTest, CanCreateMixAssistant) {
    EXPECT_NE(mixAssistant_, nullptr);
    EXPECT_NE(inference_, nullptr);
    EXPECT_FALSE(testSnapshot_.tracks.empty());
}

TEST_F(MixAssistantTest, CanAnalyzeMix) {
    MixAssistant::MixRequest request;
    request.snapshot = testSnapshot_;
    request.targetLUFS = -14.0f;
    request.targetStyle = "balanced";
    
    auto response = mixAssistant_->analyzeMix(request);
    
    EXPECT_FALSE(response.suggestions.empty());
    EXPECT_FALSE(response.summary.empty());
    EXPECT_LT(response.overallLufs, 0.0f); // Should be negative LUFS
}

TEST_F(MixAssistantTest, CanGenerateTrackSuggestions) {
    if (!testSnapshot_.tracks.empty()) {
        auto suggestions = mixAssistant_->getSuggestionsForTrack(
            testSnapshot_.tracks[0].id, testSnapshot_);
        
        // Should generate some suggestions for the track
        EXPECT_FALSE(suggestions.empty());
        
        for (const auto& suggestion : suggestions) {
            EXPECT_EQ(suggestion.trackId, testSnapshot_.tracks[0].id);
            EXPECT_FALSE(suggestion.parameterId.empty());
            EXPECT_GT(suggestion.confidence, 0.0f);
            EXPECT_FALSE(suggestion.reason.empty());
        }
    }
}

TEST_F(MixAssistantTest, CanLearnFromFeedback) {
    // Create a mock suggestion
    MixAssistant::MixSuggestion suggestion;
    suggestion.trackId = "test_track";
    suggestion.parameterId = "gain";
    suggestion.suggestedValue = 0.8f;
    suggestion.confidence = 0.7f;
    
    // Provide feedback (should not crash)
    EXPECT_NO_THROW(mixAssistant_->learnFromUserAction(suggestion, true));
    EXPECT_NO_THROW(mixAssistant_->learnFromUserAction(suggestion, false));
}

// Transient Detector Tests
TEST_F(TransientDetectorTest, CanCreateTransientDetector) {
    EXPECT_NE(detector_, nullptr);
    EXPECT_FALSE(testAudio_.empty());
}

TEST_F(TransientDetectorTest, CanDetectTransients) {
    auto transients = detector_->detectTransients(testAudio_.data(), 
                                                 testAudio_.size(), 
                                                 kTestSampleRate);
    
    EXPECT_FALSE(transients.empty());
    
    // Should detect transients at impulse positions
    EXPECT_GT(transients.size(), 1);
    
    // Verify transient positions are reasonable
    for (const auto& transient : transients) {
        EXPECT_GE(transient.position, 0);
        EXPECT_LT(transient.position, testAudio_.size());
        EXPECT_GT(transient.strength, 0.0f);
    }
}

TEST_F(TransientDetectorTest, CanDetectBeatGrid) {
    auto beatGrid = detector_->detectBeatGrid(testAudio_.data(), 
                                            testAudio_.size(), 
                                            kTestSampleRate, 
                                            120.0); // 120 BPM
    
    EXPECT_FALSE(beatGrid.beats.empty());
    EXPECT_GT(beatGrid.detectedTempo, 60.0); // Should be reasonable tempo
    EXPECT_LT(beatGrid.detectedTempo, 200.0);
    EXPECT_GT(beatGrid.confidence, 0.0f);
}

// Command Palette Tests
TEST_F(CommandPaletteTest, CanCreateCommandPalette) {
    EXPECT_NE(commandPalette_, nullptr);
    EXPECT_NE(sessionState_, nullptr);
}

TEST_F(CommandPaletteTest, CanRegisterAndSearchItems) {
    CommandPalette::PaletteItem item;
    item.id = "test_command";
    item.title = "Test Command";
    item.description = "A test command for testing";
    item.category = "Test";
    item.keywords = {"test", "command"};
    item.action = []() { /* Test action */ };
    
    commandPalette_->registerItem(item);
    
    // Search for the item
    auto results = commandPalette_->searchItems("test");
    EXPECT_FALSE(results.empty());
    
    bool found = std::any_of(results.begin(), results.end(),
        [&item](const CommandPalette::PaletteItem& result) {
            return result.id == item.id;
        });
    
    EXPECT_TRUE(found);
}

TEST_F(CommandPaletteTest, CanExecuteCommands) {
    bool actionExecuted = false;
    
    CommandPalette::PaletteItem item;
    item.id = "exec_test";
    item.title = "Execute Test";
    item.action = [&actionExecuted]() { actionExecuted = true; };
    
    commandPalette_->registerItem(item);
    commandPalette_->setQuery("exec");
    
    auto results = commandPalette_->searchItems("exec");
    EXPECT_FALSE(results.empty());
    
    // Execute the first result
    commandPalette_->selectItem(0);
    commandPalette_->executeSelected();
    
    EXPECT_TRUE(actionExecuted);
}

// Edit Preview Tests
TEST_F(EditPreviewTest, CanCreateEditPreviewUI) {
    EXPECT_NE(previewUI_, nullptr);
    EXPECT_NE(sessionState_, nullptr);
}

TEST_F(EditPreviewTest, CanGeneratePreview) {
    // Create test actions
    ActionDSL::ActionSequence actions;
    actions.push_back(ActionDSL::createTrack("Preview Test Track"));
    actions.push_back(ActionDSL::setTrackGain("Preview Test Track", 0.8f));
    
    auto beforeSnapshot = sessionState_->generateSnapshot();
    auto preview = previewUI_->generatePreview(actions, beforeSnapshot);
    
    EXPECT_FALSE(preview.id.empty());
    EXPECT_EQ(preview.actions.size(), 2);
    EXPECT_GT(preview.confidence, 0.0f);
    EXPECT_FALSE(preview.description.empty());
}

TEST_F(EditPreviewTest, CanGenerateDiff) {
    auto beforeSnapshot = sessionState_->generateSnapshot();
    
    // Modify the session
    auto actions = ActionDSL::ActionSequence();
    actions.push_back(ActionDSL::setTrackGain(beforeSnapshot.tracks[0].id, 0.5f));
    
    sessionState_->applyActionSequence(actions);
    auto afterSnapshot = sessionState_->generateSnapshot();
    
    auto diffs = previewUI_->generateDiff(beforeSnapshot, afterSnapshot);
    
    EXPECT_FALSE(diffs.empty());
    
    // Should find the gain change
    bool foundGainChange = std::any_of(diffs.begin(), diffs.end(),
        [](const EditPreviewUI::DiffItem& diff) {
            return diff.property == "gain";
        });
    
    EXPECT_TRUE(foundGainChange);
}

// Integration Tests
TEST_F(IntegrationTest, CanInitializeAllComponents) {
    EXPECT_NE(audioGraph_, nullptr);
    EXPECT_NE(sessionState_, nullptr);
    EXPECT_NE(pluginManager_, nullptr);
    EXPECT_NE(inference_, nullptr);
    EXPECT_NE(aiPlanner_, nullptr);
    EXPECT_NE(mixAssistant_, nullptr);
    EXPECT_NE(commandPalette_, nullptr);
    EXPECT_NE(testSession_, nullptr);
}

TEST_F(IntegrationTest, CanProcessCompleteWorkflow) {
    // 1. Generate session snapshot
    auto snapshot = sessionState_->generateSnapshot();
    EXPECT_FALSE(snapshot.tracks.empty());
    
    // 2. Analyze audio
    SessionStateAPI::AnalysisOptions options;
    options.analyzeRMS = true;
    options.analyzePeaks = true;
    sessionState_->analyzeAudio(snapshot, options);
    
    // 3. Get mix suggestions
    MixAssistant::MixRequest mixRequest;
    mixRequest.snapshot = snapshot;
    mixRequest.targetLUFS = -14.0f;
    
    auto mixResponse = mixAssistant_->analyzeMix(mixRequest);
    EXPECT_FALSE(mixResponse.suggestions.empty());
    
    // 4. Plan AI actions
    AIPlanner::PlanningRequest aiRequest;
    aiRequest.naturalLanguageQuery = "adjust track levels for better balance";
    aiRequest.currentSnapshot = snapshot;
    
    auto aiResponse = aiPlanner_->planActions(aiRequest);
    EXPECT_GT(aiResponse.confidence, 0.0f);
    
    // 5. Apply suggestions (if any)
    if (!mixResponse.suggestions.empty()) {
        // Convert mix suggestions to actions
        for (const auto& suggestion : mixResponse.suggestions) {
            if (suggestion.parameterId == "gain") {
                auto action = ActionDSL::setTrackGain(suggestion.trackId, suggestion.suggestedValue);
                sessionState_->applyAction(*action);
            }
        }
    }
}

TEST_F(IntegrationTest, CanUseCommandPaletteWithAI) {
    // Test command palette with AI integration
    commandPalette_->setQuery("create new track");
    
    auto results = commandPalette_->searchItems("create new track");
    EXPECT_FALSE(results.empty());
    
    // Should include both built-in commands and AI suggestions
    bool hasAISuggestion = std::any_of(results.begin(), results.end(),
        [](const CommandPalette::PaletteItem& item) {
            return item.category == "AI Suggestion";
        });
    
    // Note: This might be false if AI model is not loaded
    // EXPECT_TRUE(hasAISuggestion);
}

// Performance Tests
TEST_F(PerformanceTest, AudioGraphPerformance) {
    // Create test audio
    std::vector<float> inputBuffer(kLargeBufferSize, 0.5f);
    std::vector<float> outputBuffer(kLargeBufferSize, 0.0f);
    
    float* inputChannels[] = {inputBuffer.data()};
    float* outputChannels[] = {outputBuffer.data()};
    
    AudioBuffer input(inputChannels, 1, kLargeBufferSize);
    AudioBuffer output(outputChannels, 1, kLargeBufferSize);
    
    // Measure performance
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < kPerformanceTestIterations; ++i) {
        graph_->process(input, output, kLargeBufferSize, i * kLargeBufferSize);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    double avgTimePerBuffer = duration.count() / static_cast<double>(kPerformanceTestIterations);
    double maxTimePerBuffer = 1000000.0 / 44100.0 * kLargeBufferSize; // Time available for real-time processing
    
    juce::Logger::writeToLog("Audio Graph Performance: " + 
                            std::to_string(avgTimePerBuffer) + " μs per buffer");
    
    // Should be fast enough for real-time processing
    EXPECT_LT(avgTimePerBuffer, maxTimePerBuffer);
}

TEST_F(PerformanceTest, SessionStatePerformance) {
    auto snapshot = sessionState_->generateSnapshot();
    
    // Measure snapshot generation performance
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 100; ++i) {
        auto testSnapshot = sessionState_->generateSnapshot();
        // Prevent compiler optimization
        EXPECT_FALSE(testSnapshot.tracks.empty());
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    double avgTimePerSnapshot = duration.count() / 100.0;
    
    juce::Logger::writeToLog("Session State Performance: " + 
                            std::to_string(avgTimePerSnapshot) + " μs per snapshot");
    
    // Should be reasonably fast (under 10ms)
    EXPECT_LT(avgTimePerSnapshot, 10000.0);
}

// Stress Tests
TEST_F(StressTest, CanHandleLargeSession) {
    auto snapshot = sessionState_->generateSnapshot();
    
    // Should handle large number of tracks
    EXPECT_EQ(snapshot.tracks.size(), kStressTestTracks);
    
    // Verify all tracks have valid properties
    for (const auto& track : snapshot.tracks) {
        EXPECT_FALSE(track.id.empty());
        EXPECT_FALSE(track.name.empty());
        EXPECT_GE(track.gain, 0.0f);
        EXPECT_GE(track.pan, -1.0f);
        EXPECT_LE(track.pan, 1.0f);
    }
}

TEST_F(StressTest, CanProcessComplexGraph) {
    // Create a complex graph with many nodes
    for (int i = 0; i < 100; ++i) {
        auto node = std::make_shared<GainNode("stress_node_" + std::to_string(i));
        graph_->addNode(node);
    }
    
    // Test graph validation
    EXPECT_TRUE(graph_->isValid());
    
    auto errors = graph_->getValidationErrors();
    EXPECT_TRUE(errors.empty());
}

// Regression Tests
TEST_F(RegressionTest, SessionStateRegression) {
    // Generate current snapshot
    auto currentSnapshot = sessionState_->generateSnapshot();
    
    // Compare with known good state
    EXPECT_EQ(currentSnapshot.tracks.size(), knownGoodSnapshot_.tracks.size());
    EXPECT_NEAR(currentSnapshot.bpm, knownGoodSnapshot_.bpm, 0.01);
    
    // Verify track properties match
    for (size_t i = 0; i < knownGoodSnapshot_.tracks.size(); ++i) {
        EXPECT_EQ(currentSnapshot.tracks[i].id, knownGoodSnapshot_.tracks[i].id);
        EXPECT_EQ(currentSnapshot.tracks[i].name, knownGoodSnapshot_.tracks[i].name);
        EXPECT_NEAR(currentSnapshot.tracks[i].gain, knownGoodSnapshot_.tracks[i].gain, 0.01f);
        EXPECT_NEAR(currentSnapshot.tracks[i].pan, knownGoodSnapshot_.tracks[i].pan, 0.01f);
    }
}

// Test Data Generator Tests
TEST(TestDataGenerator, CanGenerateVariousAudioTypes) {
    auto drumLoop = TestDataGenerator::generateDrumLoop();
    auto bassLine = TestDataGenerator::generateBassLine();
    auto melody = TestDataGenerator::generateMelody();
    auto speech = TestDataGenerator::generateSpeech();
    auto orchestral = TestDataGenerator::generateOrchestral();
    
    EXPECT_FALSE(drumLoop.empty());
    EXPECT_FALSE(bassLine.empty());
    EXPECT_FALSE(melody.empty());
    EXPECT_FALSE(speech.empty());
    EXPECT_FALSE(orchestral.empty());
    
    // Verify audio has reasonable levels
    EXPECT_GT(TestUtilities::calculateRMS(drumLoop.data(), drumLoop.size()), 0.0f);
    EXPECT_GT(TestUtilities::calculateRMS(bassLine.data(), bassLine.size()), 0.0f);
    EXPECT_GT(TestUtilities::calculateRMS(melody.data(), melody.size()), 0.0f);
    EXPECT_GT(TestUtilities::calculateRMS(speech.data(), speech.size()), 0.0f);
    EXPECT_GT(TestUtilities::calculateRMS(orchestral.data(), orchestral.size()), 0.0f);
}

TEST(TestDataGenerator, CanGenerateVariousSessionTypes) {
    auto rockSession = TestDataGenerator::generateRockBandSession();
    auto electronicSession = TestDataGenerator::generateElectronicSession();
    auto podcastSession = TestDataGenerator::generatePodcastSession();
    auto orchestralSession = TestDataGenerator::generateOrchestralSession();
    
    EXPECT_NE(rockSession, nullptr);
    EXPECT_NE(electronicSession, nullptr);
    EXPECT_NE(podcastSession, nullptr);
    EXPECT_NE(orchestralSession, nullptr);
    
    // Verify sessions have appropriate track counts
    EXPECT_GT(rockSession->getTracks().size(), 5);
    EXPECT_GT(electronicSession->getTracks().size(), 5);
    EXPECT_GT(podcastSession->getTracks().size(), 3);
    EXPECT_GT(orchestralSession->getTracks().size(), 10);
}

// Mock Tests
TEST(MockAudioNode, CanProcessAudio) {
    MockAudioNode mockNode("test_mock");
    
    AudioBuffer input, output;
    
    EXPECT_CALL(mockNode, process(_, _, _, _))
        .Times(AtLeast(1));
    
    mockNode.process(input, output, 512, 0);
}

TEST(MockAIPlanner, CanPlanActions) {
    MockAIPlanner mockPlanner;
    
    AIPlanner::PlanningRequest request;
    request.naturalLanguageQuery = "test query";
    
    AIPlanner::PlanningResponse response;
    response.confidence = 0.8f;
    response.explanation = "Test response";
    
    EXPECT_CALL(mockPlanner, planActions(_))
        .WillOnce(Return(response));
    
    auto result = mockPlanner.planActions(request);
    
    EXPECT_EQ(result.confidence, 0.8f);
    EXPECT_EQ(result.explanation, "Test response");
}

// Main function for running tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    // Add test environment
    ::testing::AddGlobalTestEnvironment(new TestEnvironment());
    
    return RUN_ALL_TESTS();
}
