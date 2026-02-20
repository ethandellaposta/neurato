#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "engine/AudioEngine.h"

namespace neurato {

// Panel for selecting audio device, sample rate, buffer size.
class AudioSettingsPanel : public juce::Component
{
public:
    explicit AudioSettingsPanel(AudioEngine& engine);
    ~AudioSettingsPanel() override;

    void resized() override;

private:
    AudioEngine& engine_;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSettingsPanel)
};

} // namespace neurato
