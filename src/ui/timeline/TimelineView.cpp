#include "ui/timeline/TimelineView.hpp"
#include <algorithm>
#include <cmath>

namespace
{
struct HeaderActionRects
{
    juce::Rectangle<int> mute;
    juce::Rectangle<int> solo;
};

HeaderActionRects getHeaderActionRects(juce::Rectangle<int> headerContentArea)
{
    HeaderActionRects rects;
    auto actionRow = headerContentArea.reduced(6, 4).removeFromTop(18);
    constexpr int buttonW = 18;
    rects.solo = actionRow.removeFromRight(buttonW);
    actionRow.removeFromRight(4);
    rects.mute = actionRow.removeFromRight(buttonW);
    return rects;
}
} // namespace

namespace ampl
{

TimelineView::TimelineView(AudioEngine &engine, Session &session)
    : engine_(engine), session_(session)
{
}

TimelineView::~TimelineView() = default;

void TimelineView::handleAudioMessage(const AudioToUIMessage &msg)
{
    switch (msg.type)
    {
    case AudioToUIMessage::Type::PlayheadPosition:
        if (playheadPositionSamples_ != msg.intValue)
        {
            playheadPositionSamples_ = msg.intValue;
            repaintPending_ = true;
        }
        break;
    case AudioToUIMessage::Type::PeakLevel:
    case AudioToUIMessage::Type::TransportStateChanged:
        break;
    }
}

void TimelineView::updateDisplay()
{
    if (repaintPending_)
    {
        repaint();
        repaintPending_ = false;
    }
}

// --- Coordinate conversion ---

int TimelineView::sampleToPixelX(SampleCount sample) const
{
    double relSample = static_cast<double>(sample - scrollPositionSamples_);
    return kTrackHeaderWidth + static_cast<int>(relSample * pixelsPerSample_);
}

SampleCount TimelineView::pixelXToSample(int pixelX) const
{
    double relPixel = static_cast<double>(pixelX - kTrackHeaderWidth);
    return scrollPositionSamples_ + static_cast<SampleCount>(relPixel / pixelsPerSample_);
}

int TimelineView::trackIndexAtY(int y) const
{
    if (y < kRulerHeight)
        return -1;
    // Account for vertical scroll offset
    int trackY = y - kRulerHeight + verticalScrollOffset_;
    int index = trackY / kTrackHeight;
    if (index < 0 || index >= static_cast<int>(session_.getTracks().size()))
        return -1;
    return index;
}

// --- Vertical scroll ---

int TimelineView::getTotalContentHeight() const
{
    int numTracks = static_cast<int>(session_.getTracks().size());
    return kRulerHeight + numTracks * kTrackHeight;
}

void TimelineView::setVerticalScrollOffset(int offset)
{
    verticalScrollOffset_ = offset;
    clampVerticalScroll();
    repaintPending_ = true;
    repaint();
}

void TimelineView::clampVerticalScroll()
{
    int totalContent = getTotalContentHeight();
    int viewHeight = getHeight();
    int maxScroll = std::max(0, totalContent - viewHeight);
    verticalScrollOffset_ = juce::jlimit(0, maxScroll, verticalScrollOffset_);
}

void TimelineView::ensureTrackVisible(int trackIndex)
{
    if (trackIndex < 0)
        return;
    int trackTop = trackIndex * kTrackHeight;
    int trackBottom = trackTop + kTrackHeight;
    int visibleTop = verticalScrollOffset_;
    int visibleBottom = verticalScrollOffset_ + getHeight() - kRulerHeight;

    if (trackTop < visibleTop)
        setVerticalScrollOffset(trackTop);
    else if (trackBottom > visibleBottom)
        setVerticalScrollOffset(trackBottom - (getHeight() - kRulerHeight));
}

// --- Track selection ---

void TimelineView::selectTrack(int trackIndex)
{
    const int trackCount = static_cast<int>(session_.getTracks().size());
    if (trackCount <= 0)
    {
        selectedTrackIndex_ = -1;
        return;
    }

    selectedTrackIndex_ = juce::jlimit(0, trackCount - 1, trackIndex);
    selectedClipId_.clear();
    ensureTrackVisible(selectedTrackIndex_);
    repaintPending_ = true;
    repaint();
}

void TimelineView::selectNextTrack()
{
    const int trackCount = static_cast<int>(session_.getTracks().size());
    if (trackCount <= 0)
        return;

    const int next =
        selectedTrackIndex_ < 0 ? 0 : std::min(selectedTrackIndex_ + 1, trackCount - 1);
    selectTrack(next);
}

void TimelineView::selectPreviousTrack()
{
    const int trackCount = static_cast<int>(session_.getTracks().size());
    if (trackCount <= 0)
        return;

    const int prev = selectedTrackIndex_ < 0 ? 0 : std::max(selectedTrackIndex_ - 1, 0);
    selectTrack(prev);
}

// --- Zoom ---

void TimelineView::zoomIn()
{
    pixelsPerSample_ = std::min(pixelsPerSample_ * 1.5, 1.0);
    repaintPending_ = true;
    repaint();
}

void TimelineView::zoomOut()
{
    pixelsPerSample_ = std::max(pixelsPerSample_ / 1.5, 0.0001);
    repaintPending_ = true;
    repaint();
}

void TimelineView::zoomToFit()
{
    SampleCount maxEnd = 0;
    for (const auto &track : session_.getTracks())
        for (const auto &clip : track.clips)
            maxEnd = std::max(maxEnd, clip.getTimelineEndSample());

    if (maxEnd > 0)
    {
        int availableWidth = getWidth() - kTrackHeaderWidth - 40;
        if (availableWidth > 0)
        {
            pixelsPerSample_ = static_cast<double>(availableWidth) / static_cast<double>(maxEnd);
            scrollPositionSamples_ = 0;
        }
    }
    repaintPending_ = true;
    repaint();
}

void TimelineView::setScrollPositionSamples(SampleCount pos)
{
    scrollPositionSamples_ = std::max(SampleCount(0), pos);
    repaintPending_ = true;
    repaint();
}

void TimelineView::centerOnPlayhead()
{
    int availableWidth = getWidth() - kTrackHeaderWidth;
    if (availableWidth <= 0 || pixelsPerSample_ <= 0.0)
        return;

    auto visibleSamples =
        static_cast<SampleCount>(static_cast<double>(availableWidth) / pixelsPerSample_);
    auto targetLeft = playheadPositionSamples_ - (visibleSamples / 2);
    setScrollPositionSamples(std::max<SampleCount>(0, targetLeft));
}

// --- Paint ---

void TimelineView::paint(juce::Graphics &g)
{
    g.fillAll(juce::Colour(ampl::Theme::bgDeepest));

    auto bounds = getLocalBounds();

    // Ruler at top (always fixed, not affected by vertical scroll)
    auto rulerArea = bounds.removeFromTop(kRulerHeight);
    paintRuler(g, rulerArea);

    // Clip to the track area so tracks don't draw over the ruler
    g.saveState();
    g.reduceClipRegion(bounds);

    // Track lanes — offset by vertical scroll
    const auto &tracks = session_.getTracks();
    for (int i = 0; i < static_cast<int>(tracks.size()); ++i)
    {
        int laneTop = kRulerHeight + i * kTrackHeight - verticalScrollOffset_;
        int laneBottom = laneTop + kTrackHeight;

        // Skip tracks that are entirely off-screen
        if (laneBottom < kRulerHeight || laneTop > getHeight())
            continue;

        auto laneArea = juce::Rectangle<int>(0, laneTop, getWidth(), kTrackHeight);
        paintTrackLane(g, laneArea, tracks[static_cast<size_t>(i)], i);
    }

    // Empty area below all tracks
    int lastTrackBottom = kRulerHeight + static_cast<int>(tracks.size()) * kTrackHeight - verticalScrollOffset_;
    if (lastTrackBottom < getHeight())
    {
        auto emptyArea = juce::Rectangle<int>(0, lastTrackBottom, getWidth(),
                                              getHeight() - lastTrackBottom);
        g.setColour(juce::Colour(ampl::Theme::bgBase));
        g.fillRect(emptyArea);

        // Empty-state guidance when plenty of space
        if (emptyArea.getHeight() > 80)
        {
            auto hintArea = emptyArea.reduced(40, 0);
            hintArea = hintArea.withSizeKeepingCentre(hintArea.getWidth(), 60);

            g.setColour(juce::Colour(ampl::Theme::textDim).withAlpha(0.22f));
            g.setFont(ampl::Theme::headingFont());
            g.drawText("Drop audio files here or click + Audio / + MIDI to add tracks", hintArea,
                       juce::Justification::centredTop, true);

            auto subArea = hintArea.translated(0, 28);
            g.setFont(ampl::Theme::smallFont());
            g.setColour(juce::Colour(ampl::Theme::textDim).withAlpha(0.15f));
            g.drawText("Cmd+O Open project | Space Play/Pause | Return Stop | J Return to Playhead",
                       subArea, juce::Justification::centredTop, true);
        }
    }

    g.restoreState();

    // Loop region overlay (over everything except ruler area)
    paintLoopRegion(g);

    // Playhead (drawn last, on top of everything)
    paintPlayhead(g);

    // Scrollbar indicators
    paintScrollbars(g);
}

void TimelineView::paintRuler(juce::Graphics &g, juce::Rectangle<int> area)
{
    g.setColour(juce::Colour(ampl::Theme::toolbarBg));
    g.fillRect(area);

    // Draw beat/bar grid lines and labels
    double bpm = session_.getBpm();
    double sr = session_.getSampleRate();
    if (bpm <= 0 || sr <= 0)
        return;

    double samplesPerBeat = (60.0 / bpm) * sr;
    int timeSigNum = session_.getTimeSigNumerator();
    double samplesPerBar = samplesPerBeat * timeSigNum;

    // Determine grid resolution based on zoom level
    double pixelsPerBar = samplesPerBar * pixelsPerSample_;
    double gridSamples;

    if (pixelsPerBar > 200)
    {
        gridSamples = samplesPerBeat; // Show every beat
    }
    else if (pixelsPerBar > 50)
    {
        gridSamples = samplesPerBar; // Show every bar
    }
    else
    {
        gridSamples = samplesPerBar * 4; // Show every 4 bars
    }

    // Find first visible grid line
    SampleCount firstVisible = scrollPositionSamples_;
    SampleCount firstGrid = static_cast<SampleCount>(
        std::floor(static_cast<double>(firstVisible) / gridSamples) * gridSamples);

    g.setFont(juce::Font(juce::FontOptions(11.0f)));

    for (SampleCount s = firstGrid;; s += static_cast<SampleCount>(gridSamples))
    {
        int x = sampleToPixelX(s);
        if (x > area.getRight())
            break;
        if (x < kTrackHeaderWidth)
            continue;

        // Determine bar/beat number
        double beatNum = static_cast<double>(s) / samplesPerBeat;
        int bar = static_cast<int>(beatNum / timeSigNum) + 1;
        int beat = static_cast<int>(std::fmod(beatNum, timeSigNum)) + 1;

        bool isBar = (beat == 1);

        g.setColour(isBar ? juce::Colour(ampl::Theme::pianoGridBar)
                          : juce::Colour(ampl::Theme::pianoGridBeat));
        g.drawVerticalLine(x, static_cast<float>(area.getY()),
                           static_cast<float>(area.getBottom()));

        // Label
        g.setColour(juce::Colour(ampl::Theme::pianoRulerText));
        if (isBar || pixelsPerBar > 200)
        {
            juce::String label = juce::String(bar) + "." + juce::String(beat);
            g.drawText(label, x + 2, area.getY(), 40, area.getHeight(),
                       juce::Justification::centredLeft);
        }
    }

    // Track header area label
    g.setColour(juce::Colour(ampl::Theme::toolbarBg));
    g.fillRect(area.withWidth(kTrackHeaderWidth));
    g.setColour(juce::Colour(ampl::Theme::border));
    g.drawVerticalLine(kTrackHeaderWidth, static_cast<float>(area.getY()),
                       static_cast<float>(area.getBottom()));

    // Context-aware discoverability hint for clip state actions
    if (area.getWidth() > 540 && (hoveredClipId_.isNotEmpty() || selectedClipId_.isNotEmpty()))
    {
        auto hintArea = area.withTrimmedLeft(kTrackHeaderWidth + 10).removeFromRight(330);
        g.setColour(juce::Colour(ampl::Theme::textDim).withAlpha(0.9f));
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        const auto nowMs = juce::Time::getMillisecondCounter();
        const bool showFullHint = nowMs < fullHintVisibleUntilMs_;
        const juce::String hintText = showFullHint ? "Alt+Click: Mute Clip   Shift+Click: Lock Clip"
                                                   : "Alt mute \xe2\x80\xa2 Shift lock";
        g.drawText(hintText, hintArea, juce::Justification::centredRight, true);
    }

    // Bottom border
    g.setColour(juce::Colour(ampl::Theme::border));
    g.drawHorizontalLine(area.getBottom() - 1, 0.0f, static_cast<float>(area.getRight()));
}

void TimelineView::paintTrackLane(juce::Graphics &g, juce::Rectangle<int> area,
                                  const TrackState &track, int trackIndex)
{
    // Track header
    auto headerArea = area.removeFromLeft(kTrackHeaderWidth);
    const bool trackSelected = (selectedTrackIndex_ == trackIndex);
    g.setColour(trackSelected ? juce::Colour(ampl::Theme::bgElevated)
                              : juce::Colour(ampl::Theme::bgSurface));
    g.fillRect(headerArea);

    // Track type indicator
    auto typeBar = headerArea.removeFromLeft(3);
    g.setColour(juce::Colour(track.isMidi() ? ampl::Theme::accentCyan : ampl::Theme::accentBlue));
    g.fillRect(typeBar);

    const auto actionRects = getHeaderActionRects(headerArea);

    auto drawAction =
        [&](juce::Rectangle<int> rect, const juce::String &label, bool on, juce::Colour onColour)
    {
        g.setColour(on ? onColour.withAlpha(0.9f) : juce::Colour(ampl::Theme::bgHighlight));
        g.fillRoundedRectangle(rect.toFloat(), 3.0f);
        g.setColour(juce::Colour(ampl::Theme::borderLight).withAlpha(0.85f));
        g.drawRoundedRectangle(rect.toFloat(), 3.0f, 1.0f);
        g.setColour(on ? juce::Colours::white : juce::Colour(ampl::Theme::textSecondary));
        g.setFont(ampl::Theme::smallFont());
        g.drawText(label, rect, juce::Justification::centred, false);
    };

    drawAction(actionRects.mute, "M", track.muted, juce::Colour(ampl::Theme::accentOrange));
    drawAction(actionRects.solo, "S", track.solo, juce::Colour(ampl::Theme::accentYellow));

    // Track name
    g.setColour(juce::Colour(track.muted ? ampl::Theme::textDisabled : ampl::Theme::textPrimary));
    g.setFont(ampl::Theme::headingFont());
    auto textArea = headerArea.reduced(6, 4);
    textArea.removeFromRight(46);
    g.drawText(track.name, textArea, juce::Justification::topLeft);

    // Track info
    g.setFont(ampl::Theme::smallFont());
    g.setColour(juce::Colour(ampl::Theme::textDim));
    juce::String typeStr = track.isMidi() ? "MIDI" : "Audio";
    int clipCount = track.isMidi() ? static_cast<int>(track.midiClips.size())
                                   : static_cast<int>(track.clips.size());
    juce::String info =
        typeStr + " | " + juce::String(clipCount) + " clip" + (clipCount != 1 ? "s" : "");
    if (track.muted)
        info += " [M]";
    if (track.solo)
        info += " [S]";
    g.drawText(info, textArea, juce::Justification::bottomLeft);

    // Header border
    g.setColour(juce::Colour(ampl::Theme::border));
    g.drawVerticalLine(headerArea.getRight(), static_cast<float>(area.getY()),
                       static_cast<float>(area.getBottom()));

    // Lane background
    bool isEven = (trackIndex % 2 == 0);
    juce::Colour laneColour =
        juce::Colour(isEven ? ampl::Theme::trackLaneEven : ampl::Theme::trackLaneOdd);
    if (trackSelected)
        laneColour = laneColour.brighter(0.18f);
    g.setColour(laneColour);
    g.fillRect(area);

    // Draw beat grid lines in lane
    double bpm = session_.getBpm();
    double sr = session_.getSampleRate();
    if (bpm > 0 && sr > 0)
    {
        double samplesPerBeat = (60.0 / bpm) * sr;
        int timeSigNum = session_.getTimeSigNumerator();
        double samplesPerBar = samplesPerBeat * timeSigNum;

        SampleCount firstVisible = scrollPositionSamples_;
        SampleCount firstBar = static_cast<SampleCount>(
            std::floor(static_cast<double>(firstVisible) / samplesPerBar) * samplesPerBar);

        for (SampleCount s = firstBar;; s += static_cast<SampleCount>(samplesPerBar))
        {
            int x = sampleToPixelX(s);
            if (x > area.getRight())
                break;
            if (x < area.getX())
                continue;

            g.setColour(juce::Colour(ampl::Theme::pianoGridBar).withAlpha(0.45f));
            g.drawVerticalLine(x, static_cast<float>(area.getY()),
                               static_cast<float>(area.getBottom()));

            // Optional beat subdivision lines at closer zoom levels
            double beatSamples = samplesPerBar / static_cast<double>(timeSigNum);
            double pixelsPerBeat = beatSamples * pixelsPerSample_;
            if (pixelsPerBeat > 36.0)
            {
                for (int beat = 1; beat < timeSigNum; ++beat)
                {
                    auto beatSample = s + static_cast<SampleCount>(beatSamples * beat);
                    int bx = sampleToPixelX(beatSample);
                    if (bx <= area.getX() || bx >= area.getRight())
                        continue;
                    g.setColour(juce::Colour(ampl::Theme::pianoGridBeat).withAlpha(0.35f));
                    g.drawVerticalLine(bx, static_cast<float>(area.getY()),
                                       static_cast<float>(area.getBottom()));
                }
            }
        }
    }

    // Draw clips (audio or MIDI depending on track type)
    if (track.isAudio())
    {
        for (const auto &clip : track.clips)
        {
            int clipX = sampleToPixelX(clip.timelineStartSample);
            int clipEndX = sampleToPixelX(clip.getTimelineEndSample());
            int clipWidth = clipEndX - clipX;

            if (clipEndX < area.getX() || clipX > area.getRight())
                continue;

            auto clipArea =
                juce::Rectangle<int>(clipX, area.getY() + 2, clipWidth, area.getHeight() - 4);
            clipArea = clipArea.getIntersection(area);

            if (!clipArea.isEmpty())
                paintClip(g, clipArea, clip);
        }
    }
    else
    {
        for (const auto &mclip : track.midiClips)
        {
            int clipX = sampleToPixelX(mclip.timelineStartSample);
            int clipEndX = sampleToPixelX(mclip.getTimelineEndSample());
            int clipWidth = clipEndX - clipX;

            if (clipEndX < area.getX() || clipX > area.getRight())
                continue;

            auto clipArea =
                juce::Rectangle<int>(clipX, area.getY() + 2, clipWidth, area.getHeight() - 4);
            clipArea = clipArea.getIntersection(area);

            if (!clipArea.isEmpty())
                paintMidiClip(g, clipArea, mclip);
        }
    }

    // Lane bottom border
    g.setColour(juce::Colour(ampl::Theme::border));
    g.drawHorizontalLine(area.getBottom() - 1, static_cast<float>(headerArea.getX()),
                         static_cast<float>(area.getRight()));
}

void TimelineView::paintClip(juce::Graphics &g, juce::Rectangle<int> area, const Clip &clip)
{
    const bool isSelected = (selectedClipId_ == clip.id);
    const bool isHovered = (hoveredClipId_ == clip.id);
    const auto clipKey = clip.id.toStdString();
    const bool isMuted = mutedClipIds_.count(clipKey) > 0;
    const bool isLocked = lockedClipIds_.count(clipKey) > 0;

    // Clip background
    g.setColour(juce::Colour(ampl::Theme::clipAudioBg).withAlpha(isMuted ? 0.45f : 1.0f));
    g.fillRoundedRectangle(area.toFloat(), ampl::Theme::clipRadius);

    if (isSelected)
    {
        g.setColour(juce::Colour(ampl::Theme::accentBlue).withAlpha(0.16f));
        g.fillRoundedRectangle(area.reduced(1).toFloat(), ampl::Theme::clipRadius);
    }

    // Waveform
    auto waveArea = area.reduced(1, 8);
    if (waveArea.getWidth() > 2 && clip.asset)
        paintWaveform(g, waveArea, clip);

    // Clip border
    juce::Colour borderColour = juce::Colour(ampl::Theme::clipAudioBorder);
    float borderThickness = 1.0f;
    if (isSelected)
    {
        borderColour = juce::Colour(ampl::Theme::accentBlue).brighter(0.2f);
        borderThickness = 2.0f;
    }
    else if (isHovered)
    {
        borderColour = juce::Colour(ampl::Theme::borderLight).brighter(0.2f);
        borderThickness = 1.5f;
    }
    g.setColour(borderColour);
    g.drawRoundedRectangle(area.toFloat(), ampl::Theme::clipRadius, borderThickness);

    // Clip name
    if (area.getWidth() > 40 && clip.asset)
    {
        g.setColour(juce::Colour(ampl::Theme::textPrimary).withAlpha(isMuted ? 0.65f : 0.88f));
        g.setFont(ampl::Theme::smallFont());
        g.drawText(clip.asset->fileName, area.reduced(4, 2), juce::Justification::topLeft, true);
    }

    if (isMuted)
    {
        g.setColour(juce::Colour(ampl::Theme::bgDeepest).withAlpha(0.25f));
        g.fillRoundedRectangle(area.toFloat(), ampl::Theme::clipRadius);
        if (area.getWidth() > 36)
        {
            auto badge = juce::Rectangle<int>(area.getX() + 4, area.getBottom() - 16, 28, 12);
            g.setColour(juce::Colour(ampl::Theme::accentOrange).withAlpha(0.85f));
            g.fillRoundedRectangle(badge.toFloat(), 3.0f);
            g.setColour(juce::Colours::white);
            g.setFont(ampl::Theme::smallFont());
            g.drawText("M", badge, juce::Justification::centred);
        }
    }

    if (isLocked)
    {
        g.setColour(juce::Colour(ampl::Theme::bgDeepest).withAlpha(0.22f));
        g.fillRoundedRectangle(area.toFloat(), ampl::Theme::clipRadius);
        if (area.getWidth() > 42)
        {
            auto badge = juce::Rectangle<int>(area.getRight() - 34, area.getBottom() - 16, 30, 12);
            g.setColour(juce::Colour(ampl::Theme::accentPurple).withAlpha(0.88f));
            g.fillRoundedRectangle(badge.toFloat(), 3.0f);
            g.setColour(juce::Colours::white);
            g.setFont(ampl::Theme::smallFont());
            g.drawText("L", badge, juce::Justification::centred);
        }
    }

    // Fade overlays
    if (clip.fadeInSamples > 0)
    {
        int fadeWidth =
            static_cast<int>(static_cast<double>(clip.fadeInSamples) * pixelsPerSample_);
        fadeWidth = std::min(fadeWidth, area.getWidth());
        juce::Path fadePath;
        fadePath.startNewSubPath(static_cast<float>(area.getX()),
                                 static_cast<float>(area.getBottom()));
        fadePath.lineTo(static_cast<float>(area.getX() + fadeWidth),
                        static_cast<float>(area.getY()));
        fadePath.lineTo(static_cast<float>(area.getX()), static_cast<float>(area.getY()));
        fadePath.closeSubPath();
        g.setColour(juce::Colour(0x40000000u));
        g.fillPath(fadePath);
    }

    if (clip.fadeOutSamples > 0)
    {
        int fadeWidth =
            static_cast<int>(static_cast<double>(clip.fadeOutSamples) * pixelsPerSample_);
        fadeWidth = std::min(fadeWidth, area.getWidth());
        juce::Path fadePath;
        fadePath.startNewSubPath(static_cast<float>(area.getRight()),
                                 static_cast<float>(area.getBottom()));
        fadePath.lineTo(static_cast<float>(area.getRight() - fadeWidth),
                        static_cast<float>(area.getY()));
        fadePath.lineTo(static_cast<float>(area.getRight()), static_cast<float>(area.getY()));
        fadePath.closeSubPath();
        g.setColour(juce::Colour(0x40000000u));
        g.fillPath(fadePath);
    }
}

void TimelineView::paintMidiClip(juce::Graphics &g, juce::Rectangle<int> area, const MidiClip &clip)
{
    const bool isSelected = (selectedClipId_ == clip.id);
    const bool isHovered = (hoveredClipId_ == clip.id);
    const auto clipKey = clip.id.toStdString();
    const bool isMuted = mutedClipIds_.count(clipKey) > 0;
    const bool isLocked = lockedClipIds_.count(clipKey) > 0;

    // MIDI clip background
    g.setColour(juce::Colour(ampl::Theme::clipMidiBg).withAlpha(isMuted ? 0.45f : 1.0f));
    g.fillRoundedRectangle(area.toFloat(), ampl::Theme::clipRadius);

    if (isSelected)
    {
        g.setColour(juce::Colour(ampl::Theme::accentCyan).withAlpha(0.18f));
        g.fillRoundedRectangle(area.reduced(1).toFloat(), ampl::Theme::clipRadius);
    }

    // Draw mini note rectangles
    if (!clip.notes.empty())
    {
        int loNote = clip.getLowestNote();
        int hiNote = clip.getHighestNote();
        int noteRange = std::max(hiNote - loNote + 1, 12);
        float noteH = static_cast<float>(area.getHeight() - 16) / static_cast<float>(noteRange);

        g.setColour(juce::Colour(ampl::Theme::clipMidiNote).withAlpha(isMuted ? 0.55f : 0.95f));

        for (const auto &note : clip.notes)
        {
            SampleCount absStart = clip.timelineStartSample + note.startSample;
            SampleCount absEnd = absStart + note.lengthSamples;

            int nx1 = sampleToPixelX(absStart);
            int nx2 = sampleToPixelX(absEnd);
            int ny = area.getBottom() - 8 -
                     static_cast<int>(static_cast<float>(note.noteNumber - loNote) * noteH);

            nx1 = std::max(nx1, area.getX() + 1);
            nx2 = std::min(nx2, area.getRight() - 1);

            if (nx2 > nx1)
            {
                int h = std::max(static_cast<int>(noteH) - 1, 1);
                g.fillRect(nx1, ny - h, nx2 - nx1, h);
            }
        }
    }

    // Clip border
    juce::Colour borderColour = juce::Colour(ampl::Theme::clipMidiBorder);
    float borderThickness = 1.0f;
    if (isSelected)
    {
        borderColour = juce::Colour(ampl::Theme::accentCyan).brighter(0.15f);
        borderThickness = 2.0f;
    }
    else if (isHovered)
    {
        borderColour = juce::Colour(ampl::Theme::borderLight).brighter(0.2f);
        borderThickness = 1.5f;
    }
    g.setColour(borderColour);
    g.drawRoundedRectangle(area.toFloat(), ampl::Theme::clipRadius, borderThickness);

    // Clip name
    if (area.getWidth() > 30)
    {
        g.setColour(juce::Colour(ampl::Theme::textPrimary).withAlpha(isMuted ? 0.65f : 0.88f));
        g.setFont(ampl::Theme::smallFont());
        g.drawText(clip.name, area.reduced(4, 2), juce::Justification::topLeft, true);
    }

    if (isMuted)
    {
        g.setColour(juce::Colour(ampl::Theme::bgDeepest).withAlpha(0.25f));
        g.fillRoundedRectangle(area.toFloat(), ampl::Theme::clipRadius);
        if (area.getWidth() > 36)
        {
            auto badge = juce::Rectangle<int>(area.getX() + 4, area.getBottom() - 16, 28, 12);
            g.setColour(juce::Colour(ampl::Theme::accentOrange).withAlpha(0.85f));
            g.fillRoundedRectangle(badge.toFloat(), 3.0f);
            g.setColour(juce::Colours::white);
            g.setFont(ampl::Theme::smallFont());
            g.drawText("M", badge, juce::Justification::centred);
        }
    }

    if (isLocked)
    {
        g.setColour(juce::Colour(ampl::Theme::bgDeepest).withAlpha(0.22f));
        g.fillRoundedRectangle(area.toFloat(), ampl::Theme::clipRadius);
        if (area.getWidth() > 42)
        {
            auto badge = juce::Rectangle<int>(area.getRight() - 34, area.getBottom() - 16, 30, 12);
            g.setColour(juce::Colour(ampl::Theme::accentPurple).withAlpha(0.88f));
            g.fillRoundedRectangle(badge.toFloat(), 3.0f);
            g.setColour(juce::Colours::white);
            g.setFont(ampl::Theme::smallFont());
            g.drawText("L", badge, juce::Justification::centred);
        }
    }
}

void TimelineView::paintWaveform(juce::Graphics &g, juce::Rectangle<int> area, const Clip &clip)
{
    if (!clip.asset || clip.asset->numChannels == 0)
        return;

    auto cacheKey = clip.id.toStdString();
    auto &cache = waveformCaches_[cacheKey];

    // Regenerate if zoom changed or clip changed
    if (cache.clipId != clip.id || std::abs(cache.pixelsPerSample - pixelsPerSample_) > 1e-10 ||
        static_cast<int>(cache.peaks.size()) != area.getWidth() * 2)
    {
        generateWaveformPeaks(clip, cache, area.getWidth());
    }

    if (cache.peaks.empty())
        return;

    float midY = static_cast<float>(area.getCentreY());
    float halfHeight = static_cast<float>(area.getHeight()) * 0.45f;

    g.setColour(juce::Colour(ampl::Theme::clipAudioWave));

    int numPeaks = static_cast<int>(cache.peaks.size()) / 2;
    for (int i = 0; i < numPeaks && i < area.getWidth(); ++i)
    {
        float minVal = cache.peaks[static_cast<size_t>(i * 2)];
        float maxVal = cache.peaks[static_cast<size_t>(i * 2 + 1)];

        float y1 = midY - maxVal * halfHeight;
        float y2 = midY - minVal * halfHeight;

        g.drawVerticalLine(area.getX() + i, y1, y2);
    }
}

void TimelineView::generateWaveformPeaks(const Clip &clip, WaveformCache &cache, int widthPixels)
{
    cache.clipId = clip.id;
    cache.pixelsPerSample = pixelsPerSample_;
    cache.peaks.clear();

    if (!clip.asset || clip.asset->numChannels == 0 || widthPixels <= 0)
        return;

    cache.peaks.resize(static_cast<size_t>(widthPixels * 2), 0.0f);

    const auto &channelData = clip.asset->channels[0]; // Use first channel
    double samplesPerPixel = 1.0 / pixelsPerSample_;

    for (int px = 0; px < widthPixels; ++px)
    {
        SampleCount startSample =
            clip.sourceStartSample +
            static_cast<SampleCount>(static_cast<double>(px) * samplesPerPixel);
        SampleCount endSample =
            clip.sourceStartSample +
            static_cast<SampleCount>(static_cast<double>(px + 1) * samplesPerPixel);

        startSample = std::max(startSample, SampleCount(0));
        endSample = std::min(endSample, clip.asset->lengthInSamples);

        float minVal = 0.0f, maxVal = 0.0f;
        for (SampleCount s = startSample; s < endSample; ++s)
        {
            float val = channelData[static_cast<size_t>(s)];
            minVal = std::min(minVal, val);
            maxVal = std::max(maxVal, val);
        }

        cache.peaks[static_cast<size_t>(px * 2)] = minVal;
        cache.peaks[static_cast<size_t>(px * 2 + 1)] = maxVal;
    }
}

void TimelineView::paintPlayhead(juce::Graphics &g)
{
    int x = sampleToPixelX(playheadPositionSamples_);
    if (x < kTrackHeaderWidth || x > getWidth())
        return;

    auto playheadColour = juce::Colour(ampl::Theme::playhead);

    g.setColour(playheadColour.withAlpha(0.25f));
    g.drawVerticalLine(x - 1, static_cast<float>(kRulerHeight), static_cast<float>(getHeight()));

    g.setColour(playheadColour);
    g.drawVerticalLine(x, static_cast<float>(kRulerHeight), static_cast<float>(getHeight()));

    // Playhead triangle on ruler
    juce::Path triangle;
    float tx = static_cast<float>(x);
    triangle.addTriangle(tx - 5.0f, 0.0f, tx + 5.0f, 0.0f, tx,
                         static_cast<float>(kRulerHeight - 2));
    g.setColour(playheadColour);
    g.fillPath(triangle);

    // Time bubble for legibility/discoverability
    double sr = session_.getSampleRate();
    if (sr <= 0.0)
        sr = 44100.0;
    double sec = static_cast<double>(playheadPositionSamples_) / sr;

    int mins = static_cast<int>(sec / 60.0);
    int secs = static_cast<int>(sec) % 60;
    int millis = static_cast<int>((sec - std::floor(sec)) * 1000.0);
    juce::String timeLabel = juce::String::formatted("%d:%02d.%03d", mins, secs, millis);

    auto bubble = juce::Rectangle<int>(x + 8, 4, 74, 16);
    if (bubble.getRight() > getWidth() - 4)
        bubble.setX(x - bubble.getWidth() - 8);
    bubble = bubble.withPosition(std::max(kTrackHeaderWidth + 4, bubble.getX()), bubble.getY());

    g.setColour(juce::Colour(ampl::Theme::bgDeepest).withAlpha(0.84f));
    g.fillRoundedRectangle(bubble.toFloat(), 4.0f);
    g.setColour(playheadColour.withAlpha(0.8f));
    g.drawRoundedRectangle(bubble.toFloat(), 4.0f, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.92f));
    g.setFont(ampl::Theme::smallFont());
    g.drawText(timeLabel, bubble, juce::Justification::centred, false);
}

void TimelineView::paintLoopRegion(juce::Graphics &g)
{
    const auto &loop = session_.getLoopRegion();
    if (!loop.enabled)
        return;

    int startX = sampleToPixelX(loop.startSample);
    int endX = sampleToPixelX(loop.endSample);

    if (endX < kTrackHeaderWidth || startX > getWidth())
        return;

    startX = std::max(startX, kTrackHeaderWidth);
    endX = std::min(endX, getWidth());

    // Semi-transparent overlay on ruler
    g.setColour(juce::Colour(0x302ecc71u));
    g.fillRect(startX, 0, endX - startX, kRulerHeight);

    // Loop boundary lines
    g.setColour(juce::Colour(0xff2ecc71));
    g.drawVerticalLine(startX, 0.0f, static_cast<float>(getHeight()));
    g.drawVerticalLine(endX, 0.0f, static_cast<float>(getHeight()));
}

void TimelineView::paintScrollbars(juce::Graphics &g)
{
    int numTracks = static_cast<int>(session_.getTracks().size());
    int totalContentH = numTracks * kTrackHeight;
    int viewH = getHeight() - kRulerHeight;

    // Vertical scrollbar (right edge, below ruler)
    if (totalContentH > viewH && viewH > 0)
    {
        int scrollTrackH = viewH;
        float thumbRatio = static_cast<float>(viewH) / static_cast<float>(totalContentH);
        int thumbH = std::max(20, static_cast<int>(scrollTrackH * thumbRatio));
        int maxScroll = totalContentH - viewH;
        float thumbPos = (maxScroll > 0)
                             ? static_cast<float>(verticalScrollOffset_) / static_cast<float>(maxScroll)
                             : 0.0f;
        int thumbY = kRulerHeight + static_cast<int>((scrollTrackH - thumbH) * thumbPos);

        // Scrollbar track
        auto trackRect = juce::Rectangle<int>(getWidth() - kScrollbarThickness, kRulerHeight,
                                              kScrollbarThickness, scrollTrackH);
        g.setColour(juce::Colour(ampl::Theme::bgDeepest).withAlpha(0.3f));
        g.fillRect(trackRect);

        // Scrollbar thumb
        auto thumbRect = juce::Rectangle<int>(getWidth() - kScrollbarThickness, thumbY,
                                              kScrollbarThickness, thumbH);
        g.setColour(juce::Colour(ampl::Theme::textDim).withAlpha(0.4f));
        g.fillRoundedRectangle(thumbRect.toFloat(), 4.0f);
    }

    // Horizontal scrollbar (bottom edge, right of track headers)
    // Show a simple indicator of horizontal scroll position
    int viewW = getWidth() - kTrackHeaderWidth - kScrollbarThickness;
    if (viewW > 0 && pixelsPerSample_ > 0.0)
    {
        // Estimate total timeline width
        SampleCount maxEnd = 0;
        for (const auto &track : session_.getTracks())
        {
            for (const auto &clip : track.clips)
                maxEnd = std::max(maxEnd, clip.getTimelineEndSample());
            for (const auto &mc : track.midiClips)
                maxEnd = std::max(maxEnd, mc.getTimelineEndSample());
        }

        if (maxEnd > 0)
        {
            int totalW = static_cast<int>(static_cast<double>(maxEnd) * pixelsPerSample_) + 200;
            if (totalW > viewW)
            {
                float thumbRatio = static_cast<float>(viewW) / static_cast<float>(totalW);
                int thumbW = std::max(20, static_cast<int>(viewW * thumbRatio));
                float scrollFrac = (totalW - viewW > 0)
                                       ? static_cast<float>(scrollPositionSamples_) * static_cast<float>(pixelsPerSample_) / static_cast<float>(totalW - viewW)
                                       : 0.0f;
                scrollFrac = juce::jlimit(0.0f, 1.0f, scrollFrac);
                int thumbX = kTrackHeaderWidth + static_cast<int>((viewW - thumbW) * scrollFrac);

                // Scrollbar track
                auto trackRect = juce::Rectangle<int>(kTrackHeaderWidth, getHeight() - kScrollbarThickness,
                                                      viewW, kScrollbarThickness);
                g.setColour(juce::Colour(ampl::Theme::bgDeepest).withAlpha(0.3f));
                g.fillRect(trackRect);

                // Scrollbar thumb
                auto thumbRect = juce::Rectangle<int>(thumbX, getHeight() - kScrollbarThickness,
                                                      thumbW, kScrollbarThickness);
                g.setColour(juce::Colour(ampl::Theme::textDim).withAlpha(0.4f));
                g.fillRoundedRectangle(thumbRect.toFloat(), 4.0f);
            }
        }
    }
}

// --- Mouse interaction ---

void TimelineView::mouseDown(const juce::MouseEvent &e)
{
    // Check if clicking on vertical scrollbar
    {
        int numTracks = static_cast<int>(session_.getTracks().size());
        int totalContentH = numTracks * kTrackHeight;
        int viewH = getHeight() - kRulerHeight;
        if (totalContentH > viewH && e.x >= getWidth() - kScrollbarThickness && e.y >= kRulerHeight)
        {
            scrollbarDrag_ = ScrollbarDrag::Vertical;
            scrollbarDragStartOffset_ = verticalScrollOffset_;
            scrollbarDragStartMouse_ = e.y;
            return;
        }
    }

    // Check if clicking on horizontal scrollbar
    if (e.y >= getHeight() - kScrollbarThickness && e.x >= kTrackHeaderWidth)
    {
        scrollbarDrag_ = ScrollbarDrag::Horizontal;
        scrollbarDragStartOffset_ = static_cast<int>(scrollPositionSamples_);
        scrollbarDragStartMouse_ = e.x;
        return;
    }

    int trackIdx = trackIndexAtY(e.y);

    if (e.x < kTrackHeaderWidth)
    {
        if (trackIdx < 0)
            return;

        selectedTrackIndex_ = trackIdx;
        selectedClipId_.clear();

        int laneTopOnScreen = kRulerHeight + trackIdx * kTrackHeight - verticalScrollOffset_;
        auto laneHeader = juce::Rectangle<int>(0, laneTopOnScreen, kTrackHeaderWidth, kTrackHeight);
        laneHeader.removeFromLeft(3);
        auto actionRects = getHeaderActionRects(laneHeader);

        if (auto *track = session_.getTrack(trackIdx))
        {
            if (actionRects.mute.contains(e.getPosition()))
            {
                const bool newMuted = !track->muted;
                if (onTrackMuteRequested)
                    onTrackMuteRequested(trackIdx, newMuted);
                else
                {
                    track->muted = newMuted;
                    if (onSessionChanged)
                        onSessionChanged();
                }
                repaintPending_ = true;
                repaint();
                return;
            }

            if (actionRects.solo.contains(e.getPosition()))
            {
                bool newSolo = !track->solo;
                if (onTrackSoloRequested)
                    onTrackSoloRequested(trackIdx, newSolo);
                else
                {
                    if (newSolo)
                    {
                        for (auto &t : session_.getTracks())
                            t.solo = false;
                    }
                    track->solo = newSolo;
                    if (onSessionChanged)
                        onSessionChanged();
                }
                repaintPending_ = true;
                repaint();
                return;
            }
        }

        repaintPending_ = true;
        repaint();
        return;
    }

    if (e.y < kRulerHeight)
    {
        // Click on ruler = seek
        dragMode_ = DragMode::Seeking;
        SampleCount seekPos = pixelXToSample(e.x);
        seekPos = std::max(SampleCount(0), seekPos);
        if (onSeek)
            onSeek(seekPos);
        return;
    }

    if (trackIdx < 0)
        return;

    selectedTrackIndex_ = trackIdx;

    // Check if clicking on a clip
    const auto &tracks = session_.getTracks();
    if (trackIdx < static_cast<int>(tracks.size()))
    {
        const auto &track = tracks[static_cast<size_t>(trackIdx)];
        SampleCount clickSample = pixelXToSample(e.x);

        if (track.isAudio())
        {
            for (const auto &clip : track.clips)
            {
                if (clickSample >= clip.timelineStartSample &&
                    clickSample < clip.getTimelineEndSample())
                {
                    selectedClipId_ = clip.id;
                    if (!hasShownClipActionHint_)
                    {
                        hasShownClipActionHint_ = true;
                        fullHintVisibleUntilMs_ = juce::Time::getMillisecondCounter() + 4500;
                    }

                    auto key = clip.id.toStdString();
                    if (e.mods.isAltDown())
                    {
                        if (mutedClipIds_.count(key) > 0)
                            mutedClipIds_.erase(key);
                        else
                            mutedClipIds_.insert(key);
                        repaintPending_ = true;
                        repaint();
                        return;
                    }

                    if (e.mods.isShiftDown())
                    {
                        if (lockedClipIds_.count(key) > 0)
                            lockedClipIds_.erase(key);
                        else
                            lockedClipIds_.insert(key);
                        repaintPending_ = true;
                        repaint();
                        return;
                    }

                    if (lockedClipIds_.count(key) > 0)
                    {
                        repaintPending_ = true;
                        repaint();
                        return;
                    }

                    // Check for trim handles (first/last 8 pixels of clip)
                    int clipStartX = sampleToPixelX(clip.timelineStartSample);
                    int clipEndX = sampleToPixelX(clip.getTimelineEndSample());

                    if (e.x < clipStartX + 8)
                        dragMode_ = DragMode::TrimLeft;
                    else if (e.x > clipEndX - 8)
                        dragMode_ = DragMode::TrimRight;
                    else
                        dragMode_ = DragMode::MovingClip;

                    dragClipId_ = clip.id;
                    dragStartSample_ = clickSample;
                    dragClipOriginalStart_ = clip.timelineStartSample;
                    dragTrackIndex_ = trackIdx;
                    repaintPending_ = true;
                    repaint();
                    return;
                }
            }
        }
        else
        {
            for (const auto &mclip : track.midiClips)
            {
                if (clickSample >= mclip.timelineStartSample &&
                    clickSample < mclip.getTimelineEndSample())
                {
                    selectedClipId_ = mclip.id;
                    if (!hasShownClipActionHint_)
                    {
                        hasShownClipActionHint_ = true;
                        fullHintVisibleUntilMs_ = juce::Time::getMillisecondCounter() + 4500;
                    }
                    auto key = mclip.id.toStdString();

                    if (e.mods.isAltDown())
                    {
                        if (mutedClipIds_.count(key) > 0)
                            mutedClipIds_.erase(key);
                        else
                            mutedClipIds_.insert(key);
                    }
                    else if (e.mods.isShiftDown())
                    {
                        if (lockedClipIds_.count(key) > 0)
                            lockedClipIds_.erase(key);
                        else
                            lockedClipIds_.insert(key);
                    }

                    repaintPending_ = true;
                    repaint();
                    return;
                }
            }
        }
    }

    // Click on empty area = seek
    selectedClipId_.clear();
    dragMode_ = DragMode::Seeking;
    SampleCount seekPos = std::max(SampleCount(0), pixelXToSample(e.x));
    if (onSeek)
        onSeek(seekPos);
    repaintPending_ = true;
    repaint();
}

void TimelineView::mouseDrag(const juce::MouseEvent &e)
{
    // Scrollbar dragging
    if (scrollbarDrag_ == ScrollbarDrag::Vertical)
    {
        int numTracks = static_cast<int>(session_.getTracks().size());
        int totalContentH = numTracks * kTrackHeight;
        int viewH = getHeight() - kRulerHeight;
        int maxScroll = std::max(1, totalContentH - viewH);
        float ratio = static_cast<float>(totalContentH) / static_cast<float>(viewH);
        int delta = static_cast<int>(static_cast<float>(e.y - scrollbarDragStartMouse_) * ratio);
        setVerticalScrollOffset(scrollbarDragStartOffset_ + delta);
        return;
    }
    if (scrollbarDrag_ == ScrollbarDrag::Horizontal)
    {
        // Estimate total timeline width for mapping
        SampleCount maxEnd = 0;
        for (const auto &track : session_.getTracks())
            for (const auto &clip : track.clips)
                maxEnd = std::max(maxEnd, clip.getTimelineEndSample());
        int viewW = getWidth() - kTrackHeaderWidth - kScrollbarThickness;
        if (viewW > 0 && maxEnd > 0)
        {
            int totalW = static_cast<int>(static_cast<double>(maxEnd) * pixelsPerSample_) + 200;
            float ratio = static_cast<float>(totalW) / static_cast<float>(viewW);
            int deltaPixels = static_cast<int>(static_cast<float>(e.x - scrollbarDragStartMouse_) * ratio);
            SampleCount deltaSamples = static_cast<SampleCount>(static_cast<double>(deltaPixels) / pixelsPerSample_);
            setScrollPositionSamples(std::max<SampleCount>(0, static_cast<SampleCount>(scrollbarDragStartOffset_) + deltaSamples));
        }
        return;
    }

    if (dragMode_ == DragMode::Seeking)
    {
        SampleCount seekPos = std::max(SampleCount(0), pixelXToSample(e.x));
        if (onSeek)
            onSeek(seekPos);
    }
    else if (dragMode_ == DragMode::MovingClip)
    {
        if (lockedClipIds_.count(dragClipId_.toStdString()) > 0)
            return;

        SampleCount currentSample = pixelXToSample(e.x);
        SampleCount delta = currentSample - dragStartSample_;
        SampleCount newStart = std::max(SampleCount(0), dragClipOriginalStart_ + delta);

        if (auto *clip = session_.findClip(dragClipId_))
        {
            clip->timelineStartSample = newStart;
            if (onSessionChanged)
                onSessionChanged();
            repaintPending_ = true;
        }
    }
}

void TimelineView::mouseUp(const juce::MouseEvent & /*e*/)
{
    scrollbarDrag_ = ScrollbarDrag::None;
    dragMode_ = DragMode::None;
    dragClipId_ = {};
    repaintPending_ = true;
    repaint();
}

void TimelineView::mouseMove(const juce::MouseEvent &e)
{
    juce::String newHovered;

    if (e.y >= kRulerHeight && e.x >= kTrackHeaderWidth)
    {
        int trackIdx = trackIndexAtY(e.y);
        if (trackIdx >= 0)
        {
            const auto &tracks = session_.getTracks();
            const auto &track = tracks[static_cast<size_t>(trackIdx)];
            SampleCount sample = pixelXToSample(e.x);

            if (track.isAudio())
            {
                for (const auto &clip : track.clips)
                {
                    if (sample >= clip.timelineStartSample && sample < clip.getTimelineEndSample())
                    {
                        newHovered = clip.id;
                        break;
                    }
                }
            }
            else
            {
                for (const auto &clip : track.midiClips)
                {
                    if (sample >= clip.timelineStartSample && sample < clip.getTimelineEndSample())
                    {
                        newHovered = clip.id;
                        break;
                    }
                }
            }
        }
    }

    if (newHovered != hoveredClipId_)
    {
        hoveredClipId_ = newHovered;
        if (hoveredClipId_.isNotEmpty() && !hasShownClipActionHint_)
        {
            hasShownClipActionHint_ = true;
            fullHintVisibleUntilMs_ = juce::Time::getMillisecondCounter() + 4500;
        }
        repaintPending_ = true;
        repaint();
    }
}

void TimelineView::mouseExit(const juce::MouseEvent &)
{
    if (hoveredClipId_.isNotEmpty())
    {
        hoveredClipId_.clear();
        repaintPending_ = true;
        repaint();
    }
}

void TimelineView::mouseDoubleClick(const juce::MouseEvent &e)
{
    if (e.x < kTrackHeaderWidth || e.y < kRulerHeight)
        return;

    int trackIdx = trackIndexAtY(e.y);
    if (trackIdx < 0)
        return;

    const auto &tracks = session_.getTracks();
    if (trackIdx >= static_cast<int>(tracks.size()))
        return;

    const auto &track = tracks[static_cast<size_t>(trackIdx)];
    SampleCount clickSample = pixelXToSample(e.x);

    if (track.isMidi())
    {
        for (const auto &mc : track.midiClips)
        {
            if (clickSample >= mc.timelineStartSample && clickSample < mc.getTimelineEndSample())
            {
                if (onClipDoubleClicked)
                    onClipDoubleClicked(trackIdx, mc.id, true);
                return;
            }
        }
    }
    else
    {
        for (const auto &clip : track.clips)
        {
            if (clickSample >= clip.timelineStartSample &&
                clickSample < clip.getTimelineEndSample())
            {
                if (onClipDoubleClicked)
                    onClipDoubleClicked(trackIdx, clip.id, false);
                return;
            }
        }
    }
}

void TimelineView::mouseWheelMove(const juce::MouseEvent &e, const juce::MouseWheelDetails &wheel)
{
    if (e.mods.isCommandDown())
    {
        // Cmd+scroll = zoom horizontally
        SampleCount sampleAtMouse = pixelXToSample(e.x);

        if (wheel.deltaY > 0)
            pixelsPerSample_ = std::min(pixelsPerSample_ * 1.2, 1.0);
        else
            pixelsPerSample_ = std::max(pixelsPerSample_ / 1.2, 0.0001);

        // Keep the sample under the mouse at the same pixel position
        int newPixelX = sampleToPixelX(sampleAtMouse);
        SampleCount correction =
            static_cast<SampleCount>(static_cast<double>(newPixelX - e.x) / pixelsPerSample_);
        scrollPositionSamples_ += correction;
        scrollPositionSamples_ = std::max(SampleCount(0), scrollPositionSamples_);
    }
    else if (e.mods.isShiftDown() || wheel.deltaX != 0.0f)
    {
        // Shift+scroll or horizontal trackpad gesture = horizontal scroll
        float delta = (wheel.deltaX != 0.0f) ? wheel.deltaX : wheel.deltaY;
        double scrollAmount = -delta * 5000.0 / pixelsPerSample_;
        scrollPositionSamples_ += static_cast<SampleCount>(scrollAmount);
        scrollPositionSamples_ = std::max(SampleCount(0), scrollPositionSamples_);
    }
    else
    {
        // Normal scroll = vertical scroll through tracks
        int scrollAmount = static_cast<int>(-wheel.deltaY * 200.0);
        setVerticalScrollOffset(verticalScrollOffset_ + scrollAmount);
    }

    repaint();
    repaintPending_ = false;
}

void TimelineView::resized()
{
    // Invalidate waveform caches on resize
    waveformCaches_.clear();
    clampVerticalScroll();
    repaintPending_ = true;
}

} // namespace ampl
