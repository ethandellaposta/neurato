#include "engine/OfflineRenderer.h"
#include <cmath>

namespace neurato {

bool OfflineRenderer::render(const Session& session,
                              const juce::File& outputFile,
                              const Settings& settings,
                              std::function<void(const Progress&)> progressCallback,
                              std::atomic<bool>* cancelFlag)
{
    // Determine render length
    SampleCount endSample = settings.endSample;
    if (endSample <= 0)
    {
        // Auto-detect: find the end of the last clip across all tracks
        for (const auto& track : session.getTracks())
        {
            for (const auto& clip : track.clips)
            {
                SampleCount clipEnd = clip.getTimelineEndSample();
                if (clipEnd > endSample)
                    endSample = clipEnd;
            }
        }

        // Add a small tail (1 second) for reverb/delay tails
        endSample += static_cast<SampleCount>(settings.sampleRate);
    }

    if (endSample <= settings.startSample)
    {
        if (progressCallback)
        {
            Progress p;
            p.error = "Nothing to render";
            p.complete = true;
            progressCallback(p);
        }
        return false;
    }

    // Create output file writer
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> outputStream(outputFile.createOutputStream());
    if (!outputStream)
    {
        if (progressCallback)
        {
            Progress p;
            p.error = "Could not create output file";
            p.complete = true;
            progressCallback(p);
        }
        return false;
    }

    JUCE_BEGIN_IGNORE_WARNINGS_MSVC(4996)
    #if __clang__
    _Pragma("clang diagnostic push")
    _Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"")
    #endif

    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(outputStream.get(),
                                   settings.sampleRate,
                                   static_cast<unsigned int>(settings.numChannels),
                                   settings.bitsPerSample,
                                   {}, 0));

    #if __clang__
    _Pragma("clang diagnostic pop")
    #endif
    JUCE_END_IGNORE_WARNINGS_MSVC

    if (!writer)
    {
        if (progressCallback)
        {
            Progress p;
            p.error = "Could not create WAV writer";
            p.complete = true;
            progressCallback(p);
        }
        return false;
    }

    // Writer takes ownership of the stream
    outputStream.release();

    // Render loop
    const SampleCount totalSamples = endSample - settings.startSample;
    juce::AudioBuffer<float> buffer(settings.numChannels, settings.blockSize);

    SampleCount position = settings.startSample;
    SampleCount samplesRendered = 0;

    while (position < endSample)
    {
        // Check for cancellation
        if (cancelFlag && cancelFlag->load(std::memory_order_acquire))
        {
            if (progressCallback)
            {
                Progress p;
                p.cancelled = true;
                p.complete = true;
                progressCallback(p);
            }
            return false;
        }

        int blockSize = static_cast<int>(std::min(
            static_cast<SampleCount>(settings.blockSize),
            endSample - position));

        buffer.clear();
        processBlock(session, buffer, position, blockSize);

        // Write to file
        writer->writeFromAudioSampleBuffer(buffer, 0, blockSize);

        position += blockSize;
        samplesRendered += blockSize;

        // Report progress
        if (progressCallback)
        {
            Progress p;
            p.fraction = static_cast<double>(samplesRendered) / static_cast<double>(totalSamples);
            progressCallback(p);
        }
    }

    // Flush writer
    writer.reset();

    if (progressCallback)
    {
        Progress p;
        p.fraction = 1.0;
        p.complete = true;
        progressCallback(p);
    }

    return true;
}

void OfflineRenderer::processBlock(const Session& session,
                                    juce::AudioBuffer<float>& buffer,
                                    SampleCount position,
                                    int numSamples)
{
    const int numChannels = buffer.getNumChannels();

    // Check for any soloed tracks
    bool hasSolo = false;
    for (const auto& t : session.getTracks())
        if (t.solo) { hasSolo = true; break; }

    for (const auto& track : session.getTracks())
    {
        if (track.muted)
            continue;
        if (hasSolo && !track.solo)
            continue;

        float trackGainLinear = juce::Decibels::decibelsToGain(track.gainDb);

        // Pre-compute pan coefficients
        float panAngle = (track.pan + 1.0f) * 0.5f;
        float panL = std::cos(panAngle * 1.5707963f);
        float panR = std::sin(panAngle * 1.5707963f);

        for (const auto& clip : track.clips)
        {
            if (!clip.asset || clip.asset->numChannels == 0)
                continue;

            // Check if this clip overlaps the current block
            SampleCount clipStart = clip.timelineStartSample;
            SampleCount clipEnd = clip.getTimelineEndSample();

            if (position >= clipEnd || position + numSamples <= clipStart)
                continue;

            float clipGainLinear = juce::Decibels::decibelsToGain(clip.gainDb);
            float totalGain = trackGainLinear * clipGainLinear;

            for (int i = 0; i < numSamples; ++i)
            {
                SampleCount samplePos = position + i;

                if (samplePos < clipStart || samplePos >= clipEnd)
                    continue;

                // Position within the source audio
                SampleCount sourcePos = clip.sourceStartSample + (samplePos - clipStart);
                if (sourcePos < 0 || sourcePos >= clip.asset->lengthInSamples)
                    continue;

                auto srcIdx = static_cast<size_t>(sourcePos);

                // Apply fade envelope
                float envelope = 1.0f;
                SampleCount posInClip = samplePos - clipStart;
                SampleCount clipLength = clip.sourceLengthSamples;

                if (clip.fadeInSamples > 0 && posInClip < clip.fadeInSamples)
                {
                    envelope *= static_cast<float>(posInClip) /
                                static_cast<float>(clip.fadeInSamples);
                }

                if (clip.fadeOutSamples > 0 && posInClip > clipLength - clip.fadeOutSamples)
                {
                    SampleCount fadePos = clipLength - posInClip;
                    envelope *= static_cast<float>(fadePos) /
                                static_cast<float>(clip.fadeOutSamples);
                }

                float gain = totalGain * envelope;

                // Mix into output buffer with pan
                if (numChannels >= 2)
                {
                    float sampleL = clip.asset->channels[0][srcIdx] * gain * panL;
                    int srcChR = (clip.asset->numChannels > 1) ? 1 : 0;
                    float sampleR = clip.asset->channels[static_cast<size_t>(srcChR)][srcIdx] * gain * panR;
                    buffer.addSample(0, i, sampleL);
                    buffer.addSample(1, i, sampleR);
                }
                else
                {
                    float sample = clip.asset->channels[0][srcIdx] * gain;
                    buffer.addSample(0, i, sample);
                }
            }
        }
    }

    // Apply master bus gain/pan
    float masterGain = juce::Decibels::decibelsToGain(session.getMasterGainDb());
    float masterPanAngle = (session.getMasterPan() + 1.0f) * 0.5f;
    float masterPanL = std::cos(masterPanAngle * 1.5707963f);
    float masterPanR = std::sin(masterPanAngle * 1.5707963f);

    if (masterGain != 1.0f || masterPanL != 1.0f || masterPanR != 1.0f)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            if (numChannels >= 2)
            {
                buffer.setSample(0, i, buffer.getSample(0, i) * masterGain * masterPanL);
                buffer.setSample(1, i, buffer.getSample(1, i) * masterGain * masterPanR);
            }
            else
            {
                buffer.setSample(0, i, buffer.getSample(0, i) * masterGain);
            }
        }
    }
}

} // namespace neurato
