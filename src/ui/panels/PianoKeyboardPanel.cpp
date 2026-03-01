#include "ui/panels/PianoKeyboardPanel.hpp"
#include "ui/Theme.hpp"
#include <cmath>

namespace ampl
{

// ─── Musical Typing Key Map ─────────────────────────────────────
// Matches Logic Pro X / GarageBand layout:
//
//  Black keys:  W  E     T  Y  U     O  P
//  White keys: A  S  D  F  G  H  J  K  L  ;  '
//
//  Semitone offsets from C of the base octave:
//    A=0(C) W=1(C#) S=2(D) E=3(D#) D=4(E) F=5(F)
//    T=6(F#) G=7(G) Y=8(G#) H=9(A) U=10(A#) J=11(B)
//    K=12(C+1) O=13(C#+1) L=14(D+1) P=15(D#+1)
//    ;=16(E+1) '=17(F+1)

PianoKeyboardPanel::PianoKeyboardPanel()
{
    setWantsKeyboardFocus(true);

    keyboard_ = std::make_unique<juce::MidiKeyboardComponent>(
        keyboardState_, juce::MidiKeyboardComponent::horizontalKeyboard);

    keyboard_->setAvailableRange(21, 108); // Full piano range A0–C8
    keyboard_->setOctaveForMiddleC(4);
    keyboard_->setKeyWidth(22.0f);

    // Style the keyboard to match the dark theme
    keyboard_->setColour(juce::MidiKeyboardComponent::whiteNoteColourId,
                         juce::Colour(0xffe8ecf2));
    keyboard_->setColour(juce::MidiKeyboardComponent::blackNoteColourId,
                         juce::Colour(0xff1a1e28));
    keyboard_->setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId,
                         juce::Colour(ampl::Theme::border));
    keyboard_->setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId,
                         juce::Colour(ampl::Theme::accentBlue).withAlpha(0.3f));
    keyboard_->setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,
                         juce::Colour(ampl::Theme::accentBlue).withAlpha(0.6f));
    keyboard_->setColour(juce::MidiKeyboardComponent::shadowColourId,
                         juce::Colour(0x66000000));
    keyboard_->setColour(juce::MidiKeyboardComponent::upDownButtonBackgroundColourId,
                         juce::Colour(ampl::Theme::bgSurface));
    keyboard_->setColour(juce::MidiKeyboardComponent::upDownButtonArrowColourId,
                         juce::Colour(ampl::Theme::textSecondary));

    addAndMakeVisible(keyboard_.get());
    keyboard_->setWantsKeyboardFocus(false); // We handle keyboard ourselves

    // Listen for note events (for visual feedback / status)
    keyboardState_.addListener(this);

    // Status labels
    auto setupLabel = [this](juce::Label &label, const juce::String &text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setFont(juce::Font(12.0f));
        label.setColour(juce::Label::textColourId,
                        juce::Colour(ampl::Theme::textSecondary));
        label.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(label);
    };

    setupLabel(octaveLabel_, "Octave: C3");
    setupLabel(velocityLabel_, "Vel: 100");
    setupLabel(typingLabel_, "Musical Typing: ON");

    buildKeyMapping();
    updateLabels();
}

PianoKeyboardPanel::~PianoKeyboardPanel()
{
    keyboardState_.removeListener(this);
}

void PianoKeyboardPanel::buildKeyMapping()
{
    keyToNoteOffset_.clear();

    // White keys: A S D F G H J K L ; '
    keyToNoteOffset_['A'] = 0;   // C
    keyToNoteOffset_['S'] = 2;   // D
    keyToNoteOffset_['D'] = 4;   // E
    keyToNoteOffset_['F'] = 5;   // F
    keyToNoteOffset_['G'] = 7;   // G
    keyToNoteOffset_['H'] = 9;   // A
    keyToNoteOffset_['J'] = 11;  // B
    keyToNoteOffset_['K'] = 12;  // C+1
    keyToNoteOffset_['L'] = 14;  // D+1
    keyToNoteOffset_[';'] = 16;  // E+1
    keyToNoteOffset_['\''] = 17; // F+1

    // Black keys: W E T Y U O P
    keyToNoteOffset_['W'] = 1;   // C#
    keyToNoteOffset_['E'] = 3;   // D#
    keyToNoteOffset_['T'] = 6;   // F#
    keyToNoteOffset_['Y'] = 8;   // G#
    keyToNoteOffset_['U'] = 10;  // A#
    keyToNoteOffset_['O'] = 13;  // C#+1
    keyToNoteOffset_['P'] = 15;  // D#+1
}

int PianoKeyboardPanel::getMidiNoteForKey(int keyCode) const
{
    auto it = keyToNoteOffset_.find(keyCode);
    if (it == keyToNoteOffset_.end())
        return -1;

    int midiNote = (baseOctave_ * 12) + it->second;
    if (midiNote < 0 || midiNote > 127)
        return -1;

    return midiNote;
}

void PianoKeyboardPanel::setMusicalTypingEnabled(bool enabled)
{
    if (musicalTypingEnabled_ == enabled)
        return;

    if (!enabled)
        allMusicalTypingNotesOff();

    musicalTypingEnabled_ = enabled;
    updateLabels();
    repaint();
}

void PianoKeyboardPanel::setBaseOctave(int octave)
{
    octave = juce::jlimit(0, 8, octave);
    if (baseOctave_ == octave)
        return;

    allMusicalTypingNotesOff();
    baseOctave_ = octave;
    updateLabels();
    repaint();
}

void PianoKeyboardPanel::setTypingVelocity(float velocity)
{
    typingVelocity_ = juce::jlimit(0.0f, 1.0f, velocity);
    updateLabels();
    repaint();
}

void PianoKeyboardPanel::musicalTypingNoteOn(int noteNumber)
{
    if (noteNumber < 0 || noteNumber > 127)
        return;

    if (heldNotes_.count(noteNumber) > 0)
        return; // Already playing

    heldNotes_.insert(noteNumber);
    keyboardState_.noteOn(kMidiChannel, noteNumber, typingVelocity_);
}

void PianoKeyboardPanel::musicalTypingNoteOff(int noteNumber)
{
    if (noteNumber < 0 || noteNumber > 127)
        return;

    if (heldNotes_.count(noteNumber) == 0)
        return; // Not playing

    heldNotes_.erase(noteNumber);
    keyboardState_.noteOff(kMidiChannel, noteNumber, 0.0f);
}

void PianoKeyboardPanel::allMusicalTypingNotesOff()
{
    for (int note : heldNotes_)
        keyboardState_.noteOff(kMidiChannel, note, 0.0f);

    heldNotes_.clear();
    heldKeyCodes_.clear();
}

// ─── Component overrides ────────────────────────────────────────

void PianoKeyboardPanel::resized()
{
    auto area = getLocalBounds();

    // Status bar at the top (24px)
    auto statusBar = area.removeFromTop(24);
    int labelWidth = 140;

    typingLabel_.setBounds(statusBar.removeFromLeft(labelWidth + 20));
    statusBar.removeFromLeft(8);
    octaveLabel_.setBounds(statusBar.removeFromLeft(labelWidth));
    statusBar.removeFromLeft(8);
    velocityLabel_.setBounds(statusBar.removeFromLeft(labelWidth));

    // Piano keyboard fills the rest
    keyboard_->setBounds(area);
}

void PianoKeyboardPanel::paint(juce::Graphics &g)
{
    // Panel background
    g.setColour(juce::Colour(ampl::Theme::bgSurface));
    g.fillRect(getLocalBounds());

    // Top border
    g.setColour(juce::Colour(ampl::Theme::border));
    g.drawHorizontalLine(0, 0.0f, static_cast<float>(getWidth()));

    // Status bar background
    auto statusBar = getLocalBounds().removeFromTop(24);
    g.setColour(juce::Colour(ampl::Theme::bgElevated));
    g.fillRect(statusBar);

    // Status bar bottom divider
    g.setColour(juce::Colour(ampl::Theme::border));
    g.drawHorizontalLine(24, 0.0f, static_cast<float>(getWidth()));
}

bool PianoKeyboardPanel::keyPressed(const juce::KeyPress &key)
{
    if (!musicalTypingEnabled_)
        return false;

    auto keyChar = static_cast<int>(juce::CharacterFunctions::toUpperCase(
        static_cast<juce::juce_wchar>(key.getTextCharacter())));
    auto keyCode = key.getKeyCode();

    // Z = octave down
    if (keyChar == 'Z')
    {
        setBaseOctave(baseOctave_ - 1);
        return true;
    }
    // X = octave up
    if (keyChar == 'X')
    {
        setBaseOctave(baseOctave_ + 1);
        return true;
    }
    // C = velocity down
    if (keyChar == 'C')
    {
        setTypingVelocity(typingVelocity_ - 0.1f);
        return true;
    }
    // V = velocity up
    if (keyChar == 'V')
    {
        setTypingVelocity(typingVelocity_ + 0.1f);
        return true;
    }

    // Musical typing note keys
    int midiNote = getMidiNoteForKey(keyChar);
    if (midiNote >= 0)
    {
        if (heldKeyCodes_.count(keyCode) == 0)
        {
            heldKeyCodes_.insert(keyCode);
            musicalTypingNoteOn(midiNote);
        }
        return true;
    }

    return false;
}

bool PianoKeyboardPanel::keyStateChanged(bool /*isKeyDown*/)
{
    if (!musicalTypingEnabled_)
        return false;

    // Check for released keys
    bool anyReleased = false;
    std::vector<int> releasedCodes;

    for (int code : heldKeyCodes_)
    {
        if (!juce::KeyPress::isKeyCurrentlyDown(code))
            releasedCodes.push_back(code);
    }

    for (int code : releasedCodes)
    {
        heldKeyCodes_.erase(code);

        // Convert key code back to character
        auto keyChar = static_cast<int>(juce::CharacterFunctions::toUpperCase(
            static_cast<juce::juce_wchar>(code)));
        int midiNote = getMidiNoteForKey(keyChar);
        if (midiNote >= 0)
        {
            musicalTypingNoteOff(midiNote);
            anyReleased = true;
        }
    }

    return anyReleased;
}

void PianoKeyboardPanel::focusLost(FocusChangeType /*cause*/)
{
    // Release all notes when focus is lost
    allMusicalTypingNotesOff();
}

// ─── MidiKeyboardState::Listener ────────────────────────────────

void PianoKeyboardPanel::handleNoteOn(juce::MidiKeyboardState *, int /*midiChannel*/,
                                       int /*midiNoteNumber*/, float /*velocity*/)
{
    // Visual feedback is handled by MidiKeyboardComponent automatically
}

void PianoKeyboardPanel::handleNoteOff(juce::MidiKeyboardState *, int /*midiChannel*/,
                                        int /*midiNoteNumber*/, float /*velocity*/)
{
    // Visual feedback is handled by MidiKeyboardComponent automatically
}

// ─── Internal helpers ───────────────────────────────────────────

void PianoKeyboardPanel::updateLabels()
{
    static const char *noteNames[] = {"C", "C#", "D", "D#", "E", "F",
                                       "F#", "G", "G#", "A", "A#", "B"};
    (void)noteNames;

    octaveLabel_.setText("Octave: C" + juce::String(baseOctave_) + " - C" +
                             juce::String(baseOctave_ + 2),
                         juce::dontSendNotification);

    int velPercent = static_cast<int>(std::round(typingVelocity_ * 127.0f));
    velocityLabel_.setText("Vel: " + juce::String(velPercent), juce::dontSendNotification);

    typingLabel_.setText(juce::String("Musical Typing: ") +
                             (musicalTypingEnabled_ ? "ON" : "OFF"),
                         juce::dontSendNotification);

    typingLabel_.setColour(juce::Label::textColourId,
                           musicalTypingEnabled_
                               ? juce::Colour(ampl::Theme::accentGreen)
                               : juce::Colour(ampl::Theme::textDim));
}

} // namespace ampl
