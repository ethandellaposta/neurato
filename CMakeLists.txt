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
    # JUCE 8 no longer uses splash screen flag
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
)

# Tests (using JUCE's built-in test runner for now)
# Full test suite will use Catch2 in a later milestone
