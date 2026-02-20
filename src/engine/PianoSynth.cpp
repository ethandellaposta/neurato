#include "engine/PianoSynth.h"
#include <juce_core/juce_core.h>
#include <cmath>

namespace neurato {

PianoSynth::PianoSynth()
{
    voices_.resize(MAX_VOICES);
    generateDefaultPianoSample();
}

PianoSynth::~PianoSynth()
{
}

void PianoSynth::prepare(float sampleRate)
{
    sampleRate_ = sampleRate;
    
    // Regenerate default sample if needed
    if (!sampleLoaded_)
    {
        generateDefaultPianoSample();
    }
}

void PianoSynth::noteOn(int noteNumber, float velocity)
{
    // Find existing voice for this note (retrigger)
    auto it = activeNotes_.find(noteNumber);
    if (it != activeNotes_.end())
    {
        // Retrigger existing voice
        Voice* voice = &voices_[it->second];
        startVoice(voice, noteNumber, velocity);
        return;
    }
    
    // Find free voice
    Voice* voice = findFreeVoice();
    if (!voice)
    {
        // Steal oldest voice (simple LRU)
        voice = &voices_[0];
        for (auto& [note, voiceIdx] : activeNotes_)
        {
            if (voices_[voiceIdx].envelopePhase < voice->envelopePhase)
            {
                voice = &voices_[voiceIdx];
                activeNotes_.erase(note);
                break;
            }
        }
    }
    
    startVoice(voice, noteNumber, velocity);
}

void PianoSynth::noteOff(int noteNumber)
{
    auto it = activeNotes_.find(noteNumber);
    if (it != activeNotes_.end())
    {
        Voice* voice = &voices_[it->second];
        stopVoice(voice);
        activeNotes_.erase(it);
    }
}

void PianoSynth::render(float* leftOut, float* rightOut, int numSamples)
{
    if (!sampleLoaded_ || pianoSample_.empty())
        return;
    
    // Clear output
    if (leftOut) std::memset(leftOut, 0, numSamples * sizeof(float));
    if (rightOut) std::memset(rightOut, 0, numSamples * sizeof(float));
    
    // Render active voices
    for (auto& voice : voices_)
    {
        if (!voice.active)
            continue;
        
        for (int i = 0; i < numSamples; ++i)
        {
            float sample = getNextSample(voice);
            float envelope = calculateEnvelope(voice);
            float output = sample * voice.velocity * envelope * 0.5f; // Scale down
            
            if (leftOut) leftOut[i] += output;
            if (rightOut) rightOut[i] += output;
            
            // Check if voice has finished
            if (voice.envelopePhase == Idle)
            {
                voice.active = false;
                break;
            }
        }
    }
}

bool PianoSynth::loadPianoSample(const juce::File& sampleFile)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    
    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(sampleFile));
    
    if (!reader)
    {
        juce::Logger::writeToLog("Failed to load piano sample: " + sampleFile.getFullPathName());
        return false;
    }
    
    // Read sample data (convert to mono if needed)
    pianoSample_.resize(reader->lengthInSamples);
    
    if (reader->numChannels == 1)
    {
        reader->read(&pianoSample_[0], reader->lengthInSamples, 0, true, true);
    }
    else
    {
        // Mix down to mono
        std::vector<float> tempBuffer(reader->lengthInSamples * reader->numChannels);
        reader->read(&tempBuffer[0], reader->lengthInSamples, 0, true, true);
        
        for (int64_t i = 0; i < reader->lengthInSamples; ++i)
        {
            float sum = 0.0f;
            for (int ch = 0; ch < reader->numChannels; ++ch)
            {
                sum += tempBuffer[i * reader->numChannels + ch];
            }
            pianoSample_[i] = sum / reader->numChannels;
        }
    }
    
    sampleLoaded_ = true;
    juce::Logger::writeToLog("Loaded piano sample: " + sampleFile.getFullPathName());
    return true;
}

void PianoSynth::generateDefaultPianoSample()
{
    // Generate a simple piano-like sample using harmonics
    constexpr int sampleLength = 44100; // 1 second at 44.1kHz
    constexpr float fundamentalFreq = 220.0f; // A3
    
    pianoSample_.resize(sampleLength);
    
    for (int i = 0; i < sampleLength; ++i)
    {
        float t = static_cast<float>(i) / sampleRate_;
        float envelope = std::exp(-t * 2.0f); // Quick decay
        
        // Fundamental + harmonics for piano-like tone
        float sample = 0.0f;
        
        // Fundamental (A3)
        sample += std::sin(2.0f * juce::MathConstants<float>::pi * fundamentalFreq * t) * 0.6f;
        
        // 2nd harmonic (octave)
        sample += std::sin(2.0f * juce::MathConstants<float>::pi * fundamentalFreq * 2.0f * t) * 0.3f;
        
        // 3rd harmonic (fifth above octave)
        sample += std::sin(2.0f * juce::MathConstants<float>::pi * fundamentalFreq * 3.0f * t) * 0.2f;
        
        // 4th harmonic (two octaves)
        sample += std::sin(2.0f * juce::MathConstants<float>::pi * fundamentalFreq * 4.0f * t) * 0.1f;
        
        // Add some inharmonicity for piano character
        sample += std::sin(2.0f * juce::MathConstants<float>::pi * fundamentalFreq * 4.2f * t) * 0.05f;
        
        pianoSample_[i] = sample * envelope;
    }
    
    sampleLoaded_ = true;
    juce::Logger::writeToLog("Generated default piano sample");
}

PianoSynth::Voice* PianoSynth::findFreeVoice()
{
    for (auto& voice : voices_)
    {
        if (!voice.active)
            return &voice;
    }
    return nullptr;
}

PianoSynth::Voice* PianoSynth::findVoiceForNote(int noteNumber)
{
    auto it = activeNotes_.find(noteNumber);
    return (it != activeNotes_.end()) ? &voices_[it->second] : nullptr;
}

void PianoSynth::startVoice(Voice* voice, int noteNumber, float velocity)
{
    voice->active = true;
    voice->noteNumber = noteNumber;
    voice->velocity = velocity;
    voice->phase = 0.0f;
    voice->samplePosition = 0;
    voice->envelopePhase = Attack;
    voice->sampleRate = sampleRate_;
    voice->sampleData = pianoSample_.data();
    voice->sampleLength = static_cast<int>(pianoSample_.size());
    voice->pitchRatio = calculatePitchRatio(noteNumber);
    
    // Calculate phase increment for pitch-shifted playback
    voice->phaseInc = voice->pitchRatio;
    
    // Map note to voice
    activeNotes_[noteNumber] = static_cast<int>(voice - &voices_[0]);
}

void PianoSynth::stopVoice(Voice* voice)
{
    voice->envelopePhase = Release;
}

float PianoSynth::getNextSample(Voice& voice)
{
    if (!voice.sampleData || voice.samplePosition >= voice.sampleLength)
        return 0.0f;
    
    // Linear interpolation for pitch shifting
    float pos = voice.samplePosition;
    int index = static_cast<int>(pos);
    float frac = pos - index;
    
    float sample1 = voice.sampleData[index];
    float sample2 = (index + 1 < voice.sampleLength) ? voice.sampleData[index + 1] : 0.0f;
    
    float sample = sample1 + (sample2 - sample1) * frac;
    
    // Advance position with pitch ratio
    voice.samplePosition += voice.phaseInc;
    
    // Loop if needed (for sustained notes)
    if (voice.samplePosition >= voice.sampleLength)
    {
        voice.samplePosition = voice.samplePosition - voice.sampleLength;
    }
    
    return sample;
}

float PianoSynth::calculatePitchRatio(int noteNumber)
{
    // A4 = note 69, 440Hz
    // Calculate semitone difference from A4
    float semitonesFromA4 = static_cast<float>(noteNumber - 69);
    
    // Calculate frequency ratio: 2^(semitones/12)
    return std::pow(2.0f, semitonesFromA4 / 12.0f);
}

float PianoSynth::calculateEnvelope(Voice& voice)
{
    const float sampleRate = voice.sampleRate;
    const float attackTime = 0.01f;  // 10ms
    const float decayTime = 0.1f;     // 100ms
    const float sustainLevel = 0.7f;
    const float releaseTime = 0.3f;   // 300ms
    
    switch (voice.envelopePhase)
    {
        case Attack:
        {
            float attackSamples = attackTime * sampleRate;
            voice.envelope = voice.envelopePhase / attackSamples;
            if (voice.envelope >= 1.0f)
            {
                voice.envelope = 1.0f;
                voice.envelopePhase = Decay;
                voice.envelopePhase = 0.0f;
            }
            break;
        }
        
        case Decay:
        {
            float decaySamples = decayTime * sampleRate;
            voice.envelope = 1.0f - (1.0f - sustainLevel) * (voice.envelopePhase / decaySamples);
            voice.envelopePhase += 1.0f;
            if (voice.envelopePhase >= decaySamples)
            {
                voice.envelope = sustainLevel;
                voice.envelopePhase = Sustain;
                voice.envelopePhase = 0.0f;
            }
            break;
        }
        
        case Sustain:
            voice.envelope = sustainLevel;
            break;
            
        case Release:
        {
            float releaseSamples = releaseTime * sampleRate;
            float startLevel = voice.envelope;
            voice.envelope = startLevel * (1.0f - voice.envelopePhase / releaseSamples);
            voice.envelopePhase += 1.0f;
            if (voice.envelopePhase >= releaseSamples)
            {
                voice.envelope = 0.0f;
                voice.envelopePhase = Idle;
            }
            break;
        }
        
        case Idle:
        default:
            voice.envelope = 0.0f;
            break;
    }
    
    return voice.envelope;
}

} // namespace neurato
