#pragma once

#include "engine/core/AudioEngine.hpp"
#include <juce_audio_utils/juce_audio_utils.h>

namespace ampl
{

// Panel for selecting audio device, sample rate, buffer size, and plugin scanning.
class AudioSettingsPanel : public juce::Component
{
  public:
    explicit AudioSettingsPanel(AudioEngine &engine);
    ~AudioSettingsPanel() override;

    void resized() override;

  private:
    void startPluginScan();

    AudioEngine &engine_;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector_;
    juce::TextButton scanButton_{"Scan Plugins"};
    juce::Label scanStatusLabel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSettingsPanel)
};

} // namespace ampl
