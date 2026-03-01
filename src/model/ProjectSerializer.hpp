#pragma once

#include "model/Session.hpp"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

namespace ampl
{

// Serializes/deserializes a Session to/from a JSON project file.
// Project format: .ampl (JSON metadata) + embedded audio assets (base64) with
// a relative path fallback for backward compatibility.
class ProjectSerializer
{
  public:
    // Save session to a .ampl project file.
    // Audio assets are referenced by relative path from the project directory.
    // Returns true on success.
    static bool save(const Session &session, const juce::File &projectFile);

    // Load session from a .ampl project file.
    // Audio assets are resolved relative to the project file location.
    // Returns true on success.
    static bool load(Session &session, const juce::File &projectFile,
                     juce::AudioFormatManager &formatManager);

    // Project file extension
    static constexpr const char *kFileExtension = ".ampl";

  private:
    static juce::var sessionToJson(const Session &session, const juce::File &projectDir);
    static bool jsonToSession(const juce::var &json, Session &session, const juce::File &projectDir,
                              juce::AudioFormatManager &formatManager);

    static juce::var trackToJson(const TrackState &track, const juce::File &projectDir);
    static juce::var clipToJson(const Clip &clip, const juce::File &projectDir);
    static juce::var midiClipToJson(const MidiClip &clip);
    static juce::var midiNoteToJson(const MidiNote &note);
    static juce::var pluginSlotToJson(const PluginSlot &slot);
    static PluginSlot pluginSlotFromJson(const juce::var &json);

    static juce::String makeRelativePath(const juce::File &file, const juce::File &projectDir);
    static juce::File resolveRelativePath(const juce::String &relativePath,
                                          const juce::File &projectDir);

    static constexpr int kFormatVersion = 4;
};

} // namespace ampl
