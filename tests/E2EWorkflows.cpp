#include <gtest/gtest.h>

#include "JuceGuiFixture.hpp"
#include "commands/ClipCommands.hpp"
#include "commands/CommandManager.hpp"
#include "engine/render/OfflineRenderer.hpp"
#include "model/ProjectSerializer.hpp"
#include "model/Session.hpp"
#include "ui/panels/mixer/TrackInfoPanel.hpp"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <atomic>
#include <cmath>

namespace ampl
{
namespace
{

juce::File createSineWaveTestFile(const juce::File &parentDir, const juce::String &fileName,
                                  double sampleRate, int numSamples)
{
    auto file = parentDir.getChildFile(fileName);
    juce::WavAudioFormat wavFormat;
    auto stream = file.createOutputStream();
    if (stream == nullptr)
        return {};

    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(stream.get(), sampleRate, 1, 24, {}, 0));
    if (writer == nullptr)
        return {};

    stream.release();

    juce::AudioBuffer<float> buffer(1, numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        const auto phase = static_cast<float>(2.0 * juce::MathConstants<double>::pi * 220.0 *
                                              (static_cast<double>(i) / sampleRate));
        buffer.setSample(0, i, 0.2f * std::sin(phase));
    }

    writer->writeFromAudioSampleBuffer(buffer, 0, numSamples);
    return file;
}

TEST(E2EWorkflow, MixerCommandsAndUndoRedoWorkEndToEnd)
{
    Session session;
    CommandManager commandManager;

    const int track0 = session.addTrack("Drums");
    const int track1 = session.addTrack("Bass");
    ASSERT_EQ(track0, 0);
    ASSERT_EQ(track1, 1);

    commandManager.execute(std::make_unique<SetTrackGainCommand>(track0, -6.5f), session);
    commandManager.execute(std::make_unique<SetTrackPanCommand>(track0, -0.35f), session);
    commandManager.execute(std::make_unique<SetTrackMuteCommand>(track0, true), session);
    commandManager.execute(std::make_unique<SetTrackSoloCommand>(track1, true), session);
    commandManager.execute(std::make_unique<SetMasterGainCommand>(-2.0f), session);
    commandManager.execute(std::make_unique<SetMasterPanCommand>(0.22f), session);

    const auto *drums = session.getTrack(track0);
    const auto *bass = session.getTrack(track1);
    ASSERT_NE(drums, nullptr);
    ASSERT_NE(bass, nullptr);

    EXPECT_NEAR(drums->gainDb, -6.5f, 0.001f);
    EXPECT_NEAR(drums->pan, -0.35f, 0.001f);
    EXPECT_TRUE(drums->muted);
    EXPECT_TRUE(bass->solo);
    EXPECT_NEAR(session.getMasterGainDb(), -2.0f, 0.001f);
    EXPECT_NEAR(session.getMasterPan(), 0.22f, 0.001f);

    EXPECT_TRUE(commandManager.undo(session)); // master pan
    EXPECT_TRUE(commandManager.undo(session)); // master gain
    EXPECT_TRUE(commandManager.undo(session)); // bass solo

    EXPECT_NEAR(session.getMasterPan(), 0.0f, 0.001f);
    EXPECT_NEAR(session.getMasterGainDb(), 0.0f, 0.001f);
    EXPECT_FALSE(session.getTrack(track1)->solo);

    EXPECT_TRUE(commandManager.redo(session));
    EXPECT_TRUE(commandManager.redo(session));
    EXPECT_TRUE(commandManager.redo(session));

    EXPECT_TRUE(session.getTrack(track1)->solo);
    EXPECT_NEAR(session.getMasterGainDb(), -2.0f, 0.001f);
    EXPECT_NEAR(session.getMasterPan(), 0.22f, 0.001f);
}

TEST_F(JuceGuiFixture, InspectorEditingUpdatesSessionState)
{
    Session session;
    CommandManager commandManager;

    const int trackIndex = session.addTrack("Vox");
    ASSERT_EQ(trackIndex, 0);

    TrackInfoPanel panel(session, commandManager);
    panel.setBounds(0, 0, 420, 700);
    panel.resized();
    panel.setSelectedTrackIndex(trackIndex);
    panel.refresh();

    auto *nameEditor =
        dynamic_cast<juce::TextEditor *>(panel.findChildWithID("trackInfo.trackName"));
    auto *applyNameButton =
        dynamic_cast<juce::TextButton *>(panel.findChildWithID("trackInfo.applyName"));
    auto *gainSlider = dynamic_cast<juce::Slider *>(panel.findChildWithID("trackInfo.gain"));
    auto *panSlider = dynamic_cast<juce::Slider *>(panel.findChildWithID("trackInfo.pan"));
    auto *muteButton = dynamic_cast<juce::TextButton *>(panel.findChildWithID("trackInfo.mute"));
    auto *soloButton = dynamic_cast<juce::TextButton *>(panel.findChildWithID("trackInfo.solo"));

    ASSERT_NE(nameEditor, nullptr);
    ASSERT_NE(applyNameButton, nullptr);
    ASSERT_NE(gainSlider, nullptr);
    ASSERT_NE(panSlider, nullptr);
    ASSERT_NE(muteButton, nullptr);
    ASSERT_NE(soloButton, nullptr);

    EXPECT_TRUE(panel.editSelectedTrackName("Lead Vox"));
    EXPECT_TRUE(panel.editSelectedTrackGain(-8.2f));
    EXPECT_TRUE(panel.editSelectedTrackPan(0.18f));
    EXPECT_TRUE(panel.editSelectedTrackMute(true));
    EXPECT_TRUE(panel.editSelectedTrackSolo(true));

    const auto *track = session.getTrack(trackIndex);
    ASSERT_NE(track, nullptr);

    EXPECT_EQ(track->name, "Lead Vox");
    EXPECT_NEAR(track->gainDb, -8.2f, 0.001f);
    EXPECT_NEAR(track->pan, 0.18f, 0.001f);
    EXPECT_TRUE(track->muted);
    EXPECT_TRUE(track->solo);

    EXPECT_GE(commandManager.getUndoStackSize(), 5);
}

TEST_F(JuceGuiFixture, InspectorDetailsShowPluginAndEqState)
{
    Session session;
    CommandManager commandManager;

    const int trackIndex = session.addTrack("Keys", TrackType::Midi);
    auto *track = session.getTrack(trackIndex);
    ASSERT_NE(track, nullptr);

    PluginSlot instrument;
    instrument.pluginName = "Retro Synth";
    instrument.pluginFormat = "AU";
    track->instrumentPlugin = instrument;

    PluginSlot eq;
    eq.pluginName = "Channel EQ";
    eq.pluginFormat = "AU";

    PluginSlot compressor;
    compressor.pluginName = "Compressor";
    compressor.pluginFormat = "VST3";
    compressor.bypassed = true;

    track->pluginChain.push_back(eq);
    track->pluginChain.push_back(compressor);

    TrackInfoPanel panel(session, commandManager);
    panel.setBounds(0, 0, 420, 700);
    panel.resized();
    panel.setSelectedTrackIndex(trackIndex);
    panel.refresh();

    auto *detailsEditor =
        dynamic_cast<juce::TextEditor *>(panel.findChildWithID("trackInfo.details"));
    ASSERT_NE(detailsEditor, nullptr);

    const auto details = detailsEditor->getText();
    EXPECT_TRUE(details.contains("Instrument: Retro Synth [AU]"));
    EXPECT_TRUE(details.contains("Channel EQ"));
    EXPECT_TRUE(details.contains("Compressor [VST3] (Bypassed)"));
    EXPECT_TRUE(details.contains("EQ Plugin Present: On"));
}

TEST(E2EWorkflow, ProjectRoundTripPersistsInspectorAndMixerFields)
{
    Session session;

    const int audioTrackIndex = session.addTrack("Drums", TrackType::Audio);
    const int midiTrackIndex = session.addTrack("Synth", TrackType::Midi);

    auto *audioTrack = session.getTrack(audioTrackIndex);
    auto *midiTrack = session.getTrack(midiTrackIndex);
    ASSERT_NE(audioTrack, nullptr);
    ASSERT_NE(midiTrack, nullptr);

    audioTrack->gainDb = -4.5f;
    audioTrack->pan = -0.2f;
    audioTrack->muted = true;

    midiTrack->gainDb = 1.3f;
    midiTrack->pan = 0.4f;
    midiTrack->solo = true;

    PluginSlot instrument;
    instrument.pluginName = "Sampler";
    instrument.pluginFormat = "AU";
    midiTrack->instrumentPlugin = instrument;

    PluginSlot fx;
    fx.pluginName = "FabFilter Pro-Q";
    fx.pluginFormat = "VST3";
    midiTrack->pluginChain.push_back(fx);

    session.setMasterGainDb(-1.5f);
    session.setMasterPan(0.15f);

    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("ampl_e2e_" + juce::Uuid().toString());
    ASSERT_TRUE(tempDir.createDirectory());

    auto projectFile = tempDir.getChildFile("roundtrip.ampl");
    ASSERT_TRUE(ProjectSerializer::save(session, projectFile));

    Session loaded;
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    ASSERT_TRUE(ProjectSerializer::load(loaded, projectFile, formatManager));

    ASSERT_EQ(loaded.getTracks().size(), 2u);

    const auto *loadedAudio = loaded.getTrack(0);
    const auto *loadedMidi = loaded.getTrack(1);
    ASSERT_NE(loadedAudio, nullptr);
    ASSERT_NE(loadedMidi, nullptr);

    EXPECT_EQ(loadedAudio->name, "Drums");
    EXPECT_NEAR(loadedAudio->gainDb, -4.5f, 0.001f);
    EXPECT_NEAR(loadedAudio->pan, -0.2f, 0.001f);
    EXPECT_TRUE(loadedAudio->muted);

    EXPECT_EQ(loadedMidi->name, "Synth");
    EXPECT_NEAR(loadedMidi->gainDb, 1.3f, 0.001f);
    EXPECT_NEAR(loadedMidi->pan, 0.4f, 0.001f);
    EXPECT_TRUE(loadedMidi->solo);
    ASSERT_TRUE(loadedMidi->instrumentPlugin.has_value());
    EXPECT_EQ(loadedMidi->instrumentPlugin->pluginName, "Sampler");
    ASSERT_EQ(loadedMidi->pluginChain.size(), 1u);
    EXPECT_EQ(loadedMidi->pluginChain[0].pluginName, "FabFilter Pro-Q");

    EXPECT_NEAR(loaded.getMasterGainDb(), -1.5f, 0.001f);
    EXPECT_NEAR(loaded.getMasterPan(), 0.15f, 0.001f);

    tempDir.deleteRecursively();
}

TEST(E2EWorkflow, ProjectSaveEmbedsAudioData)
{
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getChildFile("ampl_embed_" + juce::Uuid().toString());
    ASSERT_TRUE(tempDir.createDirectory());

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    auto sourceFile = createSineWaveTestFile(tempDir, "embed.wav", 44100.0, 22050);
    ASSERT_TRUE(sourceFile.existsAsFile());

    Session session;
    const int trackIndex = session.addTrack("Audio", TrackType::Audio);

    auto asset = session.loadAudioAsset(sourceFile, formatManager);
    ASSERT_TRUE(asset != nullptr);

    auto clip = Clip::fromAsset(asset, 0);
    ASSERT_TRUE(session.addClipToTrack(trackIndex, clip));

    auto projectFile = tempDir.getChildFile("embedded.ampl");
    ASSERT_TRUE(ProjectSerializer::save(session, projectFile));
    ASSERT_TRUE(projectFile.existsAsFile());

    // Remove original asset to verify load relies on embedded data, not disk
    ASSERT_TRUE(sourceFile.deleteFile());

    Session loaded;
    ASSERT_TRUE(ProjectSerializer::load(loaded, projectFile, formatManager));

    ASSERT_EQ(loaded.getTracks().size(), 1u);
    const auto *loadedTrack = loaded.getTrack(0);
    ASSERT_NE(loadedTrack, nullptr);
    ASSERT_EQ(loadedTrack->clips.size(), 1u);

    const auto &loadedClip = loadedTrack->clips.front();
    ASSERT_TRUE(loadedClip.asset != nullptr);
    EXPECT_GT(loadedClip.asset->lengthInSamples, 0);
    EXPECT_EQ(loadedClip.asset->numChannels, 1);

    tempDir.deleteRecursively();
}

TEST(E2EWorkflow, Phase1Workflow_CreateImportAddMidiAndBounce)
{
    auto workspaceDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                            .getChildFile("ampl_phase1_e2e_" + juce::Uuid().toString());
    ASSERT_TRUE(workspaceDir.createDirectory());

    auto importFile = createSineWaveTestFile(workspaceDir, "import.wav", 44100.0, 44100);
    ASSERT_TRUE(importFile.existsAsFile());

    Session session;
    CommandManager commandManager;
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    const int audioTrackIndex = session.addTrack("Audio 1", TrackType::Audio);
    ASSERT_GE(audioTrackIndex, 0);

    auto asset = session.loadAudioAsset(importFile, formatManager);
    ASSERT_TRUE(asset != nullptr);
    EXPECT_GT(asset->lengthInSamples, 0);

    auto importedClip = Clip::fromAsset(asset, 0);
    commandManager.execute(std::make_unique<AddClipCommand>(audioTrackIndex, importedClip),
                           session);

    const auto *audioTrack = session.getTrack(audioTrackIndex);
    ASSERT_NE(audioTrack, nullptr);
    EXPECT_EQ(audioTrack->clips.size(), 1u);

    commandManager.execute(std::make_unique<AddTrackCommand>(TrackType::Midi, "MIDI 1", true),
                           session);

    ASSERT_EQ(session.getTracks().size(), 2u);
    const auto *midiTrack = session.getTrack(1);
    ASSERT_NE(midiTrack, nullptr);
    EXPECT_EQ(midiTrack->type, TrackType::Midi);
    EXPECT_FALSE(midiTrack->midiClips.empty());

    OfflineRenderer::Settings settings;
    settings.sampleRate = 44100.0;
    settings.blockSize = 512;
    settings.startSample = 0;
    settings.endSample = 44100;

    auto bounceFile = workspaceDir.getChildFile("bounce.wav");
    std::atomic<bool> cancelFlag{false};

    const bool renderOk =
        OfflineRenderer::render(session, bounceFile, settings, nullptr, &cancelFlag);
    EXPECT_TRUE(renderOk);
    EXPECT_TRUE(bounceFile.existsAsFile());
    EXPECT_GT(bounceFile.getSize(), 44);

    workspaceDir.deleteRecursively();
}

TEST(E2EWorkflow, Phase2UndoRedo_AddTrackCommandMaintainsExpectedState)
{
    Session session;
    CommandManager commandManager;

    EXPECT_TRUE(session.getTracks().empty());

    commandManager.execute(
        std::make_unique<AddTrackCommand>(TrackType::Audio, "Audio Undoable", false), session);
    commandManager.execute(
        std::make_unique<AddTrackCommand>(TrackType::Midi, "MIDI Undoable", true), session);

    ASSERT_EQ(session.getTracks().size(), 2u);
    EXPECT_EQ(session.getTracks()[0].name, "Audio Undoable");
    EXPECT_EQ(session.getTracks()[0].type, TrackType::Audio);
    EXPECT_TRUE(session.getTracks()[0].midiClips.empty());

    EXPECT_EQ(session.getTracks()[1].name, "MIDI Undoable");
    EXPECT_EQ(session.getTracks()[1].type, TrackType::Midi);
    EXPECT_FALSE(session.getTracks()[1].midiClips.empty());

    EXPECT_TRUE(commandManager.undo(session));
    ASSERT_EQ(session.getTracks().size(), 1u);
    EXPECT_EQ(session.getTracks()[0].name, "Audio Undoable");

    EXPECT_TRUE(commandManager.undo(session));
    EXPECT_TRUE(session.getTracks().empty());

    EXPECT_TRUE(commandManager.redo(session));
    ASSERT_EQ(session.getTracks().size(), 1u);
    EXPECT_EQ(session.getTracks()[0].name, "Audio Undoable");

    EXPECT_TRUE(commandManager.redo(session));
    ASSERT_EQ(session.getTracks().size(), 2u);
    EXPECT_EQ(session.getTracks()[1].name, "MIDI Undoable");
    EXPECT_FALSE(session.getTracks()[1].midiClips.empty());
}

} // namespace
} // namespace ampl
