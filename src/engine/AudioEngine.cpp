#include "engine/AudioEngine.h"

namespace neurato {

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine()
{
    shutdown();
}

void AudioEngine::initialise()
{
    formatManager_.registerBasicFormats();

    // Initialize audio device with default settings
    auto result = deviceManager_.initialiseWithDefaultDevices(0, 2); // 0 inputs, 2 outputs
    if (result.isNotEmpty())
    {
        DBG("Audio device init error: " + result);
    }

    deviceManager_.addAudioCallback(this);
}

void AudioEngine::shutdown()
{
    deviceManager_.removeAudioCallback(this);
    deviceManager_.closeAudioDevice();
}

// --- UI thread: send commands ---

void AudioEngine::sendPlay()
{
    UIToAudioMessage msg;
    msg.type = UIToAudioMessage::Type::Play;
    uiToAudioQueue_.tryPush(msg);
}

void AudioEngine::sendStop()
{
    UIToAudioMessage msg;
    msg.type = UIToAudioMessage::Type::Stop;
    uiToAudioQueue_.tryPush(msg);
}

void AudioEngine::sendTogglePlayStop()
{
    if (transport_.getState() == Transport::State::Playing)
        sendStop();
    else
        sendPlay();
}

void AudioEngine::sendSetBpm(double bpm)
{
    UIToAudioMessage msg;
    msg.type = UIToAudioMessage::Type::SetBpm;
    msg.doubleValue = bpm;
    uiToAudioQueue_.tryPush(msg);
}

void AudioEngine::sendSeek(SampleCount position)
{
    UIToAudioMessage msg;
    msg.type = UIToAudioMessage::Type::Seek;
    msg.intValue = position;
    uiToAudioQueue_.tryPush(msg);
}

void AudioEngine::sendSetMetronomeEnabled(bool enabled)
{
    UIToAudioMessage msg;
    msg.type = UIToAudioMessage::Type::SetMetronomeEnabled;
    msg.boolValue = enabled;
    uiToAudioQueue_.tryPush(msg);
}

void AudioEngine::sendSetMetronomeGain(float gain)
{
    UIToAudioMessage msg;
    msg.type = UIToAudioMessage::Type::SetMetronomeGain;
    msg.floatValue = gain;
    uiToAudioQueue_.tryPush(msg);
}

void AudioEngine::sendSetTrackGain(float gain)
{
    UIToAudioMessage msg;
    msg.type = UIToAudioMessage::Type::SetTrackGain;
    msg.floatValue = gain;
    uiToAudioQueue_.tryPush(msg);
}

void AudioEngine::sendSetTrackMute(bool mute)
{
    UIToAudioMessage msg;
    msg.type = UIToAudioMessage::Type::SetTrackMute;
    msg.boolValue = mute;
    uiToAudioQueue_.tryPush(msg);
}

std::optional<AudioToUIMessage> AudioEngine::pollAudioMessage()
{
    return audioToUIQueue_.tryPop();
}

void AudioEngine::publishSession(const Session& session)
{
    sessionRenderer_.publishSession(session);
    useSessionRenderer_ = true;
}

bool AudioEngine::loadTrackAudio(const juce::File& file)
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
    const float* const* /*inputChannelData*/,
    int /*numInputChannels*/,
    float* const* outputChannelData,
    int numOutputChannels,
    int numSamples,
    const juce::AudioIODeviceCallbackContext& /*context*/)
{
    // Process pending UI commands (RT-safe: lock-free queue reads)
    processUIMessages();

    // Clear output buffers
    for (int ch = 0; ch < numOutputChannels; ++ch)
    {
        if (outputChannelData[ch] != nullptr)
            std::fill(outputChannelData[ch], outputChannelData[ch] + numSamples, 0.0f);
    }

    float* leftOut  = (numOutputChannels > 0) ? outputChannelData[0] : nullptr;
    float* rightOut = (numOutputChannels > 1) ? outputChannelData[1] : nullptr;

    // Get transport position BEFORE advancing
    const SampleCount pos = transport_.getPositionInSamples();

    // Render track audio
    if (transport_.getState() == Transport::State::Playing)
    {
        if (useSessionRenderer_)
            sessionRenderer_.process(leftOut, rightOut, numSamples, pos);
        else
            track_.process(leftOut, rightOut, numSamples, pos);
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
                if (absL > peakL) peakL = absL;
            }
            if (rightOut != nullptr)
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    float absR = std::abs(rightOut[i]);
                    if (absR > peakR) peakR = absR;
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

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    const double sr = device->getCurrentSampleRate();
    transport_.setSampleRate(sr);
    metronome_.setSampleRate(sr);
    sessionRenderer_.setSampleRate(sr);
}

void AudioEngine::audioDeviceStopped()
{
    // Nothing to clean up — all state is in atomics
}

} // namespace neurato
