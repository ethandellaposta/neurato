#include "../ui/ModernLookAndFeel.cpp"
#include "commands/ClipCommands.hpp"
#include "commands/CommandManager.hpp"
#include "commands/MidiCommands.hpp"
#include "engine/core/AudioEngine.hpp"
#include "engine/render/OfflineRenderer.hpp"
#include "import/LogicImporter.hpp"
#include "model/ProjectSerializer.hpp"
#include "model/Session.hpp"
#include "ui/Theme.hpp"
#include "ui/editors/AudioClipEditor.hpp"
#include "ui/editors/PianoRollEditor.hpp"
#include "ui/panels/PianoKeyboardPanel.hpp"
#include "ui/panels/io/AudioFileBrowser.hpp"
#include "ui/panels/io/AudioSettingsPanel.hpp"
#include "ui/panels/io/BounceProgressDialog.hpp"
#include "ui/panels/mixer/MixerPanel.hpp"
#include "ui/panels/mixer/TrackInfoPanel.hpp"
#include "ui/timeline/TimelineView.hpp"
#include "ui/timeline/TrackView.hpp"
#include "ui/timeline/TransportBar.hpp"
#include "util/RecentProjects.hpp"
#include <algorithm>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <thread>
#include <vector>

namespace ampl
{

//==============================================================================
// Main content component — holds transport bar, timeline, and settings
//==============================================================================
class MainContentComponent : public juce::Component,
                             public juce::Timer,
                             public juce::MenuBarModel,
                             public juce::MidiKeyboardState::Listener
{
  public:
    MainContentComponent()
    {
        setLookAndFeel(&ampl::Theme::getLookAndFeel());
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(true);
        setFocusContainerType(juce::Component::FocusContainerType::focusContainer);

        engine_.initialise();

        // Defer plugin scanning until the message loop is fully running.
        // Some VST3 plugins (like SpliceBridge) crash if scanned too early
        // because they start juce::Timers during instantiation.
        juce::MessageManager::callAsync(
            [this]
            {
                if (auto *pm = engine_.getPluginManager())
                    pm->refreshPluginList();
            });

        // Create a default track in the session
        session_.addTrack("Track 1");

        transportBar_ = std::make_unique<TransportBar>(engine_);
        addAndMakeVisible(transportBar_.get());

        timelineView_ = std::make_unique<TimelineView>(engine_, session_);
        timelineView_->onSeek = [this](SampleCount pos) { engine_.sendSeek(pos); };
        timelineView_->onSessionChanged = [this]
        {
            markDirty();
            syncSessionToEngine();
        };
        timelineView_->onTrackMuteRequested = [this](int trackIdx, bool muted)
        {
            auto cmd = std::make_unique<SetTrackMuteCommand>(trackIdx, muted);
            commandManager_.execute(std::move(cmd), session_);
            markDirty();
            syncSessionToEngine();
            timelineView_->repaint();
        };
        timelineView_->onTrackSoloRequested = [this](int trackIdx, bool solo)
        {
            if (solo)
            {
                const auto trackCount = static_cast<int>(session_.getTracks().size());
                for (int i = 0; i < trackCount; ++i)
                {
                    if (i == trackIdx)
                        continue;
                    if (auto *track = session_.getTrack(i); track != nullptr && track->solo)
                    {
                        auto clearSolo = std::make_unique<SetTrackSoloCommand>(i, false);
                        commandManager_.execute(std::move(clearSolo), session_);
                    }
                }
            }

            auto cmd = std::make_unique<SetTrackSoloCommand>(trackIdx, solo);
            commandManager_.execute(std::move(cmd), session_);
            markDirty();
            syncSessionToEngine();
            timelineView_->repaint();
        };
        addAndMakeVisible(timelineView_.get());

        // Toolbar buttons
        settingsButton_ = std::make_unique<juce::TextButton>("Audio Settings");
        settingsButton_->onClick = [this] { showAudioSettings(); };
        addAndMakeVisible(settingsButton_.get());

        loadButton_ = std::make_unique<juce::TextButton>("Import");
        loadButton_->onClick = [this] { loadAudioFile(); };
        addAndMakeVisible(loadButton_.get());

        addTrackButton_ = std::make_unique<juce::TextButton>("+ Audio");
        addTrackButton_->onClick = [this]
        {
            commandManager_.execute(
                std::make_unique<AddTrackCommand>(TrackType::Audio, juce::String{}, false),
                session_);
            markDirty();
            syncSessionToEngine();
            timelineView_->repaint();
        };
        ampl::Theme::styleTextButton(*addTrackButton_, true); // Primary button
        addAndMakeVisible(addTrackButton_.get());

        addMidiTrackButton_ = std::make_unique<juce::TextButton>("+ MIDI");
        addMidiTrackButton_->onClick = [this]
        {
            commandManager_.execute(
                std::make_unique<AddTrackCommand>(TrackType::Midi, juce::String{}, true), session_);
            markDirty();
            syncSessionToEngine();
            timelineView_->repaint();
        };
        ampl::Theme::styleTextButton(*addMidiTrackButton_, true); // Primary button
        addAndMakeVisible(addMidiTrackButton_.get());

        zoomInButton_ = std::make_unique<juce::TextButton>(juce::String::fromUTF8("\xef\x80\x8e"));
        zoomInButton_->setButtonText("+");
        zoomInButton_->onClick = [this] { timelineView_->zoomIn(); };
        addAndMakeVisible(zoomInButton_.get());

        zoomOutButton_ = std::make_unique<juce::TextButton>(juce::String::fromUTF8("\xef\x80\x10"));
        zoomOutButton_->setButtonText("-");
        zoomOutButton_->onClick = [this] { timelineView_->zoomOut(); };
        addAndMakeVisible(zoomOutButton_.get());

        zoomFitButton_ = std::make_unique<juce::TextButton>("Fit");
        zoomFitButton_->onClick = [this] { timelineView_->zoomToFit(); };
        addAndMakeVisible(zoomFitButton_.get());

        returnToPlayheadButton_ = std::make_unique<juce::TextButton>("To Playhead");
        returnToPlayheadButton_->onClick = [this] { timelineView_->centerOnPlayhead(); };
        addAndMakeVisible(returnToPlayheadButton_.get());

        undoButton_ = std::make_unique<juce::TextButton>(juce::String::fromUTF8("\xe2\x86\xa9"));
        undoButton_->onClick = [this]
        {
            if (commandManager_.undo(session_))
            {
                markDirty();
                syncSessionToEngine();
                timelineView_->repaint();
            }
        };
        addAndMakeVisible(undoButton_.get());

        redoButton_ = std::make_unique<juce::TextButton>(juce::String::fromUTF8("\xe2\x86\xaa"));
        redoButton_->onClick = [this]
        {
            if (commandManager_.redo(session_))
            {
                markDirty();
                syncSessionToEngine();
                timelineView_->repaint();
            }
        };
        addAndMakeVisible(redoButton_.get());

        // Update undo/redo button state
        commandManager_.onStateChanged = [this]
        {
            undoButton_->setEnabled(commandManager_.canUndo());
            redoButton_->setEnabled(commandManager_.canRedo());

            auto undoDesc = commandManager_.getUndoDescription();
            undoButton_->setTooltip(undoDesc.isEmpty() ? "Nothing to undo" : "Undo: " + undoDesc);
            auto redoDesc = commandManager_.getRedoDescription();
            redoButton_->setTooltip(redoDesc.isEmpty() ? "Nothing to redo" : "Redo: " + redoDesc);
        };
        undoButton_->setEnabled(false);
        redoButton_->setEnabled(false);

        saveButton_ = std::make_unique<juce::TextButton>("Save");
        saveButton_->onClick = [this] { saveProject(); };
        addAndMakeVisible(saveButton_.get());

        openButton_ = std::make_unique<juce::TextButton>("Open");
        openButton_->onClick = [this] { openProject(); };
        addAndMakeVisible(openButton_.get());

        bounceButton_ = std::make_unique<juce::TextButton>("Bounce");
        bounceButton_->onClick = [this] { bounceToFile(); };
        addAndMakeVisible(bounceButton_.get());

        browserToggleButton_ = std::make_unique<juce::TextButton>("Files");
        browserToggleButton_->setClickingTogglesState(true);
        browserToggleButton_->onClick = [this]
        {
            fileBrowserVisible_ = browserToggleButton_->getToggleState();
            resized();
        };
        addAndMakeVisible(browserToggleButton_.get());

        // Audio file browser panel (hidden by default)
        fileBrowser_ = std::make_unique<AudioFileBrowser>();
        fileBrowser_->onFileSelected = [this](const juce::File &file) { importAudioFile(file); };
        addChildComponent(fileBrowser_.get());

        // Mixer panel (hidden by default)
        mixerPanel_ = std::make_unique<MixerPanel>(session_, commandManager_);
        mixerPanel_->onSessionChanged = [this]
        {
            markDirty();
            syncSessionToEngine();
            timelineView_->repaint();
        };
        addChildComponent(mixerPanel_.get());

        // Track info panel (hidden by default)
        trackInfoPanel_ = std::make_unique<TrackInfoPanel>(session_, commandManager_);
        trackInfoPanel_->setPluginManager(engine_.getPluginManager());
        trackInfoPanel_->onSessionChanged = [this]
        {
            markDirty();
            syncSessionToEngine();
            timelineView_->repaint();
        };
        addChildComponent(trackInfoPanel_.get());

        // Editors (hidden by default)
        pianoRollEditor_ = std::make_unique<PianoRollEditor>(session_, commandManager_);
        pianoRollEditor_->onSessionChanged = [this]
        {
            markDirty();
            syncSessionToEngine();
            timelineView_->repaint();
        };
        pianoRollEditor_->onCloseRequested = [this] { closeEditor(); };
        addChildComponent(pianoRollEditor_.get());

        audioClipEditor_ = std::make_unique<AudioClipEditor>(session_, commandManager_);
        audioClipEditor_->onSessionChanged = [this]
        {
            markDirty();
            syncSessionToEngine();
            timelineView_->repaint();
        };
        audioClipEditor_->onCloseRequested = [this] { closeEditor(); };
        addChildComponent(audioClipEditor_.get());

        // Wire timeline double-click to open editors
        timelineView_->onClipDoubleClicked = [this](int trackIdx, juce::String clipId, bool isMidi)
        { openEditor(trackIdx, clipId, isMidi); };

        mixerToggleButton_ = std::make_unique<juce::TextButton>("Mixer");
        mixerToggleButton_->setClickingTogglesState(true);
        mixerToggleButton_->onClick = [this]
        {
            mixerVisible_ = mixerToggleButton_->getToggleState();
            resized();
        };
        addAndMakeVisible(mixerToggleButton_.get());

        infoToggleButton_ = std::make_unique<juce::TextButton>("Info");
        infoToggleButton_->setClickingTogglesState(true);
        infoToggleButton_->onClick = [this]
        {
            trackInfoVisible_ = infoToggleButton_->getToggleState();
            resized();
        };
        addAndMakeVisible(infoToggleButton_.get());

        // Piano keyboard panel (hidden by default)
        pianoKeyboardPanel_ = std::make_unique<PianoKeyboardPanel>();
        // Wire keyboard state to audio engine's MIDI collector
        pianoKeyboardPanel_->getKeyboardState().addListener(this);
        addChildComponent(pianoKeyboardPanel_.get());

        pianoKeyboardToggleButton_ = std::make_unique<juce::TextButton>("Keys");
        pianoKeyboardToggleButton_->setClickingTogglesState(true);
        pianoKeyboardToggleButton_->onClick = [this]
        {
            pianoKeyboardVisible_ = pianoKeyboardToggleButton_->getToggleState();
            if (pianoKeyboardVisible_)
                pianoKeyboardPanel_->grabKeyboardFocus();
            resized();
        };
        addAndMakeVisible(pianoKeyboardToggleButton_.get());

        // Consistent modern styling across toolbar controls
        ampl::Theme::styleTextButton(*settingsButton_);
        ampl::Theme::styleTextButton(*loadButton_);
        ampl::Theme::styleTextButton(*saveButton_);
        ampl::Theme::styleTextButton(*openButton_);
        ampl::Theme::styleTextButton(*zoomInButton_);
        ampl::Theme::styleTextButton(*zoomOutButton_);
        ampl::Theme::styleTextButton(*zoomFitButton_);
        ampl::Theme::styleTextButton(*returnToPlayheadButton_);
        ampl::Theme::styleTextButton(*undoButton_);
        ampl::Theme::styleTextButton(*redoButton_);
        ampl::Theme::styleTextButton(*bounceButton_);
        ampl::Theme::styleTextButton(*browserToggleButton_);
        ampl::Theme::styleTextButton(*mixerToggleButton_);
        ampl::Theme::styleTextButton(*infoToggleButton_);
        ampl::Theme::styleTextButton(*pianoKeyboardToggleButton_);

        browserToggleButton_->setColour(juce::TextButton::buttonOnColourId,
                                        juce::Colour(ampl::Theme::bgHighlight));
        mixerToggleButton_->setColour(juce::TextButton::buttonOnColourId,
                                      juce::Colour(ampl::Theme::bgHighlight));
        infoToggleButton_->setColour(juce::TextButton::buttonOnColourId,
                                     juce::Colour(ampl::Theme::bgHighlight));
        pianoKeyboardToggleButton_->setColour(juce::TextButton::buttonOnColourId,
                                              juce::Colour(ampl::Theme::bgHighlight));

        loadButton_->setTooltip("Import audio file");
        saveButton_->setTooltip("Save project (Cmd+S)");
        openButton_->setTooltip("Open project (Cmd+O)");
        addTrackButton_->setTooltip("Add audio track");
        addMidiTrackButton_->setTooltip("Add MIDI track");
        undoButton_->setTooltip("Undo (Cmd+Z)");
        redoButton_->setTooltip("Redo (Cmd+Shift+Z)");
        zoomInButton_->setTooltip("Zoom in (Cmd+=)");
        zoomOutButton_->setTooltip("Zoom out (Cmd+-)");
        zoomFitButton_->setTooltip("Zoom to fit (Cmd+0)");
        returnToPlayheadButton_->setTooltip("Return view to current playhead (J)");
        bounceButton_->setTooltip("Bounce/export audio (Cmd+E)");
        browserToggleButton_->setTooltip("Toggle file browser");
        mixerToggleButton_->setTooltip("Toggle mixer (Cmd+M)");
        infoToggleButton_->setTooltip("Toggle track info (Cmd+I)");
        pianoKeyboardToggleButton_->setTooltip("Toggle piano keyboard (Cmd+K)");
        settingsButton_->setTooltip("Audio settings");

        shortcutHintLabel_.setText(
            "Space Play/Pause | Up/Down Track Select | J Return to Playhead | Cmd+I Info | ?",
            juce::dontSendNotification);
        shortcutHintLabel_.setJustificationType(juce::Justification::centredRight);
        shortcutHintLabel_.setFont(ampl::Theme::smallFont());
        shortcutHintLabel_.setColour(juce::Label::textColourId,
                                     juce::Colour(ampl::Theme::textDim).withAlpha(0.6f));
        addAndMakeVisible(shortcutHintLabel_);

        projectHeaderLabel_.setJustificationType(juce::Justification::centredLeft);
        projectHeaderLabel_.setFont(ampl::Theme::bodyFont());
        projectHeaderLabel_.setColour(juce::Label::textColourId,
                                      juce::Colour(ampl::Theme::textPrimary).withAlpha(0.9f));
        addAndMakeVisible(projectHeaderLabel_);

        statusDisplayLabel_.setJustificationType(juce::Justification::centredRight);
        statusDisplayLabel_.setFont(ampl::Theme::smallFont());
        statusDisplayLabel_.setColour(juce::Label::textColourId,
                                      juce::Colour(ampl::Theme::textSecondary).withAlpha(0.8f));
        addAndMakeVisible(statusDisplayLabel_);

        auto disableFocus = [](juce::Button *button)
        {
            if (button != nullptr)
                button->setWantsKeyboardFocus(false);
        };

        disableFocus(settingsButton_.get());
        disableFocus(loadButton_.get());
        disableFocus(addTrackButton_.get());
        disableFocus(addMidiTrackButton_.get());
        disableFocus(zoomInButton_.get());
        disableFocus(zoomOutButton_.get());
        disableFocus(zoomFitButton_.get());
        disableFocus(returnToPlayheadButton_.get());
        disableFocus(undoButton_.get());
        disableFocus(redoButton_.get());
        disableFocus(saveButton_.get());
        disableFocus(openButton_.get());
        disableFocus(bounceButton_.get());
        disableFocus(browserToggleButton_.get());
        disableFocus(mixerToggleButton_.get());
        disableFocus(infoToggleButton_.get());
        disableFocus(pianoKeyboardToggleButton_.get());

        setSize(1280, 720);
        updateWindowTitle();
        updateHeaderDisplays();

        // Ensure we use the session renderer from the start
        syncSessionToEngine();

        // Single timer polls the SPSC queue and dispatches to all UI components
        startTimerHz(30);

        juce::MessageManager::callAsync([this] { grabKeyboardFocus(); });
    }

    ~MainContentComponent() override
    {
        // Remove our listener from the keyboard state before destruction
        if (pianoKeyboardPanel_)
            pianoKeyboardPanel_->getKeyboardState().removeListener(this);
        stopTimer();
        engine_.shutdown();
        setLookAndFeel(nullptr);
    }

    void mouseDown(const juce::MouseEvent &) override
    {
        if (!hasKeyboardFocus(false))
            grabKeyboardFocus();
    }

    // ─── MidiKeyboardState::Listener ─────────────────────────────
    // Forward on-screen piano keyboard / musical typing notes to the audio engine
    void handleNoteOn(juce::MidiKeyboardState *, int midiChannel, int midiNoteNumber,
                      float velocity) override
    {
        auto msg = juce::MidiMessage::noteOn(midiChannel, midiNoteNumber, velocity);
        msg.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
        engine_.getExternalIOManager().addMidiMessageToQueue(msg);
    }

    void handleNoteOff(juce::MidiKeyboardState *, int midiChannel, int midiNoteNumber,
                       float velocity) override
    {
        auto msg = juce::MidiMessage::noteOff(midiChannel, midiNoteNumber, velocity);
        msg.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
        engine_.getExternalIOManager().addMidiMessageToQueue(msg);
    }

    void setupToolbarGroup(juce::Rectangle<int> area,
                           const std::vector<juce::Component *> components)
    {
        int spacing = 4;
        int totalWidth = area.getWidth();
        int componentCount = static_cast<int>(components.size());
        int componentWidth = (totalWidth - (spacing * (componentCount - 1))) / componentCount;

        for (size_t i = 0; i < components.size(); ++i)
        {
            components[i]->setBounds(area.removeFromLeft(componentWidth));
            if (i + 1 < components.size())
            {
                area.removeFromLeft(spacing);
            }
        }
    }

    void paintToolbarSeparator(juce::Graphics &g, int x, int y, int height)
    {
        g.setColour(juce::Colour(ampl::Theme::border).withAlpha(0.5f));
        g.drawVerticalLine(x, static_cast<float>(y + 4), static_cast<float>(y + height - 4));
    }

    void paint(juce::Graphics &g) override
    {
        auto bounds = getLocalBounds().toFloat();
        juce::ColourGradient backgroundGradient(juce::Colour(ampl::Theme::bgDeepest), 0.0f, 0.0f,
                                                juce::Colour(ampl::Theme::bgBase), 0.0f,
                                                bounds.getHeight(), false);
        g.setGradientFill(backgroundGradient);
        g.fillRect(bounds);

        // Ambient glows for depth and contrast
        juce::ColourGradient leftGlow(juce::Colour(ampl::Theme::accentBlue).withAlpha(0.18f),
                                      bounds.getX() + 120.0f, bounds.getY() + 60.0f,
                                      juce::Colour(ampl::Theme::accentBlue).withAlpha(0.0f),
                                      bounds.getX() + 520.0f, bounds.getY() + 420.0f, true);
        g.setGradientFill(leftGlow);
        g.fillEllipse(bounds.getX() - 180.0f, bounds.getY() - 220.0f, 780.0f, 560.0f);

        juce::ColourGradient rightGlow(juce::Colour(ampl::Theme::accentPurple).withAlpha(0.12f),
                                       bounds.getRight() - 140.0f, bounds.getY() + 40.0f,
                                       juce::Colour(ampl::Theme::accentPurple).withAlpha(0.0f),
                                       bounds.getRight() - 540.0f, bounds.getY() + 360.0f, true);
        g.setGradientFill(rightGlow);
        g.fillEllipse(bounds.getRight() - 560.0f, bounds.getY() - 220.0f, 760.0f, 520.0f);

        // Subtle horizontal depth lines
        g.setColour(juce::Colour(ampl::Theme::borderLight).withAlpha(0.10f));
        for (int y = toolbarAreaBottom_ + 24; y < getHeight(); y += 56)
            g.drawHorizontalLine(y, 0.0f, static_cast<float>(getWidth()));

        // Toolbar chrome background
        auto toolbarChrome = getLocalBounds().toFloat();
        toolbarChrome = toolbarChrome.removeFromTop(static_cast<float>(toolbarAreaBottom_));
        juce::ColourGradient toolbarGradient(juce::Colour(ampl::Theme::bgSurface).withAlpha(0.88f),
                                             toolbarChrome.getTopLeft(),
                                             juce::Colour(ampl::Theme::bgBase).withAlpha(0.70f),
                                             toolbarChrome.getBottomLeft(), false);
        g.setGradientFill(toolbarGradient);
        g.fillRect(toolbarChrome);

        // Toolbar bottom divider
        g.setColour(juce::Colour(ampl::Theme::border));
        g.drawHorizontalLine(toolbarAreaBottom_, 0.0f, static_cast<float>(getWidth()));

        // Group separators (drawn between button groups in the toolbar row)
        for (auto sep : toolbarSeparatorXPositions_)
            paintToolbarSeparator(g, sep, transportBar_->getBottom() + 6, 34);
    }

    void resized() override
    {
        toolbarSeparatorXPositions_.clear();
        auto area = getLocalBounds().reduced(8, 4);

        transportBar_->setBounds(area.removeFromTop(52));
        area.removeFromTop(4);

        auto toolbar = area.removeFromTop(32);
        constexpr int gap = 6;    // breathing room between groups
        constexpr int sepGap = 8; // gap around separator

        // --- Left: Project actions ---
        auto projectGroup = toolbar.removeFromLeft(200);
        setupToolbarGroup(projectGroup, {loadButton_.get(), saveButton_.get(), openButton_.get()});

        toolbar.removeFromLeft(sepGap);
        toolbarSeparatorXPositions_.push_back(toolbar.getX());
        toolbar.removeFromLeft(sepGap);

        // --- Track creation ---
        auto trackGroup = toolbar.removeFromLeft(160);
        setupToolbarGroup(trackGroup, {addTrackButton_.get(), addMidiTrackButton_.get()});

        toolbar.removeFromLeft(sepGap);
        toolbarSeparatorXPositions_.push_back(toolbar.getX());
        toolbar.removeFromLeft(sepGap);

        // --- Edit: Undo/Redo (compact) + Zoom ---
        auto undoRedoGroup = toolbar.removeFromLeft(64);
        setupToolbarGroup(undoRedoGroup, {undoButton_.get(), redoButton_.get()});
        toolbar.removeFromLeft(gap);

        auto zoomGroup = toolbar.removeFromLeft(206);
        setupToolbarGroup(zoomGroup, {zoomInButton_.get(), zoomOutButton_.get(),
                                      zoomFitButton_.get(), returnToPlayheadButton_.get()});

        toolbar.removeFromLeft(sepGap);
        toolbarSeparatorXPositions_.push_back(toolbar.getX());
        toolbar.removeFromLeft(sepGap);

        // --- Export ---
        auto exportGroup = toolbar.removeFromLeft(76);
        setupToolbarGroup(exportGroup, {bounceButton_.get()});

        toolbar.removeFromLeft(sepGap);
        toolbarSeparatorXPositions_.push_back(toolbar.getX());
        toolbar.removeFromLeft(sepGap);

        // --- View toggles ---
        auto viewGroup = toolbar.removeFromLeft(240);
        setupToolbarGroup(viewGroup, {browserToggleButton_.get(), mixerToggleButton_.get(),
                                      infoToggleButton_.get(), pianoKeyboardToggleButton_.get()});

        // --- Right: Settings + hint ---
        auto settingsGroup = toolbar.removeFromRight(100);
        settingsButton_->setBounds(settingsGroup);

        toolbar.removeFromRight(gap);
        auto shortcutArea = toolbar.removeFromRight(360);
        shortcutHintLabel_.setBounds(shortcutArea);

        auto statusArea = toolbar.removeFromRight(300);
        statusDisplayLabel_.setBounds(statusArea);

        projectHeaderLabel_.setBounds(toolbar);

        toolbarAreaBottom_ = area.getY();

        area.removeFromTop(6);

        if (trackInfoVisible_)
        {
            trackInfoPanel_->setVisible(true);
            trackInfoPanel_->setBounds(area.removeFromLeft(320));
            area.removeFromLeft(6);
        }
        else
        {
            trackInfoPanel_->setVisible(false);
        }

        if (fileBrowserVisible_)
        {
            fileBrowser_->setVisible(true);
            fileBrowser_->setBounds(area.removeFromLeft(250));
            area.removeFromLeft(6);
        }
        else
        {
            fileBrowser_->setVisible(false);
        }

        // Editor panel at bottom (if visible) — takes priority over mixer
        if (editorVisible_)
        {
            if (editorIsMidi_)
            {
                pianoRollEditor_->setVisible(true);
                audioClipEditor_->setVisible(false);
                pianoRollEditor_->setBounds(area.removeFromBottom(ampl::Theme::editorHeight));
            }
            else
            {
                audioClipEditor_->setVisible(true);
                pianoRollEditor_->setVisible(false);
                audioClipEditor_->setBounds(area.removeFromBottom(ampl::Theme::editorHeight));
            }
        }
        else
        {
            pianoRollEditor_->setVisible(false);
            audioClipEditor_->setVisible(false);
        }

        // Mixer panel at bottom (if visible)
        if (mixerVisible_)
        {
            mixerPanel_->setVisible(true);
            mixerPanel_->setBounds(area.removeFromBottom(ampl::Theme::mixerHeight));
        }
        else
        {
            mixerPanel_->setVisible(false);
        }

        // Piano keyboard panel at bottom (if visible) — below mixer/editors
        if (pianoKeyboardVisible_)
        {
            pianoKeyboardPanel_->setVisible(true);
            pianoKeyboardPanel_->setBounds(area.removeFromBottom(120));
        }
        else
        {
            pianoKeyboardPanel_->setVisible(false);
        }

        timelineView_->setBounds(area.reduced(0, 1));
    }

    void timerCallback() override
    {
        // Single consumer of the SPSC queue — dispatch to all UI components
        while (auto msg = engine_.pollAudioMessage())
        {
            transportBar_->handleAudioMessage(*msg);
            timelineView_->handleAudioMessage(*msg);
        }

        transportBar_->updateDisplay();
        timelineView_->updateDisplay();
        updateTrackInfoPanel();
        updateHeaderDisplays();
    }

    bool keyPressed(const juce::KeyPress &key) override
    {
        // Space = toggle play/pause
        if (key == juce::KeyPress::spaceKey)
        {
            engine_.sendTogglePlayStop();
            return true;
        }
        // Return/Enter = stop and return playhead to start
        if (key.getKeyCode() == juce::KeyPress::returnKey || key.getTextCharacter() == '\n' ||
            key.getTextCharacter() == '\r')
        {
            engine_.sendStop();
            return true;
        }
        // Cmd+Z = undo
        if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0))
        {
            if (commandManager_.undo(session_))
            {
                markDirty();
                syncSessionToEngine();
                timelineView_->repaint();
            }
            return true;
        }
        // Cmd+Shift+Z = redo
        if (key ==
            juce::KeyPress(
                'z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0))
        {
            if (commandManager_.redo(session_))
            {
                markDirty();
                syncSessionToEngine();
                timelineView_->repaint();
            }
            return true;
        }
        // Cmd+= / Cmd++ = zoom in
        if (key == juce::KeyPress('=', juce::ModifierKeys::commandModifier, 0))
        {
            timelineView_->zoomIn();
            return true;
        }
        // Cmd+- = zoom out
        if (key == juce::KeyPress('-', juce::ModifierKeys::commandModifier, 0))
        {
            timelineView_->zoomOut();
            return true;
        }
        // Cmd+0 = zoom to fit
        if (key == juce::KeyPress('0', juce::ModifierKeys::commandModifier, 0))
        {
            timelineView_->zoomToFit();
            return true;
        }
        // J = return view to current playhead
        if (key == juce::KeyPress('j', juce::ModifierKeys::noModifiers, 0))
        {
            timelineView_->centerOnPlayhead();
            return true;
        }
        // Up/Down = select previous/next track
        if (key.getKeyCode() == juce::KeyPress::upKey)
        {
            timelineView_->selectPreviousTrack();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::downKey)
        {
            timelineView_->selectNextTrack();
            return true;
        }
        // Cmd+S = save project
        if (key == juce::KeyPress('s', juce::ModifierKeys::commandModifier, 0))
        {
            saveProject();
            return true;
        }
        // Cmd+O = open project
        if (key == juce::KeyPress('o', juce::ModifierKeys::commandModifier, 0))
        {
            openProject();
            return true;
        }
        // Cmd+E = export/bounce
        if (key == juce::KeyPress('e', juce::ModifierKeys::commandModifier, 0))
        {
            bounceToFile();
            return true;
        }
        // Cmd+N = new project
        if (key == juce::KeyPress('n', juce::ModifierKeys::commandModifier, 0))
        {
            newProject();
            return true;
        }
        // Cmd+M = toggle mixer
        if (key == juce::KeyPress('m', juce::ModifierKeys::commandModifier, 0))
        {
            mixerVisible_ = !mixerVisible_;
            mixerToggleButton_->setToggleState(mixerVisible_, juce::dontSendNotification);
            resized();
            return true;
        }
        // Cmd+I = toggle track info view
        if (key == juce::KeyPress('i', juce::ModifierKeys::commandModifier, 0))
        {
            trackInfoVisible_ = !trackInfoVisible_;
            infoToggleButton_->setToggleState(trackInfoVisible_, juce::dontSendNotification);
            resized();
            return true;
        }
        // Cmd+K = toggle piano keyboard
        if (key == juce::KeyPress('k', juce::ModifierKeys::commandModifier, 0))
        {
            pianoKeyboardVisible_ = !pianoKeyboardVisible_;
            pianoKeyboardToggleButton_->setToggleState(pianoKeyboardVisible_,
                                                       juce::dontSendNotification);
            if (pianoKeyboardVisible_)
                pianoKeyboardPanel_->grabKeyboardFocus();
            resized();
            return true;
        }
        // Escape = close editor
        if (key == juce::KeyPress::escapeKey)
        {
            if (editorVisible_)
            {
                closeEditor();
                return true;
            }
        }
        if (key.getTextCharacter() == '?' || key == juce::KeyPress::F1Key)
        {
            showShortcutHelp();
            return true;
        }
        return false;
    }

    // --- MenuBarModel ---
    juce::StringArray getMenuBarNames() override
    {
        return {"File", "View"};
    }

    juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String & /*menuName*/) override
    {
        juce::PopupMenu menu;

        if (menuIndex == 0) // File
        {
            menu.addItem(1, "New Project", true, false);

            juce::PopupMenu templateMenu;
            templateMenu.addItem(14, "Empty Session");
            templateMenu.addItem(15, "Songwriter Starter");
            templateMenu.addItem(16, "Podcast Starter");
            templateMenu.addItem(17, "Electronic Starter");
            menu.addSubMenu("New From Template", templateMenu);

            menu.addItem(2, "Open...", true, false);

            // Recent Projects submenu
            juce::PopupMenu recentMenu;
            auto recentFiles = recentProjects_.getFiles();
            if (recentFiles.empty())
            {
                recentMenu.addItem(100, "(No Recent Projects)", false, false);
            }
            else
            {
                for (int i = 0; i < static_cast<int>(recentFiles.size()); ++i)
                    recentMenu.addItem(200 + i, recentFiles[static_cast<size_t>(i)].getFileName());
                recentMenu.addSeparator();
                recentMenu.addItem(199, "Clear Recent Projects");
            }
            menu.addSubMenu("Recent Projects", recentMenu);

            menu.addSeparator();
            menu.addItem(3, "Save", true, false);
            menu.addItem(4, "Save As...", true, false);
            menu.addSeparator();
            menu.addItem(7, "Import Logic Project...", true, false);
            menu.addSeparator();
            menu.addItem(5, "Export Audio...", true, false);
            menu.addSeparator();
            menu.addItem(6, "Audio Settings...", true, false);

#if !JUCE_MAC
            menu.addSeparator();
            menu.addItem(99, "Quit");
#endif
        }
        else if (menuIndex == 1) // View
        {
            menu.addItem(10, "Toggle Mixer", true, mixerVisible_);
            menu.addItem(11, "Toggle File Browser", true, fileBrowserVisible_);
            menu.addItem(13, "Toggle Track Info", true, trackInfoVisible_);
            menu.addItem(18, "Toggle Piano Keyboard", true, pianoKeyboardVisible_);
            menu.addSeparator();
            menu.addItem(12, "Keyboard Shortcuts");
        }

        return menu;
    }

    void menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/) override
    {
        switch (menuItemID)
        {
        case 1:
            newProject();
            break;
        case 2:
            openProject();
            break;
        case 14:
            newProjectFromTemplate(0);
            break;
        case 15:
            newProjectFromTemplate(1);
            break;
        case 16:
            newProjectFromTemplate(2);
            break;
        case 17:
            newProjectFromTemplate(3);
            break;
        case 3:
            saveProject();
            break;
        case 4:
            saveProjectAs();
            break;
        case 5:
            bounceToFile();
            break;
        case 6:
            showAudioSettings();
            break;
        case 7:
            importLogicProject();
            break;
        case 10:
            mixerVisible_ = !mixerVisible_;
            mixerToggleButton_->setToggleState(mixerVisible_, juce::dontSendNotification);
            resized();
            break;
        case 11:
            fileBrowserVisible_ = !fileBrowserVisible_;
            browserToggleButton_->setToggleState(fileBrowserVisible_, juce::dontSendNotification);
            resized();
            break;
        case 12:
            showShortcutHelp();
            break;
        case 13:
            trackInfoVisible_ = !trackInfoVisible_;
            infoToggleButton_->setToggleState(trackInfoVisible_, juce::dontSendNotification);
            resized();
            break;
        case 18:
            pianoKeyboardVisible_ = !pianoKeyboardVisible_;
            pianoKeyboardToggleButton_->setToggleState(pianoKeyboardVisible_,
                                                       juce::dontSendNotification);
            if (pianoKeyboardVisible_)
                pianoKeyboardPanel_->grabKeyboardFocus();
            resized();
            break;
        case 99:
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
            break;
        case 199:
            recentProjects_.clear();
            break;
        default:
        {
            // Recent project items (200+)
            if (menuItemID >= 200)
            {
                auto recentFiles = recentProjects_.getFiles();
                int idx = menuItemID - 200;
                if (idx >= 0 && idx < static_cast<int>(recentFiles.size()))
                {
                    auto file = recentFiles[static_cast<size_t>(idx)];
                    if (file.existsAsFile())
                        loadProjectFile(file);
                    else
                        recentProjects_.removeFile(file);
                }
            }
            break;
        }
        }
    }

  private:
    void showAudioSettings()
    {
        auto *settingsPanel = new AudioSettingsPanel(engine_);
        settingsPanel->setSize(500, 450);

        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(settingsPanel);
        options.dialogTitle = "Audio Settings";
        options.dialogBackgroundColour = juce::Colour(0xff1a1a2e);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;
        options.launchAsync();
    }

    void loadAudioFile()
    {
        auto chooser = std::make_shared<juce::FileChooser>("Select an audio file...", juce::File{},
                                                           "*.wav;*.aiff;*.aif;*.mp3;*.flac;*.ogg");

        chooser->launchAsync(juce::FileBrowserComponent::openMode |
                                 juce::FileBrowserComponent::canSelectFiles,
                             [this, chooser](const juce::FileChooser &fc)
                             {
                                 auto file = fc.getResult();
                                 if (!file.existsAsFile())
                                     return;
                                 importAudioFile(file);
                             });
    }

    void importAudioFile(const juce::File &file)
    {
        // Load asset into session
        auto asset = session_.loadAudioAsset(file, engine_.getFormatManager());
        if (!asset)
            return;

        // Create clip and add to first track via command
        auto clip = Clip::fromAsset(asset, 0);

        // If no tracks exist, create one
        if (session_.getTracks().empty())
            session_.addTrack("Track 1");

        auto cmd = std::make_unique<AddClipCommand>(0, clip);
        commandManager_.execute(std::move(cmd), session_);

        markDirty();
        syncSessionToEngine(); // Publish session to audio renderer!
        timelineView_->zoomToFit();
        timelineView_->repaint();
    }

    void saveProject()
    {
        if (currentProjectFile_.existsAsFile())
        {
            if (ProjectSerializer::save(session_, currentProjectFile_))
            {
                dirty_ = false;
                updateWindowTitle();
            }
            return;
        }

        saveProjectAs();
    }

    void saveProjectAs()
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Save Project...", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.ampl");

        chooser->launchAsync(juce::FileBrowserComponent::saveMode |
                                 juce::FileBrowserComponent::canSelectFiles,
                             [this, chooser](const juce::FileChooser &fc)
                             {
                                 auto file = fc.getResult();
                                 if (file == juce::File{})
                                     return;

                                 auto projectFile = file.withFileExtension("ampl");
                                 if (ProjectSerializer::save(session_, projectFile))
                                 {
                                     currentProjectFile_ = projectFile;
                                     recentProjects_.addFile(projectFile);
                                     dirty_ = false;
                                     updateWindowTitle();
                                 }
                             });
    }

    void openProject()
    {
        if (dirty_ && !confirmDiscardChanges())
            return;

        auto chooser = std::make_shared<juce::FileChooser>(
            "Open Project...", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.ampl");

        chooser->launchAsync(juce::FileBrowserComponent::openMode |
                                 juce::FileBrowserComponent::canSelectFiles,
                             [this, chooser](const juce::FileChooser &fc)
                             {
                                 auto file = fc.getResult();
                                 if (!file.existsAsFile())
                                     return;
                                 loadProjectFile(file);
                             });
    }

    void loadProjectFile(const juce::File &file)
    {
        Session newSession;
        if (ProjectSerializer::load(newSession, file, engine_.getFormatManager()))
        {
            session_ = std::move(newSession);
            currentProjectFile_ = file;
            commandManager_.clear();
            recentProjects_.addFile(file);

            dirty_ = false;
            syncSessionToEngine();
            updateWindowTitle();
            timelineView_->zoomToFit();
            timelineView_->repaint();
        }
    }

    void importLogicProject()
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Import Logic Pro Project...",
            juce::File::getSpecialLocation(juce::File::userMusicDirectory),
            "*.fcpxml;*.xml;*.logicx");

        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles |
                juce::FileBrowserComponent::canSelectDirectories,
            [this, chooser](const juce::FileChooser &fc)
            {
                auto file = fc.getResult();
                if (file == juce::File{})
                    return;

                // Create importer and import
                LogicImporter importer;
                // Connect to engine's plugin manager for plugin resolution
                importer.setPluginManager(engine_.getPluginManager());

                LogicImportResult result;

                if (file.isDirectory() || file.getFileExtension().equalsIgnoreCase(".logicx"))
                {
                    result =
                        importer.importFromLogicBundle(file, session_, engine_.getFormatManager());
                }
                else
                {
                    result = importer.importFromFCPXML(file, session_, engine_.getFormatManager());
                }

                if (result.success)
                {
                    currentProjectFile_ = juce::File{}; // New unsaved project
                    commandManager_.clear();
                    dirty_ = true;

                    syncSessionToEngine();
                    updateWindowTitle();
                    timelineView_->zoomToFit();
                    timelineView_->repaint();

                    // Show import summary
                    juce::String summary;
                    summary << "Import Complete!\n\n";
                    summary << "Tracks: " << result.tracksImported << "\n";
                    summary << "Audio Clips: " << result.audioClipsImported << "\n";
                    summary << "MIDI Clips: " << result.midiClipsImported << "\n";
                    summary << "Plugins Resolved: " << result.pluginsResolved << "\n";
                    summary << "Plugins Unresolved: " << result.pluginsUnresolved << "\n";

                    if (!result.warnings.empty())
                    {
                        summary << "\nWarnings:\n";
                        for (size_t i = 0; i < std::min(result.warnings.size(), size_t(10)); ++i)
                            summary << "- " << result.warnings[i] << "\n";
                        if (result.warnings.size() > 10)
                            summary << "... and " << (result.warnings.size() - 10) << " more\n";
                    }

                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                                           "Logic Import", summary);
                }
                else
                {
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                           "Import Failed", result.errorMessage);
                }
            });
    }

    void bounceToFile()
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Export Audio...", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.wav");

        chooser->launchAsync(juce::FileBrowserComponent::saveMode |
                                 juce::FileBrowserComponent::canSelectFiles,
                             [this, chooser](const juce::FileChooser &fc)
                             {
                                 auto file = fc.getResult();
                                 if (file == juce::File{})
                                     return;

                                 auto outputFile = file.withFileExtension("wav");

                                 OfflineRenderer::Settings settings;
                                 settings.sampleRate = session_.getSampleRate();
                                 if (settings.sampleRate <= 0)
                                     settings.sampleRate = 44100.0;

                                 BounceProgressDialog::launch(session_, outputFile, settings);
                             });
    }

    void syncSessionToEngine()
    {
        // Sync BPM
        engine_.sendSetBpm(session_.getBpm());

        // Publish full session snapshot for multi-track rendering
        engine_.publishSession(session_);

        // Rebuild mixer strips if track count changed
        if (mixerPanel_)
            mixerPanel_->rebuildStrips();
    }

    void newProject()
    {
        if (dirty_ && !confirmDiscardChanges())
            return;

        engine_.sendStop();
        session_ = Session();
        session_.addTrack("Track 1");
        currentProjectFile_ = juce::File{};
        commandManager_.clear();
        dirty_ = false;

        syncSessionToEngine();
        updateWindowTitle();
        timelineView_->zoomToFit();
        timelineView_->repaint();
    }

    void addDefaultMidiClipToTrack(int trackIndex)
    {
        double bpm = session_.getBpm();
        double sr = session_.getSampleRate();
        if (sr <= 0.0)
            sr = 44100.0;
        if (bpm <= 0.0)
            bpm = 120.0;

        SampleCount barLen =
            static_cast<SampleCount>((60.0 / bpm) * sr * session_.getTimeSigNumerator());
        if (auto *track = session_.getTrack(trackIndex))
            track->midiClips.push_back(MidiClip::createEmpty(0, barLen * 4));
    }

    void newProjectFromTemplate(int templateId)
    {
        if (dirty_ && !confirmDiscardChanges())
            return;

        engine_.sendStop();
        session_ = Session();

        switch (templateId)
        {
        case 1: // Songwriter
        {
            session_.addTrack("Lead Vocal", TrackType::Audio);
            session_.addTrack("Acoustic Guitar", TrackType::Audio);
            int midiTrack = session_.addTrack("Piano Sketch", TrackType::Midi);
            addDefaultMidiClipToTrack(midiTrack);
            break;
        }
        case 2: // Podcast
            session_.addTrack("Host", TrackType::Audio);
            session_.addTrack("Guest", TrackType::Audio);
            session_.addTrack("Music Bed", TrackType::Audio);
            break;
        case 3: // Electronic
        {
            int drums = session_.addTrack("Drums", TrackType::Midi);
            int bass = session_.addTrack("Bass", TrackType::Midi);
            int synth = session_.addTrack("Synth", TrackType::Midi);
            addDefaultMidiClipToTrack(drums);
            addDefaultMidiClipToTrack(bass);
            addDefaultMidiClipToTrack(synth);
            session_.addTrack("FX", TrackType::Audio);
            break;
        }
        default: // Empty
            session_.addTrack("Track 1", TrackType::Audio);
            break;
        }

        currentProjectFile_ = juce::File{};
        commandManager_.clear();
        dirty_ = false;

        syncSessionToEngine();
        updateWindowTitle();
        timelineView_->zoomToFit();
        timelineView_->repaint();
    }

    void markDirty()
    {
        if (!dirty_)
        {
            dirty_ = true;
            updateWindowTitle();
        }
    }

    void updateWindowTitle()
    {
        juce::String title = "Ampl";

        if (currentProjectFile_.existsAsFile())
            title = currentProjectFile_.getFileNameWithoutExtension() + " - Ampl";
        else if (!session_.getTracks().empty() &&
                 !(session_.getTracks().size() == 1 && session_.getTracks()[0].clips.empty()))
            title = "Untitled - Ampl";

        if (dirty_)
            title = "* " + title;

        if (auto *window = findParentComponentOfClass<juce::DocumentWindow>())
            window->setName(title);

        updateHeaderDisplays();
    }

    void updateHeaderDisplays()
    {
        juce::String projectName = "Untitled";
        if (currentProjectFile_.existsAsFile())
            projectName = currentProjectFile_.getFileNameWithoutExtension();

        projectHeaderLabel_.setText("Project: " + projectName + (dirty_ ? " *" : ""),
                                    juce::dontSendNotification);

        const auto trackCount = static_cast<int>(session_.getTracks().size());
        const auto bpm = session_.getBpm();
        const auto sr = session_.getSampleRate();
        const auto transportState = engine_.getTransport().getState();
        const char *transportLabel = "Stopped";
        if (transportState == Transport::State::Playing)
            transportLabel = "Playing";
        else if (transportState == Transport::State::Paused)
            transportLabel = "Paused";

        statusDisplayLabel_.setText(
            juce::String::formatted("%s | Tracks: %d | BPM: %.1f | %d/%d | %.1fkHz", transportLabel,
                                    trackCount, bpm, session_.getTimeSigNumerator(),
                                    session_.getTimeSigDenominator(),
                                    sr > 0.0 ? sr / 1000.0 : 44.1),
            juce::dontSendNotification);
    }

    void updateTrackInfoPanel()
    {
        if (!trackInfoPanel_ || !timelineView_)
            return;

        trackInfoPanel_->setSelectedTrackIndex(timelineView_->getSelectedTrackIndex());
        trackInfoPanel_->refresh();
    }

    bool confirmDiscardChanges()
    {
        return juce::AlertWindow::showOkCancelBox(
            juce::MessageBoxIconType::QuestionIcon, "Unsaved Changes",
            "You have unsaved changes. Discard them?", "Discard", "Cancel", nullptr, nullptr);
    }

    void openEditor(int trackIndex, const juce::String &clipId, bool isMidi)
    {
        editorVisible_ = true;
        editorIsMidi_ = isMidi;
        if (isMidi)
        {
            pianoRollEditor_->setClip(trackIndex, clipId);
            audioClipEditor_->clearClip();
        }
        else
        {
            audioClipEditor_->setClip(trackIndex, clipId);
            pianoRollEditor_->clearClip();
        }
        resized();
    }

    void closeEditor()
    {
        editorVisible_ = false;
        pianoRollEditor_->clearClip();
        audioClipEditor_->clearClip();
        resized();
    }

    void showShortcutHelp()
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                               "Keyboard Shortcuts",
                                               "Space: Play/Pause\n"
                                               "Return/Enter: Stop + Return to Start\n"
                                               "Cmd+N: New Project\n"
                                               "Cmd+O: Open Project\n"
                                               "Cmd+S: Save Project\n"
                                               "Cmd+E: Bounce Export\n"
                                               "Cmd+M: Toggle Mixer\n"
                                               "Cmd+I: Toggle Track Info\n"
                                               "Cmd+K: Toggle Piano Keyboard\n"
                                               "Cmd+Z / Cmd+Shift+Z: Undo / Redo\n"
                                               "Cmd+= / Cmd+- / Cmd+0: Zoom In / Out / Fit\n"
                                               "Up/Down: Select Previous/Next Track\n"
                                               "J: Return view to current playhead\n"
                                               "Esc: Close Editor\n"
                                               "\nMusical Typing (when keyboard is open):\n"
                                               "A-L keys: Play notes\n"
                                               "W-P keys: Play sharps/flats\n"
                                               "Z/X: Octave Down/Up\n"
                                               "C/V: Velocity Down/Up\n"
                                               "?: Show this help");
    }

    AudioEngine engine_;
    Session session_;
    CommandManager commandManager_;
    RecentProjects recentProjects_;

    std::unique_ptr<TransportBar> transportBar_;
    std::unique_ptr<TimelineView> timelineView_;
    std::unique_ptr<AudioFileBrowser> fileBrowser_;
    std::unique_ptr<MixerPanel> mixerPanel_;
    std::unique_ptr<TrackInfoPanel> trackInfoPanel_;
    std::unique_ptr<PianoRollEditor> pianoRollEditor_;
    std::unique_ptr<AudioClipEditor> audioClipEditor_;
    std::unique_ptr<juce::TextButton> settingsButton_;
    std::unique_ptr<juce::TextButton> loadButton_;
    std::unique_ptr<juce::TextButton> addTrackButton_;
    std::unique_ptr<juce::TextButton> addMidiTrackButton_;
    std::unique_ptr<juce::TextButton> zoomInButton_;
    std::unique_ptr<juce::TextButton> zoomOutButton_;
    std::unique_ptr<juce::TextButton> zoomFitButton_;
    std::unique_ptr<juce::TextButton> returnToPlayheadButton_;
    std::unique_ptr<juce::TextButton> undoButton_;
    std::unique_ptr<juce::TextButton> redoButton_;
    std::unique_ptr<juce::TextButton> saveButton_;
    std::unique_ptr<juce::TextButton> openButton_;
    std::unique_ptr<juce::TextButton> bounceButton_;
    std::unique_ptr<juce::TextButton> browserToggleButton_;
    std::unique_ptr<juce::TextButton> mixerToggleButton_;
    std::unique_ptr<juce::TextButton> infoToggleButton_;
    std::unique_ptr<PianoKeyboardPanel> pianoKeyboardPanel_;
    std::unique_ptr<juce::TextButton> pianoKeyboardToggleButton_;
    juce::Label projectHeaderLabel_;
    juce::Label statusDisplayLabel_;
    juce::Label shortcutHintLabel_;

    int toolbarAreaBottom_{0};
    std::vector<int> toolbarSeparatorXPositions_;

    juce::File currentProjectFile_;
    bool dirty_{false};
    bool fileBrowserVisible_{false};
    bool mixerVisible_{false};
    bool trackInfoVisible_{false};
    bool pianoKeyboardVisible_{false};
    bool editorVisible_{false};
    bool editorIsMidi_{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};

//==============================================================================
// Main window
//==============================================================================
class MainWindow : public juce::DocumentWindow
{
  public:
    MainWindow(const juce::String &name)
        : DocumentWindow(name, juce::Colour(0xff0f0f23), DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        auto *content = new MainContentComponent();
        setContentOwned(content, true);

#if JUCE_MAC
        juce::MenuBarModel::setMacMainMenu(content);
#else
        setMenuBar(content);
#endif

        setResizable(true, true);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }

    ~MainWindow() override
    {
#if JUCE_MAC
        juce::MenuBarModel::setMacMainMenu(nullptr);
#else
        setMenuBar(nullptr);
#endif
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

  private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};

//==============================================================================
// Platform helpers (implemented in MacHelpers.mm on Apple platforms)
//==============================================================================
#if JUCE_MAC
void hideDockIconForSubprocess();                  // defined in MacHelpers.mm
void suppressDockIconEarly(int argc, char **argv); // defined in MacHelpers.mm
#endif

//==============================================================================
// Application class
//==============================================================================
class AmplDawApplication : public juce::JUCEApplication
{
  public:
    AmplDawApplication() = default;

    const juce::String getApplicationName() override
    {
        return "Ampl";
    }
    const juce::String getApplicationVersion() override
    {
        return "0.1.0";
    }
    bool moreThanOneInstanceAllowed() override
    {
        // Allow multiple instances so that the headless --scan-plugin subprocess
        // launched by PluginManager can coexist with the running UI instance.
        // The subprocess calls quit() from initialise() before creating any window
        // or audio device, so there is no contention.
        return true;
    }

    void initialise(const juce::String &commandLine) override
    {
        // ----------------------------------------------------------------
        // Headless plugin-scan subprocess mode
        // Launched by PluginManager::scanForPlugins to safely scan one VST3/AU
        // file out-of-process (fork+exec avoids the CoreFoundation fork-safety
        // restrictions that cause crashes with bare fork()).
        //
        // Usage: Ampl --scan-plugin <path>
        // Output: one XML block per PluginDescription on stdout, each terminated
        //         by a "---END_PLUGIN---" sentinel line.
        // ----------------------------------------------------------------
        if (commandLine.contains("--scan-plugin"))
        {
            // Hide from the Dock and Cmd-Tab switcher immediately so that
            // scanning many plugins does not produce a flurry of dock bounces.
#if JUCE_MAC
            ampl::hideDockIconForSubprocess();
#endif

            auto pluginPath =
                commandLine.fromFirstOccurrenceOf("--scan-plugin", false, false).trim().unquoted();

            if (pluginPath.isNotEmpty())
            {
                juce::AudioPluginFormatManager fmgr;
#if JUCE_PLUGINHOST_VST3
                fmgr.addFormat(new juce::VST3PluginFormat());
#endif
#if JUCE_PLUGINHOST_AU && JUCE_MAC
                fmgr.addFormat(new juce::AudioUnitPluginFormat());
#endif

                for (int fi = 0; fi < fmgr.getNumFormats(); ++fi)
                {
                    auto *fmt = fmgr.getFormat(fi);
                    juce::OwnedArray<juce::PluginDescription> found;
                    fmt->findAllTypesForFile(found, pluginPath);
                    for (auto *desc : found)
                    {
                        if (desc)
                        {
                            if (auto xml = desc->createXml())
                                printf("%s\n---END_PLUGIN---\n", xml->toString().toRawUTF8());
                        }
                    }
                }
                fflush(stdout);
            }

            quit();
            return;
        }

        // ----------------------------------------------------------------
        // Normal UI startup
        // ----------------------------------------------------------------
        juce::LookAndFeel::setDefaultLookAndFeel(&ampl::gModernLookAndFeel);
        mainWindow_ = std::make_unique<MainWindow>("Ampl");
    }

    void shutdown() override
    {
        mainWindow_.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String & /*commandLine*/) override {}

  private:
    std::unique_ptr<MainWindow> mainWindow_;
};

} // namespace ampl

//==============================================================================
// Custom entry point — must detect --scan-plugin BEFORE JUCE/Cocoa registers
// the process with the Dock, otherwise each scan subprocess bounces its own
// icon in the Dock for the entire duration of the scan.
//==============================================================================

// Required by JUCE: tells the framework how to instantiate the application object.
juce::JUCEApplicationBase *juce_CreateApplication()
{
    return new ampl::AmplDawApplication();
}

int main(int argc, char *argv[])
{
#if JUCE_MAC
    // Set NSApplicationActivationPolicyProhibited BEFORE JUCE starts its event
    // loop so that scan subprocesses never register a Dock icon.
    ampl::suppressDockIconEarly(argc, argv);
#endif
    // Set createInstance BEFORE calling main() — this is what START_JUCE_APPLICATION
    // does internally. Calling the 2-arg main() without setting createInstance leaves
    // a null function pointer which crashes immediately (pc = 0x0).
    juce::JUCEApplicationBase::createInstance = &juce_CreateApplication;
    return juce::JUCEApplicationBase::main(argc, (const char **)argv);
}
