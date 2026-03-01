#include "LogicMixerPanel.hpp"
#include "ui/Theme.hpp"
#include <algorithm>
#include <cmath>

namespace ampl {

namespace {
    constexpr int UI_UPDATE_FPS = 30;
    constexpr int DEFAULT_WIDTH = 800;
    constexpr int DEFAULT_HEIGHT = 400;
}

// LogicMixerPanel implementation
LogicMixerPanel::LogicMixerPanel() {
    // Setup viewport for horizontal scrolling
    viewport_ = std::make_unique<juce::Viewport>();
    channelContainer_ = std::make_unique<juce::Component>();

    viewport_->setViewedComponent(channelContainer_.get(), false);
    viewport_->setScrollBarsShown(true, false);
    viewport_->setWantsKeyboardFocus(false);

    addAndMakeVisible(viewport_.get());

    // Setup master section
    masterSection_ = std::make_unique<MasterSection>(*this);

    // Setup VCA section
    vcaSection_ = std::make_unique<VCASection>(*this);

    // Start timer for UI updates
    startTimerHz(UI_UPDATE_FPS);

    // Set default size
    setSize(DEFAULT_WIDTH, DEFAULT_HEIGHT);
}

LogicMixerPanel::~LogicMixerPanel() {
    stopTimer();
}

void LogicMixerPanel::addTrack(const std::string& trackId, const LogicMixerChannel::ChannelStrip& strip) {
    auto channelStrip = std::make_unique<ChannelStripComponent>(trackId, strip, *this);
    channelStrips_[trackId] = std::move(channelStrip);

    channelContainer_->addAndMakeVisible(channelStrips_[trackId].get());
    updateLayout();
}

void LogicMixerPanel::removeTrack(const std::string& trackId) {
    auto it = channelStrips_.find(trackId);
    if (it != channelStrips_.end()) {
        channelContainer_->removeChildComponent(it->second.get());
        channelStrips_.erase(it);
        updateLayout();
    }
}

void LogicMixerPanel::updateTrack(const std::string& trackId, const LogicMixerChannel::ChannelStrip& strip) {
    auto it = channelStrips_.find(trackId);
    if (it != channelStrips_.end()) {
        it->second->updateStrip(strip);
    }
}

void LogicMixerPanel::setTrackWidth(int width) {
    trackWidth_ = std::max(80, std::min(200, width));
    updateLayout();
}

void LogicMixerPanel::setShowPluginSlots(bool show) {
    showPluginSlots_ = show;
    updateLayout();
}

void LogicMixerPanel::setShowSends(bool show) {
    showSends_ = show;
    updateLayout();
}

void LogicMixerPanel::setShowVCAs(bool show) {
    showVCAs_ = show;
    updateLayout();
}

void LogicMixerPanel::setShowAutomation(bool show) {
    showAutomation_ = show;
    updateLayout();
}

void LogicMixerPanel::handleSoloState(const std::string& trackId, bool soloed) {
    handleSoloLogic(trackId, soloed);
    updateAllMuteStates();
}

void LogicMixerPanel::handleMuteState(const std::string& trackId, bool muted) {
    if (muted) {
        mutedTracks_.insert(trackId);
    } else {
        mutedTracks_.erase(trackId);
    }

    // Update UI
    auto it = channelStrips_.find(trackId);
    if (it != channelStrips_.end()) {
        LogicMixerChannel::ChannelStrip strip = it->second->strip_;
        strip.mute = muted;
        it->second->updateStrip(strip);
    }
}

void LogicMixerPanel::addListener(Listener* listener) {
    listeners_.add(listener);
}

void LogicMixerPanel::removeListener(Listener* listener) {
    listeners_.remove(listener);
}

void LogicMixerPanel::paint(juce::Graphics& g) {
    drawBackground(g);
}

void LogicMixerPanel::resized() {
    auto bounds = getLocalBounds();

    // Layout master section on the right
    int masterWidth = 150;
    masterSection_->setBounds(bounds.removeFromRight(masterWidth));

    // Layout VCA section if visible
    if (showVCAs_) {
        int vcaWidth = 120;
        vcaSection_->setBounds(bounds.removeFromRight(vcaWidth));
    }

    // Layout viewport for channel strips
    viewport_->setBounds(bounds);
    updateLayout();
}

void LogicMixerPanel::mouseDown(const juce::MouseEvent& event) {
    // Handle background clicks
}

void LogicMixerPanel::mouseDrag(const juce::MouseEvent& event) {
    // Handle background dragging
}

void LogicMixerPanel::mouseUp(const juce::MouseEvent& event) {
    // Handle background release
}

bool LogicMixerPanel::keyPressed(const juce::KeyPress& key) {
    // Handle keyboard shortcuts
    if (key == juce::KeyPress('m', juce::ModifierKeys::commandModifier)) {
        // Toggle mute for selected track
        return true;
    } else if (key == juce::KeyPress('s', juce::ModifierKeys::commandModifier)) {
        // Toggle solo for selected track
        return true;
    }

    return false;
}

void LogicMixerPanel::timerCallback() {
    // Update meters and other real-time UI elements
    for (auto& [trackId, strip] : channelStrips_) {
        // Update meter displays
        strip->repaint();
    }
}

void LogicMixerPanel::updateLayout() {
    if (!channelContainer_) return;

    int x = 0;
    int totalWidth = trackWidth_ * static_cast<int>(channelStrips_.size());

    // Position channel strips
    for (auto& [trackId, strip] : channelStrips_) {
        strip->setBounds(x, 0, trackWidth_, getHeight());
        x += trackWidth_;
    }

    // Update container size
    channelContainer_->setSize(totalWidth, getHeight());
}

void LogicMixerPanel::updateChannelPositions() {
    updateLayout();
}

void LogicMixerPanel::handleSoloLogic(const std::string& trackId, bool soloed) {
    if (soloed) {
        // If this track is being soloed, make it the only soloed track
        if (soloedTrack_ != trackId) {
            // Unsolo previous track
            if (!soloedTrack_.empty()) {
                auto it = channelStrips_.find(soloedTrack_);
                if (it != channelStrips_.end()) {
                    LogicMixerChannel::ChannelStrip strip = it->second->strip_;
                    strip.solo = false;
                    it->second->updateStrip(strip);
                    listeners_.call(&Listener::trackSoloChanged, soloedTrack_, false);
                }
            }
            soloedTrack_ = trackId;
        }
    } else {
        // If this track is being unsoloed and it's the soloed track
        if (soloedTrack_ == trackId) {
            soloedTrack_.clear();
        }
    }
}

void LogicMixerPanel::updateAllMuteStates() {
    // Update mute states based on solo logic
    for (auto& [trackId, strip] : channelStrips_) {
        LogicMixerChannel::ChannelStrip channelStrip = strip->strip_;

        // If any track is soloed, mute all non-soloed tracks
        if (!soloedTrack_.empty() && trackId != soloedTrack_) {
            channelStrip.mute = true;
        } else {
            // Restore original mute state
            channelStrip.mute = mutedTracks_.count(trackId) > 0;
        }

        strip->updateStrip(channelStrip);
    }
}

void LogicMixerPanel::drawBackground(juce::Graphics& g) {
    auto bounds = getLocalBounds();

    // Draw mixer background
    g.fillAll(ampl::Theme::colors.backgroundDark);

    // Draw horizontal lines
    g.setColour(ampl::Theme::colors.surface);
    for (int i = 0; i < bounds.getHeight(); i += 50) {
        g.drawHorizontalLine(i, 0, bounds.getWidth());
    }
}

void LogicMixerPanel::drawDivider(juce::Graphics& g, int x) {
    g.setColour(ampl::Theme::colors.surface);
    g.drawVerticalLine(x, 0, getHeight());
}

void LogicMixerPanel::drawTrackHeaders(juce::Graphics& g) {
    // Draw track headers if needed
}

// ChannelStripComponent implementation
LogicMixerPanel::ChannelStripComponent::ChannelStripComponent(
    const std::string& trackId,
    const LogicMixerChannel::ChannelStrip& strip,
    LogicMixerPanel& parent)
    : trackId_(trackId), strip_(strip), parent_(parent) {

    // Setup volume slider
    volumeSlider_ = std::make_unique<juce::Slider>(juce::Slider::LinearVertical,
                                                   juce::Slider::NoTextBox);
    volumeSlider_->setRange(-60.0, 12.0, 0.1);
    volumeSlider_->setValue(strip_.volume);
    volumeSlider_->onValueChange = [this](float value) { handleVolumeChange(value); };
    addAndMakeVisible(volumeSlider_.get());

    // Setup pan slider
    panSlider_ = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal,
                                               juce::Slider::NoTextBox);
    panSlider_->setRange(-1.0, 1.0, 0.01);
    panSlider_->setValue(strip_.pan);
    panSlider_->onValueChange = [this](float value) { handlePanChange(value); };
    addAndMakeVisible(panSlider_.get());

    // Setup mute button
    muteButton_ = std::make_unique<juce::TextButton>("M");
    muteButton_->setColour(juce::TextButton::buttonColourId, strip_.mute ?
                          ampl::Theme::colors.accent : ampl::Theme::colors.surface);
    muteButton_->onClick = [this]() { handleMuteToggle(); };
    addAndMakeVisible(muteButton_.get());

    // Setup solo button
    soloButton_ = std::make_unique<juce::TextButton>("S");
    soloButton_->setColour(juce::TextButton::buttonColourId, strip_.solo ?
                          ampl::Theme::colors.accent : ampl::Theme::colors.surface);
    soloButton_->onClick = [this]() { handleSoloToggle(); };
    addAndMakeVisible(soloButton_.get());

    // Setup record arm button
    recordArmButton_ = std::make_unique<juce::TextButton>("R");
    recordArmButton_->setColour(juce::TextButton::buttonColourId, strip_.recordArm ?
                               ampl::Theme::colors.warning : ampl::Theme::colors.surface);
    recordArmButton_->onClick = [this]() { handleRecordArmToggle(); };
    addAndMakeVisible(recordArmButton_.get());

    // Setup plugin slot buttons
    for (int i = 0; i < LogicMixerChannel::kPluginSlots; ++i) {
        auto button = std::make_unique<juce::TextButton>(std::to_string(i + 1));
        button->setColour(juce::TextButton::buttonColourId, ampl::Theme::colors.surface);
        button->onClick = [this, i]() { handlePluginSlotClick(i); };
        pluginButtons_[i] = std::move(button);
        addAndMakeVisible(pluginButtons_[i].get());
    }

    // Setup send controls
    for (int i = 0; i < 8; ++i) {
        auto slider = std::make_unique<juce::Slider>(juce::Slider::LinearVertical,
                                                     juce::Slider::NoTextBox);
        slider->setRange(-60.0, 12.0, 0.1);
        slider->setValue(strip_.sendLevel[i]);
        slider->onValueChange = [this, i](float value) { handleSendChange(i, value); };
        sendSliders_[i] = std::move(slider);
        addAndMakeVisible(sendSliders_[i].get());

        auto button = std::make_unique<juce::TextButton>("S" + std::to_string(i + 1));
        button->setColour(juce::TextButton::buttonColourId, ampl::Theme::colors.surface);
        sendButtons_[i] = std::move(button);
        addAndMakeVisible(sendButtons_[i].get());
    }

    // Setup VCA assignment
    vcaComboBox_ = std::make_unique<juce::ComboBox>();
    vcaComboBox_->onChange = [this]() {
        handleVCAAssignment(vcaComboBox_->getText().toStdString());
    };
    addAndMakeVisible(vcaComboBox_.get());

    // Setup name label
    nameLabel_ = std::make_unique<juce::Label>();
    nameLabel_->setText(strip_.name, juce::dontSendNotification);
    nameLabel_->setColour(juce::Label::textColourId, ampl::Theme::colors.textPrimary);
    nameLabel_->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(nameLabel_.get());

    setSize(kChannelWidth, 400);
}

void LogicMixerPanel::ChannelStripComponent::paint(juce::Graphics& g) {
    drawChannelBackground(g);

    if (parent_.showPluginSlots_) {
        drawPluginSlots(g);
    }

    if (parent_.showSends_) {
        drawSends(g);
    }

    drawControls(g);
    drawMeters(g);
}

void LogicMixerPanel::ChannelStripComponent::resized() {
    auto bounds = getLocalBounds();
    int y = 0;

    // Name label at top
    nameLabel_->setBounds(bounds.removeFromTop(20));

    // Plugin slots
    if (parent_.showPluginSlots_) {
        auto pluginArea = bounds.removeFromTop(LogicMixerChannel::kPluginSlots * kPluginSlotHeight);
        for (int i = 0; i < LogicMixerChannel::kPluginSlots; ++i) {
            pluginButtons_[i]->setBounds(pluginArea.removeFromTop(kPluginSlotHeight));
        }
    }

    // Sends
    if (parent_.showSends_) {
        auto sendArea = bounds.removeFromTop(8 * kSendHeight);
        for (int i = 0; i < 8; ++i) {
            int sendWidth = sendArea.getWidth() / 8;
            auto sendBounds = sendArea.removeFromLeft(sendWidth);
            sendButtons_[i]->setBounds(sendBounds.removeFromTop(10));
            sendSliders_[i]->setBounds(sendBounds);
        }
    }

    // VCA assignment
    if (parent_.showVCAs_) {
        vcaComboBox_->setBounds(bounds.removeFromTop(25));
    }

    // Meter area
    auto meterArea = bounds.removeFromTop(kMeterHeight);
    // meterComponent_->setBounds(meterArea);

    // Controls (volume, pan, mute, solo, record)
    auto controlArea = bounds.removeFromTop(kControlHeight);

    // Volume slider on the right
    int volumeWidth = 30;
    volumeSlider_->setBounds(controlArea.removeFromRight(volumeWidth));

    // Control buttons
    int buttonWidth = controlArea.getWidth() / 4;
    muteButton_->setBounds(controlArea.removeFromLeft(buttonWidth));
    soloButton_->setBounds(controlArea.removeFromLeft(buttonWidth));
    recordArmButton_->setBounds(controlArea.removeFromLeft(buttonWidth));

    // Pan slider in remaining space
    panSlider_->setBounds(controlArea);
}

void LogicMixerPanel::ChannelStripComponent::mouseDown(const juce::MouseEvent& event) {
    // Handle channel strip selection
}

void LogicMixerPanel::ChannelStripComponent::mouseDrag(const juce::MouseEvent& event) {
    // Handle channel strip dragging
}

void LogicMixerPanel::ChannelStripComponent::mouseUp(const juce::MouseEvent& event) {
    // Handle channel strip release
}

bool LogicMixerPanel::ChannelStripComponent::keyPressed(const juce::KeyPress& key) {
    // Handle channel strip keyboard shortcuts
    return false;
}

void LogicMixerPanel::ChannelStripComponent::updateStrip(const LogicMixerChannel::ChannelStrip& strip) {
    strip_ = strip;
    updateVolumeSlider();
    updatePanSlider();
    updateMuteButton();
    updateSoloButton();
    updateRecordArmButton();
    updatePluginButtons();
    updateSendControls();
    updateVCAAssignment();
    nameLabel_->setText(strip.name, juce::dontSendNotification);

    repaint();
}

void LogicMixerPanel::ChannelStripComponent::updateVolumeSlider() {
    volumeSlider_->setValue(strip_.volume);
}

void LogicMixerPanel::ChannelStripComponent::updatePanSlider() {
    panSlider_->setValue(strip_.pan);
}

void LogicMixerPanel::ChannelStripComponent::updateMuteButton() {
    muteButton_->setColour(juce::TextButton::buttonColourId, strip_.mute ?
                          ampl::Theme::colors.accent : ampl::Theme::colors.surface);
}

void LogicMixerPanel::ChannelStripComponent::updateSoloButton() {
    soloButton_->setColour(juce::TextButton::buttonColourId, strip_.solo ?
                          ampl::Theme::colors.warning : ampl::Theme::colors.surface);
}

void LogicMixerPanel::ChannelStripComponent::updateRecordArmButton() {
    recordArmButton_->setColour(juce::TextButton::buttonColourId, strip_.recordArm ?
                               ampl::Theme::colors.warning : ampl::Theme::colors.surface);
}

void LogicMixerPanel::ChannelStripComponent::updatePluginButtons() {
    for (int i = 0; i < LogicMixerChannel::kPluginSlots; ++i) {
        if (!strip_.pluginChain[i].empty()) {
            pluginButtons_[i]->setButtonText(strip_.pluginChain[i]);
            if (strip_.pluginBypass[i]) {
                pluginButtons_[i]->setColour(juce::TextButton::buttonColourId, ampl::Theme::colors.surface);
            } else {
                pluginButtons_[i]->setColour(juce::TextButton::buttonColourId, ampl::Theme::colors.accent);
            }
        } else {
            pluginButtons_[i]->setButtonText(std::to_string(i + 1));
            pluginButtons_[i]->setColour(juce::TextButton::buttonColourId, ampl::Theme::colors.surface);
        }
    }
}

void LogicMixerPanel::ChannelStripComponent::updateSendControls() {
    for (int i = 0; i < 8; ++i) {
        sendSliders_[i]->setValue(strip_.sendLevel[i]);
        if (!strip_.sendTargets[i].empty()) {
            sendButtons_[i]->setButtonText(strip_.sendTargets[i]);
            sendButtons_[i]->setColour(juce::TextButton::buttonColourId,
                                       strip_.sendPreFader[i] ? ampl::Theme::colors.accent : ampl::Theme::colors.surface);
        } else {
            sendButtons_[i]->setButtonText("S" + std::to_string(i + 1));
            sendButtons_[i]->setColour(juce::TextButton::buttonColourId, ampl::Theme::colors.surface);
        }
    }
}

void LogicMixerPanel::ChannelStripComponent::updateVCAAssignment() {
    vcaComboBox_->clear();
    vcaComboBox_->addItem("None", 1);
    vcaComboBox_->setSelectedId(1);

    // Add VCA options from parent
    // This would need access to the environment
}

void LogicMixerPanel::ChannelStripComponent::handleVolumeChange(float dB) {
    strip_.volume = dB;
    parent_.listeners_.call(&Listener::trackVolumeChanged, trackId_, dB);
}

void LogicMixerPanel::ChannelStripComponent::handlePanChange(float pan) {
    strip_.pan = pan;
    parent_.listeners_.call(&Listener::trackPanChanged, trackId_, pan);
}

void LogicMixerPanel::ChannelStripComponent::handleMuteToggle() {
    strip_.mute = !strip_.mute;
    updateMuteButton();
    parent_.handleMuteState(trackId_, strip_.mute);
    parent_.listeners_.call(&Listener::trackMuteChanged, trackId_, strip_.mute);
}

void LogicMixerPanel::ChannelStripComponent::handleSoloToggle() {
    strip_.solo = !strip_.solo;
    updateSoloButton();
    parent_.handleSoloState(trackId_, strip_.solo);
    parent_.listeners_.call(&Listener::trackSoloChanged, trackId_, strip_.solo);
}

void LogicMixerPanel::ChannelStripComponent::handleRecordArmToggle() {
    strip_.recordArm = !strip_.recordArm;
    updateRecordArmButton();
    parent_.listeners_.call(&Listener::trackRecordArmChanged, trackId_, strip_.recordArm);
}

void LogicMixerPanel::ChannelStripComponent::handlePluginSlotClick(int slot) {
    parent_.listeners_.call(&Listener::pluginSlotClicked, trackId_, slot);
}

void LogicMixerPanel::ChannelStripComponent::handleSendChange(int sendIndex, float level) {
    strip_.sendLevel[sendIndex] = level;
    parent_.listeners_.call(&Listener::sendLevelChanged, trackId_, sendIndex, level);
}

void LogicMixerPanel::ChannelStripComponent::handleVCAAssignment(const std::string& vcaId) {
    strip_.vcaAssignment = vcaId;
    parent_.listeners_.call(&Listener::vcaAssignmentChanged, trackId_, vcaId);
}

void LogicMixerPanel::ChannelStripComponent::drawChannelBackground(juce::Graphics& g) {
    auto bounds = getLocalBounds();

    // Draw channel background
    g.fillAll(ampl::Theme::colors.backgroundDark);

    // Draw selection highlight if selected
    if (strip_.solo) {
        g.setColour(ampl::Theme::colors.warning.withAlpha(0.2f));
        g.fillRect(bounds);
    }

    // Draw border
    g.setColour(ampl::Theme::colors.surface);
    g.drawRect(bounds, 1);
}

void LogicMixerPanel::ChannelStripComponent::drawPluginSlots(juce::Graphics& g) {
    // Plugin slots are drawn by the buttons themselves
}

void LogicMixerPanel::ChannelStripComponent::drawSends(juce::Graphics& g) {
    // Sends are drawn by the sliders and buttons themselves
}

void LogicMixerPanel::ChannelStripComponent::drawControls(juce::Graphics& g) {
    // Controls are drawn by the components themselves
}

void LogicMixerPanel::ChannelStripComponent::drawMeters(juce::Graphics& g) {
    auto bounds = getLocalBounds();
    int meterAreaTop = 20 + (parent_.showPluginSlots_ ? LogicMixerChannel::kPluginSlots * kPluginSlotHeight : 0) +
                      (parent_.showSends_ ? 8 * kSendHeight : 0) + (parent_.showVCAs_ ? 25 : 0);

    auto meterBounds = bounds.withTop(meterAreaTop).withHeight(kMeterHeight);

    // Draw meter background
    g.setColour(ampl::Theme::colors.surface);
    g.fillRect(meterBounds);

    // Draw meter levels (simplified - would use actual meter data)
    g.setColour(ampl::Theme::colors.accent);
    int meterHeight = static_cast<int>(meterBounds.getHeight() * 0.7f); // 70% level
    g.fillRect(meterBounds.withTrimmedBottom(meterBounds.getHeight() - meterHeight));

    // Draw peak indicator
    g.setColour(ampl::Theme::colors.warning);
    int peakHeight = 5;
    g.fillRect(meterBounds.withTop(meterBounds.getBottom() - peakHeight).withHeight(peakHeight));
}

// MasterSection implementation
LogicMixerPanel::MasterSection::MasterSection(LogicMixerPanel& parent) : parent_(parent) {
    // Setup master volume slider
    masterVolumeSlider_ = std::make_unique<juce::Slider>(juce::Slider::LinearVertical,
                                                        juce::Slider::NoTextBox);
    masterVolumeSlider_->setRange(-60.0, 12.0, 0.1);
    masterVolumeSlider_->setValue(0.0);
    addAndMakeVisible(masterVolumeSlider_.get());

    // Setup master pan slider
    masterPanSlider_ = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal,
                                                     juce::Slider::NoTextBox);
    masterPanSlider_->setRange(-1.0, 1.0, 0.01);
    masterPanSlider_->setValue(0.0);
    addAndMakeVisible(masterPanSlider_.get());

    // Setup master mute button
    masterMuteButton_ = std::make_unique<juce::TextButton>("M");
    masterMuteButton_->setColour(juce::TextButton::buttonColourId, ampl::Theme::colors.surface);
    addAndMakeVisible(masterMuteButton_.get());

    setSize(kMasterWidth, 400);
}

void LogicMixerPanel::MasterSection::updateMasterBus(const LogicEnvironment::Bus& bus) {
    masterVolumeSlider_->setValue(bus.volume);
    masterPanSlider_->setValue(bus.pan);
    masterMuteButton_->setColour(juce::TextButton::buttonColourId,
                                 bus.mute ? ampl::Theme::colors.accent : ampl::Theme::colors.surface);
    repaint();
}

void LogicMixerPanel::MasterSection::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();

    // Draw master section background
    g.fillAll(ampl::Theme::colors.backgroundDarker);

    // Draw "MASTER" label
    g.setColour(ampl::Theme::colors.textPrimary);
    g.setFont(ampl::Theme::fonts.label);
    g.drawText("MASTER", bounds.removeFromTop(20), juce::Justification::centred);

    // Draw border
    g.setColour(ampl::Theme::colors.accent);
    g.drawRect(bounds, 2);
}

void LogicMixerPanel::MasterSection::resized() {
    auto bounds = getLocalBounds().withTrimmedTop(20);

    // Master volume on the right
    int volumeWidth = 40;
    masterVolumeSlider_->setBounds(bounds.removeFromRight(volumeWidth));

    // Master pan at bottom
    int panHeight = 30;
    masterPanSlider_->setBounds(bounds.removeFromBottom(panHeight));

    // Master mute button
    masterMuteButton_->setBounds(bounds.withSizeKeepingCentre(50, 25));
}

void LogicMixerPanel::MasterSection::mouseDown(const juce::MouseEvent& event) {
    // Handle master section interactions
}

void LogicMixerPanel::MasterSection::mouseDrag(const juce::MouseEvent& event) {
    // Handle master section dragging
}

void LogicMixerPanel::MasterSection::mouseUp(const juce::MouseEvent& event) {
    // Handle master section release
}

// VCASection implementation
LogicMixerPanel::VCASection::VCASection(LogicMixerPanel& parent) : parent_(parent) {
    setSize(120, 400);
}

void LogicMixerPanel::VCASection::updateVCAs(const std::vector<LogicEnvironment::VCA>& vcas) {
    // Update VCA components
    vcaComponents_.clear();

    int y = 20;
    for (const auto& vca : vcas) {
        auto component = std::make_unique<juce::Component>();
        component->setBounds(0, y, 100, 30);
        vcaComponents_[vca.id] = std::move(component);
        y += 35;
    }

    resized();
    repaint();
}

void LogicMixerPanel::VCASection::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();

    // Draw VCA section background
    g.fillAll(ampl::Theme::colors.backgroundDark);

    // Draw "VCA" label
    g.setColour(ampl::Theme::colors.textPrimary);
    g.setFont(ampl::Theme::fonts.label);
    g.drawText("VCA", bounds.removeFromTop(20), juce::Justification::centred);

    // Draw border
    g.setColour(ampl::Theme::colors.surface);
    g.drawRect(bounds, 1);
}

void LogicMixerPanel::VCASection::resized() {
    // Position VCA components
    int y = 20;
    for (auto& [id, component] : vcaComponents_) {
        component->setBounds(10, y, 100, 30);
        y += 35;
    }
}

} // namespace ampl
