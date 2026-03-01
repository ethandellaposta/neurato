#include "ui/timeline/TrackView.hpp"

using namespace juce;

namespace ampl
{

TrackView::TrackView(AudioEngine &engine) : engine_(engine)
{
    // Track name
    trackNameLabel_.setFont(juce::Font("Inter", 16.0f, juce::Font::bold));
    trackNameLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF222222));
    addAndMakeVisible(trackNameLabel_);

    // Load button
    loadButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2D2D2D));
    loadButton_.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    loadButton_.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    loadButton_.onClick = [this]
    {
        auto chooser = std::make_shared<juce::FileChooser>("Select an audio file...", juce::File{},
                                                           "*.wav;*.aiff;*.aif;*.mp3;*.flac;*.ogg");

        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser &fc)
            {
                auto file = fc.getResult();
                if (file.existsAsFile())
                {
                    if (engine_.loadTrackAudio(file))
                    {
                        auto &track = engine_.getTrack();
                        double lengthSecs =
                            static_cast<double>(track.getLengthInSamples()) / track.getSampleRate();
                        fileInfoLabel_.setText(track.getFileName() + " | " +
                                                   juce::String(track.getNumChannels()) + "ch | " +
                                                   juce::String(track.getSampleRate() / 1000.0, 1) +
                                                   "kHz | " + juce::String(lengthSecs, 1) + "s",
                                               juce::dontSendNotification);
                        updateWaveform();
                        repaint();
                    }
                }
            });
    };
    addAndMakeVisible(loadButton_);

    // Gain slider
    gainSlider_.setRange(-60.0, 6.0, 0.1);
    gainSlider_.setValue(0.0);
    gainSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    gainSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    gainSlider_.setTextValueSuffix(" dB");
    gainSlider_.setColour(juce::Slider::thumbColourId, juce::Colour(0xFF4A90E2));
    gainSlider_.setColour(juce::Slider::trackColourId, juce::Colour(0xFFB0B0B0));
    gainSlider_.setColour(juce::Slider::backgroundColourId, juce::Colour(0xFFF5F5F5));
    gainSlider_.onValueChange = [this]
    {
        float linearGain =
            juce::Decibels::decibelsToGain(static_cast<float>(gainSlider_.getValue()));
        engine_.sendSetTrackGain(linearGain);
    };
    addAndMakeVisible(gainSlider_);

    // Mute button
    muteButton_.setColour(juce::ToggleButton::textColourId, juce::Colour(0xFF222222));
    muteButton_.onClick = [this] { engine_.sendSetTrackMute(muteButton_.getToggleState()); };
    addAndMakeVisible(muteButton_);

    // File info
    fileInfoLabel_.setFont(juce::Font("Inter", 12.0f, juce::Font::plain));
    fileInfoLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
    addAndMakeVisible(fileInfoLabel_);
}

TrackView::~TrackView() = default;

void TrackView::paint(juce::Graphics &g)
{
    auto bounds = getLocalBounds();

    // Background
    g.setColour(juce::Colour(0xff16213e));
    g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

    // Waveform area
    auto waveformArea = bounds.reduced(8);
    waveformArea.removeFromTop(32);    // header
    waveformArea.removeFromBottom(32); // controls

    if (!waveformPeaks_.empty())
    {
        g.setColour(juce::Colour(0xff0a0a1a));
        g.fillRect(waveformArea);

        const float midY = waveformArea.getCentreY();
        const float halfHeight = waveformArea.getHeight() * 0.45f;
        const int numPeaks = static_cast<int>(waveformPeaks_.size());
        const float xScale =
            static_cast<float>(waveformArea.getWidth()) / static_cast<float>(numPeaks);

        g.setColour(juce::Colour(0xff3498db));
        for (int i = 0; i < numPeaks; ++i)
        {
            float x = waveformArea.getX() + static_cast<float>(i) * xScale;
            float peakHeight = waveformPeaks_[static_cast<size_t>(i)] * halfHeight;
            g.drawVerticalLine(static_cast<int>(x), midY - peakHeight, midY + peakHeight);
        }

        // Center line
        g.setColour(juce::Colour(0xff444466));
        g.drawHorizontalLine(static_cast<int>(midY), static_cast<float>(waveformArea.getX()),
                             static_cast<float>(waveformArea.getRight()));
    }
    else
    {
        g.setColour(juce::Colour(0xff0a0a1a));
        g.fillRect(waveformArea);
        g.setColour(juce::Colour(0xff555555));
        g.drawText("Drop or load an audio file", waveformArea, juce::Justification::centred);
    }
}

void TrackView::resized()
{
    auto area = getLocalBounds().reduced(8);
    // Header row with Grid
    auto header = area.removeFromTop(28);
    juce::Grid headerGrid;
    headerGrid.templateRows = {juce::Grid::TrackInfo(1_fr)};
    headerGrid.templateColumns = {juce::Grid::TrackInfo(100_px), juce::Grid::TrackInfo(8_px),
                                  juce::Grid::TrackInfo(100_px), juce::Grid::TrackInfo(8_px),
                                  juce::Grid::TrackInfo(1_fr)};
    headerGrid.items = {juce::GridItem(trackNameLabel_), juce::GridItem(),
                        juce::GridItem(loadButton_), juce::GridItem(),
                        juce::GridItem(fileInfoLabel_)};
    headerGrid.performLayout(header);
    // Bottom controls with Grid
    area.removeFromTop(4);
    auto controls = area.removeFromBottom(28);
    juce::Grid controlsGrid;
    controlsGrid.templateRows = {juce::Grid::TrackInfo(1_fr)};
    controlsGrid.templateColumns = {juce::Grid::TrackInfo(40_px), juce::Grid::TrackInfo(8_px),
                                    juce::Grid::TrackInfo(200_px)};
    controlsGrid.items = {juce::GridItem(muteButton_), juce::GridItem(),
                          juce::GridItem(gainSlider_)};
    controlsGrid.performLayout(controls);
    // Remaining area is for waveform (handled in paint)
    if (waveformDirty_)
        updateWaveform();
}

void TrackView::updateWaveform()
{
    waveformPeaks_.clear();

    const auto &track = engine_.getTrack();
    if (!track.hasAudio())
    {
        waveformDirty_ = false;
        return;
    }

    // Generate waveform peaks for display
    // We read from the track's loaded audio data
    // This is called on the UI thread, which is safe since we only read
    const int displayWidth = std::max(getWidth() - 16, 100);
    const SampleCount totalSamples = track.getLengthInSamples();
    (void)totalSamples;

    waveformPeaks_.resize(static_cast<size_t>(displayWidth), 0.0f);

    // We need to access the raw audio data — for now, we'll re-read the file
    // In a future milestone, we'll cache waveform thumbnails properly
    // For MVP, we generate peaks by processing through the track
    // Actually, let's just mark it as needing update and use a simple approach

    waveformDirty_ = false;

    // For now, generate a simple placeholder based on track length
    // Real waveform rendering will come in Milestone 2 with the timeline
    for (size_t i = 0; i < waveformPeaks_.size(); ++i)
    {
        // Simple sine-based placeholder to show something
        double phase = static_cast<double>(i) / static_cast<double>(waveformPeaks_.size());
        waveformPeaks_[i] = static_cast<float>(0.3 + 0.7 * std::abs(std::sin(phase * 20.0)));
    }
}

} // namespace ampl
