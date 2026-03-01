#include "ui/timeline/TransportBar.hpp"
#include "ui/Theme.hpp"
#include <cmath>

namespace ampl
{

TransportBar::TransportBar(AudioEngine &engine) : engine_(engine)
{
    // Single transport button: play ↔ pause
    ampl::Theme::styleTextButton(transportButton_, true);
    transportButton_.setTooltip("Play/Pause (Space)");
    transportButton_.setWantsKeyboardFocus(false);
    transportButton_.onClick = [this] { engine_.sendTogglePlayStop(); };
    addAndMakeVisible(transportButton_);

    // BPM slider
    bpmSlider_.setRange(20.0, 300.0, 0.1);
    bpmSlider_.setValue(120.0);
    bpmSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    bpmSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 24);
    ampl::Theme::styleSlider(bpmSlider_);
    bpmSlider_.setTooltip("Tempo (BPM)");
    bpmSlider_.onValueChange = [this] { engine_.sendSetBpm(bpmSlider_.getValue()); };
    addAndMakeVisible(bpmSlider_);

    bpmLabel_.setJustificationType(juce::Justification::centredRight);
    ampl::Theme::styleLabel(bpmLabel_);
    addAndMakeVisible(bpmLabel_);

    // Time display
    timeDisplay_.setFont(ampl::Theme::timeFont());
    timeDisplay_.setJustificationType(juce::Justification::centred);
    timeDisplay_.setColour(juce::Label::textColourId, juce::Colour(ampl::Theme::textPrimary));
    addAndMakeVisible(timeDisplay_);

    // Beat display
    beatDisplay_.setFont(ampl::Theme::bodyFont());
    beatDisplay_.setJustificationType(juce::Justification::centred);
    ampl::Theme::styleLabel(beatDisplay_);
    beatDisplay_.setColour(juce::Label::textColourId, juce::Colour(ampl::Theme::textSecondary));
    addAndMakeVisible(beatDisplay_);

    // Metronome toggle
    metronomeToggle_.setColour(juce::ToggleButton::textColourId,
                               juce::Colour(ampl::Theme::textSecondary));
    metronomeToggle_.setColour(juce::ToggleButton::tickColourId,
                               juce::Colour(ampl::Theme::accentBlue));
    metronomeToggle_.setColour(juce::ToggleButton::tickDisabledColourId,
                               juce::Colour(ampl::Theme::textDisabled));
    metronomeToggle_.setToggleState(true, juce::dontSendNotification);
    metronomeToggle_.setTooltip("Toggle metronome click");
    metronomeToggle_.setWantsKeyboardFocus(false);
    metronomeToggle_.onClick = [this]
    { engine_.sendSetMetronomeEnabled(metronomeToggle_.getToggleState()); };
    addAndMakeVisible(metronomeToggle_);

    // Metronome gain
    metronomeGainSlider_.setRange(0.0, 1.0, 0.01);
    metronomeGainSlider_.setValue(0.5);
    metronomeGainSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    metronomeGainSlider_.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    ampl::Theme::styleSlider(metronomeGainSlider_);
    metronomeGainSlider_.setTooltip("Metronome gain");
    metronomeGainSlider_.onValueChange = [this]
    { engine_.sendSetMetronomeGain(static_cast<float>(metronomeGainSlider_.getValue())); };
    addAndMakeVisible(metronomeGainSlider_);
}

TransportBar::~TransportBar() = default;

void TransportBar::paint(juce::Graphics &g)
{
    auto bounds = getLocalBounds().toFloat();
    juce::ColourGradient gradient(
        juce::Colour(ampl::Theme::transportBg).brighter(0.08f), 0.0f, bounds.getY(),
        juce::Colour(ampl::Theme::transportBg).darker(0.08f), 0.0f, bounds.getBottom(), false);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(bounds, 6.0f);

    g.setColour(juce::Colour(ampl::Theme::borderLight).withAlpha(0.8f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

    // Draw peak meters
    const int meterWidth = 60;
    const int meterHeight = 8;
    const int meterX = getWidth() - meterWidth - 10;
    const int meterY = getHeight() / 2 - meterHeight - 1;

    // Left channel
    g.setColour(juce::Colour(ampl::Theme::meterBg));
    g.fillRoundedRectangle(static_cast<float>(meterX), static_cast<float>(meterY),
                           static_cast<float>(meterWidth), static_cast<float>(meterHeight), 2.0f);
    float levelWidthL = std::min(peakL_, 1.0f) * static_cast<float>(meterWidth);
    g.setColour(peakL_ > 0.9f   ? juce::Colour(ampl::Theme::meterRed)
                : peakL_ > 0.7f ? juce::Colour(ampl::Theme::meterYellow)
                                : juce::Colour(ampl::Theme::meterGreen));
    g.fillRoundedRectangle(static_cast<float>(meterX), static_cast<float>(meterY), levelWidthL,
                           static_cast<float>(meterHeight), 2.0f);

    // Right channel
    g.setColour(juce::Colour(ampl::Theme::meterBg));
    g.fillRoundedRectangle(static_cast<float>(meterX), static_cast<float>(meterY + meterHeight + 2),
                           static_cast<float>(meterWidth), static_cast<float>(meterHeight), 2.0f);
    float levelWidthR = std::min(peakR_, 1.0f) * static_cast<float>(meterWidth);
    g.setColour(peakR_ > 0.9f   ? juce::Colour(ampl::Theme::meterRed)
                : peakR_ > 0.7f ? juce::Colour(ampl::Theme::meterYellow)
                                : juce::Colour(ampl::Theme::meterGreen));
    g.fillRoundedRectangle(static_cast<float>(meterX), static_cast<float>(meterY + meterHeight + 2),
                           levelWidthR, static_cast<float>(meterHeight), 2.0f);

    g.setColour(juce::Colour(ampl::Theme::textPrimary).withAlpha(0.03f));
    g.fillRect(getLocalBounds().removeFromTop(1));
}

void TransportBar::resized()
{
    auto area = getLocalBounds().reduced(10, 6);

    auto transportSlot = area.removeFromLeft(84);
    transportButton_.setBounds(transportSlot.withSizeKeepingCentre(72, 32));
    area.removeFromLeft(16);

    bpmLabel_.setBounds(area.removeFromLeft(38));
    bpmSlider_.setBounds(area.removeFromLeft(140));
    area.removeFromLeft(16);

    timeDisplay_.setBounds(area.removeFromLeft(110));
    area.removeFromLeft(4);
    beatDisplay_.setBounds(area.removeFromLeft(80));
    area.removeFromLeft(16);

    metronomeToggle_.setBounds(area.removeFromLeft(60));
    area.removeFromLeft(4);
    metronomeGainSlider_.setBounds(area.removeFromLeft(80));
}

void TransportBar::handleAudioMessage(const AudioToUIMessage &msg)
{
    switch (msg.type)
    {
    case AudioToUIMessage::Type::PlayheadPosition:
    {
        double seconds = msg.doubleValue;
        int mins = static_cast<int>(seconds) / 60;
        double secs = seconds - (mins * 60);
        timeDisplay_.setText(juce::String::formatted("%d:%06.3f", mins, secs),
                             juce::dontSendNotification);

        // Calculate bar.beat.tick
        double bpm = engine_.getTransport().getBpm();
        int timeSigNum = engine_.getTransport().getTimeSigNumerator();
        if (bpm > 0.0)
        {
            double totalBeats = seconds * (bpm / 60.0);
            int bar = static_cast<int>(totalBeats / timeSigNum) + 1;
            int beat = static_cast<int>(std::fmod(totalBeats, timeSigNum)) + 1;
            int tick = static_cast<int>(std::fmod(totalBeats * 100.0, 100.0));
            beatDisplay_.setText(juce::String::formatted("%d.%d.%02d", bar, beat, tick),
                                 juce::dontSendNotification);
        }
        repaintPending_ = true;
        break;
    }
    case AudioToUIMessage::Type::PeakLevel:
    {
        // Smooth decay
        auto nextL = std::max(msg.floatValue1, peakL_ * 0.85f);
        auto nextR = std::max(msg.floatValue2, peakR_ * 0.85f);
        if (std::abs(nextL - peakL_) > 0.002f || std::abs(nextR - peakR_) > 0.002f)
            repaintPending_ = true;
        peakL_ = nextL;
        peakR_ = nextR;
        break;
    }
    case AudioToUIMessage::Type::TransportStateChanged:
        break;
    }
}

void TransportBar::updateDisplay()
{
    // Update icon/tooltip based on transport state
    bool isPlaying = engine_.getTransport().getState() == Transport::State::Playing;
    if (isPlaying != lastPlayingState_)
    {
        transportButton_.setButtonText(isPlaying ? juce::String::fromUTF8("\xe2\x8f\xb8")
                                                 : juce::String::fromUTF8("\xe2\x96\xb6"));
        transportButton_.setTooltip(isPlaying ? "Pause (Space)" : "Play (Space)");
        lastPlayingState_ = isPlaying;
        repaintPending_ = true;
    }

    if (repaintPending_)
    {
        repaint();
        repaintPending_ = false;
    }
}

} // namespace ampl
