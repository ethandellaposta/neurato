#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "engine/AudioEngine.h"

namespace neurato {

// Transport bar UI: play/stop, BPM, time display, metronome toggle.
class TransportBar : public juce::Component
{
public:
    explicit TransportBar(AudioEngine& engine);
    ~TransportBar() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Called by parent to dispatch audio thread messages
    void handleAudioMessage(const AudioToUIMessage& msg);

    // Called by parent on timer tick to refresh display state
    void updateDisplay();

private:
    AudioEngine& engine_;

    juce::TextButton playButton_{"Play"};
    juce::TextButton stopButton_{"Stop"};
    juce::Slider bpmSlider_;
    juce::Label bpmLabel_{"", "BPM"};
    juce::Label timeDisplay_{"", "0:00.000"};
    juce::Label beatDisplay_{"", "1.1.0"};
    juce::ToggleButton metronomeToggle_{"Click"};
    juce::Slider metronomeGainSlider_;

    // Peak level display
    float peakL_{0.0f};
    float peakR_{0.0f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportBar)
};

} // namespace neurato
