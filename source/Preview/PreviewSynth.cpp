#include "PreviewSynth.h"
#include <cmath>

namespace preview
{
    void PreviewSynth::prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;

        for (auto& v : voices)
        {
            v.adsr.setSampleRate (sampleRate);
            v.adsr.setParameters (adsrParams);
        }

        *toneFilter.coefficients = *juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, 4200.0f, 0.7f);
        toneFilter.prepare (juce::dsp::ProcessSpec { sampleRate, static_cast<juce::uint32> (maxBlockSize), 1 });

        scratchMono.setSize (1, juce::jmax (1, maxBlockSize), false, true, true);

        reset();
    }

    void PreviewSynth::reset() noexcept
    {
        for (auto& v : voices)
        {
            v.active = false;
            v.midiNote = -1;
            v.phase1 = v.phase2 = 0.0;
            v.adsr.reset();
        }

        toneFilter.reset();
    }

    PreviewSynth::Voice* PreviewSynth::findFreeVoice() noexcept
    {
        for (auto& v : voices)
            if (! v.active)
                return &v;

        // With 24 voices and triads/sevenths of at most 4 notes each, this
        // fallback essentially never triggers in normal chord-pad use --
        // it would take 6+ overlapping chords held at once. Stealing the
        // first voice is a simple, deterministic policy for that edge case.
        return &voices.front();
    }

    PreviewSynth::Voice* PreviewSynth::findVoiceForNote (int midiNote) noexcept
    {
        for (auto& v : voices)
            if (v.active && v.midiNote == midiNote)
                return &v;

        return nullptr;
    }

    void PreviewSynth::noteOn (int midiNote, float velocity01) noexcept
    {
        Voice* voice = findVoiceForNote (midiNote);
        if (voice == nullptr)
            voice = findFreeVoice();

        const double freq = 440.0 * std::pow (2.0, (midiNote - 69) / 12.0);

        voice->active = true;
        voice->midiNote = midiNote;
        voice->velocity = juce::jlimit (0.0f, 1.0f, velocity01);
        voice->phase1 = 0.0;
        voice->phase2 = 0.0;
        voice->phaseIncrement1 = juce::MathConstants<double>::twoPi * freq / sampleRate;
        // A touch sharp of a true octave, for a slow, gentle beating
        // rather than a static, lifeless double-sine tone.
        voice->phaseIncrement2 = juce::MathConstants<double>::twoPi * (freq * 2.005) / sampleRate;
        voice->adsr.setParameters (adsrParams);
        voice->adsr.noteOn();
    }

    void PreviewSynth::noteOff (int midiNote) noexcept
    {
        if (auto* voice = findVoiceForNote (midiNote))
            voice->adsr.noteOff();
    }

    void PreviewSynth::allNotesOff() noexcept
    {
        for (auto& v : voices)
            if (v.active)
                v.adsr.noteOff();
    }

    void PreviewSynth::renderBlock (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept
    {
        if (numSamples <= 0 || numSamples > scratchMono.getNumSamples())
            return;

        scratchMono.clear (0, 0, numSamples);
        auto* mono = scratchMono.getWritePointer (0);

        for (auto& v : voices)
        {
            if (! v.active)
                continue;

            for (int i = 0; i < numSamples; ++i)
            {
                const float env = v.adsr.getNextSample();

                const auto sample1 = static_cast<float> (std::sin (v.phase1));
                const auto sample2 = static_cast<float> (std::sin (v.phase2));
                const float blended = sample1 * 0.72f + sample2 * 0.28f;

                // 0.5 headroom per voice, ahead of the mix filter/gain, so
                // a full chord or two doesn't clip before the user's own
                // Preview Volume control even comes into it.
                mono[i] += blended * env * v.velocity * 0.5f;

                v.phase1 += v.phaseIncrement1;
                v.phase2 += v.phaseIncrement2;

                if (v.phase1 >= juce::MathConstants<double>::twoPi) v.phase1 -= juce::MathConstants<double>::twoPi;
                if (v.phase2 >= juce::MathConstants<double>::twoPi) v.phase2 -= juce::MathConstants<double>::twoPi;

                if (! v.adsr.isActive())
                {
                    v.active = false;
                    break;
                }
            }
        }

        juce::dsp::AudioBlock<float> monoBlock (scratchMono);
        auto trimmedBlock = monoBlock.getSubBlock (0, static_cast<size_t> (numSamples));
        juce::dsp::ProcessContextReplacing<float> context (trimmedBlock);
        toneFilter.process (context);

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.addFrom (channel, startSample, scratchMono, 0, 0, numSamples, gain);
    }
}
