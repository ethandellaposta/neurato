#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace neurato {

// Centralized modern dark theme for all Neurato UI components.
// Inspired by Logic Pro / Ableton with a refined, minimal aesthetic.
struct Theme
{
    // --- Background layers (darkest to lightest) ---
    static constexpr uint32_t bgDeepest     = 0xff0c0c14;  // Main window bg
    static constexpr uint32_t bgBase        = 0xff12121e;  // Panel backgrounds
    static constexpr uint32_t bgSurface     = 0xff1a1a2a;  // Cards, strips, headers
    static constexpr uint32_t bgElevated    = 0xff222236;  // Hover states, raised elements
    static constexpr uint32_t bgHighlight   = 0xff2a2a44;  // Selected/active bg

    // --- Borders & dividers ---
    static constexpr uint32_t border        = 0xff2a2a3e;
    static constexpr uint32_t borderLight   = 0xff363650;
    static constexpr uint32_t borderAccent  = 0xff4a4a6a;

    // --- Text ---
    static constexpr uint32_t textPrimary   = 0xffe8e8f0;
    static constexpr uint32_t textSecondary = 0xff9898b0;
    static constexpr uint32_t textDim       = 0xff686880;
    static constexpr uint32_t textDisabled  = 0xff484860;

    // --- Accent colors ---
    static constexpr uint32_t accentBlue    = 0xff4c8dff;  // Primary accent
    static constexpr uint32_t accentCyan    = 0xff3ecfcf;  // MIDI clips
    static constexpr uint32_t accentGreen   = 0xff3ddc84;  // Play, meters OK
    static constexpr uint32_t accentOrange  = 0xffff9f43;  // Master bus, warnings
    static constexpr uint32_t accentRed     = 0xffef5350;  // Stop, mute, clip
    static constexpr uint32_t accentYellow  = 0xffffd740;  // Solo
    static constexpr uint32_t accentPurple  = 0xffb388ff;  // Automation, FX

    // --- Audio clip colors ---
    static constexpr uint32_t clipAudioBg   = 0xff1a3a5c;
    static constexpr uint32_t clipAudioWave = 0xff5dade2;
    static constexpr uint32_t clipAudioBorder= 0xff4c8dff;

    // --- MIDI clip colors ---
    static constexpr uint32_t clipMidiBg    = 0xff1a4a4a;
    static constexpr uint32_t clipMidiNote  = 0xff3ecfcf;
    static constexpr uint32_t clipMidiBorder= 0xff3ecfcf;

    // --- Piano roll ---
    static constexpr uint32_t pianoWhiteKey = 0xff303044;  // Keyboard sidebar white
    static constexpr uint32_t pianoBlackKey = 0xff1c1c2a;  // Keyboard sidebar black
    static constexpr uint32_t pianoKeyBorder= 0xff3a3a50;
    static constexpr uint32_t pianoNoteColor= 0xff4c8dff;
    static constexpr uint32_t pianoNoteSelected = 0xff7fb3ff;
    static constexpr uint32_t pianoGrid     = 0xff1e1e30;  // Fine subdivision lines
    static constexpr uint32_t pianoGridBeat = 0xff2c2c44;  // Beat lines
    static constexpr uint32_t pianoGridBar  = 0xff444466;  // Bar lines (brightest)
    static constexpr uint32_t pianoVelocity = 0xff4c8dff;
    static constexpr uint32_t pianoRowWhite = 0xff181828;  // Grid row for white keys
    static constexpr uint32_t pianoRowBlack = 0xff121220;  // Grid row for black keys (darker)
    static constexpr uint32_t pianoRulerBg  = 0xff14141f;  // Beat ruler background
    static constexpr uint32_t pianoRulerText= 0xff7878a0;  // Beat ruler labels

    // --- Transport ---
    static constexpr uint32_t transportBg   = 0xff161624;
    static constexpr uint32_t playButton    = 0xff3ddc84;
    static constexpr uint32_t stopButton    = 0xffef5350;

    // --- Playhead ---
    static constexpr uint32_t playhead      = 0xffef5350;

    // --- Meter colors ---
    static constexpr uint32_t meterGreen    = 0xff3ddc84;
    static constexpr uint32_t meterYellow   = 0xffffd740;
    static constexpr uint32_t meterRed      = 0xffef5350;
    static constexpr uint32_t meterBg       = 0xff1a1a28;

    // --- Track lane alternating ---
    static constexpr uint32_t trackLaneEven = 0xff101020;
    static constexpr uint32_t trackLaneOdd  = 0xff131326;

    // --- Toolbar ---
    static constexpr uint32_t toolbarBg     = 0xff14141f;

    // --- Font helpers ---
    static juce::Font labelFont()    { return juce::Font(juce::FontOptions(11.0f)); }
    static juce::Font bodyFont()     { return juce::Font(juce::FontOptions(12.0f)); }
    static juce::Font headingFont()  { return juce::Font(juce::FontOptions(13.0f).withStyle("Bold")); }
    static juce::Font monoFont()     { return juce::Font(juce::FontOptions(13.0f).withStyle("Bold")); }
    static juce::Font smallFont()    { return juce::Font(juce::FontOptions(10.0f)); }
    static juce::Font timeFont()     { return juce::Font(juce::FontOptions(18.0f).withStyle("Bold")); }

    // --- Rounded rect radius ---
    static constexpr float cornerRadius     = 4.0f;
    static constexpr float clipRadius       = 3.0f;

    // --- Layout constants ---
    static constexpr int transportHeight    = 44;
    static constexpr int toolbarHeight      = 30;
    static constexpr int trackHeight        = 80;
    static constexpr int trackHeaderWidth   = 140;
    static constexpr int rulerHeight        = 28;
    static constexpr int mixerHeight        = 200;
    static constexpr int editorHeight       = 280;
    static constexpr int pianoKeyWidth      = 56;
    static constexpr int channelStripWidth  = 72;
};

} // namespace neurato
