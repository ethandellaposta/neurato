#pragma once

#include "../engine/logic/LogicFeatures.hpp"
#include "ui/Theme.hpp"
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <map>

namespace ampl {

// Logic Pro X-style mixer panel
class LogicMixerPanel : public juce::Component, private juce::Timer {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void trackVolumeChanged(const std::string& trackId, float dB) = 0;
        virtual void trackPanChanged(const std::string& trackId, float pan) = 0;
        virtual void trackMuteChanged(const std::string& trackId, bool muted) = 0;
        virtual void trackSoloChanged(const std::string& trackId, bool soloed) = 0;
        virtual void trackRecordArmChanged(const std::string& trackId, bool armed) = 0;
        virtual void sendLevelChanged(const std::string& trackId, int sendIndex, float level) = 0;
        virtual void pluginSlotClicked(const std::string& trackId, int slot) = 0;
        virtual void vcaAssignmentChanged(const std::string& trackId, const std::string& vcaId) = 0;
    };

    LogicMixerPanel();
    ~LogicMixerPanel() override;

    // Track management
    void addTrack(const std::string& trackId, const LogicMixerChannel::ChannelStrip& strip);
    void removeTrack(const std::string& trackId);
    void updateTrack(const std::string& trackId, const LogicMixerChannel::ChannelStrip& strip);

    // View management
    void setTrackWidth(int width);
    void setShowPluginSlots(bool show);
    void setShowSends(bool show);
    void setShowVCAs(bool show);
    void setShowAutomation(bool show);

    // Solo/mute handling
    void handleSoloState(const std::string& trackId, bool soloed);
    void handleMuteState(const std::string& trackId, bool muted);

    // Listener management
    void addListener(Listener* listener);
    void removeListener(Listener* listener);

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    // Individual channel strip component
    class ChannelStripComponent : public juce::Component {
    public:
        ChannelStripComponent(const std::string& trackId,
                            const LogicMixerChannel::ChannelStrip& strip,
                            LogicMixerPanel& parent);
        ~ChannelStripComponent() override = default;

        void updateStrip(const LogicMixerChannel::ChannelStrip& strip);
        const std::string& getTrackId() const { return trackId_; }

        // Component overrides
        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;
        bool keyPressed(const juce::KeyPress& key) override;

    private:
        std::string trackId_;
        LogicMixerChannel::ChannelStrip strip_;
        LogicMixerPanel& parent_;

        // UI elements
        std::unique_ptr<juce::Slider> volumeSlider_;
        std::unique_ptr<juce::Slider> panSlider_;
        std::unique_ptr<juce::TextButton> muteButton_;
        std::unique_ptr<juce::TextButton> soloButton_;
        std::unique_ptr<juce::TextButton> recordArmButton_;

        // Plugin slots
        std::array<std::unique_ptr<juce::TextButton>, LogicMixerChannel::kPluginSlots> pluginButtons_;

        // Send controls
        std::array<std::unique_ptr<juce::Slider>, 8> sendSliders_;
        std::array<std::unique_ptr<juce::TextButton>, 8> sendButtons_;

        // VCA assignment
        std::unique_ptr<juce::ComboBox> vcaComboBox_;

        // Track name display
        std::unique_ptr<juce::Label> nameLabel_;

        // Meter display
        std::unique_ptr<juce::Component> meterComponent_;

        // Interaction state
        bool isDraggingVolume_{false};
        bool isDraggingPan_{false};
        float dragStartValue_{0.0f};

        // Layout constants
        static constexpr int kChannelWidth = 120;
        static constexpr int kPluginSlotHeight = 20;
        static constexpr int kSendHeight = 15;
        static constexpr int kMeterHeight = 100;
        static constexpr int kControlHeight = 30;

        // Helper methods
        void updateVolumeSlider();
        void updatePanSlider();
        void updateMuteButton();
        void updateSoloButton();
        void updateRecordArmButton();
        void updatePluginButtons();
        void updateSendControls();
        void updateVCAAssignment();

        void handleVolumeChange(float dB);
        void handlePanChange(float pan);
        void handleMuteToggle();
        void handleSoloToggle();
        void handleRecordArmToggle();
        void handlePluginSlotClick(int slot);
        void handleSendChange(int sendIndex, float level);
        void handleVCAAssignment(const std::string& vcaId);

        // Drawing helpers
        void drawChannelBackground(juce::Graphics& g);
        void drawPluginSlots(juce::Graphics& g);
        void drawSends(juce::Graphics& g);
        void drawControls(juce::Graphics& g);
        void drawMeters(juce::Graphics& g);
    };

    // Master output section
    class MasterSection : public juce::Component {
    public:
        MasterSection(LogicMixerPanel& parent);
        ~MasterSection() override = default;

        void updateMasterBus(const LogicEnvironment::Bus& bus);

        // Component overrides
        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;

    private:
        LogicMixerPanel& parent_;

        std::unique_ptr<juce::Slider> masterVolumeSlider_;
        std::unique_ptr<juce::Slider> masterPanSlider_;
        std::unique_ptr<juce::TextButton> masterMuteButton_;

        static constexpr int kMasterWidth = 150;
    };

    // VCA section
    class VCASection : public juce::Component {
    public:
        VCASection(LogicMixerPanel& parent);
        ~VCASection() override = default;

        void updateVCAs(const std::vector<LogicEnvironment::VCA>& vcas);

        // Component overrides
        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        LogicMixerPanel& parent_;

        std::map<std::string, std::unique_ptr<juce::Component>> vcaComponents_;
    };

    // Viewport for horizontal scrolling
    std::unique_ptr<juce::Viewport> viewport_;
    std::unique_ptr<juce::Component> channelContainer_;

    // Channel strips
    std::map<std::string, std::unique_ptr<ChannelStripComponent>> channelStrips_;

    // Fixed sections
    std::unique_ptr<MasterSection> masterSection_;
    std::unique_ptr<VCASection> vcaSection_;

    // View options
    int trackWidth_{120};
    bool showPluginSlots_{true};
    bool showSends_{true};
    bool showVCAs_{true};
    bool showAutomation_{true};

    // Interaction state
    std::string soloedTrack_;
    std::set<std::string> mutedTracks_;

    // Listeners
    juce::ListenerList<Listener> listeners_;

    // Timer for UI updates
    void timerCallback() override;

    // Helper methods
    void updateLayout();
    void updateChannelPositions();
    void handleSoloLogic(const std::string& trackId, bool soloed);
    void updateAllMuteStates();

    // Drawing helpers
    void drawBackground(juce::Graphics& g);
    void drawDivider(juce::Graphics& g, int x);
    void drawTrackHeaders(juce::Graphics& g);
};

// Logic Pro X-style channel strip popup editor
class ChannelStripEditor : public juce::DocumentWindow {
public:
    ChannelStripEditor(const std::string& trackId,
                      const LogicMixerChannel::ChannelStrip& strip);
    ~ChannelStripEditor() override;

    void updateStrip(const LogicMixerChannel::ChannelStrip& strip);

    // DocumentWindow overrides
    void closeButtonPressed() override;
    void resized() override;

private:
    std::string trackId_;
    LogicMixerChannel::ChannelStrip strip_;

    // Editor components
    std::unique_ptr<juce::Slider> volumeSlider_;
    std::unique_ptr<juce::Slider> panSlider_;
    std::unique_ptr<juce::Slider> gainSlider_;
    std::unique_ptr<juce::ToggleButton> polarityInvertButton_;

    // Plugin chain editor
    std::unique_ptr<juce::Component> pluginChainEditor_;

    // Send editor
    std::unique_ptr<juce::Component> sendEditor_;

    // Automation editor
    std::unique_ptr<juce::Component> automationEditor_;

    void setupEditor();
    void handleParameterChange(const std::string& parameter, float value);
};

// Logic Pro X-style mixer toolbar
class LogicMixerToolbar : public juce::Component, private juce::Button::Listener {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void viewOptionChanged(const std::string& option, bool enabled) = 0;
        virtual void createNewTrack() = 0;
        virtual void deleteSelectedTrack() = 0;
        virtual void toggleTrackType() = 0;
    };

    LogicMixerToolbar();
    ~LogicMixerToolbar() override;

    // Toolbar actions
    void setViewOption(const std::string& option, bool enabled);
    bool getViewOption(const std::string& option) const;

    // Listener management
    void addListener(Listener* listener);
    void removeListener(Listener* listener);

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::ListenerList<Listener> listeners_;

    // View option buttons
    std::map<std::string, std::unique_ptr<juce::ToggleButton>> viewButtons_;

    // Action buttons
    std::unique_ptr<juce::TextButton> newTrackButton_;
    std::unique_ptr<juce::TextButton> deleteTrackButton_;
    std::unique_ptr<juce::TextButton> trackTypeButton_;

    // Zoom controls
    std::unique_ptr<juce::Slider> zoomSlider_;
    std::unique_ptr<juce::TextButton> zoomFitButton_;

    void buttonClicked(juce::Button* button) override;
    void setupToolbar();
};

} // namespace ampl
