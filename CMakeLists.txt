cmake_minimum_required(VERSION 3.22)
project(Neurato VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Fetch JUCE
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# Main application target
juce_add_gui_app(Neurato
    PRODUCT_NAME "Neurato"
    COMPANY_NAME "Neurato"
    BUNDLE_ID "com.neurato.daw"
    VERSION "0.1.0"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/Neurato.icns"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/Neurato.icns"
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
)

target_sources(Neurato PRIVATE
    src/app/Main.cpp
    src/engine/AudioEngine.cpp
    src/engine/Transport.cpp
    src/engine/Metronome.cpp
    src/engine/AudioTrack.cpp
    src/engine/OfflineRenderer.cpp
    src/engine/SessionRenderer.cpp
    src/engine/PluginManager.cpp
    src/engine/PianoSynth.cpp
    # Milestone 5: Audio Graph & Automation
    src/engine/AudioGraph.cpp
    src/engine/Automation.cpp
    src/engine/AudioProcessors.cpp
    # Milestone 6: Plugin Hosting
    src/engine/PluginHost.cpp
    src/engine/SandboxHost.cpp
    # Milestone 7: AI Layer
    src/ai/AIComponents.cpp
    src/ai/AIImplementation.cpp
    src/ai/MixAssistant.cpp
    # Logic Pro X-style Features
    src/engine/LogicFeatures.cpp
    src/engine/LogicAudioEngine.cpp
    src/ui/LogicMixerPanel.cpp
    src/model/Session.cpp
    src/model/ProjectSerializer.cpp
    src/commands/CommandManager.cpp
    src/ui/TransportBar.cpp
    src/ui/TrackView.cpp
    src/ui/TimelineView.cpp
    src/ui/AudioSettingsPanel.cpp
    src/ui/AudioFileBrowser.cpp
    src/ui/BounceProgressDialog.cpp
    src/ui/MixerPanel.cpp
    src/ui/PianoRollEditor.cpp
    src/ui/AudioClipEditor.cpp
    src/util/RecentProjects.cpp
)

target_include_directories(Neurato PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(Neurato PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Neurato,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Neurato,JUCE_VERSION>"
    # Plugin support
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_VST2=0
    # AI/ML support
    NEURATO_ENABLE_AI=1
    NEURATO_ENABLE_LOCAL_INFERENCE=1
    # Advanced features
    NEURATO_ENABLE_SANDBOX=1
    NEURATO_ENABLE_AUTOMATION=1
    NEURATO_ENABLE_AUDIO_GRAPH=1
    # Logic Pro X-style features
    NEURATO_ENABLE_LOGIC_FEATURES=1
    NEURATO_ENABLE_ADVANCED_MIXER=1
    NEURATO_ENABLE_VCA_GROUPS=1
    NEURATO_ENABLE_FLEX_TIME=1
)

target_link_libraries(Neurato PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
    # AI/ML libraries (if available)
    # $<IF:$<TARGET_EXISTS:ONNXRuntime::ONNXRuntime>,ONNXRuntime::ONNXRuntime,>
    # $<IF:$<TARGET_EXISTS:llama::llama>,llama::llama,>
)

# Test suite
option(BUILD_TESTS "Build test suite" ON)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Optional dependencies for AI features
option(ENABLE_AI_FEATURES "Enable AI features" ON)
if(ENABLE_AI_FEATURES)
    # Try to find ONNX Runtime
    find_package(ONNXRuntime QUIET)
    if(ONNXRuntime_FOUND)
        target_link_libraries(Neurato PRIVATE ONNXRuntime::ONNXRuntime)
        target_compile_definitions(Neurato PRIVATE NEURATO_ONNX_AVAILABLE=1)
    endif()

    # Try to find llama.cpp
    find_package(llama QUIET)
    if(llama_FOUND)
        target_link_libraries(Neurato PRIVATE llama::llama)
        target_compile_definitions(Neurato PRIVATE NEURATO_LLAMA_AVAILABLE=1)
    endif()
endif()

# Documentation
option(BUILD_DOCS "Build documentation" OFF)
if(BUILD_DOCS)
    find_package(Doxygen QUIET)
    if(DOXYGEN_FOUND)
        set(DOXYGEN_INPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
        set(DOXYGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/docs)

        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in
                       ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)

        add_custom_target(docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
    endif()
endif()

# Installation
install(TARGETS Neurato
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
