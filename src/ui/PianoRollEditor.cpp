#include "ui/PianoRollEditor.h"
#include "commands/MidiCommands.h"
#include <cmath>

namespace neurato {

PianoRollEditor::PianoRollEditor(Session& session, CommandManager& commandManager)
    : session_(session), commandManager_(commandManager)
{
    // Two-tool system like Logic: Pointer and Pencil
    pointerToolBtn_.setButtonText(juce::String::fromUTF8("\xe2\x86\x96")); // arrow
    pointerToolBtn_.setClickingTogglesState(true);
    pointerToolBtn_.setRadioGroupId(1001);
    pointerToolBtn_.setTooltip("Pointer Tool (select, move, resize)");
    pointerToolBtn_.onClick = [this] { setTool(Tool::Pointer); };
    addAndMakeVisible(pointerToolBtn_);

    pencilToolBtn_.setButtonText(juce::String::fromUTF8("\xe2\x9c\x8f")); // pencil
    pencilToolBtn_.setClickingTogglesState(true);
    pencilToolBtn_.setRadioGroupId(1001);
    pencilToolBtn_.setToggleState(true, juce::dontSendNotification);
    pencilToolBtn_.setTooltip("Pencil Tool (draw notes, click to erase)");
    pencilToolBtn_.onClick = [this] { setTool(Tool::Pencil); };
    addAndMakeVisible(pencilToolBtn_);

    // Grid division combo — musical note values
    gridCombo_.addItem("Off", 1);
    gridCombo_.addItem("1 Bar", 2);
    gridCombo_.addItem("1/2", 3);
    gridCombo_.addItem("1/4", 4);
    gridCombo_.addItem("1/8", 5);
    gridCombo_.addItem("1/16", 6);
    gridCombo_.setSelectedId(5); // Default: 1/8
    gridCombo_.setTooltip("Grid Division");
    gridCombo_.onChange = [this] {
        switch (gridCombo_.getSelectedId())
        {
            case 1: gridDiv_ = GridDiv::Off; break;
            case 2: gridDiv_ = GridDiv::Bar; break;
            case 3: gridDiv_ = GridDiv::Half; break;
            case 4: gridDiv_ = GridDiv::Quarter; break;
            case 5: gridDiv_ = GridDiv::Eighth; break;
            case 6: gridDiv_ = GridDiv::Sixteenth; break;
        }
        repaint();
    };
    addAndMakeVisible(gridCombo_);

    closeBtn_.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::bgElevated));
    closeBtn_.onClick = [this] { if (onCloseRequested) onCloseRequested(); };
    addAndMakeVisible(closeBtn_);

    updateCursor();
}

void PianoRollEditor::setTool(Tool tool)
{
    tool_ = tool;
    if (tool == Tool::Pointer)
        pointerToolBtn_.setToggleState(true, juce::dontSendNotification);
    else
        pencilToolBtn_.setToggleState(true, juce::dontSendNotification);
    updateCursor();
    repaint();
}

void PianoRollEditor::updateCursor()
{
    if (tool_ == Tool::Pointer)
        setMouseCursor(juce::MouseCursor::NormalCursor);
    else
        setMouseCursor(juce::MouseCursor::CrosshairCursor);
}

void PianoRollEditor::setClip(int trackIndex, const juce::String& clipId)
{
    trackIndex_ = trackIndex;
    clipId_ = clipId;
    selectedNoteIds_.clear();
    repaint();
}

void PianoRollEditor::clearClip()
{
    trackIndex_ = -1;
    clipId_ = {};
    selectedNoteIds_.clear();
    repaint();
}

// --- Layout helpers ---

juce::Rectangle<int> PianoRollEditor::getToolbarArea() const
{
    return getLocalBounds().removeFromTop(kToolbarHeight);
}

juce::Rectangle<int> PianoRollEditor::getKeyboardArea() const
{
    auto area = getLocalBounds();
    area.removeFromTop(kToolbarHeight);
    area.removeFromTop(kBeatRulerHeight);
    area.removeFromBottom(kVelocityHeight);
    return area.removeFromLeft(Theme::pianoKeyWidth);
}

juce::Rectangle<int> PianoRollEditor::getGridArea() const
{
    auto area = getLocalBounds();
    area.removeFromTop(kToolbarHeight);
    area.removeFromTop(kBeatRulerHeight);
    area.removeFromBottom(kVelocityHeight);
    area.removeFromLeft(Theme::pianoKeyWidth);
    return area;
}

juce::Rectangle<int> PianoRollEditor::getBeatRulerArea() const
{
    auto area = getLocalBounds();
    area.removeFromTop(kToolbarHeight);
    area.removeFromLeft(Theme::pianoKeyWidth);
    return area.removeFromTop(kBeatRulerHeight);
}

juce::Rectangle<int> PianoRollEditor::getVelocityArea() const
{
    auto area = getLocalBounds();
    area.removeFromLeft(Theme::pianoKeyWidth);
    return area.removeFromBottom(kVelocityHeight);
}

// --- Coordinate conversion ---

int PianoRollEditor::noteToY(int noteNumber, int gridTop, int gridHeight) const
{
    int noteRange = highestVisibleNote_ - lowestVisibleNote_;
    if (noteRange <= 0) return gridTop;
    int noteOffset = highestVisibleNote_ - noteNumber;
    return gridTop + (noteOffset * gridHeight) / noteRange;
}

int PianoRollEditor::yToNote(int y, int gridTop, int gridHeight) const
{
    int noteRange = highestVisibleNote_ - lowestVisibleNote_;
    if (noteRange <= 0 || gridHeight <= 0) return 60;
    int noteOffset = ((y - gridTop) * noteRange) / gridHeight;
    return std::clamp(highestVisibleNote_ - noteOffset, 0, 127);
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

double PianoRollEditor::getGridSizeSamples() const
{
    double bpm = session_.getBpm();
    double sr = session_.getSampleRate();
    if (bpm <= 0 || sr <= 0) return 0.0;

    double samplesPerBeat = (60.0 / bpm) * sr; // quarter note

    switch (gridDiv_)
    {
        case GridDiv::Off:       return 0.0;
        case GridDiv::Bar:       return samplesPerBeat * session_.getTimeSigNumerator();
        case GridDiv::Half:      return samplesPerBeat * 2.0;
        case GridDiv::Quarter:   return samplesPerBeat;
        case GridDiv::Eighth:    return samplesPerBeat / 2.0;
        case GridDiv::Sixteenth: return samplesPerBeat / 4.0;
    }
    return 0.0;
}

juce::String PianoRollEditor::gridDivLabel(GridDiv div) const
{
    switch (div)
    {
        case GridDiv::Off:       return "Off";
        case GridDiv::Bar:       return "Bar";
        case GridDiv::Half:      return "1/2";
        case GridDiv::Quarter:   return "1/4";
        case GridDiv::Eighth:    return "1/8";
        case GridDiv::Sixteenth: return "1/16";
    }
    return "Off";
}

SampleCount PianoRollEditor::snapSample(SampleCount sample) const
{
    double gridSize = getGridSizeSamples();
    if (gridSize <= 0.0) return sample;
    return static_cast<SampleCount>(std::round(static_cast<double>(sample) / gridSize) * gridSize);
}

bool PianoRollEditor::isBlackKey(int noteNumber)
{
    int n = noteNumber % 12;
    return (n == 1 || n == 3 || n == 6 || n == 8 || n == 10);
}

juce::String PianoRollEditor::noteName(int noteNumber)
{
    static const char* names[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    int octave = (noteNumber / 12) - 1;
    return juce::String(names[noteNumber % 12]) + juce::String(octave);
}

// --- Paint ---

void PianoRollEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(Theme::bgBase));

    auto toolbarArea = getToolbarArea();
    auto rulerArea = getBeatRulerArea();
    auto keyArea = getKeyboardArea();
    auto gridArea = getGridArea();
    auto velArea = getVelocityArea();

    paintToolbar(g, toolbarArea);
    paintBeatRuler(g, rulerArea);
    paintKeyboard(g, keyArea);
    paintNoteGrid(g, gridArea);
    paintNotes(g, gridArea);
    paintVelocityLane(g, velArea);

    // Borders
    g.setColour(juce::Colour(Theme::border));
    g.drawHorizontalLine(toolbarArea.getBottom(), 0.0f, static_cast<float>(getWidth()));
    g.drawHorizontalLine(rulerArea.getBottom(), static_cast<float>(Theme::pianoKeyWidth),
                         static_cast<float>(getWidth()));
    g.drawVerticalLine(Theme::pianoKeyWidth, static_cast<float>(toolbarArea.getBottom()),
                       static_cast<float>(getHeight() - kVelocityHeight));
    g.drawHorizontalLine(getHeight() - kVelocityHeight, static_cast<float>(Theme::pianoKeyWidth),
                         static_cast<float>(getWidth()));
}

void PianoRollEditor::paintToolbar(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(juce::Colour(Theme::toolbarBg));
    g.fillRect(area);

    // Clip name
    if (trackIndex_ >= 0)
    {
        if (auto* track = session_.getTrack(trackIndex_))
        {
            if (auto* clip = track->findMidiClip(clipId_))
            {
                g.setColour(juce::Colour(Theme::textSecondary));
                g.setFont(Theme::bodyFont());
                g.drawText(track->name + " > " + clip->name,
                           area.withTrimmedLeft(220).withTrimmedRight(40),
                           juce::Justification::centredLeft);
            }
        }
    }
}

void PianoRollEditor::paintKeyboard(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(juce::Colour(Theme::bgSurface));
    g.fillRect(area);

    int noteRange = highestVisibleNote_ - lowestVisibleNote_;
    if (noteRange <= 0) return;

    float noteH = static_cast<float>(area.getHeight()) / static_cast<float>(noteRange);

    for (int note = lowestVisibleNote_; note < highestVisibleNote_; ++note)
    {
        int y = noteToY(note, area.getY(), area.getHeight());
        int nextY = noteToY(note - 1, area.getY(), area.getHeight());
        int h = nextY - y;

        bool black = isBlackKey(note);
        auto keyRect = juce::Rectangle<int>(area.getX(), y, area.getWidth(), h);

        g.setColour(juce::Colour(black ? Theme::pianoBlackKey : Theme::pianoWhiteKey));
        g.fillRect(keyRect);

        g.setColour(juce::Colour(Theme::pianoKeyBorder));
        g.drawHorizontalLine(y, static_cast<float>(area.getX()), static_cast<float>(area.getRight()));

        // Labels: always show C notes; show all notes if zoomed in enough
        bool isC = (note % 12 == 0);
        bool showLabel = isC || (noteH >= 10.0f);

        if (showLabel && h > 4)
        {
            g.setFont(Theme::smallFont());
            if (isC)
            {
                // C notes: bold, brighter
                g.setColour(juce::Colour(Theme::textSecondary));
            }
            else
            {
                g.setColour(juce::Colour(black ? Theme::textDim : Theme::textDim).withAlpha(0.7f));
            }
            g.drawText(noteName(note), keyRect.reduced(2, 0), juce::Justification::centredRight);
        }
    }
}

void PianoRollEditor::paintNoteGrid(juce::Graphics& g, juce::Rectangle<int> area)
{
    // Fill entire grid background
    g.setColour(juce::Colour(Theme::pianoRowWhite));
    g.fillRect(area);

    int noteRange = highestVisibleNote_ - lowestVisibleNote_;
    if (noteRange <= 0) return;

    // --- Row shading: white keys lighter, black keys darker (like Logic) ---
    for (int note = lowestVisibleNote_; note < highestVisibleNote_; ++note)
    {
        int y = noteToY(note, area.getY(), area.getHeight());
        int nextY = noteToY(note - 1, area.getY(), area.getHeight());
        int rowH = nextY - y;
        if (rowH <= 0) continue;

        if (isBlackKey(note))
        {
            g.setColour(juce::Colour(Theme::pianoRowBlack));
            g.fillRect(area.getX(), y, area.getWidth(), rowH);
        }

        // Horizontal row separator
        g.setColour(juce::Colour(Theme::pianoGrid));
        g.drawHorizontalLine(y, static_cast<float>(area.getX()), static_cast<float>(area.getRight()));

        // Stronger line at each C (octave boundary)
        if (note % 12 == 0)
        {
            g.setColour(juce::Colour(Theme::pianoGridBeat));
            g.drawHorizontalLine(y, static_cast<float>(area.getX()), static_cast<float>(area.getRight()));
        }
    }

    // --- Vertical grid lines synced with musical divisions ---
    double bpm = session_.getBpm();
    double sr = session_.getSampleRate();
    if (bpm <= 0 || sr <= 0) return;

    double samplesPerBeat = (60.0 / bpm) * sr;  // quarter note
    int timeSigNum = session_.getTimeSigNumerator();
    double samplesPerBar = samplesPerBeat * timeSigNum;

    // The grid subdivision matches the selected grid division
    double gridSize = getGridSizeSamples();
    if (gridSize <= 0.0)
        gridSize = samplesPerBeat; // fallback to quarter note if Off

    // Find first grid line at or before scroll position
    SampleCount firstGrid = static_cast<SampleCount>(
        std::floor(static_cast<double>(scrollSamples_) / gridSize) * gridSize);

    for (SampleCount s = firstGrid; ; s += static_cast<SampleCount>(gridSize))
    {
        int x = sampleToX(s, area.getX(), area.getWidth());
        if (x > area.getRight()) break;
        if (x < area.getX()) continue;

        double sDouble = static_cast<double>(s);
        bool isBar = (samplesPerBar > 0 && std::fmod(sDouble, samplesPerBar) < 1.0);
        bool isBeat = (samplesPerBeat > 0 && std::fmod(sDouble, samplesPerBeat) < 1.0);

        if (isBar)
            g.setColour(juce::Colour(Theme::pianoGridBar));
        else if (isBeat)
            g.setColour(juce::Colour(Theme::pianoGridBeat));
        else
            g.setColour(juce::Colour(Theme::pianoGrid));

        g.drawVerticalLine(x, static_cast<float>(area.getY()), static_cast<float>(area.getBottom()));
    }
}

void PianoRollEditor::paintBeatRuler(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(juce::Colour(Theme::pianoRulerBg));
    g.fillRect(area);

    double bpm = session_.getBpm();
    double sr = session_.getSampleRate();
    if (bpm <= 0 || sr <= 0) return;

    double samplesPerBeat = (60.0 / bpm) * sr;
    int timeSigNum = session_.getTimeSigNumerator();
    double samplesPerBar = samplesPerBeat * timeSigNum;

    g.setFont(Theme::smallFont());

    // Draw beat ticks and labels
    double gridSize = getGridSizeSamples();
    if (gridSize <= 0.0) gridSize = samplesPerBeat;

    SampleCount firstGrid = static_cast<SampleCount>(
        std::floor(static_cast<double>(scrollSamples_) / gridSize) * gridSize);

    for (SampleCount s = firstGrid; ; s += static_cast<SampleCount>(gridSize))
    {
        int x = sampleToX(s, area.getX(), area.getWidth());
        if (x > area.getRight()) break;
        if (x < area.getX()) continue;

        double sDouble = static_cast<double>(s);
        bool isBar = (samplesPerBar > 0 && std::fmod(sDouble, samplesPerBar) < 1.0);
        bool isBeat = (samplesPerBeat > 0 && std::fmod(sDouble, samplesPerBeat) < 1.0);

        if (isBar)
        {
            // Bar number label
            int barNum = static_cast<int>(sDouble / samplesPerBar) + 1;
            g.setColour(juce::Colour(Theme::textSecondary));
            g.drawText(juce::String(barNum), x + 2, area.getY(), 30, area.getHeight(),
                       juce::Justification::centredLeft, false);

            // Tall tick
            g.setColour(juce::Colour(Theme::pianoGridBar));
            g.drawVerticalLine(x, static_cast<float>(area.getY()),
                              static_cast<float>(area.getBottom()));
        }
        else if (isBeat)
        {
            // Beat number within bar
            int beatInBar = static_cast<int>(std::fmod(sDouble / samplesPerBeat, timeSigNum)) + 1;
            g.setColour(juce::Colour(Theme::pianoRulerText));
            g.drawText(juce::String(beatInBar), x + 1, area.getY(), 20, area.getHeight(),
                       juce::Justification::centredLeft, false);

            // Medium tick
            g.setColour(juce::Colour(Theme::pianoGridBeat));
            g.drawVerticalLine(x, static_cast<float>(area.getY() + area.getHeight() / 3),
                              static_cast<float>(area.getBottom()));
        }
        else
        {
            // Subdivision tick
            g.setColour(juce::Colour(Theme::pianoGrid));
            g.drawVerticalLine(x, static_cast<float>(area.getY() + area.getHeight() * 2 / 3),
                              static_cast<float>(area.getBottom()));
        }
    }
}

void PianoRollEditor::paintNotes(juce::Graphics& g, juce::Rectangle<int> area)
{
    if (trackIndex_ < 0) return;
    auto* track = session_.getTrack(trackIndex_);
    if (!track) return;
    auto* clip = track->findMidiClip(clipId_);
    if (!clip) return;

    int noteRange = highestVisibleNote_ - lowestVisibleNote_;
    if (noteRange <= 0) return;

    float noteH = static_cast<float>(area.getHeight()) / static_cast<float>(noteRange);

    for (const auto& note : clip->notes)
    {
        if (note.noteNumber < lowestVisibleNote_ || note.noteNumber >= highestVisibleNote_)
            continue;

        // Note position is relative to clip start on timeline
        SampleCount absStart = clip->timelineStartSample + note.startSample;
        SampleCount absEnd = absStart + note.lengthSamples;

        int x1 = sampleToX(absStart, area.getX(), area.getWidth());
        int x2 = sampleToX(absEnd, area.getX(), area.getWidth());
        int y = noteToY(note.noteNumber, area.getY(), area.getHeight());

        int w = std::max(x2 - x1, 2);
        int h = std::max(static_cast<int>(noteH) - 1, 2);

        auto noteRect = juce::Rectangle<int>(x1, y, w, h);

        if (!noteRect.intersects(area))
            continue;

        bool selected = selectedNoteIds_.count(note.id) > 0;

        // Note body
        float alpha = note.velocity;
        auto noteColor = juce::Colour(selected ? Theme::pianoNoteSelected : Theme::pianoNoteColor)
                             .withMultipliedAlpha(0.5f + alpha * 0.5f);
        g.setColour(noteColor);
        g.fillRoundedRectangle(noteRect.toFloat(), 2.0f);

        // Note border
        g.setColour(juce::Colour(selected ? Theme::pianoNoteSelected : Theme::pianoNoteColor));
        g.drawRoundedRectangle(noteRect.toFloat(), 2.0f, selected ? 1.5f : 0.8f);

        // Note name label if wide enough
        if (w > 30)
        {
            g.setColour(juce::Colour(Theme::textPrimary).withAlpha(0.8f));
            g.setFont(Theme::smallFont());
            g.drawText(noteName(note.noteNumber), noteRect.reduced(3, 0),
                       juce::Justification::centredLeft, true);
        }
    }
}

void PianoRollEditor::paintVelocityLane(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(juce::Colour(Theme::bgSurface));
    g.fillRect(area);

    if (trackIndex_ < 0) return;
    auto* track = session_.getTrack(trackIndex_);
    if (!track) return;
    auto* clip = track->findMidiClip(clipId_);
    if (!clip) return;

    auto gridArea = getGridArea();

    for (const auto& note : clip->notes)
    {
        SampleCount absStart = clip->timelineStartSample + note.startSample;
        int x = sampleToX(absStart, gridArea.getX(), gridArea.getWidth());

        if (x < area.getX() || x > area.getRight())
            continue;

        int barH = static_cast<int>(note.velocity * static_cast<float>(area.getHeight() - 4));
        bool selected = selectedNoteIds_.count(note.id) > 0;

        g.setColour(juce::Colour(selected ? Theme::pianoNoteSelected : Theme::pianoVelocity)
                        .withAlpha(0.8f));
        g.fillRect(x - 1, area.getBottom() - barH - 2, 3, barH);
    }

    // Label
    g.setColour(juce::Colour(Theme::textDim));
    g.setFont(Theme::smallFont());
    g.drawText("Velocity", area.reduced(4, 2), juce::Justification::topLeft);
}

void PianoRollEditor::resized()
{
    auto toolbar = getToolbarArea().reduced(4, 2);
    pointerToolBtn_.setBounds(toolbar.removeFromLeft(30));
    toolbar.removeFromLeft(2);
    pencilToolBtn_.setBounds(toolbar.removeFromLeft(30));
    toolbar.removeFromLeft(10);
    gridCombo_.setBounds(toolbar.removeFromLeft(70));

    closeBtn_.setBounds(toolbar.removeFromRight(26));
}

// --- Mouse interaction ---

void PianoRollEditor::mouseDown(const juce::MouseEvent& e)
{
    auto gridArea = getGridArea();
    auto velArea = getVelocityArea();

    if (!gridArea.contains(e.getPosition()) && !velArea.contains(e.getPosition()))
        return;

    if (trackIndex_ < 0) return;
    auto* track = session_.getTrack(trackIndex_);
    if (!track) return;
    auto* clip = track->findMidiClip(clipId_);
    if (!clip) return;

    if (velArea.contains(e.getPosition()))
    {
        dragMode_ = DragMode::VelocityDrag;
        dragStartPoint_ = e.getPosition();
        return;
    }

    SampleCount clickSample = xToSample(e.x, gridArea.getX(), gridArea.getWidth());
    int clickNote = yToNote(e.y, gridArea.getY(), gridArea.getHeight());

    int noteRange = highestVisibleNote_ - lowestVisibleNote_;
    float noteH = (noteRange > 0) ? static_cast<float>(gridArea.getHeight()) / static_cast<float>(noteRange) : 10.0f;

    // Check if clicking on an existing note
    for (const auto& note : clip->notes)
    {
        SampleCount absStart = clip->timelineStartSample + note.startSample;
        SampleCount absEnd = absStart + note.lengthSamples;

        int nx1 = sampleToX(absStart, gridArea.getX(), gridArea.getWidth());
        int nx2 = sampleToX(absEnd, gridArea.getX(), gridArea.getWidth());
        int ny = noteToY(note.noteNumber, gridArea.getY(), gridArea.getHeight());
        int nh = std::max(static_cast<int>(noteH) - 1, 2);

        auto noteRect = juce::Rectangle<int>(nx1, ny, nx2 - nx1, nh);

        if (noteRect.contains(e.getPosition()))
        {
            // Pencil tool on existing note = erase it
            if (tool_ == Tool::Pencil)
            {
                auto cmd = std::make_unique<RemoveMidiNoteCommand>(trackIndex_, clipId_, note.id);
                commandManager_.execute(std::move(cmd), session_);
                if (onSessionChanged) onSessionChanged();
                return;
            }

            // Pointer tool: check if near right edge for resize
            if (e.x > nx2 - 6)
            {
                dragMode_ = DragMode::ResizingNote;
                dragNoteId_ = note.id;
                dragStartSample_ = clickSample;
                return;
            }

            // Pointer tool: select / start move
            if (!e.mods.isShiftDown())
                selectedNoteIds_.clear();
            selectedNoteIds_.insert(note.id);

            dragMode_ = DragMode::MovingNote;
            dragNoteId_ = note.id;
            dragStartSample_ = clickSample;
            dragStartNote_ = clickNote;
            dragNoteOrigStart_ = note.startSample;
            dragNoteOrigNote_ = note.noteNumber;
            repaint();
            return;
        }
    }

    // Clicked on empty space
    if (tool_ == Tool::Pencil)
    {
        // Draw a new note
        SampleCount noteSample = snapSample(clickSample - clip->timelineStartSample);
        noteSample = std::max(SampleCount(0), noteSample);

        // Default note length = one grid division
        double gridSize = getGridSizeSamples();
        SampleCount defaultLength;
        if (gridSize > 0.0)
            defaultLength = static_cast<SampleCount>(gridSize);
        else
        {
            double bpm = session_.getBpm();
            double sr = session_.getSampleRate();
            defaultLength = static_cast<SampleCount>((60.0 / bpm) * sr / 2.0); // 1/8 note fallback
        }

        MidiNote newNote;
        newNote.noteNumber = clickNote;
        newNote.startSample = noteSample;
        newNote.lengthSamples = defaultLength;
        newNote.velocity = 0.8f;

        auto cmd = std::make_unique<AddMidiNoteCommand>(trackIndex_, clipId_, newNote);
        commandManager_.execute(std::move(cmd), session_);

        selectedNoteIds_.clear();
        selectedNoteIds_.insert(newNote.id);
        dragMode_ = DragMode::ResizingNote;
        dragNoteId_ = newNote.id;
        dragStartSample_ = clickSample;

        if (onSessionChanged) onSessionChanged();
    }
    else // Pointer tool on empty space = start selection box
    {
        selectedNoteIds_.clear();
        dragMode_ = DragMode::SelectBox;
        dragStartPoint_ = e.getPosition();
        repaint();
    }
}

void PianoRollEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (trackIndex_ < 0) return;
    auto* track = session_.getTrack(trackIndex_);
    if (!track) return;
    auto* clip = track->findMidiClip(clipId_);
    if (!clip) return;

    auto gridArea = getGridArea();

    if (dragMode_ == DragMode::MovingNote)
    {
        auto* note = clip->findNote(dragNoteId_);
        if (!note) return;

        SampleCount currentSample = xToSample(e.x, gridArea.getX(), gridArea.getWidth());
        int currentNote = yToNote(e.y, gridArea.getY(), gridArea.getHeight());

        SampleCount sampleDelta = currentSample - dragStartSample_;
        int noteDelta = currentNote - dragStartNote_;

        SampleCount newStart = snapSample(std::max(SampleCount(0), dragNoteOrigStart_ + sampleDelta));
        int newNote = std::clamp(dragNoteOrigNote_ + noteDelta, 0, 127);

        note->startSample = newStart;
        note->noteNumber = newNote;
        repaint();
    }
    else if (dragMode_ == DragMode::ResizingNote)
    {
        auto* note = clip->findNote(dragNoteId_);
        if (!note) return;

        SampleCount currentSample = xToSample(e.x, gridArea.getX(), gridArea.getWidth());
        SampleCount relSample = currentSample - clip->timelineStartSample;
        SampleCount newLength = snapSample(std::max(SampleCount(1), relSample - note->startSample));
        note->lengthSamples = newLength;
        repaint();
    }
    else if (dragMode_ == DragMode::VelocityDrag)
    {
        auto velArea = getVelocityArea();
        float vel = 1.0f - static_cast<float>(e.y - velArea.getY()) / static_cast<float>(velArea.getHeight());
        vel = std::clamp(vel, 0.01f, 1.0f);

        // Apply to selected notes or nearest note
        for (auto& note : clip->notes)
        {
            if (selectedNoteIds_.count(note.id) > 0)
                note.velocity = vel;
        }
        repaint();
    }
}

void PianoRollEditor::mouseUp(const juce::MouseEvent& /*e*/)
{
    if (dragMode_ == DragMode::MovingNote || dragMode_ == DragMode::ResizingNote)
    {
        if (onSessionChanged) onSessionChanged();
    }
    dragMode_ = DragMode::None;
    dragNoteId_ = {};
}

void PianoRollEditor::mouseDoubleClick(const juce::MouseEvent& /*e*/)
{
    // Double-click toggles between Pointer and Pencil (like Logic's T key)
    if (tool_ == Tool::Pointer)
        setTool(Tool::Pencil);
    else
        setTool(Tool::Pointer);
}

void PianoRollEditor::mouseWheelMove(const juce::MouseEvent& e,
                                      const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCommandDown())
    {
        // Zoom horizontally
        if (wheel.deltaY > 0)
            pixelsPerSample_ = std::min(pixelsPerSample_ * 1.2, 0.5);
        else
            pixelsPerSample_ = std::max(pixelsPerSample_ / 1.2, 0.0001);
    }
    else if (e.mods.isShiftDown())
    {
        // Scroll vertically (shift note range)
        int shift = (wheel.deltaY > 0) ? 2 : -2;
        lowestVisibleNote_ = std::clamp(lowestVisibleNote_ + shift, 0, 115);
        highestVisibleNote_ = std::clamp(highestVisibleNote_ + shift, 12, 127);
    }
    else
    {
        // Scroll horizontally
        double scrollAmount = -wheel.deltaY * 3000.0 / pixelsPerSample_;
        scrollSamples_ += static_cast<SampleCount>(scrollAmount);
        scrollSamples_ = std::max(SampleCount(0), scrollSamples_);
    }
    repaint();
}

} // namespace neurato
