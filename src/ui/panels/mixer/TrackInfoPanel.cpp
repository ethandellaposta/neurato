#include "ui/panels/mixer/TrackInfoPanel.hpp"
#include "commands/ClipCommands.hpp"
#include "engine/plugins/manager/PluginManager.hpp"
#include <cmath>

namespace ampl
{

TrackInfoPanel::TrackInfoPanel(Session &session, CommandManager &commandManager)
    : session_(session), commandManager_(commandManager)
{
    titleLabel_.setText("Inspector", juce::dontSendNotification);
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setColour(juce::Label::textColourId, juce::Colour(Theme::textPrimary));
    titleLabel_.setFont(Theme::headingFont());
    addAndMakeVisible(titleLabel_);

    trackNameLabel_.setText("Track Name", juce::dontSendNotification);
    trackNameLabel_.setColour(juce::Label::textColourId, juce::Colour(Theme::textSecondary));
    trackNameLabel_.setFont(Theme::smallFont());
    addAndMakeVisible(trackNameLabel_);

    trackNameEditor_.setComponentID("trackInfo.trackName");
    trackNameEditor_.setMultiLine(false);
    trackNameEditor_.setReturnKeyStartsNewLine(false);
    trackNameEditor_.setTextToShowWhenEmpty("Select a track", juce::Colour(Theme::textDim));
    trackNameEditor_.setColour(juce::TextEditor::backgroundColourId,
                               juce::Colour(Theme::bgSurface).withAlpha(0.9f));
    trackNameEditor_.setColour(juce::TextEditor::textColourId, juce::Colour(Theme::textPrimary));
    trackNameEditor_.setColour(juce::TextEditor::outlineColourId,
                               juce::Colour(Theme::borderLight).withAlpha(0.85f));
    trackNameEditor_.onReturnKey = [this] { commitTrackName(); };
    trackNameEditor_.onFocusLost = [this] { commitTrackName(); };
    addAndMakeVisible(trackNameEditor_);

    applyNameButton_.setComponentID("trackInfo.applyName");
    Theme::styleTextButton(applyNameButton_);
    applyNameButton_.onClick = [this] { commitTrackName(); };
    addAndMakeVisible(applyNameButton_);

    gainLabel_.setText("Gain", juce::dontSendNotification);
    gainLabel_.setColour(juce::Label::textColourId, juce::Colour(Theme::textSecondary));
    gainLabel_.setFont(Theme::smallFont());
    addAndMakeVisible(gainLabel_);

    gainSlider_.setComponentID("trackInfo.gain");
    gainSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    gainSlider_.setRange(-60.0, 12.0, 0.1);
    gainSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 18);
    gainSlider_.onValueChange = [this]
    { commitTrackGain(static_cast<float>(gainSlider_.getValue())); };
    Theme::styleSlider(gainSlider_);
    addAndMakeVisible(gainSlider_);

    panLabel_.setText("Pan", juce::dontSendNotification);
    panLabel_.setColour(juce::Label::textColourId, juce::Colour(Theme::textSecondary));
    panLabel_.setFont(Theme::smallFont());
    addAndMakeVisible(panLabel_);

    panSlider_.setComponentID("trackInfo.pan");
    panSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    panSlider_.setRange(-1.0, 1.0, 0.01);
    panSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 18);
    panSlider_.onValueChange = [this]
    { commitTrackPan(static_cast<float>(panSlider_.getValue())); };
    Theme::styleSlider(panSlider_);
    addAndMakeVisible(panSlider_);

    muteButton_.setComponentID("trackInfo.mute");
    muteButton_.setClickingTogglesState(true);
    Theme::styleTextButton(muteButton_);
    muteButton_.setColour(juce::TextButton::buttonOnColourId, juce::Colour(Theme::accentRed));
    muteButton_.onClick = [this] { commitTrackMute(muteButton_.getToggleState()); };
    addAndMakeVisible(muteButton_);

    soloButton_.setComponentID("trackInfo.solo");
    soloButton_.setClickingTogglesState(true);
    Theme::styleTextButton(soloButton_);
    soloButton_.setColour(juce::TextButton::buttonOnColourId, juce::Colour(Theme::accentYellow));
    soloButton_.onClick = [this] { commitTrackSolo(soloButton_.getToggleState()); };
    addAndMakeVisible(soloButton_);

    pluginsLabel_.setText("Plugins", juce::dontSendNotification);
    pluginsLabel_.setColour(juce::Label::textColourId, juce::Colour(Theme::textSecondary));
    pluginsLabel_.setFont(Theme::smallFont());
    addAndMakeVisible(pluginsLabel_);

    Theme::styleTextButton(setInstrumentButton_);
    setInstrumentButton_.setComponentID("trackInfo.setInstrument");
    setInstrumentButton_.onClick = [this] { showInstrumentPicker(); };
    addAndMakeVisible(setInstrumentButton_);

    Theme::styleTextButton(addFxButton_);
    addFxButton_.setComponentID("trackInfo.addFx");
    addFxButton_.onClick = [this] { showFxPicker(); };
    addAndMakeVisible(addFxButton_);

    detailsLabel_.setText("Track Details", juce::dontSendNotification);
    detailsLabel_.setColour(juce::Label::textColourId, juce::Colour(Theme::textSecondary));
    detailsLabel_.setFont(Theme::smallFont());
    addAndMakeVisible(detailsLabel_);

    infoText_.setMultiLine(true);
    infoText_.setReadOnly(true);
    infoText_.setScrollbarsShown(true);
    infoText_.setCaretVisible(false);
    infoText_.setPopupMenuEnabled(true);
    infoText_.setFont(Theme::smallFont());
    infoText_.setColour(juce::TextEditor::backgroundColourId,
                        juce::Colour(Theme::bgSurface).withAlpha(0.75f));
    infoText_.setColour(juce::TextEditor::textColourId, juce::Colour(Theme::textSecondary));
    infoText_.setColour(juce::TextEditor::outlineColourId,
                        juce::Colour(Theme::borderLight).withAlpha(0.85f));
    infoText_.setText("Select a track in the timeline to see details.", juce::dontSendNotification);
    infoText_.setComponentID("trackInfo.details");
    addAndMakeVisible(infoText_);

    updateEditingControls();
}

void TrackInfoPanel::paint(juce::Graphics &g)
{
    auto area = getLocalBounds().toFloat();

    juce::ColourGradient panelGradient(juce::Colour(Theme::bgSurface).brighter(0.04f),
                                       area.getTopLeft(), juce::Colour(Theme::bgBase).darker(0.08f),
                                       area.getBottomRight(), false);
    g.setGradientFill(panelGradient);
    g.fillRoundedRectangle(area.reduced(1.0f), Theme::cornerRadius + 2.0f);

    // Subtle ambient edge glow
    g.setColour(juce::Colour(Theme::accentBlue).withAlpha(0.12f));
    g.drawRoundedRectangle(area.reduced(1.5f), Theme::cornerRadius + 2.0f, 1.5f);
    g.setColour(juce::Colour(Theme::accentBlue).withAlpha(0.20f));
    g.drawLine(8.0f, 1.5f, area.getRight() - 8.0f, 1.5f, 1.2f);

    g.setColour(juce::Colour(Theme::border).withAlpha(0.85f));
    g.drawRoundedRectangle(area.reduced(0.5f), Theme::cornerRadius + 2.0f, 1.0f);
}

void TrackInfoPanel::resized()
{
    auto area = getLocalBounds().reduced(8, 8);

    titleLabel_.setBounds(area.removeFromTop(24));
    area.removeFromTop(4);

    trackNameLabel_.setBounds(area.removeFromTop(18));

    auto trackNameRow = area.removeFromTop(24);
    applyNameButton_.setBounds(trackNameRow.removeFromRight(76));
    trackNameRow.removeFromRight(6);
    trackNameEditor_.setBounds(trackNameRow);

    area.removeFromTop(6);

    gainLabel_.setBounds(area.removeFromTop(18));
    gainSlider_.setBounds(area.removeFromTop(24));

    area.removeFromTop(4);

    panLabel_.setBounds(area.removeFromTop(18));
    panSlider_.setBounds(area.removeFromTop(24));

    area.removeFromTop(6);

    auto buttonRow = area.removeFromTop(24);
    muteButton_.setBounds(buttonRow.removeFromLeft(buttonRow.getWidth() / 2).reduced(0, 0));
    buttonRow.removeFromLeft(6);
    soloButton_.setBounds(buttonRow);

    area.removeFromTop(8);
    pluginsLabel_.setBounds(area.removeFromTop(18));
    area.removeFromTop(2);

    auto instrumentRow = area.removeFromTop(24);
    setInstrumentButton_.setBounds(instrumentRow.removeFromLeft(instrumentRow.getWidth() * 2 / 3));
    instrumentRow.removeFromLeft(4);
    addFxButton_.setBounds(instrumentRow);

    area.removeFromTop(8);
    detailsLabel_.setBounds(area.removeFromTop(18));
    area.removeFromTop(4);
    infoText_.setBounds(area);
}

void TrackInfoPanel::setSelectedTrackIndex(int trackIndex)
{
    if (selectedTrackIndex_ == trackIndex)
        return;

    selectedTrackIndex_ = trackIndex;
    refresh();
}

void TrackInfoPanel::refresh()
{
    updateEditingControls();

    const auto &tracks = session_.getTracks();
    if (selectedTrackIndex_ < 0 || selectedTrackIndex_ >= static_cast<int>(tracks.size()))
    {
        const juce::String emptyText = "Select a track in the timeline to see details.";
        if (lastSummaryText_ != emptyText)
        {
            infoText_.setText(emptyText, juce::dontSendNotification);
            lastSummaryText_ = emptyText;
        }
        return;
    }

    const auto &track = tracks[static_cast<size_t>(selectedTrackIndex_)];
    const auto summary = buildTrackSummary(track, selectedTrackIndex_);
    if (summary != lastSummaryText_)
    {
        infoText_.setText(summary, juce::dontSendNotification);
        lastSummaryText_ = summary;
    }
}

bool TrackInfoPanel::editSelectedTrackName(const juce::String &newName)
{
    auto *track = session_.getTrack(selectedTrackIndex_);
    if (track == nullptr)
        return false;

    const auto before = track->name;
    trackNameEditor_.setText(newName, juce::dontSendNotification);
    commitTrackName();
    return before != track->name;
}

bool TrackInfoPanel::editSelectedTrackGain(float gainDb)
{
    auto *track = session_.getTrack(selectedTrackIndex_);
    if (track == nullptr)
        return false;

    const auto before = track->gainDb;
    commitTrackGain(gainDb);
    return std::abs(before - track->gainDb) > 0.001f;
}

bool TrackInfoPanel::editSelectedTrackPan(float pan)
{
    auto *track = session_.getTrack(selectedTrackIndex_);
    if (track == nullptr)
        return false;

    const auto before = track->pan;
    commitTrackPan(pan);
    return std::abs(before - track->pan) > 0.001f;
}

bool TrackInfoPanel::editSelectedTrackMute(bool muted)
{
    auto *track = session_.getTrack(selectedTrackIndex_);
    if (track == nullptr)
        return false;

    const auto before = track->muted;
    commitTrackMute(muted);
    return before != track->muted;
}

bool TrackInfoPanel::editSelectedTrackSolo(bool solo)
{
    auto *track = session_.getTrack(selectedTrackIndex_);
    if (track == nullptr)
        return false;

    const auto before = track->solo;
    commitTrackSolo(solo);
    return before != track->solo;
}

void TrackInfoPanel::updateEditingControls()
{
    const auto &tracks = session_.getTracks();
    const bool hasTrack =
        selectedTrackIndex_ >= 0 && selectedTrackIndex_ < static_cast<int>(tracks.size());

    isRefreshingUi_ = true;

    trackNameEditor_.setEnabled(hasTrack);
    applyNameButton_.setEnabled(hasTrack);
    gainSlider_.setEnabled(hasTrack);
    panSlider_.setEnabled(hasTrack);
    muteButton_.setEnabled(hasTrack);
    soloButton_.setEnabled(hasTrack);
    setInstrumentButton_.setEnabled(hasTrack);
    addFxButton_.setEnabled(hasTrack);

    if (hasTrack)
    {
        const auto &track = tracks[static_cast<size_t>(selectedTrackIndex_)];
        if (!trackNameEditor_.hasKeyboardFocus(true))
            trackNameEditor_.setText(track.name, juce::dontSendNotification);
        gainSlider_.setValue(track.gainDb, juce::dontSendNotification);
        panSlider_.setValue(track.pan, juce::dontSendNotification);
        muteButton_.setToggleState(track.muted, juce::dontSendNotification);
        soloButton_.setToggleState(track.solo, juce::dontSendNotification);
    }
    else
    {
        trackNameEditor_.setText({}, juce::dontSendNotification);
        gainSlider_.setValue(0.0, juce::dontSendNotification);
        panSlider_.setValue(0.0, juce::dontSendNotification);
        muteButton_.setToggleState(false, juce::dontSendNotification);
        soloButton_.setToggleState(false, juce::dontSendNotification);
    }

    isRefreshingUi_ = false;
}

void TrackInfoPanel::commitTrackName()
{
    if (isRefreshingUi_)
        return;

    auto *track = session_.getTrack(selectedTrackIndex_);
    if (track == nullptr)
        return;

    auto newName = trackNameEditor_.getText().trim();
    if (newName.isEmpty() || newName == track->name)
        return;

    auto cmd = std::make_unique<RenameTrackCommand>(selectedTrackIndex_, newName);
    commandManager_.execute(std::move(cmd), session_);

    if (onSessionChanged)
        onSessionChanged();

    refresh();
}

void TrackInfoPanel::commitTrackGain(float gainDb)
{
    if (isRefreshingUi_)
        return;

    auto *track = session_.getTrack(selectedTrackIndex_);
    if (track == nullptr || std::abs(track->gainDb - gainDb) < 0.01f)
        return;

    auto cmd = std::make_unique<SetTrackGainCommand>(selectedTrackIndex_, gainDb);
    commandManager_.execute(std::move(cmd), session_);

    if (onSessionChanged)
        onSessionChanged();

    refresh();
}

void TrackInfoPanel::commitTrackPan(float pan)
{
    if (isRefreshingUi_)
        return;

    auto *track = session_.getTrack(selectedTrackIndex_);
    if (track == nullptr || std::abs(track->pan - pan) < 0.001f)
        return;

    auto cmd = std::make_unique<SetTrackPanCommand>(selectedTrackIndex_, pan);
    commandManager_.execute(std::move(cmd), session_);

    if (onSessionChanged)
        onSessionChanged();

    refresh();
}

void TrackInfoPanel::commitTrackMute(bool muted)
{
    if (isRefreshingUi_)
        return;

    auto *track = session_.getTrack(selectedTrackIndex_);
    if (track == nullptr || track->muted == muted)
        return;

    auto cmd = std::make_unique<SetTrackMuteCommand>(selectedTrackIndex_, muted);
    commandManager_.execute(std::move(cmd), session_);

    if (onSessionChanged)
        onSessionChanged();

    refresh();
}

void TrackInfoPanel::commitTrackSolo(bool solo)
{
    if (isRefreshingUi_)
        return;

    auto *track = session_.getTrack(selectedTrackIndex_);
    if (track == nullptr || track->solo == solo)
        return;

    auto cmd = std::make_unique<SetTrackSoloCommand>(selectedTrackIndex_, solo);
    commandManager_.execute(std::move(cmd), session_);

    if (onSessionChanged)
        onSessionChanged();

    refresh();
}

void TrackInfoPanel::showInstrumentPicker()
{
    auto *track = session_.getTrack(selectedTrackIndex_);
    if (track == nullptr)
        return;

    juce::PopupMenu menu;

    // Always offer "None" to clear
    menu.addItem(1, "None (clear instrument)", track->instrumentPlugin.has_value());
    menu.addSeparator();

    std::vector<PluginManager::PluginInfo> instruments;
    if (pluginManager_)
        instruments = pluginManager_->getInstruments();

    if (instruments.empty())
    {
        menu.addItem(2, "(No instruments found — scan in Audio Settings)", false);
    }
    else
    {
        int id = 100;
        for (auto &p : instruments)
            menu.addItem(id++, p.description.name + " [" + p.format + "]");
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(setInstrumentButton_),
                       [this, instruments](int result)
                       {
                           if (result == 1)
                           {
                               // Clear instrument
                               auto cmd = std::make_unique<SetTrackInstrumentCommand>(
                                   selectedTrackIndex_, std::nullopt);
                               commandManager_.execute(std::move(cmd), session_);
                               if (onSessionChanged)
                                   onSessionChanged();
                               refresh();
                           }
                           else if (result >= 100)
                           {
                               int idx = result - 100;
                               if (idx < static_cast<int>(instruments.size()))
                               {
                                   const auto &info = instruments[static_cast<size_t>(idx)];
                                   PluginSlot slot;
                                   slot.pluginId = info.description.fileOrIdentifier;
                                   slot.pluginName = info.description.name;
                                   slot.pluginFormat = info.format;
                                   slot.pluginPath = info.description.fileOrIdentifier;
                                   slot.isResolved = true;
                                   auto cmd = std::make_unique<SetTrackInstrumentCommand>(
                                       selectedTrackIndex_, std::move(slot));
                                   commandManager_.execute(std::move(cmd), session_);
                                   if (onSessionChanged)
                                       onSessionChanged();
                                   refresh();
                               }
                           }
                       });
}

void TrackInfoPanel::showFxPicker()
{
    auto *track = session_.getTrack(selectedTrackIndex_);
    if (track == nullptr)
        return;

    juce::PopupMenu menu;

    std::vector<PluginManager::PluginInfo> effects;
    if (pluginManager_)
        effects = pluginManager_->getEffects();

    if (effects.empty())
    {
        menu.addItem(1, "(No plugins found — scan in Audio Settings)", false);
    }
    else
    {
        int id = 100;
        for (auto &p : effects)
            menu.addItem(id++, p.description.name + " [" + p.format + "]");
    }

    // Separator + remove options for existing chain entries
    if (!track->pluginChain.empty())
    {
        menu.addSeparator();
        int id = 1000;
        for (auto &slot : track->pluginChain)
            menu.addItem(id++, "Remove: " + slot.pluginName);
    }

    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(addFxButton_),
        [this, effects, chainSize = static_cast<int>(track->pluginChain.size())](int result)
        {
            if (result >= 100 && result < 1000)
            {
                int idx = result - 100;
                if (idx < static_cast<int>(effects.size()))
                {
                    const auto &info = effects[static_cast<size_t>(idx)];
                    PluginSlot slot;
                    slot.pluginId = info.description.fileOrIdentifier;
                    slot.pluginName = info.description.name;
                    slot.pluginFormat = info.format;
                    slot.pluginPath = info.description.fileOrIdentifier;
                    slot.isResolved = true;
                    auto cmd =
                        std::make_unique<AddTrackFxCommand>(selectedTrackIndex_, std::move(slot));
                    commandManager_.execute(std::move(cmd), session_);
                    if (onSessionChanged)
                        onSessionChanged();
                    refresh();
                }
            }
            else if (result >= 1000)
            {
                int fxIdx = result - 1000;
                if (fxIdx < chainSize)
                {
                    auto cmd = std::make_unique<RemoveTrackFxCommand>(selectedTrackIndex_, fxIdx);
                    commandManager_.execute(std::move(cmd), session_);
                    if (onSessionChanged)
                        onSessionChanged();
                    refresh();
                }
            }
        });
}

juce::String TrackInfoPanel::buildTrackSummary(const TrackState &track, int trackIndex) const
{
    juce::String text;

    text << "Quick Help\n";
    text << "- Click a track header to inspect it\n";
    text << "- Cmd+I toggles this inspector\n";
    text << "- Edit name/gain/pan/mute/solo above\n\n";

    text << "Region: Audio Defaults\n";
    text << "- Mute: Off\n";
    text << "- Loop: Off\n";
    text << "- Quantize: Off\n";
    text << "- Q-Swing: 0\n";
    text << "- Transpose: 0\n";
    text << "- Fine Tune: 0\n";
    text << "- Pitch Source: Off\n";
    text << "- Flex & Follow: Off\n";
    text << "- Gain: 0.0 dB\n\n";

    text << "Track: " << track.name << "\n";
    text << "- Index: " << (trackIndex + 1) << "\n";
    text << "- Type: " << (track.isMidi() ? "MIDI" : "Audio") << "\n";

    text << "- Clips: "
         << (track.isMidi() ? static_cast<int>(track.midiClips.size())
                            : static_cast<int>(track.clips.size()))
         << "\n";
    text << "- Output: Stereo Out\n";
    text << "- Group: None\n";
    text << "- Automation: Read\n\n";

    text << "Channel Strip\n";
    text << "- Gain: " << juce::String(track.gainDb, 1) << " dB\n";
    text << "- Pan: " << formatPan(track.pan) << "\n";
    text << "- Mute: " << boolText(track.muted) << "\n";
    text << "- Solo: " << boolText(track.solo) << "\n\n";

    text << "Input\n";
    text << "- Source: Input 1\n\n";

    text << "Plugins\n";

    if (track.instrumentPlugin.has_value())
    {
        const auto &instrument = track.instrumentPlugin.value();
        text << "- Instrument: " << instrument.pluginName;
        if (instrument.pluginFormat.isNotEmpty())
            text << " [" << instrument.pluginFormat << "]";
        if (instrument.bypassed)
            text << " (Bypassed)";
        text << "\n";
    }
    else if (track.isMidi())
    {
        text << "- Instrument: None\n";
    }

    if (track.pluginChain.empty())
    {
        text << "- FX Chain: Empty\n";
    }
    else
    {
        text << "- FX Chain (" << static_cast<int>(track.pluginChain.size()) << "):\n";

        bool hasEqPlugin = false;
        for (size_t i = 0; i < track.pluginChain.size(); ++i)
        {
            const auto &slot = track.pluginChain[i];
            text << "  " << juce::String(static_cast<int>(i + 1)) << ". " << slot.pluginName;
            if (slot.pluginFormat.isNotEmpty())
                text << " [" << slot.pluginFormat << "]";
            if (slot.bypassed)
                text << " (Bypassed)";
            text << "\n";

            auto lowerName = slot.pluginName.toLowerCase();
            if (lowerName.contains("eq") || lowerName.contains("pro-q") ||
                lowerName.contains("channel eq"))
                hasEqPlugin = true;
        }

        text << "- EQ Plugin Present: " << boolText(hasEqPlugin) << "\n";
    }

    if (track.pluginChain.empty())
        text << "- EQ Plugin Present: Off\n";

    text << "\nSends\n";
    text << "- Send A: Off\n";
    text << "- Send B: Off\n";

    text << "\nMore\n";
    text << "- Inspector and mixer edits are undoable\n";
    text << "- Use Cmd+Z / Cmd+Shift+Z for undo/redo\n";

    return text;
}

juce::String TrackInfoPanel::formatPan(float pan)
{
    if (std::abs(pan) < 0.01f)
        return "Center";

    if (pan < 0.0f)
        return "L " + juce::String(std::abs(pan) * 100.0f, 0) + "%";

    return "R " + juce::String(std::abs(pan) * 100.0f, 0) + "%";
}

juce::String TrackInfoPanel::boolText(bool value)
{
    return value ? "On" : "Off";
}

void TrackInfoPanel::showFxContextMenu(int fxIndex)
{
    auto *track = session_.getTrack(selectedTrackIndex_);
    if (track == nullptr)
        return;
    if (fxIndex < 0 || fxIndex >= static_cast<int>(track->pluginChain.size()))
        return;

    const auto &slot = track->pluginChain[static_cast<size_t>(fxIndex)];

    juce::PopupMenu menu;
    menu.addItem(1, "Remove \"" + slot.pluginName + "\"");
    menu.addItem(2, slot.bypassed ? "Un-bypass" : "Bypass");

    menu.showMenuAsync(juce::PopupMenu::Options(),
                       [this, fxIndex, bypassed = slot.bypassed](int result)
                       {
                           if (result == 1)
                           {
                               auto cmd = std::make_unique<RemoveTrackFxCommand>(
                                   selectedTrackIndex_, fxIndex);
                               commandManager_.execute(std::move(cmd), session_);
                               if (onSessionChanged)
                                   onSessionChanged();
                               refresh();
                           }
                           // Bypass toggle is a future enhancement; session model support needed
                           (void)bypassed;
                       });
}

} // namespace ampl
