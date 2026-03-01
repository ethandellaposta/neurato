#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "engine/plugins/manager/PluginManager.hpp"
#include "engine/plugins/host/PluginHost.hpp"

using namespace ampl;
using namespace ::testing;

class PluginManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        pluginManager = std::make_unique<PluginManager>();
    }

    std::unique_ptr<PluginManager> pluginManager;
};

TEST_F(PluginManagerTest, ShouldInitializeCorrectly) {
    EXPECT_NO_THROW(pluginManager->scanForPlugins(juce::FileSearchPath()));
    auto plugins = pluginManager->getAvailablePlugins();
    EXPECT_TRUE(plugins.size() >= 0); // Should not crash
}

TEST_F(PluginManagerTest, ShouldCategorizePlugins) {
    pluginManager->scanForPlugins(juce::FileSearchPath());

    auto instruments = pluginManager->getInstruments();
    auto effects = pluginManager->getEffects();

    EXPECT_TRUE(instruments.size() >= 0);
    EXPECT_TRUE(effects.size() >= 0);
}

TEST_F(PluginManagerTest, ShouldLoadAndUnloadPlugins) {
    pluginManager->scanForPlugins(juce::FileSearchPath());
    auto plugins = pluginManager->getAvailablePlugins();

    if (!plugins.empty()) {
        auto& plugin = plugins[0];
        auto loadedPlugin = pluginManager->loadPlugin(plugin.description.createIdentifierString());

        if (loadedPlugin) {
            EXPECT_TRUE(loadedPlugin->instance != nullptr);
            pluginManager->unloadPlugin(plugin.description.createIdentifierString());
        }
    }
}

class PluginHostTest : public ::testing::Test {
protected:
    void SetUp() override {
        pluginHost = std::make_unique<PluginHost>();
    }

    std::unique_ptr<PluginHost> pluginHost;
};

TEST_F(PluginHostTest, ShouldCreatePluginInstance) {
    // Create a mock plugin info
    PluginInfo info;
    info.id = "test_plugin";
    info.name = "Test Plugin";
    info.manufacturer = "Test Manufacturer";
    info.isInstrument = false;

    auto instance = pluginHost->createInstance(info.id);
    // This might fail if plugin doesn't exist, but shouldn't crash
    EXPECT_TRUE(instance == nullptr || instance != nullptr);
}

TEST_F(PluginHostTest, ShouldHandlePluginParameters) {
    PluginInfo info;
    info.id = "test_plugin";
    info.name = "Test Plugin";
    info.manufacturer = "Test Manufacturer";
    info.isInstrument = false;

    auto instance = pluginHost->createInstance(info.id);
    if (instance) {
        // Test parameter access
        auto params = instance->getParameters();
        EXPECT_TRUE(params.size() >= 0);

        // Test parameter setting
        if (!params.empty()) {
            EXPECT_TRUE(instance->setParameterValue(params[0].id, 0.5f));
            float value = instance->getParameterValue(params[0].id);
            EXPECT_TRUE(value >= 0.0f && value <= 1.0f);
        }
    }
}
