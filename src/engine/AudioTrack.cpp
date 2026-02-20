#include "engine/AudioTrack.h"

namespace neurato {

AudioTrack::AudioTrack() = default;

AudioTrack::~AudioTrack()
{
    unloadFile();
}

bool AudioTrack::loadFile(const juce::File& file, juce::AudioFormatManager& formatManager)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr)
        return false;

    auto* newData = new AudioData();
    newData->lengthInSamples = static_cast<SampleCount>(reader->lengthInSamples);
    newData->sampleRate = reader->sampleRate;
    newData->numChannels = static_cast<int>(reader->numChannels);

    // Read entire file into memory for RT-safe playback
    newData->channels.resize(static_cast<size_t>(newData->numChannels));
    for (auto& ch : newData->channels)
        ch.resize(static_cast<size_t>(newData->lengthInSamples), 0.0f);

    // Create a temporary buffer for reading
    juce::AudioBuffer<float> tempBuffer(newData->numChannels,
                                         static_cast<int>(newData->lengthInSamples));
    reader->read(&tempBuffer, 0, static_cast<int>(newData->lengthInSamples), 0, true, true);

    // Copy to our internal format
    for (int ch = 0; ch < newData->numChannels; ++ch)
    {
        const float* src = tempBuffer.getReadPointer(ch);
        std::copy(src, src + newData->lengthInSamples, newData->channels[static_cast<size_t>(ch)].data());
    }

    // Atomic pointer swap — audio thread will pick up the new data
    AudioData* old = audioData_.exchange(newData, std::memory_order_acq_rel);

    // Delete old data (we're on the UI thread, allocation is fine)
    delete old;

    fileName_ = file.getFileName();
    return true;
}

void AudioTrack::unloadFile()
{
    AudioData* old = audioData_.exchange(nullptr, std::memory_order_acq_rel);
    delete old;
    fileName_ = "";
}

void AudioTrack::process(float* leftChannel, float* rightChannel, int numSamples,
                          SampleCount transportPosition) noexcept
{
    if (muted_.load(std::memory_order_acquire))
        return;

    const AudioData* data = audioData_.load(std::memory_order_acquire);
    if (data == nullptr || data->lengthInSamples == 0)
        return;

    const float g = gain_.load(std::memory_order_acquire);

    for (int i = 0; i < numSamples; ++i)
    {
        const SampleCount pos = transportPosition + i;

        // Only play within the clip's range
        if (pos < 0 || pos >= data->lengthInSamples)
            continue;

        const auto sampleIndex = static_cast<size_t>(pos);

        if (leftChannel != nullptr && data->numChannels > 0)
            leftChannel[i] += data->channels[0][sampleIndex] * g;

        if (rightChannel != nullptr)
        {
            if (data->numChannels > 1)
                rightChannel[i] += data->channels[1][sampleIndex] * g;
            else if (data->numChannels > 0)
                rightChannel[i] += data->channels[0][sampleIndex] * g; // mono → both channels
        }
    }
}

bool AudioTrack::hasAudio() const noexcept
{
    return audioData_.load(std::memory_order_acquire) != nullptr;
}

SampleCount AudioTrack::getLengthInSamples() const noexcept
{
    const AudioData* data = audioData_.load(std::memory_order_acquire);
    return data ? data->lengthInSamples : 0;
}

double AudioTrack::getSampleRate() const noexcept
{
    const AudioData* data = audioData_.load(std::memory_order_acquire);
    return data ? data->sampleRate : 0.0;
}

int AudioTrack::getNumChannels() const noexcept
{
    const AudioData* data = audioData_.load(std::memory_order_acquire);
    return data ? data->numChannels : 0;
}

void AudioTrack::setGain(float gain) noexcept
{
    gain_.store(gain, std::memory_order_release);
}

float AudioTrack::getGain() const noexcept
{
    return gain_.load(std::memory_order_acquire);
}

void AudioTrack::setMute(bool mute) noexcept
{
    muted_.store(mute, std::memory_order_release);
}

bool AudioTrack::isMuted() const noexcept
{
    return muted_.load(std::memory_order_acquire);
}

} // namespace neurato
