#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <vector>
#include <unordered_map>

namespace ampl {

/**
 * A simple sampled piano synthesizer.
 * Uses a single piano sample for all notes, pitch-shifted as needed.
 * RT-safe for audio thread usage.
 */
class PianoSynth
{
public:
    enum EnvelopePhase { Attack, Decay, Sustain, Release, Idle };

    struct Voice
    {
        bool active = false;
        int noteNumber = 0;
        float velocity = 0.0f;
        float phase = 0.0f;
        float phaseInc = 0.0f;
        float envelope = 0.0f;
        EnvelopePhase envelopePhase = Idle;
        float sampleRate = 44100.0f;

        // Sample playback
        const float* sampleData = nullptr;
        int sampleLength = 0;
        int samplePosition = 0;
        float pitchRatio = 1.0f;

        // Envelope tracking
        float envelopeTime = 0.0f;
    };

public:
    PianoSynth();
    ~PianoSynth();

    // Initialize with sample rate
    void prepare(float sampleRate);

    // Note events (RT-safe)
    void noteOn(int noteNumber, float velocity);
    void noteOff(int noteNumber);

    // Audio processing (RT-safe)
    void render(float* leftOut, float* rightOut, int numSamples);

    // Load piano sample (UI thread only)
    bool loadPianoSample(const juce::File& sampleFile);
    void generateDefaultPianoSample();

private:
    static constexpr int MAX_VOICES = 32;
    static constexpr int DEFAULT_SAMPLE_RATE = 44100;
    static constexpr int PITCH_BEND_RANGE = 2; // semitones

    std::vector<Voice> voices_;
    std::unordered_map<int, int> activeNotes_; // note -> voice index

    // Piano sample data
    std::vector<float> pianoSample_;
    float sampleRate_ = DEFAULT_SAMPLE_RATE;
    bool sampleLoaded_ = false;

    // Voice management
    Voice* findFreeVoice();
    Voice* findVoiceForNote(int noteNumber);
    void startVoice(Voice* voice, int noteNumber, float velocity);
    void stopVoice(Voice* voice);

    // Sample processing
    float getNextSample(Voice& voice);
    float calculatePitchRatio(int noteNumber);

    // ADSR envelope
    float calculateEnvelope(Voice& voice);
};

} // namespace ampl
