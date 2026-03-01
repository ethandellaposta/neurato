#pragma once

#include "util/Types.hpp"
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <functional>

namespace ampl
{

/// Information about a connected MIDI device
struct MidiDeviceInfo
{
    juce::String name;
    juce::String identifier;
    bool isInput{true};
    bool isEnabled{false};
};

/// Information about an audio device channel
struct AudioInputInfo
{
    juce::String deviceName;
    int channelIndex{0};
    bool isEnabled{false};
};

/**
 * Manages external MIDI input devices and audio input routing.
 *
 * - Enumerates and opens real MIDI input devices (keyboards, controllers).
 * - Collects incoming MIDI messages into a juce::MidiMessageCollector
 *   so the audio thread can pull them lock-free via removeNextBlockOfMessages().
 * - Tracks which MIDI device is routed to which track.
 * - Provides audio input state (audio input is managed by AudioDeviceManager,
 *   this class just tracks routing intent).
 *
 * Thread safety:
 *   - enumerate/open/close methods: UI thread only
 *   - getMidiMessagesForBlock(): audio thread only (lock-free)
 */
class ExternalIOManager : public juce::MidiInputCallback
{
  public:
    ExternalIOManager();
    ~ExternalIOManager() override;

    // ─── MIDI Input ───────────────────────────────────────────────

    /// Enumerate available MIDI input devices
    std::vector<MidiDeviceInfo> getAvailableMidiInputs() const;

    /// Enable a MIDI input device by identifier. Returns true on success.
    bool enableMidiInput(const juce::String &deviceIdentifier);

    /// Disable a MIDI input device by identifier.
    void disableMidiInput(const juce::String &deviceIdentifier);

    /// Disable all MIDI inputs
    void disableAllMidiInputs();

    /// Check if a specific MIDI input is currently enabled
    bool isMidiInputEnabled(const juce::String &deviceIdentifier) const;

    /// Get list of currently enabled MIDI input device identifiers
    std::vector<juce::String> getEnabledMidiInputs() const;

    /// Set the sample rate for the MIDI collector
    void setSampleRate(double sampleRate);

    /**
     * Pull MIDI messages collected since the last call, aligned to the audio block.
     * Call this from the audio thread only.
     */
    void getMidiMessagesForBlock(juce::MidiBuffer &buffer, int numSamples);

    /// Inject a MIDI message into the collector from a virtual source
    /// (e.g. on-screen piano keyboard, musical typing). Thread-safe.
    void addMidiMessageToQueue(const juce::MidiMessage &message);

    /// Route a specific MIDI device to a track (by track ID).
    /// Empty trackId means "all tracks" (default behavior).
    void routeMidiDeviceToTrack(const juce::String &deviceIdentifier,
                                const juce::String &trackId);

    /// Get the track a MIDI device is routed to (empty = all)
    juce::String getMidiDeviceTrackRouting(const juce::String &deviceIdentifier) const;

    // ─── Audio Input ──────────────────────────────────────────────

    /// Get the number of available audio input channels from the current device
    static int getAvailableInputChannelCount(juce::AudioDeviceManager &dm);

    /// Check if audio input is enabled on the device manager
    static bool isAudioInputEnabled(juce::AudioDeviceManager &dm);

    // ─── Listener ─────────────────────────────────────────────────

    class Listener
    {
      public:
        virtual ~Listener() = default;
        virtual void midiDeviceListChanged() {}
        virtual void midiMessageReceived(const juce::MidiMessage &message,
                                         const juce::String &deviceIdentifier) {}
    };

    void addListener(Listener *listener);
    void removeListener(Listener *listener);

  private:
    // juce::MidiInputCallback
    void handleIncomingMidiMessage(juce::MidiInput *source,
                                   const juce::MidiMessage &message) override;

    juce::MidiMessageCollector midiCollector_;

    struct OpenMidiDevice
    {
        std::unique_ptr<juce::MidiInput> device;
        juce::String identifier;
        juce::String routedTrackId; // empty = all tracks
    };

    std::vector<OpenMidiDevice> openMidiDevices_;
    mutable std::mutex midiDeviceMutex_;

    std::vector<Listener *> listeners_;
    mutable std::mutex listenerMutex_;

    void notifyMidiDeviceListChanged();
    void notifyMidiMessageReceived(const juce::MidiMessage &msg,
                                   const juce::String &deviceId);
};

} // namespace ampl
