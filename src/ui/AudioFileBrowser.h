#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <functional>

namespace neurato {

// A sidebar panel that lets the user browse audio files from a chosen directory.
// Files can be dragged or double-clicked to add them to the session.
class AudioFileBrowser : public juce::Component,
                         public juce::FileBrowserListener
{
public:
    AudioFileBrowser();
    ~AudioFileBrowser() override;

    // Callback when the user wants to import a file (double-click or button)
    std::function<void(const juce::File&)> onFileSelected;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // FileBrowserListener
    void selectionChanged() override;
    void fileClicked(const juce::File& file, const juce::MouseEvent& e) override;
    void fileDoubleClicked(const juce::File& file) override;
    void browserRootChanged(const juce::File& newRoot) override;

private:
    juce::WildcardFileFilter fileFilter_;
    std::unique_ptr<juce::FileBrowserComponent> browser_;
    std::unique_ptr<juce::TextButton> importButton_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioFileBrowser)
};

} // namespace neurato
