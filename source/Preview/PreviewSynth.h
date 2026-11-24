#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>

// =============================================================================
// A small, fixed-voice-count internal synth, so the plugin is audible on
// its own with nothing downstream listening to the MIDI it generates --
// see spec section 26 and the "press A, hear it" UX goal in section 50.
//
// Deliberately simple: one clean voice (two gently-detuned sine partials
// through an ADSR, with a shared low-pass on the mix for warmth), not the
// full Piano/EP/Organ/Synth/Pad/Pluck/Strings/Bass picker described for
// later. See STATUS.md for what's scoped out of this pass.
//
// Only ever driven from processBlock -- note events and rendering both
// happen on the audio thread in the same call, so there's no cross-thread
// hazard anywhere in this class and no atomics are needed.
// =============================================================================

namespace preview
{
    class PreviewSynth
    {
    public:
        PreviewSynth() = default;

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;

        void noteOn (int midiNote, float velocity01) noexcept;
        void noteOff (int midiNote) noexcept;
        void allNotesOff() noexcept;

        void renderBlock (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept;

        void setGain (float linearGain) noexcept { gainSmoothed.setTargetValue (linearGain); }

    private:
        // Click-free by construction:
        //   * a note that is already sounding (even one still releasing) is
        //     retriggered on the SAME voice with its phase left alone, so the
        //     waveform never jumps; only the envelope restarts, from where it is;
        //   * when every voice is busy, the one that is stolen is handed to a
        //     "ghost" that fades out over a couple of milliseconds while the
        //     new note takes the voice, so nothing is ever cut off;
        //   * velocity changes and the preview volume are smoothed;
        //   * the mix goes through a soft clipper instead of hard-clipping.
        struct Voice
        {
            bool active = false;
            bool held = false;                // false once note-off has arrived
            int midiNote = -1;
            juce::uint64 age = 0;
            double phase1 = 0.0, phase2 = 0.0;
            double phaseIncrement1 = 0.0, phaseIncrement2 = 0.0;
            float velocity = 0.0f;            // target
            float velocitySmoothed = 0.0f;
            float lastLevel = 0.0f;           // envelope * velocity at the last sample
            juce::ADSR adsr;
        };

        struct Ghost
        {
            bool active = false;
            double phase1 = 0.0, phase2 = 0.0;
            double phaseIncrement1 = 0.0, phaseIncrement2 = 0.0;
            float level = 0.0f;
            float step = 0.0f;
        };

        static constexpr int maxVoices = 32;
        static constexpr int maxGhosts = 16;
        static constexpr int stealFadeSamples = 128;

        double sampleRate = 44100.0;
        juce::SmoothedValue<float> gainSmoothed { 0.7f };
        juce::ADSR::Parameters adsrParams { 0.008f, 0.25f, 0.55f, 0.45f };
        juce::uint64 ageCounter = 0;

        juce::dsp::IIR::Filter<float> toneFilter;
        juce::AudioBuffer<float> scratchMono;

        std::array<Voice, maxVoices> voices;
        std::array<Ghost, maxGhosts> ghosts;

        void renderChunk (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept;
        Voice* findVoiceForNote (int midiNote) noexcept;
        Voice* takeVoiceForNewNote() noexcept;
        void fadeOutAsGhost (const Voice&) noexcept;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PreviewSynth)
    };
}
