#include "PreviewSynth.h"
#include <cmath>

namespace preview
{
    void PreviewSynth::prepare (double newSampleRate, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        gainSmoothed.reset (sampleRate, 0.02);

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
            v.held = false;
            v.midiNote = -1;
            v.phase1 = v.phase2 = 0.0;
            v.lastLevel = 0.0f;
            v.adsr.reset();
        }

        for (auto& g : ghosts)
            g.active = false;

        gainSmoothed.reset (sampleRate, 0.02);
        toneFilter.reset();
    }

    PreviewSynth::Voice* PreviewSynth::findVoiceForNote (int midiNote) noexcept
    {
        for (auto& v : voices)
            if (v.active && v.midiNote == midiNote)
                return &v;

        return nullptr;
    }

    void PreviewSynth::fadeOutAsGhost (const Voice& v) noexcept
    {
        if (v.lastLevel < 1.0e-5f)
            return;   // already silent, nothing to fade

        Ghost* target = nullptr;

        for (auto& g : ghosts)
        {
            if (! g.active) { target = &g; break; }
            if (target == nullptr || g.level < target->level)
                target = &g;
        }

        target->active = true;
        target->phase1 = v.phase1;
        target->phase2 = v.phase2;
        target->phaseIncrement1 = v.phaseIncrement1;
        target->phaseIncrement2 = v.phaseIncrement2;
        target->level = v.lastLevel;
        target->step = v.lastLevel / static_cast<float> (stealFadeSamples);
    }

    PreviewSynth::Voice* PreviewSynth::takeVoiceForNewNote() noexcept
    {
        for (auto& v : voices)
            if (! v.active)
                return &v;

        // Every voice is busy (many fast chords, each still ringing out).
        // Steal the quietest voice that is already releasing, else the oldest
        // held one, and let its sound fade out in a ghost rather than cut it.
        Voice* best = nullptr;

        for (auto& v : voices)
            if (! v.held && (best == nullptr || v.lastLevel < best->lastLevel))
                best = &v;

        if (best == nullptr)
            for (auto& v : voices)
                if (best == nullptr || v.age < best->age)
                    best = &v;

        fadeOutAsGhost (*best);
        return best;
    }

    void PreviewSynth::noteOn (int midiNote, float velocity01) noexcept
    {
        Voice* voice = findVoiceForNote (midiNote);
        const bool retrigger = (voice != nullptr);

        if (voice == nullptr)
        {
            voice = takeVoiceForNewNote();
            voice->phase1 = 0.0;
            voice->phase2 = 0.0;
            voice->velocitySmoothed = 0.0f;
            voice->lastLevel = 0.0f;
            voice->adsr.reset();
        }

        const double freq = 440.0 * std::pow (2.0, (midiNote - 69) / 12.0);

        voice->active = true;
        voice->held = true;
        voice->midiNote = midiNote;
        voice->age = ++ageCounter;
        voice->velocity = juce::jlimit (0.0f, 1.0f, velocity01);
        voice->phaseIncrement1 = juce::MathConstants<double>::twoPi * freq / sampleRate;
        // A touch sharp of a true octave, for a slow, gentle beating
        // rather than a static, lifeless double-sine tone.
        voice->phaseIncrement2 = juce::MathConstants<double>::twoPi * (freq * 2.005) / sampleRate;
        voice->adsr.setParameters (adsrParams);
        voice->adsr.noteOn();   // attacks from the current level, so a retrigger doesn't jump
        juce::ignoreUnused (retrigger);
    }

    void PreviewSynth::noteOff (int midiNote) noexcept
    {
        if (auto* voice = findVoiceForNote (midiNote))
        {
            voice->held = false;
            voice->adsr.noteOff();
        }
    }

    void PreviewSynth::allNotesOff() noexcept
    {
        for (auto& v : voices)
            if (v.active)
            {
                v.held = false;
                v.adsr.noteOff();
            }
    }

    void PreviewSynth::renderBlock (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept
    {
        // A host may legally deliver a block larger than the maximum it
        // announced in prepareToPlay. Render in scratch-sized chunks rather
        // than dropping the block (which would be silence).
        const int chunkSize = scratchMono.getNumSamples();

        if (chunkSize <= 0)
            return;

        for (int done = 0; done < numSamples; done += chunkSize)
            renderChunk (buffer, startSample + done, juce::jmin (chunkSize, numSamples - done));
    }

    void PreviewSynth::renderChunk (juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept
    {
        if (numSamples <= 0)
            return;

        scratchMono.clear (0, 0, numSamples);
        auto* mono = scratchMono.getWritePointer (0);
        constexpr double twoPi = juce::MathConstants<double>::twoPi;

        for (auto& v : voices)
        {
            if (! v.active)
                continue;

            for (int i = 0; i < numSamples; ++i)
            {
                const float env = v.adsr.getNextSample();

                // Glide the velocity so a retrigger at a different level doesn't step.
                v.velocitySmoothed += (v.velocity - v.velocitySmoothed) * 0.01f;

                const auto sample1 = static_cast<float> (std::sin (v.phase1));
                const auto sample2 = static_cast<float> (std::sin (v.phase2));
                const float blended = sample1 * 0.72f + sample2 * 0.28f;

                v.lastLevel = env * v.velocitySmoothed;

                // 0.4 headroom per voice, ahead of the soft clipper and the
                // user's Preview Volume, so chords stack without distorting.
                mono[i] += blended * v.lastLevel * 0.4f;

                v.phase1 += v.phaseIncrement1;
                v.phase2 += v.phaseIncrement2;

                if (v.phase1 >= twoPi) v.phase1 -= twoPi;
                if (v.phase2 >= twoPi) v.phase2 -= twoPi;

                if (! v.adsr.isActive())
                {
                    v.active = false;
                    v.held = false;
                    v.lastLevel = 0.0f;
                    break;
                }
            }
        }

        // Voices that were stolen: finish their sound with a short linear fade.
        for (auto& g : ghosts)
        {
            if (! g.active)
                continue;

            for (int i = 0; i < numSamples; ++i)
            {
                const auto sample1 = static_cast<float> (std::sin (g.phase1));
                const auto sample2 = static_cast<float> (std::sin (g.phase2));
                mono[i] += (sample1 * 0.72f + sample2 * 0.28f) * g.level * 0.4f;

                g.phase1 += g.phaseIncrement1;
                g.phase2 += g.phaseIncrement2;

                if (g.phase1 >= twoPi) g.phase1 -= twoPi;
                if (g.phase2 >= twoPi) g.phase2 -= twoPi;

                g.level -= g.step;

                if (g.level <= 0.0f)
                {
                    g.active = false;
                    break;
                }
            }
        }

        juce::dsp::AudioBlock<float> monoBlock (scratchMono);
        auto trimmedBlock = monoBlock.getSubBlock (0, static_cast<size_t> (numSamples));
        juce::dsp::ProcessContextReplacing<float> context (trimmedBlock);
        toneFilter.process (context);

        // Smoothed volume and a soft clip (tanh is ~linear for small signals).
        for (int i = 0; i < numSamples; ++i)
            mono[i] = std::tanh (mono[i]) * gainSmoothed.getNextValue();

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.addFrom (channel, startSample, scratchMono, 0, 0, numSamples, 1.0f);
    }
}
