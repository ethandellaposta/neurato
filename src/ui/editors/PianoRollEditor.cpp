// PianoRollEditor stub — the original implementation was damaged by a formatter.
// The .bak file (PianoRollEditor.cpp.impl.inc.bak) contains the original mangled content.
// TODO: Rewrite full PianoRollEditor implementation.

#include "ui/editors/PianoRollEditor.hpp"
#include "commands/MidiCommands.hpp"
#include <cmath>

namespace ampl
{

PianoRollEditor::PianoRollEditor(Session &session, CommandManager &commandManager)
    : session_(session), commandManager_(commandManager)
{
    pointerToolBtn_.setButtonText(juce::String::fromUTF8("\xe2\x86\x96"));
    pointerToolBtn_.setClickingTogglesState(true);
    pointerToolBtn_.setRadioGroupId(1001);
    pointerToolBtn_.setTooltip("Pointer Tool");
    pointerToolBtn_.onClick = [this] { setTool(Tool::Pointer); };
    addAndMakeVisible(pointerToolBtn_);

    pencilToolBtn_.setButtonText(juce::String::fromUTF8("\xe2\x9c\x8f"));
    pencilToolBtn_.setClickingTogglesState(true);
    pencilToolBtn_.setRadioGroupId(1001);
    pencilToolBtn_.setToggleState(true, juce::dontSendNotification);
    pencilToolBtn_.setTooltip("Pencil Tool");
    pencilToolBtn_.onClick = [this] { setTool(Tool::Pencil); };
    addAndMakeVisible(pencilToolBtn_);

    gridCombo_.addItem("Off", 1);
    gridCombo_.addItem("1 Bar", 2);
    gridCombo_.addItem("1/2", 3);
    gridCombo_.addItem("1/4", 4);
    gridCombo_.addItem("1/8", 5);
    gridCombo_.addItem("1/16", 6);
    gridCombo_.setSelectedId(5, juce::dontSendNotification);
    gridCombo_.onChange = [this]
    {
        static const GridDiv divs[] = {GridDiv::Off, GridDiv::Bar, GridDiv::Half,
                                       GridDiv::Quarter, GridDiv::Eighth, GridDiv::Sixteenth};
        int idx = gridCombo_.getSelectedId() - 1;
        if (idx >= 0 && idx < 6)
            setGridDiv(divs[idx]);
    };
    addAndMakeVisible(gridCombo_);

    closeBtn_.setTooltip("Close Editor");
    closeBtn_.onClick = [this]
    {
        if (onCloseRequested)
            onCloseRequested();
    };
    addAndMakeVisible(closeBtn_);
}

void PianoRollEditor::paint(juce::Graphics &g)
{
    g.fillAll(juce::Colour(Theme::bgBase));

    auto bounds = getLocalBounds();
    auto toolbar = bounds.removeFromTop(kToolbarHeight);
    paintToolbar(g, toolbar);

    if (clipId_.isEmpty())
    {
        g.setColour(juce::Colour(Theme::textDim));
        g.setFont(Theme::headingFont());
        g.drawText("Double-click a MIDI clip to edit", bounds, juce::Justification::centred);
        return;
    }

    auto velocityArea = bounds.removeFromBottom(kVelocityHeight);
    auto rulerArea = bounds.removeFromTop(kBeatRulerHeight);
    auto keyboardArea = bounds.removeFromLeft(56);
    auto gridArea = bounds;

    paintKeyboard(g, keyboardArea);
    paintBeatRuler(g, rulerArea);
    paintNoteGrid(g, gridArea);
    paintNotes(g, gridArea);
    paintVelocityLane(g, velocityArea);
}

void PianoRollEditor::resized()
{
    auto toolbar = getLocalBounds().removeFromTop(kToolbarHeight).reduced(4, 2);
    pointerToolBtn_.setBounds(toolbar.removeFromLeft(28));
    toolbar.removeFromLeft(2);
    pencilToolBtn_.setBounds(toolbar.removeFromLeft(28));
    toolbar.removeFromLeft(8);
    gridCombo_.setBounds(toolbar.removeFromLeft(80));
    closeBtn_.setBounds(toolbar.removeFromRight(28));
}

void PianoRollEditor::setClip(int trackIndex, const juce::String &clipId)
{
    trackIndex_ = trackIndex;
    clipId_ = clipId;
    selectedNoteIds_.clear();
    autoFitToClip();
    repaint();
}

void PianoRollEditor::clearClip()
{
    trackIndex_ = -1;
    clipId_.clear();
    selectedNoteIds_.clear();
    repaint();
}

void PianoRollEditor::setTool(Tool tool)
{
    tool_ = tool;
    pointerToolBtn_.setToggleState(tool == Tool::Pointer, juce::dontSendNotification);
    pencilToolBtn_.setToggleState(tool == Tool::Pencil, juce::dontSendNotification);
    updateCursor();
    repaint();
}

// ── Paint helpers (stub) ────────────────────────────────────────────────

void PianoRollEditor::paintToolbar(juce::Graphics &g, juce::Rectangle<int> area)
{
    g.setColour(juce::Colour(Theme::toolbarBg));
    g.fillRect(area);
    g.setColour(juce::Colour(Theme::border));
    g.drawHorizontalLine(area.getBottom() - 1, 0.0f, static_cast<float>(area.getRight()));
}

void PianoRollEditor::paintKeyboard(juce::Graphics &g, juce::Rectangle<int> area)
{
    int range = highestVisibleNote_ - lowestVisibleNote_;
    if (range <= 0)
        return;
    float noteH = static_cast<float>(area.getHeight()) / static_cast<float>(range);

    for (int n = lowestVisibleNote_; n < highestVisibleNote_; ++n)
    {
        float y = area.getBottom() - (n - lowestVisibleNote_ + 1) * noteH;
        bool black = isBlackKey(n);
        g.setColour(juce::Colour(black ? Theme::pianoBlackKey : Theme::pianoWhiteKey));
        g.fillRect(area.getX(), static_cast<int>(y), area.getWidth(), static_cast<int>(noteH) + 1);
        g.setColour(juce::Colour(Theme::pianoKeyBorder));
        g.drawHorizontalLine(static_cast<int>(y), static_cast<float>(area.getX()),
                             static_cast<float>(area.getRight()));

        if (n % 12 == 0)
        {
            g.setColour(juce::Colour(Theme::pianoRulerText));
            g.setFont(Theme::smallFont());
            g.drawText(noteName(n), area.getX() + 2, static_cast<int>(y), area.getWidth() - 4,
                       static_cast<int>(noteH), juce::Justification::centredLeft);
        }
    }
}

void PianoRollEditor::paintNoteGrid(juce::Graphics &g, juce::Rectangle<int> area)
{
    int range = highestVisibleNote_ - lowestVisibleNote_;
    if (range <= 0)
        return;
    float noteH = static_cast<float>(area.getHeight()) / static_cast<float>(range);

    // Row backgrounds
    for (int n = lowestVisibleNote_; n < highestVisibleNote_; ++n)
    {
        float y = area.getBottom() - (n - lowestVisibleNote_ + 1) * noteH;
        bool black = isBlackKey(n);
        g.setColour(juce::Colour(black ? Theme::pianoRowBlack : Theme::pianoRowWhite));
        g.fillRect(area.getX(), static_cast<int>(y), area.getWidth(), static_cast<int>(noteH) + 1);
    }

    // Beat grid
    double bpm = session_.getBpm();
    double sr = session_.getSampleRate();
    if (bpm > 0 && sr > 0)
    {
        double samplesPerBeat = (60.0 / bpm) * sr;
        int tsNum = session_.getTimeSigNumerator();
        double samplesPerBar = samplesPerBeat * tsNum;

        SampleCount firstBar = static_cast<SampleCount>(
            std::floor(static_cast<double>(scrollSamples_) / samplesPerBar) * samplesPerBar);

        for (SampleCount s = firstBar;; s += static_cast<SampleCount>(samplesPerBar))
        {
            int x = sampleToX(s, area.getX(), area.getWidth());
            if (x > area.getRight()) break;
            if (x >= area.getX())
            {
                g.setColour(juce::Colour(Theme::pianoGridBar));
                g.drawVerticalLine(x, static_cast<float>(area.getY()),
                                   static_cast<float>(area.getBottom()));
            }

            double pixPerBeat = samplesPerBeat * pixelsPerSample_;
            if (pixPerBeat > 20)
            {
                for (int b = 1; b < tsNum; ++b)
                {
                    int bx = sampleToX(s + static_cast<SampleCount>(samplesPerBeat * b),
                                       area.getX(), area.getWidth());
                    if (bx >= area.getX() && bx <= area.getRight())
                    {
                        g.setColour(juce::Colour(Theme::pianoGridBeat));
                        g.drawVerticalLine(bx, static_cast<float>(area.getY()),
                                           static_cast<float>(area.getBottom()));
                    }
                }
            }
        }
    }
}

void PianoRollEditor::paintBeatRuler(juce::Graphics &g, juce::Rectangle<int> area)
{
    g.setColour(juce::Colour(Theme::pianoRulerBg));
    g.fillRect(area);

    double bpm = session_.getBpm();
    double sr = session_.getSampleRate();
    if (bpm <= 0 || sr <= 0)
        return;

    double samplesPerBeat = (60.0 / bpm) * sr;
    int tsNum = session_.getTimeSigNumerator();
    double samplesPerBar = samplesPerBeat * tsNum;

    SampleCount firstBar = static_cast<SampleCount>(
        std::floor(static_cast<double>(scrollSamples_) / samplesPerBar) * samplesPerBar);

    g.setFont(Theme::smallFont());
    g.setColour(juce::Colour(Theme::pianoRulerText));

    int gridLeft = area.getX() + 56; // after keyboard
    int gridWidth = area.getWidth() - 56;

    for (SampleCount s = firstBar;; s += static_cast<SampleCount>(samplesPerBar))
    {
        int x = sampleToX(s, gridLeft, gridWidth);
        if (x > area.getRight()) break;
        if (x >= area.getX())
        {
            int bar = static_cast<int>(static_cast<double>(s) / samplesPerBar) + 1;
            g.drawText(juce::String(bar), x + 2, area.getY(), 40, area.getHeight(),
                       juce::Justification::centredLeft);
        }
    }
}

void PianoRollEditor::paintNotes(juce::Graphics &g, juce::Rectangle<int> area)
{
    if (clipId_.isEmpty() || trackIndex_ < 0)
        return;

    const auto *track = session_.getTrack(trackIndex_);
    if (!track)
        return;

    const MidiClip *clip = nullptr;
    for (const auto &mc : track->midiClips)
    {
        if (mc.id == clipId_)
        {
            clip = &mc;
            break;
        }
    }
    if (!clip)
        return;

    int range = highestVisibleNote_ - lowestVisibleNote_;
    if (range <= 0)
        return;
    float noteH = static_cast<float>(area.getHeight()) / static_cast<float>(range);

    for (const auto &note : clip->notes)
    {
        SampleCount absStart = clip->timelineStartSample + note.startSample;
        SampleCount absEnd = absStart + note.lengthSamples;

        int x1 = sampleToX(absStart, area.getX(), area.getWidth());
        int x2 = sampleToX(absEnd, area.getX(), area.getWidth());
        float y = area.getBottom() - (note.noteNumber - lowestVisibleNote_ + 1) * noteH;

        if (x2 < area.getX() || x1 > area.getRight())
            continue;
        if (y < area.getY() || y > area.getBottom())
            continue;

        bool selected = selectedNoteIds_.count(note.id) > 0;
        g.setColour(juce::Colour(selected ? Theme::pianoNoteSelected : Theme::pianoNoteColor));
        g.fillRect(x1, static_cast<int>(y), std::max(1, x2 - x1), static_cast<int>(noteH));
        g.setColour(juce::Colour(Theme::pianoKeyBorder));
        g.drawRect(x1, static_cast<int>(y), std::max(1, x2 - x1), static_cast<int>(noteH));
    }
}

void PianoRollEditor::paintVelocityLane(juce::Graphics &g, juce::Rectangle<int> area)
{
    g.setColour(juce::Colour(Theme::bgSurface));
    g.fillRect(area);
    g.setColour(juce::Colour(Theme::border));
    g.drawHorizontalLine(area.getY(), 0.0f, static_cast<float>(area.getRight()));

    if (clipId_.isEmpty() || trackIndex_ < 0)
        return;

    const auto *track = session_.getTrack(trackIndex_);
    if (!track)
        return;

    const MidiClip *clip = nullptr;
    for (const auto &mc : track->midiClips)
    {
        if (mc.id == clipId_)
        {
            clip = &mc;
            break;
        }
    }
    if (!clip)
        return;

    int gridLeft = area.getX() + 56;
    int gridWidth = area.getWidth() - 56;

    for (const auto &note : clip->notes)
    {
        SampleCount absStart = clip->timelineStartSample + note.startSample;
        int x = sampleToX(absStart, gridLeft, gridWidth);
        if (x < area.getX() || x > area.getRight())
            continue;

        float velFrac = note.velocity / 127.0f;
        int barH = static_cast<int>(velFrac * (area.getHeight() - 4));
        bool selected = selectedNoteIds_.count(note.id) > 0;
        g.setColour(juce::Colour(selected ? Theme::pianoNoteSelected : Theme::pianoVelocity));
        g.fillRect(x - 1, area.getBottom() - barH - 2, 3, barH);
    }
}

void PianoRollEditor::updateCursor()
{
    if (tool_ == Tool::Pencil)
        setMouseCursor(juce::MouseCursor::CrosshairCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

// ── Coordinate conversion ───────────────────────────────────────────────

int PianoRollEditor::noteToY(int noteNumber, int gridTop, int gridHeight) const
{
    int range = highestVisibleNote_ - lowestVisibleNote_;
    if (range <= 0)
        return gridTop;
    float noteH = static_cast<float>(gridHeight) / static_cast<float>(range);
    return gridTop + gridHeight - static_cast<int>((noteNumber - lowestVisibleNote_ + 1) * noteH);
}

int PianoRollEditor::yToNote(int y, int gridTop, int gridHeight) const
{
    int range = highestVisibleNote_ - lowestVisibleNote_;
    if (range <= 0)
        return lowestVisibleNote_;
    float noteH = static_cast<float>(gridHeight) / static_cast<float>(range);
    int note = lowestVisibleNote_ + static_cast<int>(static_cast<float>(gridTop + gridHeight - y) / noteH);
    return juce::jlimit(0, 127, note);
}

int PianoRollEditor::sampleToX(SampleCount sample, int gridLeft, int /*gridWidth*/) const
{
    double relSample = static_cast<double>(sample - scrollSamples_);
    return gridLeft + static_cast<int>(relSample * pixelsPerSample_);
}

SampleCount PianoRollEditor::xToSample(int x, int gridLeft, int /*gridWidth*/) const
{
    double relPixel = static_cast<double>(x - gridLeft);
    return scrollSamples_ + static_cast<SampleCount>(relPixel / pixelsPerSample_);
}

SampleCount PianoRollEditor::snapSample(SampleCount sample) const
{
    double gridSize = getGridSizeSamples();
    if (gridSize <= 0)
        return sample;
    return static_cast<SampleCount>(std::round(static_cast<double>(sample) / gridSize) * gridSize);
}

// ── Grid areas ──────────────────────────────────────────────────────────

juce::Rectangle<int> PianoRollEditor::getKeyboardArea() const
{
    auto b = getLocalBounds();
    b.removeFromTop(kToolbarHeight);
    b.removeFromBottom(kVelocityHeight);
    b.removeFromTop(kBeatRulerHeight);
    return b.removeFromLeft(56);
}

juce::Rectangle<int> PianoRollEditor::getGridArea() const
{
    auto b = getLocalBounds();
    b.removeFromTop(kToolbarHeight);
    b.removeFromBottom(kVelocityHeight);
    b.removeFromTop(kBeatRulerHeight);
    b.removeFromLeft(56);
    return b;
}

juce::Rectangle<int> PianoRollEditor::getBeatRulerArea() const
{
    auto b = getLocalBounds();
    b.removeFromTop(kToolbarHeight);
    return b.removeFromTop(kBeatRulerHeight);
}

juce::Rectangle<int> PianoRollEditor::getVelocityArea() const
{
    auto b = getLocalBounds();
    b.removeFromTop(kToolbarHeight);
    return b.removeFromBottom(kVelocityHeight);
}

juce::Rectangle<int> PianoRollEditor::getToolbarArea() const
{
    return getLocalBounds().removeFromTop(kToolbarHeight);
}

double PianoRollEditor::getGridSizeSamples() const
{
    double bpm = session_.getBpm();
    double sr = session_.getSampleRate();
    if (bpm <= 0 || sr <= 0)
        return 0;
    double samplesPerBeat = (60.0 / bpm) * sr;
    int tsNum = session_.getTimeSigNumerator();

    switch (gridDiv_)
    {
    case GridDiv::Off:       return 0;
    case GridDiv::Bar:       return samplesPerBeat * tsNum;
    case GridDiv::Half:      return samplesPerBeat * 2;
    case GridDiv::Quarter:   return samplesPerBeat;
    case GridDiv::Eighth:    return samplesPerBeat / 2;
    case GridDiv::Sixteenth: return samplesPerBeat / 4;
    }
    return 0;
}

juce::String PianoRollEditor::gridDivLabel(GridDiv div) const
{
    switch (div)
    {
    case GridDiv::Off:       return "Off";
    case GridDiv::Bar:       return "1 Bar";
    case GridDiv::Half:      return "1/2";
    case GridDiv::Quarter:   return "1/4";
    case GridDiv::Eighth:    return "1/8";
    case GridDiv::Sixteenth: return "1/16";
    }
    return "?";
}

bool PianoRollEditor::isBlackKey(int noteNumber)
{
    int mod = noteNumber % 12;
    return mod == 1 || mod == 3 || mod == 6 || mod == 8 || mod == 10;
}

juce::String PianoRollEditor::noteName(int noteNumber)
{
    static const char *names[] = {"C", "C#", "D", "D#", "E", "F",
                                  "F#", "G", "G#", "A", "A#", "B"};
    int oct = (noteNumber / 12) - 1;
    return juce::String(names[noteNumber % 12]) + juce::String(oct);
}

void PianoRollEditor::autoFitToClip()
{
    if (trackIndex_ < 0 || clipId_.isEmpty())
        return;

    const auto *track = session_.getTrack(trackIndex_);
    if (!track)
        return;

    for (const auto &mc : track->midiClips)
    {
        if (mc.id == clipId_ && !mc.notes.empty())
        {
            int lo = mc.getLowestNote();
            int hi = mc.getHighestNote();
            lowestVisibleNote_ = std::max(0, lo - 4);
            highestVisibleNote_ = std::min(127, hi + 4);

            scrollSamples_ = mc.timelineStartSample;
            int gridWidth = getWidth() - 56;
            if (gridWidth > 0)
            {
                auto totalSamples = mc.getTimelineEndSample() - mc.timelineStartSample;
                if (totalSamples > 0)
                    pixelsPerSample_ = static_cast<double>(gridWidth) / static_cast<double>(totalSamples);
            }
            return;
        }
    }
}

void PianoRollEditor::scrollToFirstNote()
{
    // no-op stub
}

// ── Mouse interaction ───────────────────────────────────────────────────

void PianoRollEditor::mouseDown(const juce::MouseEvent &e)
{
    auto gridArea = getGridArea();
    if (!gridArea.contains(e.getPosition()))
        return;

    SampleCount clickSample = xToSample(e.x, gridArea.getX(), gridArea.getWidth());
    int clickNote = yToNote(e.y, gridArea.getY(), gridArea.getHeight());

    if (tool_ == Tool::Pencil)
    {
        // Draw a new note
        SampleCount snapped = snapSample(clickSample);
        double gridSize = getGridSizeSamples();
        SampleCount noteLen = gridSize > 0 ? static_cast<SampleCount>(gridSize)
                                           : static_cast<SampleCount>((60.0 / session_.getBpm()) * session_.getSampleRate() / 2);

        if (trackIndex_ >= 0 && clipId_.isNotEmpty())
        {
            auto *track = session_.getTrack(trackIndex_);
            if (track)
            {
                for (auto &mc : track->midiClips)
                {
                    if (mc.id == clipId_)
                    {
                        // Check if there's already a note here — if so, erase it
                        bool erased = false;
                        for (auto it = mc.notes.begin(); it != mc.notes.end(); ++it)
                        {
                            SampleCount absStart = mc.timelineStartSample + it->startSample;
                            SampleCount absEnd = absStart + it->lengthSamples;
                            if (clickNote == it->noteNumber && clickSample >= absStart && clickSample < absEnd)
                            {
                                mc.notes.erase(it);
                                erased = true;
                                break;
                            }
                        }
                        if (!erased)
                        {
                            MidiNote newNote;
                            newNote.id = juce::Uuid().toString();
                            newNote.noteNumber = clickNote;
                            newNote.startSample = snapped - mc.timelineStartSample;
                            newNote.lengthSamples = noteLen;
                            newNote.velocity = 100;
                            mc.notes.push_back(newNote);
                        }
                        if (onSessionChanged)
                            onSessionChanged();
                        repaint();
                        return;
                    }
                }
            }
        }
    }
    else // Pointer tool
    {
        selectedNoteIds_.clear();
        if (trackIndex_ >= 0 && clipId_.isNotEmpty())
        {
            auto *track = session_.getTrack(trackIndex_);
            if (track)
            {
                for (auto &mc : track->midiClips)
                {
                    if (mc.id == clipId_)
                    {
                        for (const auto &note : mc.notes)
                        {
                            SampleCount absStart = mc.timelineStartSample + note.startSample;
                            SampleCount absEnd = absStart + note.lengthSamples;
                            if (clickNote == note.noteNumber && clickSample >= absStart && clickSample < absEnd)
                            {
                                selectedNoteIds_.insert(note.id);
                                dragMode_ = DragMode::MovingNote;
                                dragNoteId_ = note.id;
                                dragStartSample_ = clickSample;
                                dragStartNote_ = clickNote;
                                dragNoteOrigStart_ = note.startSample;
                                dragNoteOrigNote_ = note.noteNumber;
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        }
        repaint();
    }
}

void PianoRollEditor::mouseDrag(const juce::MouseEvent &e)
{
    if (dragMode_ == DragMode::MovingNote && trackIndex_ >= 0 && clipId_.isNotEmpty())
    {
        auto gridArea = getGridArea();
        SampleCount currentSample = xToSample(e.x, gridArea.getX(), gridArea.getWidth());
        int currentNote = yToNote(e.y, gridArea.getY(), gridArea.getHeight());

        SampleCount sampleDelta = currentSample - dragStartSample_;
        int noteDelta = currentNote - dragStartNote_;

        auto *track = session_.getTrack(trackIndex_);
        if (track)
        {
            for (auto &mc : track->midiClips)
            {
                if (mc.id == clipId_)
                {
                    for (auto &note : mc.notes)
                    {
                        if (note.id == dragNoteId_)
                        {
                            note.startSample = std::max(SampleCount(0), dragNoteOrigStart_ + sampleDelta);
                            note.noteNumber = juce::jlimit(0, 127, dragNoteOrigNote_ + noteDelta);
                            break;
                        }
                    }
                    break;
                }
            }
        }
        if (onSessionChanged)
            onSessionChanged();
        repaint();
    }
}

void PianoRollEditor::mouseUp(const juce::MouseEvent &)
{
    dragMode_ = DragMode::None;
    dragNoteId_.clear();
}

void PianoRollEditor::mouseDoubleClick(const juce::MouseEvent &)
{
    // No-op in stub
}

void PianoRollEditor::mouseWheelMove(const juce::MouseEvent &e,
                                     const juce::MouseWheelDetails &wheel)
{
    if (e.mods.isCommandDown())
    {
        // Zoom
        if (wheel.deltaY > 0)
            pixelsPerSample_ = std::min(pixelsPerSample_ * 1.2, 1.0);
        else
            pixelsPerSample_ = std::max(pixelsPerSample_ / 1.2, 0.0001);
    }
    else if (e.mods.isShiftDown())
    {
        // Vertical scroll (note range)
        int delta = wheel.deltaY > 0 ? -2 : 2;
        lowestVisibleNote_ = juce::jlimit(0, 120, lowestVisibleNote_ + delta);
        highestVisibleNote_ = juce::jlimit(lowestVisibleNote_ + 12, 127, highestVisibleNote_ + delta);
    }
    else
    {
        // Horizontal scroll
        double amount = -wheel.deltaY * 3000.0 / pixelsPerSample_;
        scrollSamples_ = std::max(SampleCount(0), scrollSamples_ + static_cast<SampleCount>(amount));
    }
    repaint();
}

} // namespace ampl
