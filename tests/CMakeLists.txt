cmake_minimum_required(VERSION 3.22)

find_package(GTest REQUIRED)

add_executable(ampl_e2e_tests
    E2EWorkflows.cpp
    E2EPhase3AI.cpp
    ${CMAKE_SOURCE_DIR}/src/model/Session.cpp
    ${CMAKE_SOURCE_DIR}/src/model/ProjectSerializer.cpp
    ${CMAKE_SOURCE_DIR}/src/commands/CommandManager.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/OfflineRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/AIComponents.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/AIImplementation.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/MixAssistant.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/panels/mixer/TrackInfoPanel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/Theme.cpp
)

target_include_directories(ampl_e2e_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
)

target_link_libraries(ampl_e2e_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_core
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
)

target_compile_definitions(ampl_e2e_tests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

# Real I/O integration tests (MIDI, Audio, VST/AU plugins)
add_executable(ampl_real_io_tests
    e2e/RealIOTests.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/io/ExternalIOManager.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/core/AudioEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/core/Transport.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/core/Metronome.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/core/AudioTrack.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/SessionRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/OfflineRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/plugins/manager/PluginManager.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/plugins/instruments/PianoSynth.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/plugins/host/PluginHost.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/plugins/host/SandboxHost.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/graph/AudioGraph.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/graph/Automation.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/graph/AudioProcessors.cpp
    ${CMAKE_SOURCE_DIR}/src/model/Session.cpp
    ${CMAKE_SOURCE_DIR}/src/model/ProjectSerializer.cpp
    ${CMAKE_SOURCE_DIR}/src/commands/CommandManager.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/AIComponents.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/AIImplementation.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/MixAssistant.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/panels/mixer/TrackInfoPanel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/Theme.cpp
)

target_include_directories(ampl_real_io_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
)

target_link_libraries(ampl_real_io_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
)

target_compile_definitions(ampl_real_io_tests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_VST2=0
    AMPL_ENABLE_AUTOMATION=1
    AMPL_ENABLE_AUDIO_GRAPH=1
    AMPL_ENABLE_AI=1
    AMPL_ENABLE_LOCAL_INFERENCE=1
    AMPL_ENABLE_SANDBOX=1
    AMPL_ENABLE_LOGIC_FEATURES=1
    AMPL_ENABLE_ADVANCED_MIXER=1
    AMPL_ENABLE_VCA_GROUPS=1
    AMPL_ENABLE_FLEX_TIME=1
)

enable_testing()
add_test(NAME AmplE2E COMMAND ampl_e2e_tests)
add_test(NAME AmplRealIO COMMAND ampl_real_io_tests)
