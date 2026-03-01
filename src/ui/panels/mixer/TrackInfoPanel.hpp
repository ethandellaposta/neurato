#pragma once

#include "commands/CommandManager.hpp"
#include "engine/plugins/manager/PluginManager.hpp"
#include "model/Session.hpp"
#include "ui/Theme.hpp"
#include <juce_gui_basics/juce_gui_basics.h>

namespace ampl
{

class TrackInfoPanel : public juce::Component
{
  public:
    TrackInfoPanel(Session &session, CommandManager &commandManager);

    void paint(juce::Graphics &g) override;
    void resized() override;

    void setSelectedTrackIndex(int trackIndex);
    int getSelectedTrackIndex() const
    {
        return selectedTrackIndex_;
    }

    void setPluginManager(PluginManager *pm)
    {
        pluginManager_ = pm;
    }

    void refresh();

    bool editSelectedTrackName(const juce::String &newName);
    bool editSelectedTrackGain(float gainDb);
    bool editSelectedTrackPan(float pan);
    bool editSelectedTrackMute(bool muted);
    bool editSelectedTrackSolo(bool solo);

    std::function<void()> onSessionChanged;

  private:
    void updateEditingControls();
    void commitTrackName();
    void commitTrackGain(float gainDb);
    void commitTrackPan(float pan);
    void commitTrackMute(bool muted);
    void commitTrackSolo(bool solo);

    void showInstrumentPicker();
    void showFxPicker();
    void showFxContextMenu(int fxIndex);

    juce::String buildTrackSummary(const TrackState &track, int trackIndex) const;
    static juce::String formatPan(float pan);
    static juce::String boolText(bool value);

    Session &session_;
    CommandManager &commandManager_;
    PluginManager *pluginManager_{nullptr};
    int selectedTrackIndex_{-1};
    bool isRefreshingUi_{false};
    juce::String lastSummaryText_;

    juce::Label titleLabel_;
    juce::Label trackNameLabel_;
    juce::TextEditor trackNameEditor_;
    juce::TextButton applyNameButton_{"Apply"};

    juce::Label gainLabel_;
    juce::Slider gainSlider_;

    juce::Label panLabel_;
    juce::Slider panSlider_;

    juce::TextButton muteButton_{"Mute"};
    juce::TextButton soloButton_{"Solo"};

    // Plugin section
    juce::Label pluginsLabel_;
    juce::TextButton setInstrumentButton_{"Set Instrument"};
    juce::TextButton addFxButton_{"+FX"};

    juce::Label detailsLabel_;
    juce::TextEditor infoText_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackInfoPanel)
};

} // namespace ampl
