#include "engine/SessionRenderer.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace neurato {

SessionRenderer::SessionRenderer()
{
    pianoSynth_ = std::make_unique<PianoSynth>();
    pianoSynth_->prepare(static_cast<float>(sampleRate_));

    pluginManager_ = std::make_unique<PluginManager>();
    pluginManager_->refreshPluginList();
}

SessionRenderer::~SessionRenderer()
{
    delete active_;
    delete pending_.load(std::memory_order_acquire);
    delete retired_;
}

void SessionRenderer::publishSession(const Session& session)
{
    auto* snapshot = new RenderSnapshot();

    bool hasSolo = false;
    for (const auto& track : session.getTracks())
    {
        if (track.solo)
        {
            hasSolo = true;
            break;
        }
    }
    snapshot->hasSoloedTrack = hasSolo;

    // Master bus
    snapshot->masterGainLinear = juce::Decibels::decibelsToGain(session.getMasterGainDb());
    float masterPanAngle = (session.getMasterPan() + 1.0f) * 0.5f;
    snapshot->masterPanL = std::cos(masterPanAngle * 1.5707963f);
    snapshot->masterPanR = std::sin(masterPanAngle * 1.5707963f);

    for (const auto& track : session.getTracks())
    {
        RenderTrack rt;
        rt.gainLinear = juce::Decibels::decibelsToGain(track.gainDb);
        rt.muted = track.muted;
        rt.solo = track.solo;

        // Pre-compute constant-power pan coefficients
        float panAngle = (track.pan + 1.0f) * 0.5f;
        rt.panL = std::cos(panAngle * 1.5707963f);
        rt.panR = std::sin(panAngle * 1.5707963f);

        rt.isMidi = (track.type == TrackType::Midi);

        if (track.isAudio())
        {
            for (const auto& clip : track.clips)
            {
                if (!clip.asset || clip.asset->numChannels == 0)
                    continue;

                RenderClip rc;
                rc.numChannels = clip.asset->numChannels;
                rc.assetLength = clip.asset->lengthInSamples;

                rc.ch0 = clip.asset->channels[0].data();
                rc.ch1 = (clip.asset->numChannels > 1)
                          ? clip.asset->channels[1].data()
                          : rc.ch0;

                rc.timelineStart = clip.timelineStartSample;
                rc.sourceStart = clip.sourceStartSample;
                rc.sourceLength = clip.sourceLengthSamples;
                rc.gainLinear = juce::Decibels::decibelsToGain(clip.gainDb);
                rc.fadeInSamples = clip.fadeInSamples;
                rc.fadeOutSamples = clip.fadeOutSamples;

                snapshot->assetRefs.push_back(clip.asset);
                rt.clips.push_back(rc);
            }
        }
        else
        {
            for (const auto& mclip : track.midiClips)
            {
                RenderMidiClip rmc;
                for (const auto& note : mclip.notes)
                {
                    RenderMidiNote rmn;
                    rmn.noteNumber = note.noteNumber;
                    rmn.velocity = note.velocity;
                    rmn.absoluteStart = mclip.timelineStartSample + note.startSample;
                    rmn.absoluteEnd = rmn.absoluteStart + note.lengthSamples;
                    rmc.notes.push_back(rmn);
                }
                rt.midiClips.push_back(std::move(rmc));
            }
        }

        snapshot->tracks.push_back(std::move(rt));
    }

    // Atomically publish — audio thread will pick it up
    auto* old = pending_.exchange(snapshot, std::memory_order_acq_rel);
    delete old;
}

void SessionRenderer::process(float* leftOut, float* rightOut, int numSamples,
                               SampleCount position) noexcept
{
    // Check for new snapshot from UI thread
    auto* newSnapshot = pending_.exchange(nullptr, std::memory_order_acq_rel);
    if (newSnapshot != nullptr)
    {
        delete retired_;
        retired_ = active_;
        active_ = newSnapshot;
    }

    if (active_ == nullptr)
        return;

    const auto& snapshot = *active_;

    for (const auto& track : snapshot.tracks)
    {
        if (track.muted)
            continue;
        if (snapshot.hasSoloedTrack && !track.solo)
            continue;

        if (track.isMidi)
        {
            // Use PianoSynth for MIDI tracks
            if (pianoSynth_)
            {
                // Send note events to PianoSynth
                for (const auto& mclip : track.midiClips)
                {
                    for (const auto& note : mclip.notes)
                    {
                        // Check if note starts or ends in this block
                        if (note.absoluteStart >= position && note.absoluteStart < position + numSamples)
                        {
                            // Note on
                            pianoSynth_->noteOn(note.noteNumber, note.velocity);
                        }
                        if (note.absoluteEnd >= position && note.absoluteEnd < position + numSamples)
                        {
                            // Note off
                            pianoSynth_->noteOff(note.noteNumber);
                        }
                    }
                }

                // Render piano synth
                pianoSynth_->render(leftOut, rightOut, numSamples);
            }
            continue;
        }

        for (const auto& clip : track.clips)
        {
            // Quick reject: does this clip overlap the current block?
            SampleCount clipEnd = clip.timelineStart + clip.sourceLength;
            if (position >= clipEnd || position + numSamples <= clip.timelineStart)
                continue;

            // Compute the sample range within this block that overlaps the clip
            int blockStart = 0;
            int blockEnd = numSamples;

            if (position < clip.timelineStart)
                blockStart = static_cast<int>(clip.timelineStart - position);
            if (position + numSamples > clipEnd)
                blockEnd = static_cast<int>(clipEnd - position);

            float trackGain = track.gainLinear;
            float panL = track.panL;
            float panR = track.panR;

            for (int i = blockStart; i < blockEnd; ++i)
            {
                SampleCount posInClip = (position + i) - clip.timelineStart;
                SampleCount sourcePos = clip.sourceStart + posInClip;

                if (sourcePos < 0 || sourcePos >= clip.assetLength)
                    continue;

                auto srcIdx = static_cast<size_t>(sourcePos);

                // Fade envelope
                float envelope = 1.0f;
                if (clip.fadeInSamples > 0 && posInClip < clip.fadeInSamples)
                {
                    envelope = static_cast<float>(posInClip) /
                               static_cast<float>(clip.fadeInSamples);
                }
                if (clip.fadeOutSamples > 0 &&
                    posInClip >= clip.sourceLength - clip.fadeOutSamples)
                {
                    SampleCount fadePos = clip.sourceLength - posInClip;
                    envelope *= static_cast<float>(fadePos) /
                                static_cast<float>(clip.fadeOutSamples);
                }

                float gain = trackGain * clip.gainLinear * envelope;

                float sampleL = clip.ch0[srcIdx] * gain * panL;
                float sampleR = clip.ch1[srcIdx] * gain * panR;

                if (leftOut != nullptr)
                    leftOut[i] += sampleL;
                if (rightOut != nullptr)
                    rightOut[i] += sampleR;
            }
        }
    }

    // Apply master bus gain
    if (snapshot.masterGainLinear != 1.0f ||
        snapshot.masterPanL != 1.0f || snapshot.masterPanR != 1.0f)
    {
        float mL = snapshot.masterGainLinear * snapshot.masterPanL;
        float mR = snapshot.masterGainLinear * snapshot.masterPanR;
        for (int i = 0; i < numSamples; ++i)
        {
            if (leftOut != nullptr)  leftOut[i]  *= mL;
            if (rightOut != nullptr) rightOut[i] *= mR;
        }
    }
}

void SessionRenderer::renderMidiTrack(const RenderTrack& track, float* leftOut, float* rightOut,
                                       int numSamples, SampleCount position) noexcept
{
    // For each MIDI clip in this track, synthesize active notes
    for (const auto& mclip : track.midiClips)
    {
        for (const auto& note : mclip.notes)
        {
            // Quick reject: does this note overlap the current block?
            SampleCount blockEnd = position + numSamples;
            if (position >= note.absoluteEnd || blockEnd <= note.absoluteStart)
                continue;

            // Compute block range where this note is active
            int startInBlock = 0;
            int endInBlock = numSamples;

            if (position < note.absoluteStart)
                startInBlock = static_cast<int>(note.absoluteStart - position);
            if (blockEnd > note.absoluteEnd)
                endInBlock = static_cast<int>(note.absoluteEnd - position);

            // Find or allocate a voice for this note
            SynthVoice* voice = nullptr;
            for (auto& v : voices_)
            {
                if (v.active && v.noteNumber == note.noteNumber)
                {
                    voice = &v;
                    break;
                }
            }
            if (!voice)
            {
                for (auto& v : voices_)
                {
                    if (!v.active)
                    {
                        voice = &v;
                        voice->noteNumber = note.noteNumber;
                        voice->velocity = note.velocity;
                        voice->active = true;
                        voice->phase = 0.0;
                        // MIDI note to frequency: A4 = 440 Hz, note 69
                        double freq = 440.0 * std::pow(2.0, (note.noteNumber - 69) / 12.0);
                        voice->phaseInc = freq / sampleRate_;
                        break;
                    }
                }
            }

            if (!voice)
                continue; // All voices used

            float gain = track.gainLinear * note.velocity * 0.25f; // Scale down to avoid clipping
            float panL = track.panL;
            float panR = track.panR;

            for (int i = startInBlock; i < endInBlock; ++i)
            {
                // Sine wave synthesis
                float sample = static_cast<float>(std::sin(voice->phase * 6.283185307179586));

                // Simple ADSR-like envelope: quick attack, sustain, quick release
                SampleCount posInNote = (position + i) - note.absoluteStart;
                SampleCount noteLen = note.absoluteEnd - note.absoluteStart;
                float env = 1.0f;

                // Attack: 5ms
                SampleCount attackSamples = static_cast<SampleCount>(sampleRate_ * 0.005);
                if (posInNote < attackSamples)
                    env = static_cast<float>(posInNote) / static_cast<float>(attackSamples);

                // Release: 10ms
                SampleCount releaseSamples = static_cast<SampleCount>(sampleRate_ * 0.01);
                if (posInNote > noteLen - releaseSamples)
                {
                    SampleCount releasePos = noteLen - posInNote;
                    env *= static_cast<float>(releasePos) / static_cast<float>(releaseSamples);
                }

                float out = sample * gain * env;

                if (leftOut)  leftOut[i]  += out * panL;
                if (rightOut) rightOut[i] += out * panR;

                voice->phase += voice->phaseInc;
                if (voice->phase >= 1.0)
                    voice->phase -= 1.0;
            }

            // If note ends in this block, deactivate voice
            if (blockEnd >= note.absoluteEnd)
                voice->active = false;
        }
    }
}

} // namespace neurato
