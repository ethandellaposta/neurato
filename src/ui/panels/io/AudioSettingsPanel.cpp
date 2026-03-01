#include "ui/panels/io/AudioSettingsPanel.hpp"
#include "engine/plugins/manager/PluginManager.hpp"

namespace ampl
{

AudioSettingsPanel::AudioSettingsPanel(AudioEngine &engine) : engine_(engine)
{
    deviceSelector_ = std::make_unique<juce::AudioDeviceSelectorComponent>(
        engine_.getDeviceManager(), 0, 0, // min/max input channels
        1, 2,                             // min/max output channels
        false,                            // show MIDI inputs
        false,                            // show MIDI outputs
        false,                            // show channels as stereo pairs
        false                             // hide advanced options
    );
    addAndMakeVisible(deviceSelector_.get());

    scanButton_.onClick = [this] { startPluginScan(); };
    addAndMakeVisible(scanButton_);

    scanStatusLabel_.setJustificationType(juce::Justification::centredLeft);
    scanStatusLabel_.setFont(juce::Font(13.0f));
    addAndMakeVisible(scanStatusLabel_);

    // Show current known plugin count on open
    if (auto *pm = engine_.getPluginManager())
    {
        int n = pm->getKnownPluginList().getNumTypes();
        scanStatusLabel_.setText(n > 0 ? juce::String(n) + " plugins known"
                                       : "No plugins scanned yet",
                                 juce::dontSendNotification);
    }
}

AudioSettingsPanel::~AudioSettingsPanel() = default;

void AudioSettingsPanel::startPluginScan()
{
    auto *pm = engine_.getPluginManager();
    if (!pm)
        return;

    scanButton_.setEnabled(false);
    scanStatusLabel_.setText("Scanning…", juce::dontSendNotification);

    // Run on a background thread so the dialog stays responsive.
    juce::Thread::launch(
        [this, pm]
        {
            pm->refreshPluginList();
            int n = pm->getKnownPluginList().getNumTypes();
            juce::MessageManager::callAsync(
                [this, n]
                {
                    scanButton_.setEnabled(true);
                    scanStatusLabel_.setText("Scan complete \u2014 " + juce::String(n) + " plugin" +
                                                 (n == 1 ? "" : "s") + " found",
                                             juce::dontSendNotification);
                });
        });
}

void AudioSettingsPanel::resized()
{
    auto area = getLocalBounds().reduced(8);

    // Plugin scan row at the bottom
    auto scanRow = area.removeFromBottom(28);
    area.removeFromBottom(6); // gap
    scanButton_.setBounds(scanRow.removeFromLeft(130));
    scanRow.removeFromLeft(8);
    scanStatusLabel_.setBounds(scanRow);

    if (deviceSelector_)
        deviceSelector_->setBounds(area);
}

} // namespace ampl
