#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "engine/core/AudioEngine.hpp"

namespace ampl {

// Displays a single audio track: name, waveform thumbnail, gain slider, mute button.
class TrackView : public juce::Component
{
public:
    explicit TrackView(AudioEngine& engine);
    ~TrackView() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updateWaveform();

private:
    AudioEngine& engine_;

    juce::Label trackNameLabel_{"", "Track 1"};
    juce::TextButton loadButton_{"Load Audio"};
    juce::Slider gainSlider_;
    juce::ToggleButton muteButton_{"M"};
    juce::Label fileInfoLabel_{"", "No file loaded"};

    // Waveform thumbnail cache
    std::vector<float> waveformPeaks_;
    bool waveformDirty_{true};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackView)
};

} // namespace ampl
