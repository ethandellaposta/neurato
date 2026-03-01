#include "engine/core/AudioEngine.hpp"

namespace ampl
{

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine()
{
    shutdown();
}

void AudioEngine::initialise()
{
    formatManager_.registerBasicFormats();

    // Defer audio device opening until the message loop is running.
    //
    // On macOS (especially 14+/15+), calling initialiseWithDefaultDevices() with
    // input channels during app startup causes CoreAudio to immediately fire
    // ObjectsPublishedAndDied notifications on its private dispatch queues.
    // Those arrive before our constructor returns, and JUCE's
    // AudioIODeviceCombiner::stop() ends up calling through an invalid callback
    // pointer (PC == 0x1, EXC_ARM_DA_ALIGN / SIGBUS).  Deferring to the first
    // message-loop tick gives CoreAudio time to settle and matches the same
    // pattern already used for plugin scanning.
    juce::MessageManager::callAsync(
        [this]
        {
            // Initialize audio device: 2 inputs (for real audio recording) + 2 outputs
            auto result = deviceManager_.initialiseWithDefaultDevices(2, 2);
            if (result.isNotEmpty())
            {
                DBG("Audio device init error: " + result);
                // Fallback to output-only if input not available
                result = deviceManager_.initialiseWithDefaultDevices(0, 2);
                if (result.isNotEmpty())
                    DBG("Audio device fallback error: " + result);
                else
                    audioInputEnabled_ = false;
            }
            else
            {
                audioInputEnabled_ = true;
            }

            deviceManager_.addAudioCallback(this);
        });
}

void AudioEngine::setAudioInputEnabled(bool enabled)
{
    if (audioInputEnabled_ == enabled)
        return;

    audioInputEnabled_ = enabled;

    // Re-initialise device with or without inputs
    deviceManager_.removeAudioCallback(this);
    int numInputs = enabled ? 2 : 0;
    auto result = deviceManager_.initialiseWithDefaultDevices(numInputs, 2);
    if (result.isNotEmpty())
    {
        DBG("setAudioInputEnabled error: " + result);
        audioInputEnabled_ = false;
    }
    deviceManager_.addAudioCallback(this);
}

void AudioEngine::shutdown()
{
    deviceManager_.removeAudioCallback(this);
    deviceManager_.closeAudioDevice();
}

// --- UI thread: send commands ---

void AudioEngine::sendMessage(UIToAudioMessage::Type type)
{
    UIToAudioMessage msg;
    msg.type = type;
    uiToAudioQueue_.tryPush(msg);
}

void AudioEngine::sendMessageWithValue(UIToAudioMessage::Type type, double value)
{
    UIToAudioMessage msg;
    msg.type = type;
    msg.doubleValue = value;
    uiToAudioQueue_.tryPush(msg);
}

void AudioEngine::sendMessageWithValue(UIToAudioMessage::Type type, int value)
{
    UIToAudioMessage msg;
    msg.type = type;
    msg.intValue = value;
    uiToAudioQueue_.tryPush(msg);
}

void AudioEngine::sendMessageWithValue(UIToAudioMessage::Type type, bool value)
{
    UIToAudioMessage msg;
    msg.type = type;
    msg.boolValue = value;
    uiToAudioQueue_.tryPush(msg);
}

void AudioEngine::sendMessageWithValue(UIToAudioMessage::Type type, float value)
{
    UIToAudioMessage msg;
    msg.type = type;
    msg.floatValue = value;
    uiToAudioQueue_.tryPush(msg);
}

void AudioEngine::sendPlay()
{
    sendMessage(UIToAudioMessage::Type::Play);
}

void AudioEngine::sendPause()
{
    sendMessage(UIToAudioMessage::Type::Pause);
}

void AudioEngine::sendStop()
{
    sendMessage(UIToAudioMessage::Type::Stop);
}

void AudioEngine::sendTogglePlayStop()
{
    if (transport_.getState() == Transport::State::Playing)
        sendPause();
    else
        sendPlay();
}

void AudioEngine::sendSetBpm(double bpm)
{
    sendMessageWithValue(UIToAudioMessage::Type::SetBpm, bpm);
}

void AudioEngine::sendSeek(SampleCount position)
{
    sendMessageWithValue(UIToAudioMessage::Type::Seek, static_cast<int>(position));
}

void AudioEngine::sendSetMetronomeEnabled(bool enabled)
{
    sendMessageWithValue(UIToAudioMessage::Type::SetMetronomeEnabled, enabled);
}

void AudioEngine::sendSetMetronomeGain(float gain)
{
    sendMessageWithValue(UIToAudioMessage::Type::SetMetronomeGain, gain);
}

void AudioEngine::sendSetTrackGain(float gain)
{
    sendMessageWithValue(UIToAudioMessage::Type::SetTrackGain, gain);
}

void AudioEngine::sendSetTrackMute(bool mute)
{
    sendMessageWithValue(UIToAudioMessage::Type::SetTrackMute, mute);
}

std::optional<AudioToUIMessage> AudioEngine::pollAudioMessage()
{
    return audioToUIQueue_.tryPop();
}

void AudioEngine::publishSession(const Session &session)
{
    sessionRenderer_.publishSession(session);
    useSessionRenderer_ = true;
}

bool AudioEngine::loadTrackAudio(const juce::File &file)
{
    return track_.loadFile(file, formatManager_);
}

// --- Audio thread ---

void AudioEngine::processUIMessages() noexcept
{
    while (auto msg = uiToAudioQueue_.tryPop())
    {
        switch (msg->type)
        {
        case UIToAudioMessage::Type::Play:
            transport_.play();
            break;
        case UIToAudioMessage::Type::Pause:
            transport_.pause();
            break;
        case UIToAudioMessage::Type::Stop:
            transport_.stop();
            break;
        case UIToAudioMessage::Type::SetBpm:
            transport_.setBpm(msg->doubleValue);
            break;
        case UIToAudioMessage::Type::Seek:
            transport_.setPositionInSamples(msg->intValue);
            break;
        case UIToAudioMessage::Type::SetMetronomeEnabled:
            metronome_.setEnabled(msg->boolValue);
            break;
        case UIToAudioMessage::Type::SetMetronomeGain:
            metronome_.setGain(msg->floatValue);
            break;
        case UIToAudioMessage::Type::SetTrackGain:
            track_.setGain(msg->floatValue);
            break;
        case UIToAudioMessage::Type::SetTrackMute:
            track_.setMute(msg->boolValue);
            break;
        }
    }
}

void AudioEngine::audioDeviceIOCallbackWithContext(
    const float *const *inputChannelData, int numInputChannels, float *const *outputChannelData,
    int numOutputChannels, int numSamples, const juce::AudioIODeviceCallbackContext & /*context*/)
{
    // Process pending UI commands (RT-safe: lock-free queue reads)
    processUIMessages();

    // Clear output buffers
    for (int ch = 0; ch < numOutputChannels; ++ch)
    {
        if (outputChannelData[ch] != nullptr)
            std::fill(outputChannelData[ch], outputChannelData[ch] + numSamples, 0.0f);
    }

    float *leftOut = (numOutputChannels > 0) ? outputChannelData[0] : nullptr;
    float *rightOut = (numOutputChannels > 1) ? outputChannelData[1] : nullptr;

    // Collect external MIDI input for this block
    juce::MidiBuffer externalMidi;
    externalIO_.getMidiMessagesForBlock(externalMidi, numSamples);

    // Collect audio input pointers
    const float *leftIn =
        (numInputChannels > 0 && inputChannelData) ? inputChannelData[0] : nullptr;
    const float *rightIn =
        (numInputChannels > 1 && inputChannelData) ? inputChannelData[1] : nullptr;

    // Get transport position BEFORE advancing
    const SampleCount pos = transport_.getPositionInSamples();

    // Render track audio
    if (transport_.getState() == Transport::State::Playing || !externalMidi.isEmpty())
    {
        if (useSessionRenderer_)
        {
            sessionRenderer_.processWithExternalIO(leftOut, rightOut, numSamples, pos, leftIn,
                                                   rightIn, externalMidi);
        }
        else if (transport_.getState() == Transport::State::Playing)
        {
            track_.process(leftOut, rightOut, numSamples, pos);
        }
    }

    // Render metronome (additive, on top of track audio)
    // Note: metronome checks transport state internally
    metronome_.process(leftOut, rightOut, numSamples, transport_);

    // Advance transport AFTER rendering
    transport_.advance(numSamples);

    // Send playhead position to UI (throttled)
    if (++positionUpdateCounter_ >= kPositionUpdateInterval)
    {
        positionUpdateCounter_ = 0;
        AudioToUIMessage posMsg;
        posMsg.type = AudioToUIMessage::Type::PlayheadPosition;
        posMsg.intValue = transport_.getPositionInSamples();
        posMsg.doubleValue = transport_.getPositionInSeconds();
        audioToUIQueue_.tryPush(posMsg);

        // Send peak levels
        if (leftOut != nullptr)
        {
            float peakL = 0.0f, peakR = 0.0f;
            for (int i = 0; i < numSamples; ++i)
            {
                float absL = std::abs(leftOut[i]);
                if (absL > peakL)
                    peakL = absL;
            }
            if (rightOut != nullptr)
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    float absR = std::abs(rightOut[i]);
                    if (absR > peakR)
                        peakR = absR;
                }
            }
            else
            {
                peakR = peakL;
            }

            AudioToUIMessage levelMsg;
            levelMsg.type = AudioToUIMessage::Type::PeakLevel;
            levelMsg.floatValue1 = peakL;
            levelMsg.floatValue2 = peakR;
            audioToUIQueue_.tryPush(levelMsg);
        }
    }
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice *device)
{
    const double sr = device->getCurrentSampleRate();
    const int blockSize = device->getCurrentBufferSizeSamples();
    transport_.setSampleRate(sr);
    metronome_.setSampleRate(sr);
    sessionRenderer_.setSampleRate(sr);
    sessionRenderer_.setBlockSize(blockSize);
    externalIO_.setSampleRate(sr);
}

void AudioEngine::audioDeviceStopped()
{
    // Nothing to clean up — all state is in atomics
}

} // namespace ampl
