#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "engine/render/OfflineRenderer.hpp"
#include "model/Session.hpp"
#include <atomic>
#include <thread>

namespace ampl {

// Modal-style progress dialog for offline bounce.
// Runs the render on a background thread and updates a progress bar on the UI thread.
class BounceProgressDialog : public juce::Component,
                             public juce::Timer
{
public:
    BounceProgressDialog(const Session& session, const juce::File& outputFile,
                         const OfflineRenderer::Settings& settings);
    ~BounceProgressDialog() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    // Launch as a dialog window. Returns immediately; dialog closes on completion.
    static void launch(const Session& session, const juce::File& outputFile,
                       const OfflineRenderer::Settings& settings);

private:
    void startRender();
    void onRenderComplete();

    Session sessionCopy_;
    juce::File outputFile_;
    OfflineRenderer::Settings settings_;

    std::unique_ptr<juce::ProgressBar> progressBar_;
    std::unique_ptr<juce::TextButton> cancelButton_;
    std::unique_ptr<juce::Label> statusLabel_;

    double progress_{0.0};
    std::atomic<double> atomicProgress_{0.0};
    std::atomic<bool> cancelFlag_{false};
    std::atomic<bool> renderDone_{false};
    std::atomic<bool> renderSuccess_{false};
    juce::String errorMessage_;
    std::atomic<bool> hasError_{false};

    std::thread renderThread_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BounceProgressDialog)
};

} // namespace ampl
