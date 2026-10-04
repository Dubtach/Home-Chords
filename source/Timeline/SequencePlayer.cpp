#include "SequencePlayer.h"
#include "../MusicTheory/Chord.h"
#include "../MusicTheory/Voicing.h"
#include <cmath>

namespace timeline
{
    namespace
    {
        double pulseIntervalBeats (progression::RhythmPattern rhythm, double arpRateBeats) noexcept
        {
            switch (rhythm)
            {
                case progression::RhythmPattern::Whole:     return 4.0;
                case progression::RhythmPattern::Half:      return 2.0;
                case progression::RhythmPattern::Quarter:   return 1.0;
                case progression::RhythmPattern::Eighth:    return 0.5;
                case progression::RhythmPattern::Sixteenth: return 0.25;
                case progression::RhythmPattern::Arpeggio:  return juce::jmax (0.03125, arpRateBeats);
                default:                                    return -1.0;   // Sustained, Strum: no repeating pulse
            }
        }

        // Builds the actual notes for one event at its start: quality,
        // extension, and bass mode applied, root placed near the middle
        // register (each event is independent, unlike live play's row of
        // 7 slots, so there's no shared "anchor" to ascend from here).
        int voiceEventChord (const progression::ChordEvent& event, int rootOctaveOffset,
                              std::array<int, musictheory::maxChordTones>& outChordNotes,
                              int& outBassNote) noexcept
        {
            using namespace musictheory;

            const auto shape = buildOverriddenShape (event.rootPitchClass, event.quality, event.extension);
            const int rootMidi = anchorMidiNote (event.rootPitchClass, rootOctaveOffset + event.octaveOffset);

            int inversion = 0;
            if (event.bassMode == progression::BassMode::Third) inversion = 1;
            else if (event.bassMode == progression::BassMode::Fifth) inversion = 2;
            // Root and InversionBass both mean "don't override" -- the
            // sequencer doesn't auto-invert between events (see the class
            // comment in SequencePlayer.h), so both resolve to root
            // position here. Custom is handled separately below.

            const int chordCount = voiceShape (shape, rootMidi, inversion, outChordNotes);

            outBassNote = -1;
            if (event.bassMode == progression::BassMode::Custom && chordCount > 0 && chordCount < musictheory::maxChordTones)
            {
                const int lowestChordNote = outChordNotes[0];
                int bassNote = ((event.customBassPitchClass % 12) + 12) % 12;
                bassNote += 12 * ((lowestChordNote / 12) - 1);
                while (bassNote >= lowestChordNote)
                    bassNote -= 12;
                outBassNote = juce::jlimit (0, 127, bassNote);
            }

            return chordCount;
        }
    }

    void SequencePlayer::reset() noexcept
    {
        playing.store (false, std::memory_order_relaxed);
        positionBeats.store (0.0, std::memory_order_relaxed);

        for (auto& runtime : activeRuntimes)
        {
            runtime.inUse = false;
            runtime.currentNoteCount = 0;
        }
    }

    SequencePlayer::ActiveRuntime* SequencePlayer::findRuntimeFor (const juce::Uuid& id) noexcept
    {
        for (auto& runtime : activeRuntimes)
            if (runtime.inUse && runtime.eventId == id)
                return &runtime;

        return nullptr;
    }

    SequencePlayer::ActiveRuntime* SequencePlayer::allocateRuntimeFor (const juce::Uuid& id) noexcept
    {
        for (auto& runtime : activeRuntimes)
        {
            if (! runtime.inUse)
            {
                runtime = ActiveRuntime {};
                runtime.inUse = true;
                runtime.eventId = id;
                return &runtime;
            }
        }

        return nullptr;   // all slots busy -- extremely unlikely (8 simultaneous events); the new one is simply dropped
    }

    void SequencePlayer::freeRuntime (ActiveRuntime& runtime, juce::MidiBuffer& midiOut, midiengine::NoteLedger& ledger,
                                       int midiChannel, int sampleOffset) noexcept
    {
        for (int i = 0; i < runtime.currentNoteCount; ++i)
            ledger.noteOff (midiOut, midiChannel, runtime.currentNotes[static_cast<size_t> (i)], sampleOffset);

        runtime.inUse = false;
        runtime.currentNoteCount = 0;
    }

    void SequencePlayer::startEvent (const progression::ChordEvent& event, juce::MidiBuffer& midiOut, midiengine::NoteLedger& ledger,
                                      int midiChannel, int sampleOffset, int rootOctaveOffset,
                                      midiengine::ChordKeyboardEngine* voiceLeadingSink) noexcept
    {
        auto* runtime = allocateRuntimeFor (event.id);
        if (runtime == nullptr)
            return;

        const auto velocityByte = static_cast<juce::uint8> (
            musictheory::compensateVelocityForToneCount (event.velocity, event.extension == musictheory::Extension::None ? 3 : 4));

        if (event.rhythm == progression::RhythmPattern::Strum)
        {
            // A quick sequential onset (roughly 18ms between notes, capped
            // so a very short event can't strum past its own end), low to
            // high, then held for the rest of the event's duration.
            std::array<int, musictheory::maxChordTones> chordNotes {};
            int bassNote = -1;
            const int chordCount = voiceEventChord (event, rootOctaveOffset, chordNotes, bassNote);

            const int strumSampleStep = juce::jmax (1, static_cast<int> (0.018 * sampleRate));
            int index = 0;

            if (bassNote >= 0)
            {
                ledger.noteOn (midiOut, midiChannel, bassNote, velocityByte, sampleOffset);
                runtime->currentNotes[static_cast<size_t> (index++)] = bassNote;
            }

            for (int i = 0; i < chordCount; ++i)
            {
                const int offset = sampleOffset + i * strumSampleStep;
                ledger.noteOn (midiOut, midiChannel, chordNotes[static_cast<size_t> (i)], velocityByte, offset);
                runtime->currentNotes[static_cast<size_t> (index++)] = chordNotes[static_cast<size_t> (i)];
            }

            runtime->currentNoteCount = index;
            runtime->nextPulseBeat = -1.0;   // strum doesn't repeat
        }
        else if (event.rhythm == progression::RhythmPattern::Arpeggio)
        {
            runtime->currentNoteCount = 0;
            runtime->arpStepIndex = 0;
            runtime->nextPulseBeat = event.startBeats;   // fire the first step immediately, below
            advanceArp (event, *runtime, midiOut, ledger, midiChannel, sampleOffset, rootOctaveOffset);
        }
        else
        {
            std::array<int, musictheory::maxChordTones> chordNotes {};
            int bassNote = -1;
            const int chordCount = voiceEventChord (event, rootOctaveOffset, chordNotes, bassNote);

            int index = 0;
            if (bassNote >= 0)
            {
                ledger.noteOn (midiOut, midiChannel, bassNote, velocityByte, sampleOffset);
                runtime->currentNotes[static_cast<size_t> (index++)] = bassNote;
            }
            for (int i = 0; i < chordCount; ++i)
            {
                ledger.noteOn (midiOut, midiChannel, chordNotes[static_cast<size_t> (i)], velocityByte, sampleOffset);
                runtime->currentNotes[static_cast<size_t> (index++)] = chordNotes[static_cast<size_t> (i)];
            }
            runtime->currentNoteCount = index;

            const auto interval = pulseIntervalBeats (event.rhythm, event.arpRateBeats);
            runtime->nextPulseBeat = interval > 0.0 ? event.startBeats + interval : -1.0;
        }

        if (voiceLeadingSink != nullptr && runtime->currentNoteCount > 0)
            voiceLeadingSink->rememberExternallyPlayedNotes (runtime->currentNotes.data(), runtime->currentNoteCount);
    }

    void SequencePlayer::retriggerEvent (const progression::ChordEvent& event, ActiveRuntime& runtime, juce::MidiBuffer& midiOut,
                                          midiengine::NoteLedger& ledger, int midiChannel, int sampleOffset, int rootOctaveOffset) noexcept
    {
        for (int i = 0; i < runtime.currentNoteCount; ++i)
            ledger.noteOff (midiOut, midiChannel, runtime.currentNotes[static_cast<size_t> (i)], sampleOffset);

        std::array<int, musictheory::maxChordTones> chordNotes {};
        int bassNote = -1;
        const int chordCount = voiceEventChord (event, rootOctaveOffset, chordNotes, bassNote);
        const auto velocityByte = static_cast<juce::uint8> (
            musictheory::compensateVelocityForToneCount (event.velocity, event.extension == musictheory::Extension::None ? 3 : 4));

        int index = 0;
        if (bassNote >= 0)
        {
            ledger.noteOn (midiOut, midiChannel, bassNote, velocityByte, sampleOffset);
            runtime.currentNotes[static_cast<size_t> (index++)] = bassNote;
        }
        for (int i = 0; i < chordCount; ++i)
        {
            ledger.noteOn (midiOut, midiChannel, chordNotes[static_cast<size_t> (i)], velocityByte, sampleOffset);
            runtime.currentNotes[static_cast<size_t> (index++)] = chordNotes[static_cast<size_t> (i)];
        }
        runtime.currentNoteCount = index;

        const auto interval = pulseIntervalBeats (event.rhythm, event.arpRateBeats);
        runtime.nextPulseBeat += interval > 0.0 ? interval : 1.0e9;
    }

    void SequencePlayer::advanceArp (const progression::ChordEvent& event, ActiveRuntime& runtime, juce::MidiBuffer& midiOut,
                                      midiengine::NoteLedger& ledger, int midiChannel, int sampleOffset, int rootOctaveOffset) noexcept
    {
        for (int i = 0; i < runtime.currentNoteCount; ++i)
            ledger.noteOff (midiOut, midiChannel, runtime.currentNotes[static_cast<size_t> (i)], sampleOffset);
        runtime.currentNoteCount = 0;

        std::array<int, musictheory::maxChordTones> chordNotes {};
        int bassNote = -1;
        const int chordCount = voiceEventChord (event, rootOctaveOffset, chordNotes, bassNote);

        if (chordCount <= 0)
        {
            runtime.nextPulseBeat += juce::jmax (0.03125, event.arpRateBeats);
            return;
        }

        int step = 0;

        switch (event.arpMode)
        {
            case progression::ArpMode::Down:
                step = (chordCount - 1) - (runtime.arpStepIndex % chordCount);
                break;
            case progression::ArpMode::UpDown:
            {
                const int cycle = chordCount > 1 ? (chordCount * 2 - 2) : 1;
                const int position = runtime.arpStepIndex % cycle;
                step = position < chordCount ? position : cycle - position;
                break;
            }
            case progression::ArpMode::Random:
            {
                // A cheap, deterministic-enough pseudo-random sequence --
                // no <random> engine (state, allocation) needed for picking
                // one of a handful of chord tones.
                const juce::uint32 x = static_cast<juce::uint32> (runtime.arpStepIndex) * 2654435761u;
                step = static_cast<int> ((x >> 24) % static_cast<juce::uint32> (chordCount));
                break;
            }
            case progression::ArpMode::Up:
            case progression::ArpMode::Off:
            default:
                step = runtime.arpStepIndex % chordCount;
                break;
        }

        const auto velocityByte = static_cast<juce::uint8> (musictheory::compensateVelocityForToneCount (event.velocity, 1));
        ledger.noteOn (midiOut, midiChannel, chordNotes[static_cast<size_t> (step)], velocityByte, sampleOffset);
        runtime.currentNotes[0] = chordNotes[static_cast<size_t> (step)];
        runtime.currentNoteCount = 1;
        ++runtime.arpStepIndex;

        runtime.nextPulseBeat += juce::jmax (0.03125, event.arpRateBeats);
    }

    void SequencePlayer::advanceAndRender (const progression::ProgressionModel& model, juce::MidiBuffer& midiOut,
                                            midiengine::NoteLedger& ledger, int midiChannel, int numSamples, int rootOctaveOffset,
                                            midiengine::ChordKeyboardEngine* voiceLeadingSink) noexcept
    {
        if (! playing.load (std::memory_order_relaxed) || numSamples <= 0 || sampleRate <= 0.0)
            return;

        const double tempo = tempoBpm.load (std::memory_order_relaxed);
        const bool isLooping = looping.load (std::memory_order_relaxed);
        const double loopEnd = loopEndBeats.load (std::memory_order_relaxed);
        const double loopStart = loopStartBeats.load (std::memory_order_relaxed);

        const double beatsPerSample = (tempo / 60.0) / sampleRate;
        const double spanStart = positionBeats.load (std::memory_order_relaxed);
        double spanEnd = spanStart + static_cast<double> (numSamples) * beatsPerSample;

        bool willWrap = false;
        if (isLooping && spanEnd >= loopEnd)
        {
            spanEnd = loopEnd;
            willWrap = true;
        }

        const auto beatToSample = [&] (double beat) noexcept -> int
        {
            const double raw = (beat - spanStart) / beatsPerSample;
            return juce::jlimit (0, juce::jmax (0, numSamples - 1), static_cast<int> (std::llround (raw)));
        };

        // Start any event beginning in this span that isn't already active.
        for (int i = 0; i < model.size(); ++i)
        {
            const auto& event = model.getEvent (i);

            if (! event.enabled)
                continue;

            if (event.startBeats >= spanStart && event.startBeats < spanEnd && findRuntimeFor (event.id) == nullptr)
                startEvent (event, midiOut, ledger, midiChannel, beatToSample (event.startBeats), rootOctaveOffset, voiceLeadingSink);
        }

        // Advance (end, or pulse) every currently-active event.
        for (auto& runtime : activeRuntimes)
        {
            if (! runtime.inUse)
                continue;

            const progression::ChordEvent* eventPtr = nullptr;
            for (int i = 0; i < model.size(); ++i)
            {
                if (model.getEvent (i).id == runtime.eventId)
                {
                    eventPtr = &model.getEvent (i);
                    break;
                }
            }

            // The event was deleted (or disabled) while sounding -- treat
            // that as "ended right now" rather than risk a stuck note.
            if (eventPtr == nullptr || ! eventPtr->enabled)
            {
                freeRuntime (runtime, midiOut, ledger, midiChannel, 0);
                continue;
            }

            if (eventPtr->endBeats() <= spanEnd)
            {
                freeRuntime (runtime, midiOut, ledger, midiChannel, beatToSample (eventPtr->endBeats()));
                continue;
            }

            if (runtime.nextPulseBeat > 0.0 && runtime.nextPulseBeat < spanEnd)
            {
                if (eventPtr->rhythm == progression::RhythmPattern::Arpeggio)
                    advanceArp (*eventPtr, runtime, midiOut, ledger, midiChannel, 0, rootOctaveOffset);
                else
                    retriggerEvent (*eventPtr, runtime, midiOut, ledger, midiChannel, 0, rootOctaveOffset);
            }
        }

        if (willWrap)
        {
            for (auto& runtime : activeRuntimes)
                if (runtime.inUse)
                    freeRuntime (runtime, midiOut, ledger, midiChannel, juce::jmax (0, numSamples - 1));

            positionBeats.store (loopStart, std::memory_order_relaxed);
        }
        else
        {
            positionBeats.store (spanEnd, std::memory_order_relaxed);
        }
    }

    void SequencePlayer::releaseAllNotes (juce::MidiBuffer& midiOut, midiengine::NoteLedger& ledger, int midiChannel) noexcept
    {
        for (auto& runtime : activeRuntimes)
            if (runtime.inUse)
                freeRuntime (runtime, midiOut, ledger, midiChannel, 0);
    }
}
