#include "ui/TransportBar.h"
#include "ui/Theme.h"

namespace neurato {

TransportBar::TransportBar(AudioEngine& engine)
    : engine_(engine)
{
    // Play button
    playButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::playButton));
    playButton_.onClick = [this] { engine_.sendPlay(); };
    addAndMakeVisible(playButton_);

    // Stop button
    stopButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::stopButton));
    stopButton_.onClick = [this] { engine_.sendStop(); };
    addAndMakeVisible(stopButton_);

    // BPM slider
    bpmSlider_.setRange(20.0, 300.0, 0.1);
    bpmSlider_.setValue(120.0);
    bpmSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    bpmSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 24);
    bpmSlider_.onValueChange = [this] {
        engine_.sendSetBpm(bpmSlider_.getValue());
    };
    addAndMakeVisible(bpmSlider_);

    bpmLabel_.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(bpmLabel_);

    // Time display
    timeDisplay_.setFont(Theme::timeFont());
    timeDisplay_.setJustificationType(juce::Justification::centred);
    timeDisplay_.setColour(juce::Label::textColourId, juce::Colour(Theme::textPrimary));
    addAndMakeVisible(timeDisplay_);

    // Beat display
    beatDisplay_.setFont(Theme::bodyFont());
    beatDisplay_.setJustificationType(juce::Justification::centred);
    beatDisplay_.setColour(juce::Label::textColourId, juce::Colour(Theme::textSecondary));
    addAndMakeVisible(beatDisplay_);

    // Metronome toggle
    metronomeToggle_.setToggleState(true, juce::dontSendNotification);
    metronomeToggle_.onClick = [this] {
        engine_.sendSetMetronomeEnabled(metronomeToggle_.getToggleState());
    };
    addAndMakeVisible(metronomeToggle_);

    // Metronome gain
    metronomeGainSlider_.setRange(0.0, 1.0, 0.01);
    metronomeGainSlider_.setValue(0.5);
    metronomeGainSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    metronomeGainSlider_.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    metronomeGainSlider_.onValueChange = [this] {
        engine_.sendSetMetronomeGain(static_cast<float>(metronomeGainSlider_.getValue()));
    };
    addAndMakeVisible(metronomeGainSlider_);

}

TransportBar::~TransportBar() = default;

void TransportBar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(Theme::transportBg));

    // Draw peak meters
    const int meterWidth = 60;
    const int meterHeight = 8;
    const int meterX = getWidth() - meterWidth - 10;
    const int meterY = getHeight() / 2 - meterHeight - 1;

    // Left channel
    g.setColour(juce::Colour(Theme::meterBg));
    g.fillRoundedRectangle(static_cast<float>(meterX), static_cast<float>(meterY),
                           static_cast<float>(meterWidth), static_cast<float>(meterHeight), 2.0f);
    float levelWidthL = std::min(peakL_, 1.0f) * static_cast<float>(meterWidth);
    g.setColour(peakL_ > 0.9f ? juce::Colour(Theme::meterRed)
                : peakL_ > 0.7f ? juce::Colour(Theme::meterYellow)
                : juce::Colour(Theme::meterGreen));
    g.fillRoundedRectangle(static_cast<float>(meterX), static_cast<float>(meterY),
                           levelWidthL, static_cast<float>(meterHeight), 2.0f);

    // Right channel
    g.setColour(juce::Colour(Theme::meterBg));
    g.fillRoundedRectangle(static_cast<float>(meterX), static_cast<float>(meterY + meterHeight + 2),
                           static_cast<float>(meterWidth), static_cast<float>(meterHeight), 2.0f);
    float levelWidthR = std::min(peakR_, 1.0f) * static_cast<float>(meterWidth);
    g.setColour(peakR_ > 0.9f ? juce::Colour(Theme::meterRed)
                : peakR_ > 0.7f ? juce::Colour(Theme::meterYellow)
                : juce::Colour(Theme::meterGreen));
    g.fillRoundedRectangle(static_cast<float>(meterX), static_cast<float>(meterY + meterHeight + 2),
                           levelWidthR, static_cast<float>(meterHeight), 2.0f);

    // Separator line at bottom
    g.setColour(juce::Colour(Theme::border));
    g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));
}

void TransportBar::resized()
{
    auto area = getLocalBounds().reduced(8, 4);

    playButton_.setBounds(area.removeFromLeft(60));
    area.removeFromLeft(4);
    stopButton_.setBounds(area.removeFromLeft(60));
    area.removeFromLeft(12);

    bpmLabel_.setBounds(area.removeFromLeft(35));
    bpmSlider_.setBounds(area.removeFromLeft(150));
    area.removeFromLeft(12);

    timeDisplay_.setBounds(area.removeFromLeft(120));
    beatDisplay_.setBounds(area.removeFromLeft(80));
    area.removeFromLeft(12);

    metronomeToggle_.setBounds(area.removeFromLeft(60));
    metronomeGainSlider_.setBounds(area.removeFromLeft(80));
}

void TransportBar::handleAudioMessage(const AudioToUIMessage& msg)
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
            break;
        }
        case AudioToUIMessage::Type::PeakLevel:
        {
            // Smooth decay
            peakL_ = std::max(msg.floatValue1, peakL_ * 0.85f);
            peakR_ = std::max(msg.floatValue2, peakR_ * 0.85f);
            break;
        }
        case AudioToUIMessage::Type::TransportStateChanged:
            break;
    }
}

void TransportBar::updateDisplay()
{
    // Update play button text based on transport state
    bool isPlaying = engine_.getTransport().getState() == Transport::State::Playing;
    playButton_.setButtonText(isPlaying ? "Pause" : "Play");

    repaint();
}

} // namespace neurato
