#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace ampl
{

// Centralized modern dark theme for all Ampl UI components.
// Inspired by modern DAWs like Ableton, Bitwig, and Logic Pro with refined aesthetics.
struct Theme
{
    // ── Palette struct for multi-theme support ──────────────────────────
    struct Palette
    {
        uint32_t bgDeepest, bgBase, bgSurface, bgElevated, bgHighlight;
        uint32_t border, borderLight, borderAccent;
        uint32_t textPrimary, textSecondary, textDim, textDisabled;
        uint32_t accentBlue, accentGreen, accentOrange, accentRed, accentPurple, accentCyan,
            accentYellow;
        uint32_t clipAudioBg, clipAudioWave, clipAudioBorder;
        uint32_t clipMidiBg, clipMidiNote, clipMidiBorder;
        uint32_t pianoWhiteKey, pianoBlackKey, pianoKeyBorder, pianoNoteColor, pianoNoteSelected;
        uint32_t pianoGrid, pianoGridBeat, pianoGridBar, pianoVelocity, pianoRowWhite,
            pianoRowBlack, pianoRulerBg, pianoRulerText;
        uint32_t transportBg, playButton, stopButton, playhead;
        uint32_t meterGreen, meterYellow, meterRed, meterBg;
        uint32_t trackLaneEven, trackLaneOdd, toolbarBg;
    };

    // ── Default "Classic Dark" colour constants ─────────────────────────
    // Background layers (darkest to lightest)
    static constexpr uint32_t bgDeepest = 0xff060912;
    static constexpr uint32_t bgBase = 0xff0d1320;
    static constexpr uint32_t bgSurface = 0xff171f31;
    static constexpr uint32_t bgElevated = 0xff222c44;
    static constexpr uint32_t bgHighlight = 0xff2e3d5f;

    // Borders & dividers
    static constexpr uint32_t border = 0xff2a3650;
    static constexpr uint32_t borderLight = 0xff3a4a6a;
    static constexpr uint32_t borderAccent = 0xff5aa7ff;

    // Text hierarchy
    static constexpr uint32_t textPrimary = 0xffffffff;
    static constexpr uint32_t textSecondary = 0xffd0d8ec;
    static constexpr uint32_t textDim = 0xff93a2c0;
    static constexpr uint32_t textDisabled = 0xff5d6a86;

    // Accent colours
    static constexpr uint32_t accentBlue = 0xff5aa7ff;
    static constexpr uint32_t accentGreen = 0xff47df95;
    static constexpr uint32_t accentOrange = 0xffffb056;
    static constexpr uint32_t accentRed = 0xffff5e73;
    static constexpr uint32_t accentPurple = 0xffb17bff;
    static constexpr uint32_t accentCyan = 0xff61e4ff;
    static constexpr uint32_t accentYellow = 0xffffe15a;

    // Clip colours
    static constexpr uint32_t clipAudioBg = 0xff1d3f68;
    static constexpr uint32_t clipAudioWave = 0xff79c9ff;
    static constexpr uint32_t clipAudioBorder = 0xff5aa7ff;
    static constexpr uint32_t clipMidiBg = 0xff1e525a;
    static constexpr uint32_t clipMidiNote = 0xff61e4ff;
    static constexpr uint32_t clipMidiBorder = 0xff61e4ff;

    // Piano roll
    static constexpr uint32_t pianoWhiteKey = 0xff303044;
    static constexpr uint32_t pianoBlackKey = 0xff1c1c2a;
    static constexpr uint32_t pianoKeyBorder = 0xff3a3a50;
    static constexpr uint32_t pianoNoteColor = 0xff4c8dff;
    static constexpr uint32_t pianoNoteSelected = 0xff7fb3ff;
    static constexpr uint32_t pianoGrid = 0xff1e1e30;
    static constexpr uint32_t pianoGridBeat = 0xff2c2c44;
    static constexpr uint32_t pianoGridBar = 0xff444466;
    static constexpr uint32_t pianoVelocity = 0xff4c8dff;
    static constexpr uint32_t pianoRowWhite = 0xff181828;
    static constexpr uint32_t pianoRowBlack = 0xff121220;
    static constexpr uint32_t pianoRulerBg = 0xff14141f;
    static constexpr uint32_t pianoRulerText = 0xff7878a0;

    // Transport
    static constexpr uint32_t transportBg = 0xff111a2b;
    static constexpr uint32_t playButton = 0xff3ddc84;
    static constexpr uint32_t stopButton = 0xffef5350;
    static constexpr uint32_t playhead = 0xffef5350;

    // Meters
    static constexpr uint32_t meterGreen = 0xff3ddc84;
    static constexpr uint32_t meterYellow = 0xffffd740;
    static constexpr uint32_t meterRed = 0xffef5350;
    static constexpr uint32_t meterBg = 0xff131c2c;

    // Track lanes
    static constexpr uint32_t trackLaneEven = 0xff0f1a2c;
    static constexpr uint32_t trackLaneOdd = 0xff122038;
    static constexpr uint32_t toolbarBg = 0xff121a2a;

    // ── Rounded rect radii ──────────────────────────────────────────────
    static constexpr float cornerRadius = 4.0f;
    static constexpr float clipRadius = 3.0f;

    // ── Layout constants ────────────────────────────────────────────────
    static constexpr int transportHeight = 44;
    static constexpr int toolbarHeight = 30;
    static constexpr int trackHeight = 80;
    static constexpr int trackHeaderWidth = 140;
    static constexpr int rulerHeight = 28;
    static constexpr int mixerHeight = 200;
    static constexpr int editorHeight = 280;
    static constexpr int pianoKeyWidth = 56;
    static constexpr int channelStripWidth = 72;

    // ── Font helpers ────────────────────────────────────────────────────
    static juce::Font labelFont()
    {
        return juce::Font(juce::FontOptions(11.0f));
    }
    static juce::Font bodyFont()
    {
        return juce::Font(juce::FontOptions(12.0f));
    }
    static juce::Font headingFont()
    {
        return juce::Font(juce::FontOptions(13.0f).withStyle("Bold"));
    }
    static juce::Font monoFont()
    {
        return juce::Font(juce::FontOptions(13.0f).withStyle("Bold"));
    }
    static juce::Font smallFont()
    {
        return juce::Font(juce::FontOptions(10.0f));
    }
    static juce::Font timeFont()
    {
        return juce::Font(juce::FontOptions(18.0f).withStyle("Bold"));
    }

    // ── Styling helpers (declarations) ──────────────────────────────────
    static void styleButton(juce::Button &button, bool isPrimary = false, bool isAccent = false);
    static void styleSlider(juce::Slider &slider);
    static void styleLabel(juce::Label &label, bool isHeading = false);
    static void styleTextButton(juce::TextButton &button, bool isPrimary = false);
    static void styleComboBox(juce::ComboBox &comboBox);
    static void styleProgressBar(juce::ProgressBar &progressBar);

    // ── Colour accessors (use current palette) ─────────────────────────
    static juce::Colour getBackground()
    {
        return juce::Colour(getCurrentPalette().bgBase);
    }
    static juce::Colour getSurface()
    {
        return juce::Colour(getCurrentPalette().bgSurface);
    }
    static juce::Colour getElevated()
    {
        return juce::Colour(getCurrentPalette().bgElevated);
    }
    static juce::Colour getHighlight()
    {
        return juce::Colour(getCurrentPalette().bgHighlight);
    }
    static juce::Colour getBorder()
    {
        return juce::Colour(getCurrentPalette().border);
    }
    static juce::Colour getBorderLight()
    {
        return juce::Colour(getCurrentPalette().borderLight);
    }
    static juce::Colour getBorderAccent()
    {
        return juce::Colour(getCurrentPalette().borderAccent);
    }
    static juce::Colour getTextPrimary()
    {
        return juce::Colour(getCurrentPalette().textPrimary);
    }
    static juce::Colour getTextSecondary()
    {
        return juce::Colour(getCurrentPalette().textSecondary);
    }
    static juce::Colour getTextDim()
    {
        return juce::Colour(getCurrentPalette().textDim);
    }
    static juce::Colour getAccent()
    {
        return juce::Colour(getCurrentPalette().accentBlue);
    }
    static juce::Colour getSuccess()
    {
        return juce::Colour(getCurrentPalette().accentGreen);
    }
    static juce::Colour getWarning()
    {
        return juce::Colour(getCurrentPalette().accentOrange);
    }
    static juce::Colour getError()
    {
        return juce::Colour(getCurrentPalette().accentRed);
    }

    // ── Theme management ────────────────────────────────────────────────
    static const std::vector<std::pair<const char *, Palette>> &getThemes();
    static int getThemeCount();
    static const Palette &getCurrentPalette();
    static void setCurrentTheme(int idx);
    static int getCurrentThemeIndex();
    static const char *getCurrentThemeName();

    // ── LookAndFeel ─────────────────────────────────────────────────────
    static juce::LookAndFeel &getLookAndFeel();

    // ── Convenience structs for mixer/panel code ────────────────────────
    struct Colors
    {
        juce::Colour backgroundDark;
        juce::Colour backgroundDarker;
        juce::Colour surface;
        juce::Colour accent;
        juce::Colour warning;
        juce::Colour textPrimary;
    };

    struct Fonts
    {
        juce::Font label;
    };

    static inline Colors colors{juce::Colour(0xff060912u), juce::Colour(0xff0d1320u),
                                juce::Colour(0xff171f31u), juce::Colour(0xff5aa7ffu),
                                juce::Colour(0xffffb056u), juce::Colour(0xffffffffu)};
    static inline Fonts fonts{juce::Font(juce::FontOptions(11.0f))};
};

// ═══════════════════════════════════════════════════════════════════════════
// Inline definitions (outside struct body so Theme:: qualification is valid)
// ═══════════════════════════════════════════════════════════════════════════

inline const std::vector<std::pair<const char *, Theme::Palette>> &Theme::getThemes()
{
    // clang-format off
    static const std::vector<std::pair<const char *, Palette>> themes = {
        {"Classic Dark",
         {0xff060912, 0xff0d1320, 0xff171f31, 0xff222c44, 0xff2e3d5f,
          0xff2a3650, 0xff3a4a6a, 0xff5aa7ff,
          0xffffffff, 0xffd0d8ec, 0xff93a2c0, 0xff5d6a86,
          0xff5aa7ff, 0xff47df95, 0xffffb056, 0xffff5e73, 0xffb17bff, 0xff61e4ff, 0xffffe15a,
          0xff1d3f68, 0xff79c9ff, 0xff5aa7ff,
          0xff1e525a, 0xff61e4ff, 0xff61e4ff,
          0xff303044, 0xff1c1c2a, 0xff3a3a50, 0xff4c8dff, 0xff7fb3ff,
          0xff1e1e30, 0xff2c2c44, 0xff444466, 0xff4c8dff, 0xff181828, 0xff121220, 0xff14141f, 0xff7878a0,
          0xff111a2b, 0xff3ddc84, 0xffef5350, 0xffef5350,
          0xff3ddc84, 0xffffd740, 0xffef5350, 0xff131c2c,
          0xff0f1a2c, 0xff122038, 0xff121a2a}},
        {"Neon Tokyo",
         {0xff0a0f1c, 0xff1a1f3c, 0xff2a2f5c, 0xff3a3f7c, 0xff4a4ffc,
          0xffff00cc, 0xff00fff7, 0xff00ffea,
          0xffffffff, 0xffe0e0ff, 0xffa0a0ff, 0xff5050a0,
          0xff00fff7, 0xff00ffea, 0xffff00cc, 0xffff0055, 0xffa000ff, 0xff00aaff, 0xffffff00,
          0xff1c1c3f, 0xff00aaff, 0xff00fff7,
          0xff1c3f3f, 0xff00aaff, 0xff00fff7,
          0xff303060, 0xff1c1c3f, 0xff3a3a60, 0xff00aaff, 0xff00fff7,
          0xff1e1e3f, 0xff2c2c5f, 0xff4444ff, 0xff00aaff, 0xff18183f, 0xff12123f, 0xff14143f, 0xff7878ff,
          0xff111a3b, 0xff00ffea, 0xffff0055, 0xffff0055,
          0xff00ffea, 0xffffff00, 0xffff0055, 0xff131c3c,
          0xff0f1a3c, 0xff12203c, 0xff121a3a}},
        {"Midnight Groove",
         {0xff181828, 0xff23233a, 0xff2e2e4a, 0xff39395a, 0xff44446a,
          0xff4f4f7a, 0xff5a5a8a, 0xff65659a,
          0xff7070aa, 0xff7b7bba, 0xff8686ca, 0xff9191da,
          0xff9c9cea, 0xffa7a7fa, 0xffb2b2ff, 0xffbdbdff, 0xffc8c8ff, 0xffd3d3ff, 0xffdedede,
          0xffe9e9e9, 0xfff4f4f4, 0xffffffff,
          0xffe9e9e9, 0xffdedede, 0xffd3d3ff,
          0xffc8c8ff, 0xffbdbdff, 0xffb2b2ff, 0xffa7a7fa, 0xff9c9cea,
          0xff9191da, 0xff8686ca, 0xff7b7bba, 0xff7070aa, 0xff65659a, 0xff5a5a8a, 0xff4f4f7a, 0xff44446a,
          0xff39395a, 0xff2e2e4a, 0xff23233a, 0xff181828,
          0xff23233a, 0xff2e2e4a, 0xff39395a, 0xff2e2e4a,
          0xff23233a, 0xff181828, 0xff23233a}},
    };
    // clang-format on
    return themes;
}

inline int Theme::getThemeCount()
{
    return static_cast<int>(getThemes().size());
}

inline int Theme::getCurrentThemeIndex()
{
    // Shared mutable state for theme index
    static int idx = 0;
    return idx;
}

inline void Theme::setCurrentTheme(int idx)
{
    int count = getThemeCount();
    if (idx < 0)
        idx = 0;
    if (idx >= count)
        idx = count - 1;
    // Update the shared index via getCurrentThemeIndex's static
    // (We use a separate static here for write access)
    static int &themeIdx = []() -> int &
    {
        static int idx = 0;
        return idx;
    }();
    themeIdx = idx;
}

inline const Theme::Palette &Theme::getCurrentPalette()
{
    return getThemes()[static_cast<size_t>(getCurrentThemeIndex())].second;
}

inline const char *Theme::getCurrentThemeName()
{
    return getThemes()[static_cast<size_t>(getCurrentThemeIndex())].first;
}

} // namespace ampl
