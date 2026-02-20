#include "ui/AudioSettingsPanel.h"

namespace neurato {

AudioSettingsPanel::AudioSettingsPanel(AudioEngine& engine)
    : engine_(engine)
{
    deviceSelector_ = std::make_unique<juce::AudioDeviceSelectorComponent>(
        engine_.getDeviceManager(),
        0, 0,   // min/max input channels
        1, 2,   // min/max output channels
        false,   // show MIDI inputs
        false,   // show MIDI outputs
        false,   // show channels as stereo pairs
        false    // hide advanced options
    );

    addAndMakeVisible(deviceSelector_.get());
}

AudioSettingsPanel::~AudioSettingsPanel() = default;

void AudioSettingsPanel::resized()
{
    if (deviceSelector_)
        deviceSelector_->setBounds(getLocalBounds().reduced(8));
}

} // namespace neurato
