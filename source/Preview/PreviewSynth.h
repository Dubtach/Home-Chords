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

        void setGain (float linearGain) noexcept { gain = linearGain; }

    private:
        struct Voice
        {
            bool active = false;
            int midiNote = -1;
            double phase1 = 0.0, phase2 = 0.0;
            double phaseIncrement1 = 0.0, phaseIncrement2 = 0.0;
            float velocity = 0.0f;
            juce::ADSR adsr;
        };

        static constexpr int maxVoices = 24;

        double sampleRate = 44100.0;
        float gain = 0.7f;
        juce::ADSR::Parameters adsrParams { 0.008f, 0.25f, 0.55f, 0.6f };

        juce::dsp::IIR::Filter<float> toneFilter;
        juce::AudioBuffer<float> scratchMono;

        std::array<Voice, maxVoices> voices;

        Voice* findFreeVoice() noexcept;
        Voice* findVoiceForNote (int midiNote) noexcept;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PreviewSynth)
    };
}
