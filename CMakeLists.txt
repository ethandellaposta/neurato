cmake_minimum_required(VERSION 3.22)
project(Ampl VERSION 0.1.0 LANGUAGES C CXX)

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
juce_add_gui_app(Ampl
    PRODUCT_NAME "Ampl"
    COMPANY_NAME "Ampl"
    BUNDLE_ID "com.ampl.daw"
    VERSION "0.1.0"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/Ampl.icns"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/Ampl.icns"
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
)

target_sources(Ampl PRIVATE
    src/app/Main.cpp
    src/app/MacHelpers.mm
    src/engine/core/AudioEngine.cpp
    src/engine/core/Transport.cpp
    src/engine/core/Metronome.cpp
    src/engine/core/AudioTrack.cpp
    src/engine/render/OfflineRenderer.cpp
    src/engine/render/SessionRenderer.cpp
    src/engine/plugins/manager/PluginManager.cpp
    src/engine/plugins/instruments/PianoSynth.cpp
    # External I/O (MIDI + Audio input)
    src/engine/io/ExternalIOManager.cpp
    # Milestone 5: Audio Graph & Automation
    src/engine/graph/AudioGraph.cpp
    src/engine/graph/Automation.cpp
    src/engine/graph/AudioProcessors.cpp
    # Milestone 6: Plugin Hosting
    src/engine/plugins/host/PluginHost.cpp
    src/engine/plugins/host/SandboxHost.cpp
    # Milestone 7: AI Layer
    src/ai/AIComponents.cpp
    src/ai/AIImplementation.cpp
    src/ai/MixAssistant.cpp
    # Logic Pro X-style Features
    src/engine/logic/LogicFeatures.cpp
    # Import functionality
    src/import/LogicImporter.cpp
    # src/ui/LogicMixerPanel.cpp  # Temporarily disabled
    src/model/Session.cpp
    src/model/ProjectSerializer.cpp
    src/commands/CommandManager.cpp
    # UI Components
    src/ui/timeline/TransportBar.cpp
    src/ui/timeline/TrackView.cpp
    src/ui/timeline/TimelineView.cpp
    src/ui/panels/io/AudioSettingsPanel.cpp
    src/ui/panels/io/AudioFileBrowser.cpp
    src/ui/panels/io/BounceProgressDialog.cpp
    src/ui/panels/mixer/MixerPanel.cpp
    src/ui/editors/PianoRollEditor.cpp
    src/ui/editors/AudioClipEditor.cpp
    src/ui/panels/mixer/TrackInfoPanel.cpp
    src/ui/panels/PianoKeyboardPanel.cpp
    src/ui/Theme.cpp
    src/util/RecentProjects.cpp
)

target_include_directories(Ampl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(Ampl PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Ampl,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Ampl,JUCE_VERSION>"
    # Plugin support
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_VST2=0
    # AI/ML support
    AMPL_ENABLE_AI=1
    AMPL_ENABLE_LOCAL_INFERENCE=1
    # Advanced features
    AMPL_ENABLE_SANDBOX=1
    AMPL_ENABLE_AUTOMATION=1
    AMPL_ENABLE_AUDIO_GRAPH=1
    # Logic Pro X-style features
    AMPL_ENABLE_LOGIC_FEATURES=1
    AMPL_ENABLE_ADVANCED_MIXER=1
    AMPL_ENABLE_VCA_GROUPS=1
    AMPL_ENABLE_FLEX_TIME=1
)

target_link_libraries(Ampl PRIVATE
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
set(BUILD_TESTS ON CACHE BOOL "Build test suite" FORCE)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Optional dependencies for AI features
option(ENABLE_AI_FEATURES "Enable AI features" ON)
if(ENABLE_AI_FEATURES)
    # Try to find ONNX Runtime
    find_package(ONNXRuntime QUIET)
    if(ONNXRuntime_FOUND)
        target_link_libraries(Ampl PRIVATE ONNXRuntime::ONNXRuntime)
        target_compile_definitions(Ampl PRIVATE AMPL_ONNX_AVAILABLE=1)
    endif()

    # Try to find llama.cpp
    find_package(llama QUIET)
    if(llama_FOUND)
        target_link_libraries(Ampl PRIVATE llama::llama)
        target_compile_definitions(Ampl PRIVATE AMPL_LLAMA_AVAILABLE=1)
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
install(TARGETS Ampl
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
