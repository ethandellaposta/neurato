#include "ui/MixerPanel.h"
#include "ui/Theme.h"
#include "commands/ClipCommands.h"

namespace neurato {

//==============================================================================
// ChannelStrip
//==============================================================================

ChannelStrip::ChannelStrip()
{
    nameLabel_.setJustificationType(juce::Justification::centred);
    nameLabel_.setColour(juce::Label::textColourId, juce::Colour(Theme::textPrimary));
    nameLabel_.setFont(Theme::labelFont());
    addAndMakeVisible(nameLabel_);

    gainSlider_.setSliderStyle(juce::Slider::LinearVertical);
    gainSlider_.setRange(-60.0, 12.0, 0.1);
    gainSlider_.setValue(0.0);
    gainSlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
    gainSlider_.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    gainSlider_.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    gainSlider_.onValueChange = [this] {
        if (onGainChanged)
            onGainChanged(trackIndex_, static_cast<float>(gainSlider_.getValue()));
    };
    addAndMakeVisible(gainSlider_);

    panSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    panSlider_.setRange(-1.0, 1.0, 0.01);
    panSlider_.setValue(0.0);
    panSlider_.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    panSlider_.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(Theme::accentBlue));
    panSlider_.onValueChange = [this] {
        if (onPanChanged)
            onPanChanged(trackIndex_, static_cast<float>(panSlider_.getValue()));
    };
    addAndMakeVisible(panSlider_);

    muteButton_.setClickingTogglesState(true);
    muteButton_.setColour(juce::TextButton::buttonOnColourId, juce::Colour(Theme::accentRed));
    muteButton_.onClick = [this] {
        if (onMuteToggled)
            onMuteToggled(trackIndex_, muteButton_.getToggleState());
    };
    addAndMakeVisible(muteButton_);

    soloButton_.setClickingTogglesState(true);
    soloButton_.setColour(juce::TextButton::buttonOnColourId, juce::Colour(Theme::accentYellow));
    soloButton_.onClick = [this] {
        if (onSoloToggled)
            onSoloToggled(trackIndex_, soloButton_.getToggleState());
    };
    addAndMakeVisible(soloButton_);

    removeButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(Theme::bgElevated));
    removeButton_.onClick = [this] {
        if (onRemoveTrack)
            onRemoveTrack(trackIndex_);
    };
    addAndMakeVisible(removeButton_);
}

void ChannelStrip::paint(juce::Graphics& g)
{
    auto area = getLocalBounds();

    // Background
    g.setColour(juce::Colour(Theme::bgSurface));
    g.fillRoundedRectangle(area.toFloat().reduced(1.0f), Theme::cornerRadius);

    // Border
    g.setColour(juce::Colour(Theme::border));
    g.drawRoundedRectangle(area.toFloat().reduced(1.0f), Theme::cornerRadius, 1.0f);

    // Simple peak meter (two thin bars next to the gain slider)
    auto meterArea = getLocalBounds().reduced(4);
    meterArea = meterArea.removeFromRight(8);
    meterArea.removeFromTop(40);
    meterArea.removeFromBottom(60);

    int meterH = meterArea.getHeight();
    int halfW = meterArea.getWidth() / 2;

    // Left meter
    auto leftBar = meterArea.removeFromLeft(halfW).reduced(0, 0);
    g.setColour(juce::Colour(Theme::meterBg));
    g.fillRect(leftBar);
    int peakH_L = static_cast<int>(peakL_ * static_cast<float>(meterH));
    if (peakH_L > 0)
    {
        auto lit = leftBar.removeFromBottom(peakH_L);
        g.setColour(peakL_ > 0.9f ? juce::Colour(Theme::meterRed)
                    : peakL_ > 0.7f ? juce::Colour(Theme::meterYellow)
                    : juce::Colour(Theme::meterGreen));
        g.fillRect(lit);
    }

    // Right meter
    auto rightBar = meterArea.reduced(0, 0);
    g.setColour(juce::Colour(Theme::meterBg));
    g.fillRect(rightBar);
    int peakH_R = static_cast<int>(peakR_ * static_cast<float>(meterH));
    if (peakH_R > 0)
    {
        auto lit = rightBar.removeFromBottom(peakH_R);
        g.setColour(peakR_ > 0.9f ? juce::Colour(Theme::meterRed)
                    : peakR_ > 0.7f ? juce::Colour(Theme::meterYellow)
                    : juce::Colour(Theme::meterGreen));
        g.fillRect(lit);
    }
}

void ChannelStrip::resized()
{
    auto area = getLocalBounds().reduced(4);

    nameLabel_.setBounds(area.removeFromTop(18));
    removeButton_.setBounds(area.removeFromTop(16).reduced(10, 0));
    area.removeFromTop(2);
    panSlider_.setBounds(area.removeFromTop(36).reduced(8, 0));
    area.removeFromTop(2);

    // Mute/Solo row
    auto msRow = area.removeFromBottom(22);
    muteButton_.setBounds(msRow.removeFromLeft(msRow.getWidth() / 2).reduced(2, 0));
    soloButton_.setBounds(msRow.reduced(2, 0));

    // Gain slider fills remaining
    area.removeFromRight(10); // leave room for meter
    gainSlider_.setBounds(area);
}

void ChannelStrip::setTrackName(const juce::String& name)
{
    nameLabel_.setText(name, juce::dontSendNotification);
}

void ChannelStrip::setGainDb(float db)
{
    gainSlider_.setValue(static_cast<double>(db), juce::dontSendNotification);
}

void ChannelStrip::setPan(float pan)
{
    panSlider_.setValue(static_cast<double>(pan), juce::dontSendNotification);
}

void ChannelStrip::setMuted(bool muted)
{
    muteButton_.setToggleState(muted, juce::dontSendNotification);
}

void ChannelStrip::setSoloed(bool soloed)
{
    soloButton_.setToggleState(soloed, juce::dontSendNotification);
}

void ChannelStrip::setPeakLevel(float pL, float pR)
{
    peakL_ = pL;
    peakR_ = pR;
    repaint();
}

//==============================================================================
// MixerPanel
//==============================================================================

MixerPanel::MixerPanel(Session& session, CommandManager& commandManager)
    : session_(session), commandManager_(commandManager)
{
    viewport_.setViewedComponent(&stripContainer_, false);
    viewport_.setScrollBarsShown(false, true);
    addAndMakeVisible(viewport_);

    setupMasterStrip();
    rebuildStrips();
}

void MixerPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(Theme::bgBase));

    // Separator line at top
    g.setColour(juce::Colour(Theme::border));
    g.drawHorizontalLine(0, 0.0f, static_cast<float>(getWidth()));
}

void MixerPanel::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(1); // separator

    // Master strip on the right
    auto masterArea = area.removeFromRight(90);
    masterLabel_.setBounds(masterArea.removeFromTop(18));
    masterPanSlider_.setBounds(masterArea.removeFromTop(36).reduced(12, 0));
    auto msBottom = masterArea.removeFromBottom(22);
    (void)msBottom; // master has no mute/solo
    masterArea.removeFromRight(10);
    masterGainSlider_.setBounds(masterArea);

    // Separator between master and tracks
    area.removeFromRight(2);

    // Track strips in viewport
    int totalWidth = static_cast<int>(strips_.size()) * ChannelStrip::kStripWidth;
    stripContainer_.setSize(std::max(totalWidth, area.getWidth()), area.getHeight());

    int x = 0;
    for (auto& strip : strips_)
    {
        strip->setBounds(x, 0, ChannelStrip::kStripWidth, area.getHeight());
        x += ChannelStrip::kStripWidth;
    }

    viewport_.setBounds(area);
}

void MixerPanel::rebuildStrips()
{
    strips_.clear();
    stripContainer_.removeAllChildren();

    const auto& tracks = session_.getTracks();
    for (int i = 0; i < static_cast<int>(tracks.size()); ++i)
    {
        auto strip = std::make_unique<ChannelStrip>();
        strip->setTrackIndex(i);
        strip->setTrackName(tracks[static_cast<size_t>(i)].name);
        strip->setGainDb(tracks[static_cast<size_t>(i)].gainDb);
        strip->setPan(tracks[static_cast<size_t>(i)].pan);
        strip->setMuted(tracks[static_cast<size_t>(i)].muted);
        strip->setSoloed(tracks[static_cast<size_t>(i)].solo);

        strip->onGainChanged = [this](int idx, float db) {
            auto cmd = std::make_unique<SetTrackGainCommand>(idx, db);
            commandManager_.execute(std::move(cmd), session_);
            if (onSessionChanged) onSessionChanged();
        };

        strip->onPanChanged = [this](int idx, float pan) {
            auto cmd = std::make_unique<SetTrackPanCommand>(idx, pan);
            commandManager_.execute(std::move(cmd), session_);
            if (onSessionChanged) onSessionChanged();
        };

        strip->onMuteToggled = [this](int idx, bool muted) {
            auto cmd = std::make_unique<SetTrackMuteCommand>(idx, muted);
            commandManager_.execute(std::move(cmd), session_);
            if (onSessionChanged) onSessionChanged();
        };

        strip->onSoloToggled = [this](int idx, bool solo) {
            auto cmd = std::make_unique<SetTrackSoloCommand>(idx, solo);
            commandManager_.execute(std::move(cmd), session_);
            if (onSessionChanged) onSessionChanged();
        };

        strip->onRemoveTrack = [this](int idx) {
            if (session_.getTracks().size() <= 1)
                return; // Don't remove the last track
            auto cmd = std::make_unique<RemoveTrackCommand>(idx);
            commandManager_.execute(std::move(cmd), session_);
            rebuildStrips();
            if (onSessionChanged) onSessionChanged();
        };

        stripContainer_.addAndMakeVisible(strip.get());
        strips_.push_back(std::move(strip));
    }

    // Update master
    masterGainSlider_.setValue(static_cast<double>(session_.getMasterGainDb()),
                               juce::dontSendNotification);
    masterPanSlider_.setValue(static_cast<double>(session_.getMasterPan()),
                              juce::dontSendNotification);

    resized();
}

void MixerPanel::updateMeters()
{
    // For now, meters decay. Real per-track metering will come with
    // per-track peak messages from the audio thread in a future milestone.
    for (auto& strip : strips_)
    {
        float pL = 0.0f, pR = 0.0f;
        // Decay existing peaks
        strip->setPeakLevel(pL, pR);
    }
}

void MixerPanel::setupMasterStrip()
{
    masterLabel_.setText("Master", juce::dontSendNotification);
    masterLabel_.setJustificationType(juce::Justification::centred);
    masterLabel_.setColour(juce::Label::textColourId, juce::Colour(Theme::accentOrange));
    masterLabel_.setFont(Theme::headingFont());
    addAndMakeVisible(masterLabel_);

    masterGainSlider_.setSliderStyle(juce::Slider::LinearVertical);
    masterGainSlider_.setRange(-60.0, 12.0, 0.1);
    masterGainSlider_.setValue(0.0);
    masterGainSlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
    masterGainSlider_.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    masterGainSlider_.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    masterGainSlider_.setColour(juce::Slider::thumbColourId, juce::Colour(Theme::accentOrange));
    masterGainSlider_.onValueChange = [this] {
        auto cmd = std::make_unique<SetMasterGainCommand>(
            static_cast<float>(masterGainSlider_.getValue()));
        commandManager_.execute(std::move(cmd), session_);
        if (onSessionChanged) onSessionChanged();
    };
    addAndMakeVisible(masterGainSlider_);

    masterPanSlider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    masterPanSlider_.setRange(-1.0, 1.0, 0.01);
    masterPanSlider_.setValue(0.0);
    masterPanSlider_.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    masterPanSlider_.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(Theme::accentOrange));
    masterPanSlider_.onValueChange = [this] {
        session_.setMasterPan(static_cast<float>(masterPanSlider_.getValue()));
        if (onSessionChanged) onSessionChanged();
    };
    addAndMakeVisible(masterPanSlider_);
}

} // namespace neurato
