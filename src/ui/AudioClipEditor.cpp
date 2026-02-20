#include "ui/AudioClipEditor.h"
#include "commands/ClipCommands.h"
#include <cmath>

namespace neurato {

AudioClipEditor::AudioClipEditor(Session& session, CommandManager& commandManager)
    : session_(session), commandManager_(commandManager)
{
    closeBtn_.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::bgElevated));
    closeBtn_.onClick = [this] { if (onCloseRequested) onCloseRequested(); };
    addAndMakeVisible(closeBtn_);
}

void AudioClipEditor::setClip(int trackIndex, const juce::String& clipId)
{
    trackIndex_ = trackIndex;
    clipId_ = clipId;
    waveformPeaks_.clear();
    cachedPPS_ = 0.0;

    // Auto-zoom to fit clip
    if (auto* clip = getClip())
    {
        int availW = getWidth() - 20;
        if (availW > 0 && clip->sourceLengthSamples > 0)
        {
            pixelsPerSample_ = static_cast<double>(availW) / static_cast<double>(clip->sourceLengthSamples);
            scrollSamples_ = clip->timelineStartSample;
        }
    }
    repaint();
}

void AudioClipEditor::clearClip()
{
    trackIndex_ = -1;
    clipId_ = {};
    waveformPeaks_.clear();
    repaint();
}

Clip* AudioClipEditor::getClip()
{
    if (trackIndex_ < 0) return nullptr;
    return session_.findClip(clipId_);
}

const Clip* AudioClipEditor::getClip() const
{
    if (trackIndex_ < 0) return nullptr;
    // const_cast needed since Session::findClip is non-const
    return const_cast<Session&>(session_).findClip(clipId_);
}

void AudioClipEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(Theme::bgBase));

    auto area = getLocalBounds();
    auto toolbar = area.removeFromTop(kToolbarHeight);
    auto infoBar = area.removeFromBottom(kInfoHeight);

    // Toolbar
    g.setColour(juce::Colour(Theme::toolbarBg));
    g.fillRect(toolbar);

    auto* clip = getClip();
    if (clip && clip->asset)
    {
        g.setColour(juce::Colour(Theme::textSecondary));
        g.setFont(Theme::bodyFont());
        g.drawText(clip->asset->fileName, toolbar.reduced(8, 0).withTrimmedRight(40),
                   juce::Justification::centredLeft);
    }
    else
    {
        g.setColour(juce::Colour(Theme::textDim));
        g.setFont(Theme::bodyFont());
        g.drawText("No clip selected", toolbar.reduced(8, 0), juce::Justification::centredLeft);
    }

    g.setColour(juce::Colour(Theme::border));
    g.drawHorizontalLine(toolbar.getBottom(), 0.0f, static_cast<float>(getWidth()));

    // Waveform area
    if (clip && clip->asset)
    {
        paintWaveform(g, area);
        paintFadeHandles(g, area);
    }
    else
    {
        g.setColour(juce::Colour(Theme::textDim));
        g.drawText("Double-click an audio clip to edit", area, juce::Justification::centred);
    }

    // Info bar
    paintInfo(g, infoBar);
}

void AudioClipEditor::paintWaveform(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto* clip = getClip();
    if (!clip || !clip->asset || clip->asset->numChannels == 0) return;

    int w = area.getWidth();
    if (w <= 0) return;

    // Regenerate peaks if needed
    if (waveformPeaks_.size() != static_cast<size_t>(w * 2) ||
        std::abs(cachedPPS_ - pixelsPerSample_) > 1e-12)
    {
        waveformPeaks_.resize(static_cast<size_t>(w * 2), 0.0f);
        cachedPPS_ = pixelsPerSample_;

        const auto& ch = clip->asset->channels[0];
        double samplesPerPixel = 1.0 / pixelsPerSample_;

        for (int px = 0; px < w; ++px)
        {
            SampleCount s0 = clip->sourceStartSample +
                static_cast<SampleCount>(static_cast<double>(px) * samplesPerPixel);
            SampleCount s1 = clip->sourceStartSample +
                static_cast<SampleCount>(static_cast<double>(px + 1) * samplesPerPixel);

            s0 = std::max(s0, SampleCount(0));
            s1 = std::min(s1, clip->asset->lengthInSamples);

            float mn = 0.0f, mx = 0.0f;
            for (SampleCount s = s0; s < s1; ++s)
            {
                float v = ch[static_cast<size_t>(s)];
                mn = std::min(mn, v);
                mx = std::max(mx, v);
            }
            waveformPeaks_[static_cast<size_t>(px * 2)] = mn;
            waveformPeaks_[static_cast<size_t>(px * 2 + 1)] = mx;
        }
    }

    // Draw waveform
    float midY = static_cast<float>(area.getCentreY());
    float halfH = static_cast<float>(area.getHeight()) * 0.42f;

    // Background
    g.setColour(juce::Colour(Theme::bgDeepest));
    g.fillRect(area);

    // Center line
    g.setColour(juce::Colour(Theme::border));
    g.drawHorizontalLine(static_cast<int>(midY), static_cast<float>(area.getX()),
                         static_cast<float>(area.getRight()));

    // Waveform
    g.setColour(juce::Colour(Theme::clipAudioWave));
    for (int px = 0; px < w; ++px)
    {
        float mn = waveformPeaks_[static_cast<size_t>(px * 2)];
        float mx = waveformPeaks_[static_cast<size_t>(px * 2 + 1)];
        float y1 = midY - mx * halfH;
        float y2 = midY - mn * halfH;
        g.drawVerticalLine(area.getX() + px, y1, y2);
    }

    // Gain line
    float gainDb = clip->gainDb;
    float gainLin = juce::Decibels::decibelsToGain(gainDb);
    float gainY = midY - gainLin * halfH;
    g.setColour(juce::Colour(Theme::accentOrange).withAlpha(0.6f));
    g.drawHorizontalLine(static_cast<int>(gainY), static_cast<float>(area.getX()),
                         static_cast<float>(area.getRight()));
}

void AudioClipEditor::paintFadeHandles(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto* clip = getClip();
    if (!clip) return;

    // Fade in triangle
    if (clip->fadeInSamples > 0)
    {
        int fadeW = static_cast<int>(static_cast<double>(clip->fadeInSamples) * pixelsPerSample_);
        fadeW = std::min(fadeW, area.getWidth());

        juce::Path p;
        p.startNewSubPath(static_cast<float>(area.getX()), static_cast<float>(area.getBottom()));
        p.lineTo(static_cast<float>(area.getX() + fadeW), static_cast<float>(area.getY()));
        p.lineTo(static_cast<float>(area.getX()), static_cast<float>(area.getY()));
        p.closeSubPath();
        g.setColour(juce::Colour(0x30000000));
        g.fillPath(p);

        // Handle dot
        g.setColour(juce::Colour(Theme::accentBlue));
        g.fillEllipse(static_cast<float>(area.getX() + fadeW - 4),
                      static_cast<float>(area.getY() - 2), 8.0f, 8.0f);
    }

    // Fade out triangle
    if (clip->fadeOutSamples > 0)
    {
        int fadeW = static_cast<int>(static_cast<double>(clip->fadeOutSamples) * pixelsPerSample_);
        fadeW = std::min(fadeW, area.getWidth());

        juce::Path p;
        p.startNewSubPath(static_cast<float>(area.getRight()), static_cast<float>(area.getBottom()));
        p.lineTo(static_cast<float>(area.getRight() - fadeW), static_cast<float>(area.getY()));
        p.lineTo(static_cast<float>(area.getRight()), static_cast<float>(area.getY()));
        p.closeSubPath();
        g.setColour(juce::Colour(0x30000000));
        g.fillPath(p);

        g.setColour(juce::Colour(Theme::accentBlue));
        g.fillEllipse(static_cast<float>(area.getRight() - fadeW - 4),
                      static_cast<float>(area.getY() - 2), 8.0f, 8.0f);
    }
}

void AudioClipEditor::paintInfo(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour(juce::Colour(Theme::toolbarBg));
    g.fillRect(area);
    g.setColour(juce::Colour(Theme::border));
    g.drawHorizontalLine(area.getY(), 0.0f, static_cast<float>(getWidth()));

    auto* clip = getClip();
    if (!clip || !clip->asset) return;

    g.setColour(juce::Colour(Theme::textSecondary));
    g.setFont(Theme::smallFont());

    juce::String info;
    info += juce::String(clip->asset->numChannels) + "ch ";
    info += juce::String(clip->asset->sampleRate / 1000.0, 1) + "kHz  ";
    info += "Gain: " + juce::String(clip->gainDb, 1) + " dB  ";
    info += "Fade In: " + juce::String(clip->fadeInSamples) + "  ";
    info += "Fade Out: " + juce::String(clip->fadeOutSamples);

    g.drawText(info, area.reduced(8, 0), juce::Justification::centredLeft);
}

void AudioClipEditor::resized()
{
    auto toolbar = getLocalBounds().removeFromTop(kToolbarHeight).reduced(4, 2);
    closeBtn_.setBounds(toolbar.removeFromRight(26));

    waveformPeaks_.clear();
    cachedPPS_ = 0.0;
}

void AudioClipEditor::mouseDown(const juce::MouseEvent& e)
{
    auto area = getLocalBounds();
    area.removeFromTop(kToolbarHeight);
    area.removeFromBottom(kInfoHeight);

    auto* clip = getClip();
    if (!clip) return;

    // Check fade in handle
    if (clip->fadeInSamples > 0)
    {
        int fadeW = static_cast<int>(static_cast<double>(clip->fadeInSamples) * pixelsPerSample_);
        if (std::abs(e.x - (area.getX() + fadeW)) < 8 && e.y < area.getY() + 12)
        {
            dragMode_ = DragMode::FadeIn;
            return;
        }
    }

    // Check fade out handle
    if (clip->fadeOutSamples > 0)
    {
        int fadeW = static_cast<int>(static_cast<double>(clip->fadeOutSamples) * pixelsPerSample_);
        if (std::abs(e.x - (area.getRight() - fadeW)) < 8 && e.y < area.getY() + 12)
        {
            dragMode_ = DragMode::FadeOut;
            return;
        }
    }

    // Gain drag
    dragMode_ = DragMode::Gain;
    dragStartValue_ = clip->gainDb;
    dragStartY_ = e.y;
}

void AudioClipEditor::mouseDrag(const juce::MouseEvent& e)
{
    auto* clip = getClip();
    if (!clip) return;

    auto area = getLocalBounds();
    area.removeFromTop(kToolbarHeight);
    area.removeFromBottom(kInfoHeight);

    if (dragMode_ == DragMode::FadeIn)
    {
        int fadePixels = std::max(0, e.x - area.getX());
        SampleCount fadeSamples = static_cast<SampleCount>(
            static_cast<double>(fadePixels) / pixelsPerSample_);
        fadeSamples = std::max(SampleCount(0), std::min(fadeSamples, clip->sourceLengthSamples / 2));
        clip->fadeInSamples = fadeSamples;
        repaint();
    }
    else if (dragMode_ == DragMode::FadeOut)
    {
        int fadePixels = std::max(0, area.getRight() - e.x);
        SampleCount fadeSamples = static_cast<SampleCount>(
            static_cast<double>(fadePixels) / pixelsPerSample_);
        fadeSamples = std::max(SampleCount(0), std::min(fadeSamples, clip->sourceLengthSamples / 2));
        clip->fadeOutSamples = fadeSamples;
        repaint();
    }
    else if (dragMode_ == DragMode::Gain)
    {
        float deltaDb = static_cast<float>(dragStartY_ - e.y) * 0.2f;
        clip->gainDb = std::clamp(dragStartValue_ + deltaDb, -60.0f, 12.0f);
        repaint();
    }
}

void AudioClipEditor::mouseUp(const juce::MouseEvent& /*e*/)
{
    if (dragMode_ != DragMode::None)
    {
        if (onSessionChanged) onSessionChanged();
    }
    dragMode_ = DragMode::None;
}

void AudioClipEditor::mouseWheelMove(const juce::MouseEvent& e,
                                      const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isCommandDown())
    {
        if (wheel.deltaY > 0)
            pixelsPerSample_ = std::min(pixelsPerSample_ * 1.3, 1.0);
        else
            pixelsPerSample_ = std::max(pixelsPerSample_ / 1.3, 0.0001);
        waveformPeaks_.clear();
        cachedPPS_ = 0.0;
    }
    else
    {
        double scrollAmt = -wheel.deltaY * 3000.0 / pixelsPerSample_;
        scrollSamples_ += static_cast<SampleCount>(scrollAmt);
        scrollSamples_ = std::max(SampleCount(0), scrollSamples_);
    }
    repaint();
}

} // namespace neurato
