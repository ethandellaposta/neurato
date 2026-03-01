#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <unordered_map>
#include <unordered_set>

namespace ampl
{

/**
 * A playable on-screen piano keyboard with musical typing support.
 *
 * Features:
 *   - Visual piano keyboard (2-octave range, resizable)
 *   - Click/drag to play notes with the mouse
 *   - Musical Typing: map QWERTY keys to piano notes (like Logic Pro / GarageBand)
 *   - Octave shift with Z/X keys
 *   - Velocity control with C/V keys
 *   - Note-on/off events forwarded through a juce::MidiKeyboardState
 *     so the audio engine can pick them up via MidiMessageCollector.
 *
 * Musical Typing layout (matches Logic Pro):
 *   White keys: A S D F G H J K L ; '
 *   Black keys: W E   T Y U   O P
 *   Octave -/+: Z / X
 *   Velocity -/+: C / V
 */
class PianoKeyboardPanel : public juce::Component, public juce::MidiKeyboardState::Listener
{
  public:
    PianoKeyboardPanel();
    ~PianoKeyboardPanel() override;

    // Access the keyboard state (connect to MidiMessageCollector for audio routing)
    juce::MidiKeyboardState &getKeyboardState()
    {
        return keyboardState_;
    }

    // Enable or disable musical typing (computer keyboard → MIDI)
    void setMusicalTypingEnabled(bool enabled);
    bool isMusicalTypingEnabled() const
    {
        return musicalTypingEnabled_;
    }

    // Set the base octave for musical typing (default: 3, giving C3 as root)
    void setBaseOctave(int octave);
    int getBaseOctave() const
    {
        return baseOctave_;
    }

    // Set velocity for musical typing (0.0–1.0)
    void setTypingVelocity(float velocity);
    float getTypingVelocity() const
    {
        return typingVelocity_;
    }

    // juce::Component
    void resized() override;
    void paint(juce::Graphics &g) override;
    bool keyPressed(const juce::KeyPress &key) override;
    bool keyStateChanged(bool isKeyDown) override;
    void focusLost(FocusChangeType cause) override;

    // juce::MidiKeyboardState::Listener — for visual feedback
    void handleNoteOn(juce::MidiKeyboardState *, int midiChannel, int midiNoteNumber,
                      float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState *, int midiChannel, int midiNoteNumber,
                       float velocity) override;

  private:
    // Build the QWERTY → MIDI note offset mapping
    void buildKeyMapping();

    // Send note on/off from musical typing
    void musicalTypingNoteOn(int noteNumber);
    void musicalTypingNoteOff(int noteNumber);

    // Release all currently held musical-typing notes
    void allMusicalTypingNotesOff();

    // Get the MIDI note number for a given key code, or -1 if unmapped
    int getMidiNoteForKey(int keyCode) const;

    // Update status bar labels
    void updateLabels();

    juce::MidiKeyboardState keyboardState_;
    std::unique_ptr<juce::MidiKeyboardComponent> keyboard_;

    // Musical typing state
    bool musicalTypingEnabled_{true};
    int baseOctave_{3}; // C3 = MIDI note 48
    float typingVelocity_{0.8f};

    // Map from juce key code → semitone offset from (baseOctave_ * 12)
    std::unordered_map<int, int> keyToNoteOffset_;

    // Currently pressed keys (to detect key-up via keyStateChanged)
    std::unordered_set<int> heldKeyCodes_;
    std::unordered_set<int> heldNotes_;

    // Status bar labels
    juce::Label octaveLabel_;
    juce::Label velocityLabel_;
    juce::Label typingLabel_;

    static constexpr int kMidiChannel = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoKeyboardPanel)
};

} // namespace ampl
