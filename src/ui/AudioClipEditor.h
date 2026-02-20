#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "model/Session.h"
#include "commands/CommandManager.h"
#include "ui/Theme.h"
#include <functional>

namespace neurato {

// Detailed audio clip editor: zoomed waveform view with gain/fade handles.
// Opened when double-clicking an audio clip on the timeline.
class AudioClipEditor : public juce::Component
{
public:
    AudioClipEditor(Session& session, CommandManager& commandManager);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    void setClip(int trackIndex, const juce::String& clipId);
    void clearClip();
    bool hasClip() const { return !clipId_.isEmpty(); }

    std::function<void()> onSessionChanged;
    std::function<void()> onCloseRequested;

private:
    void paintWaveform(juce::Graphics& g, juce::Rectangle<int> area);
    void paintFadeHandles(juce::Graphics& g, juce::Rectangle<int> area);
    void paintInfo(juce::Graphics& g, juce::Rectangle<int> area);

    Clip* getClip();
    const Clip* getClip() const;

    Session& session_;
    CommandManager& commandManager_;

    int trackIndex_{-1};
    juce::String clipId_;

    double pixelsPerSample_{0.01};
    SampleCount scrollSamples_{0};

    enum class DragMode { None, FadeIn, FadeOut, Gain };
    DragMode dragMode_{DragMode::None};
    float dragStartValue_{0.0f};
    int dragStartY_{0};

    juce::TextButton closeBtn_{"X"};

    // Waveform cache
    std::vector<float> waveformPeaks_;
    double cachedPPS_{0.0};

    static constexpr int kToolbarHeight = 28;
    static constexpr int kInfoHeight = 24;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioClipEditor)
};

} // namespace neurato
