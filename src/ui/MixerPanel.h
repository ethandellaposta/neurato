#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "model/Session.h"
#include "engine/AudioEngine.h"
#include "commands/CommandManager.h"
#include <functional>

namespace neurato {

// A single channel strip in the mixer: gain slider, pan knob, mute/solo buttons,
// track name label, and a simple peak meter.
class ChannelStrip : public juce::Component
{
public:
    ChannelStrip();

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setTrackIndex(int index) { trackIndex_ = index; }
    int getTrackIndex() const { return trackIndex_; }

    void setTrackName(const juce::String& name);
    void setGainDb(float db);
    void setPan(float pan);
    void setMuted(bool muted);
    void setSoloed(bool soloed);
    void setPeakLevel(float peakL, float peakR);

    // Callbacks
    std::function<void(int, float)> onGainChanged;     // (trackIndex, gainDb)
    std::function<void(int, float)> onPanChanged;       // (trackIndex, pan)
    std::function<void(int, bool)>  onMuteToggled;      // (trackIndex, muted)
    std::function<void(int, bool)>  onSoloToggled;      // (trackIndex, soloed)
    std::function<void(int)>        onRemoveTrack;      // (trackIndex)

    static constexpr int kStripWidth = 80;

private:
    int trackIndex_{0};

    juce::Label nameLabel_;
    juce::Slider gainSlider_;
    juce::Slider panSlider_;
    juce::TextButton muteButton_{"M"};
    juce::TextButton soloButton_{"S"};
    juce::TextButton removeButton_{"X"};

    float peakL_{0.0f};
    float peakR_{0.0f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelStrip)
};

// The mixer panel: a row of ChannelStrips (one per track) + a master strip.
// Shown at the bottom of the main window.
class MixerPanel : public juce::Component
{
public:
    MixerPanel(Session& session, CommandManager& commandManager);

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Rebuild channel strips from session state
    void rebuildStrips();

    // Update peak meters (called from timer)
    void updateMeters();

    // Callbacks for session changes
    std::function<void()> onSessionChanged;

private:
    void setupMasterStrip();

    Session& session_;
    CommandManager& commandManager_;

    std::vector<std::unique_ptr<ChannelStrip>> strips_;

    // Master strip controls
    juce::Label masterLabel_;
    juce::Slider masterGainSlider_;
    juce::Slider masterPanSlider_;

    juce::Viewport viewport_;
    juce::Component stripContainer_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerPanel)
};

} // namespace neurato
