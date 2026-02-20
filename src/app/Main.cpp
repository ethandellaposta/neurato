#include <thread>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "engine/AudioEngine.h"
#include "engine/OfflineRenderer.h"
#include "model/Session.h"
#include "model/ProjectSerializer.h"
#include "commands/CommandManager.h"
#include "commands/ClipCommands.h"
#include "ui/TransportBar.h"
#include "ui/TrackView.h"
#include "ui/TimelineView.h"
#include "ui/AudioSettingsPanel.h"
#include "ui/AudioFileBrowser.h"
#include "ui/BounceProgressDialog.h"
#include "ui/MixerPanel.h"
#include "ui/PianoRollEditor.h"
#include "ui/AudioClipEditor.h"
#include "ui/Theme.h"
#include "commands/MidiCommands.h"
#include "util/RecentProjects.h"

namespace neurato {

//==============================================================================
// Main content component — holds transport bar, timeline, and settings
//==============================================================================
class MainContentComponent : public juce::Component,
                             public juce::Timer,
                             public juce::MenuBarModel
{
public:
    MainContentComponent()
    {
        engine_.initialise();

        // Create a default track in the session
        session_.addTrack("Track 1");

        transportBar_ = std::make_unique<TransportBar>(engine_);
        addAndMakeVisible(transportBar_.get());

        timelineView_ = std::make_unique<TimelineView>(engine_, session_);
        timelineView_->onSeek = [this](SampleCount pos) {
            engine_.sendSeek(pos);
        };
        timelineView_->onSessionChanged = [this] {
            markDirty();
            syncSessionToEngine();
        };
        addAndMakeVisible(timelineView_.get());

        // Toolbar buttons
        settingsButton_ = std::make_unique<juce::TextButton>("Audio Settings");
        settingsButton_->onClick = [this] { showAudioSettings(); };
        addAndMakeVisible(settingsButton_.get());

        loadButton_ = std::make_unique<juce::TextButton>("Load Audio");
        loadButton_->onClick = [this] { loadAudioFile(); };
        addAndMakeVisible(loadButton_.get());

        addTrackButton_ = std::make_unique<juce::TextButton>("+ Audio");
        addTrackButton_->onClick = [this] {
            session_.addTrack();
            markDirty();
            syncSessionToEngine();
            timelineView_->repaint();
        };
        addAndMakeVisible(addTrackButton_.get());

        addMidiTrackButton_ = std::make_unique<juce::TextButton>("+ MIDI");
        addMidiTrackButton_->onClick = [this] {
            int idx = session_.addMidiTrack();
            // Create a default empty MIDI clip (4 bars)
            double bpm = session_.getBpm();
            double sr = session_.getSampleRate();
            if (sr <= 0) sr = 44100.0;
            SampleCount barLen = static_cast<SampleCount>((60.0 / bpm) * sr * session_.getTimeSigNumerator());
            auto clip = MidiClip::createEmpty(0, barLen * 4);
            if (auto* track = session_.getTrack(idx))
                track->midiClips.push_back(clip);
            markDirty();
            syncSessionToEngine();
            timelineView_->repaint();
        };
        addAndMakeVisible(addMidiTrackButton_.get());

        zoomInButton_ = std::make_unique<juce::TextButton>("+");
        zoomInButton_->onClick = [this] { timelineView_->zoomIn(); };
        addAndMakeVisible(zoomInButton_.get());

        zoomOutButton_ = std::make_unique<juce::TextButton>("-");
        zoomOutButton_->onClick = [this] { timelineView_->zoomOut(); };
        addAndMakeVisible(zoomOutButton_.get());

        zoomFitButton_ = std::make_unique<juce::TextButton>("Fit");
        zoomFitButton_->onClick = [this] { timelineView_->zoomToFit(); };
        addAndMakeVisible(zoomFitButton_.get());

        undoButton_ = std::make_unique<juce::TextButton>("Undo");
        undoButton_->onClick = [this] {
            if (commandManager_.undo(session_))
            {
                markDirty();
                syncSessionToEngine();
                timelineView_->repaint();
            }
        };
        addAndMakeVisible(undoButton_.get());

        redoButton_ = std::make_unique<juce::TextButton>("Redo");
        redoButton_->onClick = [this] {
            if (commandManager_.redo(session_))
            {
                markDirty();
                syncSessionToEngine();
                timelineView_->repaint();
            }
        };
        addAndMakeVisible(redoButton_.get());

        // Update undo/redo button state
        commandManager_.onStateChanged = [this] {
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
        browserToggleButton_->onClick = [this] {
            fileBrowserVisible_ = browserToggleButton_->getToggleState();
            resized();
        };
        addAndMakeVisible(browserToggleButton_.get());

        // Audio file browser panel (hidden by default)
        fileBrowser_ = std::make_unique<AudioFileBrowser>();
        fileBrowser_->onFileSelected = [this](const juce::File& file) {
            importAudioFile(file);
        };
        addChildComponent(fileBrowser_.get());

        // Mixer panel (hidden by default)
        mixerPanel_ = std::make_unique<MixerPanel>(session_, commandManager_);
        mixerPanel_->onSessionChanged = [this] {
            markDirty();
            syncSessionToEngine();
            timelineView_->repaint();
        };
        addChildComponent(mixerPanel_.get());

        // Editors (hidden by default)
        pianoRollEditor_ = std::make_unique<PianoRollEditor>(session_, commandManager_);
        pianoRollEditor_->onSessionChanged = [this] {
            markDirty();
            syncSessionToEngine();
            timelineView_->repaint();
        };
        pianoRollEditor_->onCloseRequested = [this] { closeEditor(); };
        addChildComponent(pianoRollEditor_.get());

        audioClipEditor_ = std::make_unique<AudioClipEditor>(session_, commandManager_);
        audioClipEditor_->onSessionChanged = [this] {
            markDirty();
            syncSessionToEngine();
            timelineView_->repaint();
        };
        audioClipEditor_->onCloseRequested = [this] { closeEditor(); };
        addChildComponent(audioClipEditor_.get());

        // Wire timeline double-click to open editors
        timelineView_->onClipDoubleClicked = [this](int trackIdx, juce::String clipId, bool isMidi) {
            openEditor(trackIdx, clipId, isMidi);
        };

        mixerToggleButton_ = std::make_unique<juce::TextButton>("Mixer");
        mixerToggleButton_->setClickingTogglesState(true);
        mixerToggleButton_->onClick = [this] {
            mixerVisible_ = mixerToggleButton_->getToggleState();
            resized();
        };
        addAndMakeVisible(mixerToggleButton_.get());

        setSize(1280, 720);
        updateWindowTitle();

        // Ensure we use the session renderer from the start
        syncSessionToEngine();

        // Single timer polls the SPSC queue and dispatches to all UI components
        startTimerHz(30);
    }

    ~MainContentComponent() override
    {
        stopTimer();
        engine_.shutdown();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(Theme::bgDeepest));
    }

    void resized() override
    {
        auto area = getLocalBounds();

        // Transport bar at top
        transportBar_->setBounds(area.removeFromTop(50));

        // Toolbar row
        auto toolbar = area.removeFromTop(32);
        toolbar.removeFromLeft(8);
        loadButton_->setBounds(toolbar.removeFromLeft(80));
        toolbar.removeFromLeft(4);
        addTrackButton_->setBounds(toolbar.removeFromLeft(60));
        toolbar.removeFromLeft(2);
        addMidiTrackButton_->setBounds(toolbar.removeFromLeft(55));
        toolbar.removeFromLeft(8);
        undoButton_->setBounds(toolbar.removeFromLeft(60));
        toolbar.removeFromLeft(4);
        redoButton_->setBounds(toolbar.removeFromLeft(60));
        toolbar.removeFromLeft(12);
        zoomOutButton_->setBounds(toolbar.removeFromLeft(30));
        toolbar.removeFromLeft(2);
        zoomInButton_->setBounds(toolbar.removeFromLeft(30));
        toolbar.removeFromLeft(4);
        zoomFitButton_->setBounds(toolbar.removeFromLeft(40));
        toolbar.removeFromLeft(12);
        openButton_->setBounds(toolbar.removeFromLeft(60));
        toolbar.removeFromLeft(4);
        saveButton_->setBounds(toolbar.removeFromLeft(60));
        toolbar.removeFromLeft(4);
        bounceButton_->setBounds(toolbar.removeFromLeft(70));
        toolbar.removeFromLeft(8);
        browserToggleButton_->setBounds(toolbar.removeFromLeft(50));
        toolbar.removeFromLeft(4);
        mixerToggleButton_->setBounds(toolbar.removeFromLeft(55));

        // Settings button on the right
        settingsButton_->setBounds(toolbar.removeFromRight(110).reduced(0, 2));

        // Content area: optional file browser sidebar + timeline
        area.removeFromTop(2);

        if (fileBrowserVisible_)
        {
            fileBrowser_->setVisible(true);
            fileBrowser_->setBounds(area.removeFromLeft(250));
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
                pianoRollEditor_->setBounds(area.removeFromBottom(Theme::editorHeight));
            }
            else
            {
                audioClipEditor_->setVisible(true);
                pianoRollEditor_->setVisible(false);
                audioClipEditor_->setBounds(area.removeFromBottom(Theme::editorHeight));
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
            mixerPanel_->setBounds(area.removeFromBottom(Theme::mixerHeight));
        }
        else
        {
            mixerPanel_->setVisible(false);
        }

        timelineView_->setBounds(area);
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
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        // Space = toggle play/stop
        if (key == juce::KeyPress::spaceKey)
        {
            engine_.sendTogglePlayStop();
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
        if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier |
                                        juce::ModifierKeys::shiftModifier, 0))
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
        // Escape = close editor
        if (key == juce::KeyPress::escapeKey)
        {
            if (editorVisible_)
            {
                closeEditor();
                return true;
            }
        }
        return false;
    }

    // --- MenuBarModel ---
    juce::StringArray getMenuBarNames() override
    {
        return { "File", "View" };
    }

    juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String& /*menuName*/) override
    {
        juce::PopupMenu menu;

        if (menuIndex == 0) // File
        {
            menu.addItem(1, "New Project", true, false);
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
            menu.addItem(5, "Export Audio...", true, false);
            menu.addSeparator();
            menu.addItem(6, "Audio Settings...", true, false);

            #if ! JUCE_MAC
            menu.addSeparator();
            menu.addItem(99, "Quit");
            #endif
        }
        else if (menuIndex == 1) // View
        {
            menu.addItem(10, "Toggle Mixer", true, mixerVisible_);
            menu.addItem(11, "Toggle File Browser", true, fileBrowserVisible_);
        }

        return menu;
    }

    void menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/) override
    {
        switch (menuItemID)
        {
            case 1: newProject(); break;
            case 2: openProject(); break;
            case 3: saveProject(); break;
            case 4: saveProjectAs(); break;
            case 5: bounceToFile(); break;
            case 6: showAudioSettings(); break;
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
            case 99: juce::JUCEApplication::getInstance()->systemRequestedQuit(); break;
            case 199: recentProjects_.clear(); break;
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
        auto* settingsPanel = new AudioSettingsPanel(engine_);
        settingsPanel->setSize(500, 400);

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
        auto chooser = std::make_shared<juce::FileChooser>(
            "Select an audio file...",
            juce::File{},
            "*.wav;*.aiff;*.aif;*.mp3;*.flac;*.ogg");

        chooser->launchAsync(juce::FileBrowserComponent::openMode |
                              juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (!file.existsAsFile())
                    return;
                importAudioFile(file);
            });
    }

    void importAudioFile(const juce::File& file)
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
            "Save Project...",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.neurato");

        chooser->launchAsync(juce::FileBrowserComponent::saveMode |
                              juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (file == juce::File{})
                    return;

                auto projectFile = file.withFileExtension("neurato");
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
            "Open Project...",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.neurato");

        chooser->launchAsync(juce::FileBrowserComponent::openMode |
                              juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (!file.existsAsFile())
                    return;
                loadProjectFile(file);
            });
    }

    void loadProjectFile(const juce::File& file)
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

    void bounceToFile()
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Export Audio...",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.wav");

        chooser->launchAsync(juce::FileBrowserComponent::saveMode |
                              juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc) {
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
        juce::String title = "Neurato";

        if (currentProjectFile_.existsAsFile())
            title = currentProjectFile_.getFileNameWithoutExtension() + " - Neurato";
        else if (!session_.getTracks().empty() &&
                 !(session_.getTracks().size() == 1 && session_.getTracks()[0].clips.empty()))
            title = "Untitled - Neurato";

        if (dirty_)
            title = "* " + title;

        if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
            window->setName(title);
    }

    bool confirmDiscardChanges()
    {
        return juce::AlertWindow::showOkCancelBox(
            juce::MessageBoxIconType::QuestionIcon,
            "Unsaved Changes",
            "You have unsaved changes. Discard them?",
            "Discard", "Cancel",
            nullptr, nullptr);
    }

    void openEditor(int trackIndex, const juce::String& clipId, bool isMidi)
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

    AudioEngine engine_;
    Session session_;
    CommandManager commandManager_;
    RecentProjects recentProjects_;

    std::unique_ptr<TransportBar> transportBar_;
    std::unique_ptr<TimelineView> timelineView_;
    std::unique_ptr<AudioFileBrowser> fileBrowser_;
    std::unique_ptr<MixerPanel> mixerPanel_;
    std::unique_ptr<PianoRollEditor> pianoRollEditor_;
    std::unique_ptr<AudioClipEditor> audioClipEditor_;
    std::unique_ptr<juce::TextButton> settingsButton_;
    std::unique_ptr<juce::TextButton> loadButton_;
    std::unique_ptr<juce::TextButton> addTrackButton_;
    std::unique_ptr<juce::TextButton> addMidiTrackButton_;
    std::unique_ptr<juce::TextButton> zoomInButton_;
    std::unique_ptr<juce::TextButton> zoomOutButton_;
    std::unique_ptr<juce::TextButton> zoomFitButton_;
    std::unique_ptr<juce::TextButton> undoButton_;
    std::unique_ptr<juce::TextButton> redoButton_;
    std::unique_ptr<juce::TextButton> saveButton_;
    std::unique_ptr<juce::TextButton> openButton_;
    std::unique_ptr<juce::TextButton> bounceButton_;
    std::unique_ptr<juce::TextButton> browserToggleButton_;
    std::unique_ptr<juce::TextButton> mixerToggleButton_;

    juce::File currentProjectFile_;
    bool dirty_{false};
    bool fileBrowserVisible_{false};
    bool mixerVisible_{false};
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
    MainWindow(const juce::String& name)
        : DocumentWindow(name,
                          juce::Colour(0xff0f0f23),
                          DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        auto* content = new MainContentComponent();
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
// Application class
//==============================================================================
class NeuratoDawApplication : public juce::JUCEApplication
{
public:
    NeuratoDawApplication() = default;

    const juce::String getApplicationName() override    { return "Neurato"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override           { return false; }

    void initialise(const juce::String& /*commandLine*/) override
    {
        mainWindow_ = std::make_unique<MainWindow>("Neurato");
    }

    void shutdown() override
    {
        mainWindow_.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String& /*commandLine*/) override {}

private:
    std::unique_ptr<MainWindow> mainWindow_;
};

} // namespace neurato

//==============================================================================
// JUCE application entry point
//==============================================================================
START_JUCE_APPLICATION(neurato::NeuratoDawApplication)
