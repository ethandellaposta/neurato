#include "ui/TimelineView.h"
#include <cmath>

namespace neurato {

TimelineView::TimelineView(AudioEngine& engine, Session& session)
    : engine_(engine), session_(session)
{
}

TimelineView::~TimelineView() = default;

void TimelineView::handleAudioMessage(const AudioToUIMessage& msg)
{
    switch (msg.type)
    {
        case AudioToUIMessage::Type::PlayheadPosition:
            playheadPositionSamples_ = msg.intValue;
            break;
        case AudioToUIMessage::Type::PeakLevel:
        case AudioToUIMessage::Type::TransportStateChanged:
            break;
    }
}

void TimelineView::updateDisplay()
{
    repaint();
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
    if (y < kRulerHeight) return -1;
    int trackY = y - kRulerHeight;
    int index = trackY / kTrackHeight;
    if (index >= static_cast<int>(session_.getTracks().size()))
        return -1;
    return index;
}

// --- Zoom ---

void TimelineView::zoomIn()
{
    pixelsPerSample_ = std::min(pixelsPerSample_ * 1.5, 1.0);
    repaint();
}

void TimelineView::zoomOut()
{
    pixelsPerSample_ = std::max(pixelsPerSample_ / 1.5, 0.0001);
    repaint();
}

void TimelineView::zoomToFit()
{
    SampleCount maxEnd = 0;
    for (const auto& track : session_.getTracks())
        for (const auto& clip : track.clips)
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
    repaint();
}

void TimelineView::setScrollPositionSamples(SampleCount pos)
{
    scrollPositionSamples_ = std::max(SampleCount(0), pos);
    repaint();
}

// --- Paint ---

void TimelineView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0a0a1a));

    auto bounds = getLocalBounds();

    // Ruler at top
    auto rulerArea = bounds.removeFromTop(kRulerHeight);
    paintRuler(g, rulerArea);

    // Track lanes
    const auto& tracks = session_.getTracks();
    for (int i = 0; i < static_cast<int>(tracks.size()); ++i)
    {
        if (bounds.getHeight() <= 0) break;
        auto laneArea = bounds.removeFromTop(kTrackHeight);
        paintTrackLane(g, laneArea, tracks[static_cast<size_t>(i)], i);
    }

    // Empty area below tracks
    if (bounds.getHeight() > 0)
    {
        g.setColour(juce::Colour(0xff080818));
        g.fillRect(bounds);
    }

    // Loop region overlay
    paintLoopRegion(g);

    // Playhead (drawn last, on top of everything)
    paintPlayhead(g);
}

void TimelineView::paintRuler(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(juce::Colour(0xff1a1a2e));
    g.fillRect(area);

    // Draw beat/bar grid lines and labels
    double bpm = session_.getBpm();
    double sr = session_.getSampleRate();
    if (bpm <= 0 || sr <= 0) return;

    double samplesPerBeat = (60.0 / bpm) * sr;
    int timeSigNum = session_.getTimeSigNumerator();
    double samplesPerBar = samplesPerBeat * timeSigNum;

    // Determine grid resolution based on zoom level
    double pixelsPerBar = samplesPerBar * pixelsPerSample_;
    double gridSamples;
    juce::String gridLabel;

    if (pixelsPerBar > 200)
    {
        gridSamples = samplesPerBeat;  // Show every beat
    }
    else if (pixelsPerBar > 50)
    {
        gridSamples = samplesPerBar;   // Show every bar
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

    for (SampleCount s = firstGrid; ; s += static_cast<SampleCount>(gridSamples))
    {
        int x = sampleToPixelX(s);
        if (x > area.getRight()) break;
        if (x < kTrackHeaderWidth) continue;

        // Determine bar/beat number
        double beatNum = static_cast<double>(s) / samplesPerBeat;
        int bar = static_cast<int>(beatNum / timeSigNum) + 1;
        int beat = static_cast<int>(std::fmod(beatNum, timeSigNum)) + 1;

        bool isBar = (beat == 1);

        g.setColour(isBar ? juce::Colour(0xff555577) : juce::Colour(0xff333355));
        g.drawVerticalLine(x, static_cast<float>(area.getY()),
                           static_cast<float>(area.getBottom()));

        // Label
        g.setColour(juce::Colour(0xffaaaacc));
        if (isBar || pixelsPerBar > 200)
        {
            juce::String label = juce::String(bar) + "." + juce::String(beat);
            g.drawText(label, x + 2, area.getY(), 40, area.getHeight(),
                       juce::Justification::centredLeft);
        }
    }

    // Track header area label
    g.setColour(juce::Colour(0xff1a1a2e));
    g.fillRect(area.withWidth(kTrackHeaderWidth));
    g.setColour(juce::Colour(0xff333355));
    g.drawVerticalLine(kTrackHeaderWidth, static_cast<float>(area.getY()),
                       static_cast<float>(area.getBottom()));

    // Bottom border
    g.setColour(juce::Colour(0xff333355));
    g.drawHorizontalLine(area.getBottom() - 1, 0.0f, static_cast<float>(area.getRight()));
}

void TimelineView::paintTrackLane(juce::Graphics& g, juce::Rectangle<int> area,
                                   const TrackState& track, int trackIndex)
{
    // Track header
    auto headerArea = area.removeFromLeft(kTrackHeaderWidth);
    g.setColour(juce::Colour(Theme::bgSurface));
    g.fillRect(headerArea);

    // Track type indicator
    auto typeBar = headerArea.removeFromLeft(3);
    g.setColour(juce::Colour(track.isMidi() ? Theme::accentCyan : Theme::accentBlue));
    g.fillRect(typeBar);

    // Track name
    g.setColour(juce::Colour(track.muted ? Theme::textDisabled : Theme::textPrimary));
    g.setFont(Theme::headingFont());
    g.drawText(track.name, headerArea.reduced(6, 4), juce::Justification::topLeft);

    // Track info
    g.setFont(Theme::smallFont());
    g.setColour(juce::Colour(Theme::textDim));
    juce::String typeStr = track.isMidi() ? "MIDI" : "Audio";
    int clipCount = track.isMidi() ? static_cast<int>(track.midiClips.size())
                                   : static_cast<int>(track.clips.size());
    juce::String info = typeStr + " | " + juce::String(clipCount) + " clip" +
                        (clipCount != 1 ? "s" : "");
    if (track.muted) info += " [M]";
    if (track.solo) info += " [S]";
    g.drawText(info, headerArea.reduced(6, 4), juce::Justification::bottomLeft);

    // Header border
    g.setColour(juce::Colour(Theme::border));
    g.drawVerticalLine(headerArea.getRight(), static_cast<float>(area.getY()),
                       static_cast<float>(area.getBottom()));

    // Lane background
    bool isEven = (trackIndex % 2 == 0);
    g.setColour(juce::Colour(isEven ? Theme::trackLaneEven : Theme::trackLaneOdd));
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

        for (SampleCount s = firstBar; ; s += static_cast<SampleCount>(samplesPerBar))
        {
            int x = sampleToPixelX(s);
            if (x > area.getRight()) break;
            if (x < area.getX()) continue;

            g.setColour(juce::Colour(Theme::pianoGrid));
            g.drawVerticalLine(x, static_cast<float>(area.getY()),
                               static_cast<float>(area.getBottom()));
        }
    }

    // Draw clips (audio or MIDI depending on track type)
    if (track.isAudio())
    {
        for (const auto& clip : track.clips)
        {
            int clipX = sampleToPixelX(clip.timelineStartSample);
            int clipEndX = sampleToPixelX(clip.getTimelineEndSample());
            int clipWidth = clipEndX - clipX;

            if (clipEndX < area.getX() || clipX > area.getRight())
                continue;

            auto clipArea = juce::Rectangle<int>(clipX, area.getY() + 2,
                                                  clipWidth, area.getHeight() - 4);
            clipArea = clipArea.getIntersection(area);

            if (!clipArea.isEmpty())
                paintClip(g, clipArea, clip);
        }
    }
    else
    {
        for (const auto& mclip : track.midiClips)
        {
            int clipX = sampleToPixelX(mclip.timelineStartSample);
            int clipEndX = sampleToPixelX(mclip.getTimelineEndSample());
            int clipWidth = clipEndX - clipX;

            if (clipEndX < area.getX() || clipX > area.getRight())
                continue;

            auto clipArea = juce::Rectangle<int>(clipX, area.getY() + 2,
                                                  clipWidth, area.getHeight() - 4);
            clipArea = clipArea.getIntersection(area);

            if (!clipArea.isEmpty())
                paintMidiClip(g, clipArea, mclip);
        }
    }

    // Lane bottom border
    g.setColour(juce::Colour(Theme::border));
    g.drawHorizontalLine(area.getBottom() - 1,
                          static_cast<float>(headerArea.getX()),
                          static_cast<float>(area.getRight()));
}

void TimelineView::paintClip(juce::Graphics& g, juce::Rectangle<int> area, const Clip& clip)
{
    // Clip background
    g.setColour(juce::Colour(Theme::clipAudioBg));
    g.fillRoundedRectangle(area.toFloat(), Theme::clipRadius);

    // Waveform
    auto waveArea = area.reduced(1, 8);
    if (waveArea.getWidth() > 2 && clip.asset)
        paintWaveform(g, waveArea, clip);

    // Clip border
    g.setColour(juce::Colour(Theme::clipAudioBorder));
    g.drawRoundedRectangle(area.toFloat(), Theme::clipRadius, 1.0f);

    // Clip name
    if (area.getWidth() > 40 && clip.asset)
    {
        g.setColour(juce::Colour(Theme::textPrimary).withAlpha(0.85f));
        g.setFont(Theme::smallFont());
        g.drawText(clip.asset->fileName, area.reduced(4, 2),
                   juce::Justification::topLeft, true);
    }

    // Fade overlays
    if (clip.fadeInSamples > 0)
    {
        int fadeWidth = static_cast<int>(static_cast<double>(clip.fadeInSamples) * pixelsPerSample_);
        fadeWidth = std::min(fadeWidth, area.getWidth());
        juce::Path fadePath;
        fadePath.startNewSubPath(static_cast<float>(area.getX()), static_cast<float>(area.getBottom()));
        fadePath.lineTo(static_cast<float>(area.getX() + fadeWidth), static_cast<float>(area.getY()));
        fadePath.lineTo(static_cast<float>(area.getX()), static_cast<float>(area.getY()));
        fadePath.closeSubPath();
        g.setColour(juce::Colour(0x40000000u));
        g.fillPath(fadePath);
    }

    if (clip.fadeOutSamples > 0)
    {
        int fadeWidth = static_cast<int>(static_cast<double>(clip.fadeOutSamples) * pixelsPerSample_);
        fadeWidth = std::min(fadeWidth, area.getWidth());
        juce::Path fadePath;
        fadePath.startNewSubPath(static_cast<float>(area.getRight()), static_cast<float>(area.getBottom()));
        fadePath.lineTo(static_cast<float>(area.getRight() - fadeWidth), static_cast<float>(area.getY()));
        fadePath.lineTo(static_cast<float>(area.getRight()), static_cast<float>(area.getY()));
        fadePath.closeSubPath();
        g.setColour(juce::Colour(0x40000000u));
        g.fillPath(fadePath);
    }
}

void TimelineView::paintMidiClip(juce::Graphics& g, juce::Rectangle<int> area, const MidiClip& clip)
{
    // MIDI clip background
    g.setColour(juce::Colour(Theme::clipMidiBg));
    g.fillRoundedRectangle(area.toFloat(), Theme::clipRadius);

    // Draw mini note rectangles
    if (!clip.notes.empty())
    {
        int loNote = clip.getLowestNote();
        int hiNote = clip.getHighestNote();
        int noteRange = std::max(hiNote - loNote + 1, 12);
        float noteH = static_cast<float>(area.getHeight() - 16) / static_cast<float>(noteRange);

        g.setColour(juce::Colour(Theme::clipMidiNote));

        for (const auto& note : clip.notes)
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
    g.setColour(juce::Colour(Theme::clipMidiBorder));
    g.drawRoundedRectangle(area.toFloat(), Theme::clipRadius, 1.0f);

    // Clip name
    if (area.getWidth() > 30)
    {
        g.setColour(juce::Colour(Theme::textPrimary).withAlpha(0.85f));
        g.setFont(Theme::smallFont());
        g.drawText(clip.name, area.reduced(4, 2), juce::Justification::topLeft, true);
    }
}

void TimelineView::paintWaveform(juce::Graphics& g, juce::Rectangle<int> area, const Clip& clip)
{
    if (!clip.asset || clip.asset->numChannels == 0)
        return;

    auto cacheKey = clip.id.toStdString();
    auto& cache = waveformCaches_[cacheKey];

    // Regenerate if zoom changed or clip changed
    if (cache.clipId != clip.id ||
        std::abs(cache.pixelsPerSample - pixelsPerSample_) > 1e-10 ||
        static_cast<int>(cache.peaks.size()) != area.getWidth() * 2)
    {
        generateWaveformPeaks(clip, cache, area.getWidth());
    }

    if (cache.peaks.empty())
        return;

    float midY = static_cast<float>(area.getCentreY());
    float halfHeight = static_cast<float>(area.getHeight()) * 0.45f;

    g.setColour(juce::Colour(Theme::clipAudioWave));

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

void TimelineView::generateWaveformPeaks(const Clip& clip, WaveformCache& cache, int widthPixels)
{
    cache.clipId = clip.id;
    cache.pixelsPerSample = pixelsPerSample_;
    cache.peaks.clear();

    if (!clip.asset || clip.asset->numChannels == 0 || widthPixels <= 0)
        return;

    cache.peaks.resize(static_cast<size_t>(widthPixels * 2), 0.0f);

    const auto& channelData = clip.asset->channels[0]; // Use first channel
    double samplesPerPixel = 1.0 / pixelsPerSample_;

    for (int px = 0; px < widthPixels; ++px)
    {
        SampleCount startSample = clip.sourceStartSample +
            static_cast<SampleCount>(static_cast<double>(px) * samplesPerPixel);
        SampleCount endSample = clip.sourceStartSample +
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

void TimelineView::paintPlayhead(juce::Graphics& g)
{
    int x = sampleToPixelX(playheadPositionSamples_);
    if (x < kTrackHeaderWidth || x > getWidth())
        return;

    g.setColour(juce::Colour(Theme::playhead));
    g.drawVerticalLine(x, static_cast<float>(kRulerHeight),
                       static_cast<float>(getHeight()));

    // Playhead triangle on ruler
    juce::Path triangle;
    float tx = static_cast<float>(x);
    triangle.addTriangle(tx - 5.0f, 0.0f, tx + 5.0f, 0.0f,
                          tx, static_cast<float>(kRulerHeight - 2));
    g.setColour(juce::Colour(Theme::playhead));
    g.fillPath(triangle);
}

void TimelineView::paintLoopRegion(juce::Graphics& g)
{
    const auto& loop = session_.getLoopRegion();
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

// --- Mouse interaction ---

void TimelineView::mouseDown(const juce::MouseEvent& e)
{
    if (e.x < kTrackHeaderWidth)
        return;

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

    int trackIdx = trackIndexAtY(e.y);
    if (trackIdx < 0)
        return;

    // Check if clicking on a clip
    const auto& tracks = session_.getTracks();
    if (trackIdx < static_cast<int>(tracks.size()))
    {
        const auto& track = tracks[static_cast<size_t>(trackIdx)];
        SampleCount clickSample = pixelXToSample(e.x);

        for (const auto& clip : track.clips)
        {
            if (clickSample >= clip.timelineStartSample &&
                clickSample < clip.getTimelineEndSample())
            {
                // Check for trim handles (first/last 8 pixels of clip)
                int clipStartX = sampleToPixelX(clip.timelineStartSample);
                int clipEndX = sampleToPixelX(clip.getTimelineEndSample());

                if (e.x < clipStartX + 8)
                {
                    dragMode_ = DragMode::TrimLeft;
                }
                else if (e.x > clipEndX - 8)
                {
                    dragMode_ = DragMode::TrimRight;
                }
                else
                {
                    dragMode_ = DragMode::MovingClip;
                }

                dragClipId_ = clip.id;
                dragStartSample_ = clickSample;
                dragClipOriginalStart_ = clip.timelineStartSample;
                dragTrackIndex_ = trackIdx;
                return;
            }
        }
    }

    // Click on empty area = seek
    dragMode_ = DragMode::Seeking;
    SampleCount seekPos = std::max(SampleCount(0), pixelXToSample(e.x));
    if (onSeek)
        onSeek(seekPos);
}

void TimelineView::mouseDrag(const juce::MouseEvent& e)
{
    if (dragMode_ == DragMode::Seeking)
    {
        SampleCount seekPos = std::max(SampleCount(0), pixelXToSample(e.x));
        if (onSeek)
            onSeek(seekPos);
    }
    else if (dragMode_ == DragMode::MovingClip)
    {
        SampleCount currentSample = pixelXToSample(e.x);
        SampleCount delta = currentSample - dragStartSample_;
        SampleCount newStart = std::max(SampleCount(0), dragClipOriginalStart_ + delta);

        if (auto* clip = session_.findClip(dragClipId_))
        {
            clip->timelineStartSample = newStart;
            if (onSessionChanged)
                onSessionChanged();
        }
    }
}

void TimelineView::mouseUp(const juce::MouseEvent& /*e*/)
{
    dragMode_ = DragMode::None;
    dragClipId_ = {};
}

void TimelineView::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (e.x < kTrackHeaderWidth || e.y < kRulerHeight)
        return;

    int trackIdx = trackIndexAtY(e.y);
    if (trackIdx < 0) return;

    const auto& tracks = session_.getTracks();
    if (trackIdx >= static_cast<int>(tracks.size())) return;

    const auto& track = tracks[static_cast<size_t>(trackIdx)];
    SampleCount clickSample = pixelXToSample(e.x);

    if (track.isMidi())
    {
        for (const auto& mc : track.midiClips)
        {
            if (clickSample >= mc.timelineStartSample &&
                clickSample < mc.getTimelineEndSample())
            {
                if (onClipDoubleClicked)
                    onClipDoubleClicked(trackIdx, mc.id, true);
                return;
            }
        }
    }
    else
    {
        for (const auto& clip : track.clips)
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

void TimelineView::mouseWheelMove(const juce::MouseEvent& e,
                                    const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCommandDown())
    {
        // Cmd+scroll = zoom
        SampleCount sampleAtMouse = pixelXToSample(e.x);

        if (wheel.deltaY > 0)
            pixelsPerSample_ = std::min(pixelsPerSample_ * 1.2, 1.0);
        else
            pixelsPerSample_ = std::max(pixelsPerSample_ / 1.2, 0.0001);

        // Keep the sample under the mouse at the same pixel position
        int newPixelX = sampleToPixelX(sampleAtMouse);
        SampleCount correction = static_cast<SampleCount>(
            static_cast<double>(newPixelX - e.x) / pixelsPerSample_);
        scrollPositionSamples_ += correction;
        scrollPositionSamples_ = std::max(SampleCount(0), scrollPositionSamples_);
    }
    else
    {
        // Scroll horizontally
        double scrollAmount = -wheel.deltaY * 5000.0 / pixelsPerSample_;
        scrollPositionSamples_ += static_cast<SampleCount>(scrollAmount);
        scrollPositionSamples_ = std::max(SampleCount(0), scrollPositionSamples_);
    }

    repaint();
}

void TimelineView::resized()
{
    // Invalidate waveform caches on resize
    waveformCaches_.clear();
}

} // namespace neurato
