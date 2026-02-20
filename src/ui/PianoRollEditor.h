#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "model/Session.h"
#include "model/MidiClip.h"
#include "commands/CommandManager.h"
#include "ui/Theme.h"
#include <functional>
#include <set>

namespace neurato {

// Full-featured piano roll MIDI editor.
// Displays a piano keyboard on the left, a note grid in the center,
// and a velocity lane at the bottom.
class PianoRollEditor : public juce::Component
{
public:
    PianoRollEditor(Session& session, CommandManager& commandManager);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    // Set which clip to edit
    void setClip(int trackIndex, const juce::String& clipId);
    void clearClip();
    bool hasClip() const { return !clipId_.isEmpty(); }

    // Snap/grid division — these are musical note values
    enum class GridDiv { Off, Bar, Half, Quarter, Eighth, Sixteenth };
    void setGridDiv(GridDiv div) { gridDiv_ = div; repaint(); }
    GridDiv getGridDiv() const { return gridDiv_; }

    // Two-tool system like Logic: Pointer (select/move/resize) and Pencil (draw/erase)
    enum class Tool { Pointer, Pencil };
    void setTool(Tool tool);
    Tool getTool() const { return tool_; }

    std::function<void()> onSessionChanged;
    std::function<void()> onCloseRequested;

private:
    // --- Paint helpers ---
    void paintKeyboard(juce::Graphics& g, juce::Rectangle<int> area);
    void paintNoteGrid(juce::Graphics& g, juce::Rectangle<int> area);
    void paintBeatRuler(juce::Graphics& g, juce::Rectangle<int> area);
    void paintNotes(juce::Graphics& g, juce::Rectangle<int> area);
    void paintVelocityLane(juce::Graphics& g, juce::Rectangle<int> area);
    void paintToolbar(juce::Graphics& g, juce::Rectangle<int> area);
    void updateCursor();

    // --- Coordinate conversion ---
    int noteToY(int noteNumber, int gridTop, int gridHeight) const;
    int yToNote(int y, int gridTop, int gridHeight) const;
    int sampleToX(SampleCount sample, int gridLeft, int gridWidth) const;
    SampleCount xToSample(int x, int gridLeft, int gridWidth) const;
    SampleCount snapSample(SampleCount sample) const;

    // --- Grid areas ---
    juce::Rectangle<int> getKeyboardArea() const;
    juce::Rectangle<int> getGridArea() const;
    juce::Rectangle<int> getBeatRulerArea() const;
    juce::Rectangle<int> getVelocityArea() const;
    juce::Rectangle<int> getToolbarArea() const;

    // Grid division helpers
    double getGridSizeSamples() const;
    juce::String gridDivLabel(GridDiv div) const;

    static bool isBlackKey(int noteNumber);
    static juce::String noteName(int noteNumber);

    Session& session_;
    CommandManager& commandManager_;

    int trackIndex_{-1};
    juce::String clipId_;

    // View state
    int lowestVisibleNote_{36};   // C2
    int highestVisibleNote_{96};  // C7
    SampleCount scrollSamples_{0};
    double pixelsPerSample_{0.005};

    // Interaction
    Tool tool_{Tool::Pencil};
    GridDiv gridDiv_{GridDiv::Eighth};

    enum class DragMode { None, MovingNote, ResizingNote, DrawingNote, SelectBox, VelocityDrag };
    DragMode dragMode_{DragMode::None};
    juce::String dragNoteId_;
    SampleCount dragStartSample_{0};
    int dragStartNote_{0};
    SampleCount dragNoteOrigStart_{0};
    int dragNoteOrigNote_{0};
    juce::Point<int> dragStartPoint_;

    std::set<juce::String> selectedNoteIds_;

    // Toolbar buttons — two-tool system like Logic
    juce::TextButton pointerToolBtn_;
    juce::TextButton pencilToolBtn_;
    juce::ComboBox gridCombo_;
    juce::TextButton closeBtn_{"X"};

    static constexpr int kToolbarHeight = 28;
    static constexpr int kBeatRulerHeight = 20;
    static constexpr int kVelocityHeight = 50;
    static constexpr int kNoteHeight = 10;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRollEditor)
};

} // namespace neurato
