#pragma once

#include "engine/core/AudioTrack.hpp"
#include "engine/core/Metronome.hpp"
#include "engine/render/SessionRenderer.hpp"
#include "engine/core/Transport.hpp"
#include "engine/io/ExternalIOManager.hpp"
#include "util/LockFreeQueue.hpp"
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>

namespace ampl
{

// Messages sent from UI thread → Audio thread
struct UIToAudioMessage
{
    enum class Type
    {
        Play,
        Pause,
        Stop,
        SetBpm,
        Seek,
        SetMetronomeEnabled,
        SetMetronomeGain,
        SetTrackGain,
        SetTrackMute
    };

    Type type;
    double doubleValue{0.0};
    float floatValue{0.0f};
    int64_t intValue{0};
    bool boolValue{false};
};

// Messages sent from Audio thread → UI thread
struct AudioToUIMessage
{
    enum class Type
    {
        PlayheadPosition,
        PeakLevel,
        TransportStateChanged
    };

    Type type;
    double doubleValue{0.0};
    float floatValue1{0.0f};
    float floatValue2{0.0f};
    int64_t intValue{0};
    bool boolValue{false};
};

// The core audio engine. Owns the transport, metronome, and tracks.
// Implements juce::AudioIODeviceCallback for the real-time audio thread.
class AudioEngine : public juce::AudioIODeviceCallback
{
  public:
    AudioEngine();
    ~AudioEngine() override;

    // Setup
    juce::AudioDeviceManager &getDeviceManager()
    {
        return deviceManager_;
    }
    juce::AudioFormatManager &getFormatManager()
    {
        return formatManager_;
    }

    void initialise();
    void shutdown();

    // UI thread: send commands to audio thread
  private:
    // Helper functions for message sending
    void sendMessage(UIToAudioMessage::Type type);
    void sendMessageWithValue(UIToAudioMessage::Type type, double value);
    void sendMessageWithValue(UIToAudioMessage::Type type, int value);
    void sendMessageWithValue(UIToAudioMessage::Type type, bool value);
    void sendMessageWithValue(UIToAudioMessage::Type type, float value);

  public:
    void sendPlay();
    void sendPause();
    void sendStop();
    void sendTogglePlayStop();
    void sendSetBpm(double bpm);
    void sendSeek(SampleCount position);
    void sendSetMetronomeEnabled(bool enabled);
    void sendSetMetronomeGain(float gain);
    void sendSetTrackGain(float gain);
    void sendSetTrackMute(bool mute);

    // UI thread: poll messages from audio thread
    std::optional<AudioToUIMessage> pollAudioMessage();

    // Session-driven rendering (UI thread publishes snapshots)
    void publishSession(const Session &session);
    SessionRenderer &getSessionRenderer()
    {
        return sessionRenderer_;
    }
    PluginManager *getPluginManager()
    {
        return sessionRenderer_.getPluginManager();
    }

    // External I/O (MIDI + Audio input)
    ExternalIOManager &getExternalIOManager()
    {
        return externalIO_;
    }
    const ExternalIOManager &getExternalIOManager() const
    {
        return externalIO_;
    }

    // Audio input enable/disable (requires device restart)
    void setAudioInputEnabled(bool enabled);
    bool isAudioInputEnabled() const { return audioInputEnabled_; }

    // Legacy single-track (kept for backward compat, will be removed)
    bool loadTrackAudio(const juce::File &file);
    AudioTrack &getTrack()
    {
        return track_;
    }
    const AudioTrack &getTrack() const
    {
        return track_;
    }

    // Transport state (thread-safe reads)
    Transport &getTransport()
    {
        return transport_;
    }
    const Transport &getTransport() const
    {
        return transport_;
    }

    // juce::AudioIODeviceCallback
    void
    audioDeviceIOCallbackWithContext(const float *const *inputChannelData, int numInputChannels,
                                     float *const *outputChannelData, int numOutputChannels,
                                     int numSamples,
                                     const juce::AudioIODeviceCallbackContext &context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice *device) override;
    void audioDeviceStopped() override;

  private:
    void processUIMessages() noexcept;

    juce::AudioDeviceManager deviceManager_;
    juce::AudioFormatManager formatManager_;

    Transport transport_;
    Metronome metronome_;
    SessionRenderer sessionRenderer_;
    ExternalIOManager externalIO_;
    AudioTrack track_; // Legacy — kept for backward compat
    bool useSessionRenderer_{false};
    bool audioInputEnabled_{false};

    LockFreeQueue<UIToAudioMessage, 256> uiToAudioQueue_;
    LockFreeQueue<AudioToUIMessage, 256> audioToUIQueue_;

    // For throttling position updates from audio thread
    int positionUpdateCounter_{0};
    static constexpr int kPositionUpdateInterval = 8; // every N callbacks
};

} // namespace ampl
