#pragma once

#include <gtest/gtest.h>
#include "engine/graph/AudioGraph.hpp"
#include "engine/graph/Automation.hpp"
#include "engine/graph/AudioProcessors.hpp"
#include "engine/plugins/host/PluginHost.hpp"
#include "engine/plugins/host/SandboxHost.hpp"
#include "ai/AIComponents.hpp"
#include "ai/AIImplementation.hpp"
#include "model/Session.hpp"
#include <memory>
#include <vector>
#include <string>

namespace ampl {
namespace tests {

// Test utilities
class TestUtilities {
public:
    // Generate test audio data
    static std::vector<float> generateSineWave(float frequency, double sampleRate, int numSamples);
    static std::vector<float> generateWhiteNoise(int numSamples);
    static std::vector<float> generateImpulseTrain(int numSamples, int period);

    // Create test sessions
    static std::shared_ptr<Session> createTestSession();
    static std::shared_ptr<Session> createComplexTestSession();

    // Audio analysis helpers
    static float calculateRMS(const float* audio, int numSamples);
    static float calculatePeak(const float* audio, int numSamples);
    static std::vector<float> calculateFFT(const float* audio, int numSamples);

    // File system helpers
    static std::string getTestDataPath();
    static void createTestAudioFile(const std::string& path, const std::vector<float>& audio,
                                   double sampleRate);
    static void cleanupTestData();
};

// Audio Graph Tests
class AudioGraphTest : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    std::unique_ptr<AudioGraph> graph_;
    std::vector<std::shared_ptr<AudioNode>> nodes_;
};

// Automation Tests
class AutomationTest : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    std::unique_ptr<AutomationLane> lane_;
    std::unique_ptr<AutomationManager> manager_;
};

// Audio Processor Tests
class AudioProcessorTest : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    std::unique_ptr<GainNode> gainNode_;
    std::unique_ptr<EQNode> eqNode_;
    std::unique_ptr<CompressorNode> compressorNode_;

    std::vector<float> testAudio_;
    const int kTestSampleRate = 44100;
    const int kTestBufferSize = 512;
};

// Plugin Host Tests
class PluginHostTest : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    std::unique_ptr<PluginManager> pluginManager_;
    std::unique_ptr<PluginScanner> pluginScanner_;
};

// Sandbox Host Tests
class SandboxHostTest : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    std::unique_ptr<SandboxPluginHost> sandboxHost_;
};

// AI Components Tests
class AIComponentsTest : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    std::unique_ptr<SessionStateAPI> sessionState_;
    std::shared_ptr<Session> testSession_;
    std::unique_ptr<LocalInferenceRuntime> inference_;
};

// AI Planner Tests
class AIPlannerTest : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    std::unique_ptr<SimpleAIPlanner> planner_;
    std::unique_ptr<SessionStateAPI> sessionState_;
    std::shared_ptr<Session> testSession_;
};

// Mix Assistant Tests
class MixAssistantTest : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    std::unique_ptr<MixAssistant> mixAssistant_;
    std::unique_ptr<LocalInferenceRuntime> inference_;
    SessionSnapshot testSnapshot_;
};

// Transient Detector Tests
class TransientDetectorTest : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    std::unique_ptr<TransientDetector> detector_;
    std::vector<float> testAudio_;
    const int kTestSampleRate = 44100;
};

// Command Palette Tests
class CommandPaletteTest : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    std::unique_ptr<CommandPalette> commandPalette_;
    std::unique_ptr<SessionStateAPI> sessionState_;
};

// Edit Preview Tests
class EditPreviewTest : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    std::unique_ptr<EditPreviewUI> previewUI_;
    std::unique_ptr<SessionStateAPI> sessionState_;
};

// Integration Tests
class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    // Complete system components
    std::unique_ptr<AudioGraph> audioGraph_;
    std::unique_ptr<SessionStateAPI> sessionState_;
    std::unique_ptr<PluginManager> pluginManager_;
    std::unique_ptr<LocalInferenceRuntime> inference_;
    std::unique_ptr<SimpleAIPlanner> aiPlanner_;
    std::unique_ptr<MixAssistant> mixAssistant_;
    std::unique_ptr<CommandPalette> commandPalette_;
    std::shared_ptr<Session> testSession_;
};

// Performance Tests
class PerformanceTest : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    std::unique_ptr<AudioGraph> graph_;
    std::vector<std::shared_ptr<AudioNode>> nodes_;

    static constexpr int kPerformanceTestIterations = 1000;
    static constexpr int kLargeBufferSize = 4096;
};

// Stress Tests
class StressTest : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    std::unique_ptr<AudioGraph> graph_;
    std::unique_ptr<SessionStateAPI> sessionState_;

    static constexpr int kStressTestTracks = 100;
    static constexpr int kStressTestClips = 1000;
    static constexpr int kStressTestPlugins = 500;
};

// Regression Tests
class RegressionTest : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    std::unique_ptr<SessionStateAPI> sessionState_;
    std::shared_ptr<Session> testSession_;

    // Known good states for regression testing
    SessionSnapshot knownGoodSnapshot_;
    std::string knownGoodProjectPath_;
};

// Test Fixtures for Specific Scenarios
class RealWorldScenarioTest : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    // Simulate real-world usage scenarios
    void simulatePodcastEditing();
    void simulateMusicProduction();
    void simulateLiveRecording();
    void simulateMixingSession();

    std::unique_ptr<AudioGraph> audioGraph_;
    std::unique_ptr<SessionStateAPI> sessionState_;
    std::unique_ptr<MixAssistant> mixAssistant_;
};

// End-to-End Workflow Tests
class WorkflowTest : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;

    // Complete workflow testing
    void testCompleteAudioProductionWorkflow();
    void testAIAssistedMixingWorkflow();
    void testPluginChainingWorkflow();
    void testAutomationWorkflow();
    void testCollaborativeWorkflow();

    std::unique_ptr<AudioGraph> audioGraph_;
    std::unique_ptr<SessionStateAPI> sessionState_;
    std::unique_ptr<SimpleAIPlanner> aiPlanner_;
    std::unique_ptr<CommandPalette> commandPalette_;
};

// Test Data Generators
class TestDataGenerator {
public:
    // Generate various types of test audio
    static std::vector<float> generateDrumLoop();
    static std::vector<float> generateBassLine();
    static std::vector<float> generateMelody();
    static std::vector<float> generateSpeech();
    static std::vector<float> generateOrchestral();

    // Generate test sessions with specific characteristics
    static std::shared_ptr<Session> generateRockBandSession();
    static std::shared_ptr<Session> generateElectronicSession();
    static std::shared_ptr<Session> generatePodcastSession();
    static std::shared_ptr<Session> generateOrchestralSession();

    // Generate automation data
    static std::vector<AutomationPoint> generateVolumeAutomation();
    static std::vector<AutomationPoint> generatePanAutomation();
    static std::vector<AutomationPoint> generateEQAutomation();
    static std::vector<AutomationPoint> generateReverbAutomation();
};

// Test Assertions and Matchers
namespace TestMatchers {
    // Custom matchers for audio data
    ::testing::Matcher<const std::vector<float>&> AudioRMSNear(float expected, float tolerance = 0.01f);
    ::testing::Matcher<const std::vector<float>&> AudioPeakNear(float expected, float tolerance = 0.01f);
    ::testing::Matcher<const std::vector<float>&> AudioContainsFrequency(float frequency, float tolerance = 10.0f);

    // Custom matchers for session state
    ::testing::Matcher<const SessionSnapshot&> HasTrackCount(int expected);
    ::testing::Matcher<const SessionSnapshot&> HasClipCount(int expected);
    ::testing::Matcher<const SessionSnapshot&> HasPluginCount(int expected);

    // Custom matchers for AI responses
    ::testing::Matcher<const AIPlanner::PlanningResponse&> HasConfidenceAbove(float threshold);
    ::testing::Matcher<const MixAssistant::MixResponse&> HasSuggestionCount(int expected);
}

// Test Environment Setup
class TestEnvironment : public ::testing::Environment {
public:
    void SetUp() override;
    void TearDown() override;

private:
    std::string testDataDirectory_;
    bool cleanupAfterTests_{true};
};

// Parameterized Tests
class AudioProcessorParameterizedTest :
    public ::testing::TestWithParam<std::tuple<std::string, float, float>> {
protected:
    void SetUp() override;

    std::unique_ptr<AudioNode> processor_;
    std::vector<float> testAudio_;
};

class PluginParameterizedTest :
    public ::testing::TestWithParam<std::tuple<std::string, std::string>> {
protected:
    void SetUp() override;

    std::unique_ptr<PluginManager> pluginManager_;
    std::string testPluginPath_;
};

// Mock Objects for Testing
class MockAudioNode : public AudioNode {
public:
    MockAudioNode(const std::string& id) : AudioNode(AudioNode::Type::Gain, id) {}

    MOCK_METHOD(void, process, (AudioBuffer& input, AudioBuffer& output,
                               int numSamples, SampleCount position), (override));

    std::vector<ParameterInfo> getParameters() const override {
        return {{"gain", "Gain", 0.0f, 2.0f, 1.0f, true, ""}};
    }

    float getParameterValue(const std::string& paramId) const override {
        return 1.0f;
    }

    void setParameterValue(const std::string& paramId, float value) override {}
};

class MockAIPlanner : public AIPlanner {
public:
    MOCK_METHOD(PlanningResponse, planActions, (const PlanningRequest& request), (override));
    MOCK_METHOD(bool, isAvailable, (), (const, override));
    MOCK_METHOD(std::string, getModelInfo, (), (const, override));
    MOCK_METHOD(void, provideFeedback, (const PlanningRequest& request,
                                        const PlanningResponse& response,
                                        bool wasHelpful), (override));
};

} // namespace tests
} // namespace ampl
