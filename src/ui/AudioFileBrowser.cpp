#include "ui/AudioFileBrowser.h"

namespace neurato {

AudioFileBrowser::AudioFileBrowser()
    : fileFilter_("*.wav;*.aiff;*.aif;*.mp3;*.flac;*.ogg", "*", "Audio Files")
{
    auto defaultDir = juce::File::getSpecialLocation(juce::File::userHomeDirectory);

    browser_ = std::make_unique<juce::FileBrowserComponent>(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        defaultDir, &fileFilter_, nullptr);
    browser_->addListener(this);
    addAndMakeVisible(browser_.get());

    importButton_ = std::make_unique<juce::TextButton>("Import Selected");
    importButton_->setEnabled(false);
    importButton_->onClick = [this] {
        auto file = browser_->getSelectedFile(0);
        if (file.existsAsFile() && onFileSelected)
            onFileSelected(file);
    };
    addAndMakeVisible(importButton_.get());
}

AudioFileBrowser::~AudioFileBrowser()
{
    browser_->removeListener(this);
}

void AudioFileBrowser::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff12122a));

    g.setColour(juce::Colour(0xffa0a0c0));
    g.setFont(14.0f);
    g.drawText("Audio Files", getLocalBounds().removeFromTop(24), juce::Justification::centred);
}

void AudioFileBrowser::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(26); // Title
    importButton_->setBounds(area.removeFromBottom(28).reduced(4, 2));
    browser_->setBounds(area.reduced(2));
}

void AudioFileBrowser::selectionChanged()
{
    auto file = browser_->getSelectedFile(0);
    importButton_->setEnabled(file.existsAsFile());
}

void AudioFileBrowser::fileClicked(const juce::File& /*file*/, const juce::MouseEvent& /*e*/)
{
}

void AudioFileBrowser::fileDoubleClicked(const juce::File& file)
{
    if (file.existsAsFile() && onFileSelected)
        onFileSelected(file);
}

void AudioFileBrowser::browserRootChanged(const juce::File& /*newRoot*/)
{
}

} // namespace neurato
