#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "engine/core/AudioEngine.hpp"
#include "engine/graph/AudioProcessors.hpp"
#include "model/Session.hpp"

using namespace ampl;
using namespace ::testing;

class AudioEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = std::make_unique<AudioEngine>();
        session = std::make_unique<Session>();
    }

    void TearDown() override {
        engine.reset();
        session.reset();
    }

    std::unique_ptr<AudioEngine> engine;
    std::unique_ptr<Session> session;
};

TEST_F(AudioEngineTest, ShouldInitializeWithDefaultParameters) {
    EXPECT_TRUE(engine->initialize(44100.0, 512));
    EXPECT_EQ(engine->getSampleRate(), 44100.0);
    EXPECT_EQ(engine->getBufferSize(), 512);
    EXPECT_EQ(engine->getState(), AudioEngine::State::Stopped);
}

TEST_F(AudioEngineTest, ShouldHandleTransportControls) {
    ASSERT_TRUE(engine->initialize(44100.0, 512));

    // Test play
    engine->play();
    EXPECT_EQ(engine->getState(), AudioEngine::State::Playing);

    // Test pause
    engine->pause();
    EXPECT_EQ(engine->getState(), AudioEngine::State::Paused);

    // Test stop
    engine->stop();
    EXPECT_EQ(engine->getState(), AudioEngine::State::Stopped);
}

TEST_F(AudioEngineTest, ShouldProcessAudioWithoutErrors) {
    ASSERT_TRUE(engine->initialize(44100.0, 512));

    // Create test buffers
    std::vector<float*> inputBuffers(2);
    std::vector<float*> outputBuffers(2);
    std::vector<std::vector<float>> channelData(2, std::vector<float>(512, 0.0f));

    for (int i = 0; i < 2; ++i) {
        inputBuffers[i] = channelData[i].data();
        outputBuffers[i] = channelData[i].data();
    }

    // Process audio - should not crash
    EXPECT_NO_THROW(engine->processAudio(inputBuffers.data(), outputBuffers.data(), 2, 512, 0));
}

TEST_F(AudioEngineTest, ShouldManageSessionCorrectly) {
    ASSERT_TRUE(engine->initialize(44100.0, 512));

    // Test session loading
    EXPECT_TRUE(engine->loadSession(session.get()));
    EXPECT_EQ(engine->getCurrentSession(), session.get());

    // Test session creation
    auto newSession = engine->createSession("Test Session");
    EXPECT_NE(newSession, nullptr);
    EXPECT_EQ(newSession->getName(), "Test Session");
}

class AudioProcessorsTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor = std::make_unique<AudioProcessors>();
    }

    std::unique_ptr<AudioProcessors> processor;
};

TEST_F(AudioProcessorsTest, ShouldCreateEQProcessor) {
    auto eq = processor->createEQ("test_eq");
    EXPECT_NE(eq, nullptr);
    EXPECT_EQ(eq->getType(), AudioProcessor::Type::EQ);
}

TEST_F(AudioProcessorsTest, ShouldCreateCompressorProcessor) {
    auto compressor = processor->createCompressor("test_compressor");
    EXPECT_NE(compressor, nullptr);
    EXPECT_EQ(compressor->getType(), AudioProcessor::Type::Compressor);
}

TEST_F(AudioProcessorsTest, ShouldHandleProcessorParameters) {
    auto eq = processor->createEQ("test_eq");
    ASSERT_NE(eq, nullptr);

    // Test parameter setting
    EXPECT_TRUE(eq->setParameter("frequency", 1000.0f));
    EXPECT_FLOAT_EQ(eq->getParameter("frequency"), 1000.0f);

    // Test invalid parameter
    EXPECT_FALSE(eq->setParameter("invalid_param", 1.0f));
}
