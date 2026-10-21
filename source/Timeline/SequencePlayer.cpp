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

        for (auto& entry : pending)
            entry.used = false;

        rngState = 0x9E3779B9u;
        haveCentroid = false;
    }

    float SequencePlayer::nextRandom01() noexcept
    {
        rngState ^= rngState << 13;
        rngState ^= rngState >> 17;
        rngState ^= rngState << 5;
        return static_cast<float> (rngState >> 8) * (1.0f / 16777216.0f);
    }

    // Builds the actual notes for one event at its start: quality,
    // extension, bass mode and the voicing style applied, root placed near
    // the middle register (each event is independent, unlike live play's row
    // of 7 slots, so there's no shared "anchor" to ascend from here).
    // The inversion an event with Bass = "Inversion" gets: the one nearest the
    // previous event's chord when auto-inversion is on (and there is a
    // previous chord), otherwise root position.
    int SequencePlayer::chooseEventInversion (const progression::ChordEvent& event, int rootOctaveOffset) const noexcept
    {
        using namespace musictheory;

        if (! autoInversion.load (std::memory_order_relaxed) || ! haveCentroid
            || event.bassMode != progression::BassMode::InversionBass)
            return 0;

        const auto shape = buildOverriddenShape (event.rootPitchClass, event.quality, event.extension);
        const int rootMidi = anchorMidiNote (event.rootPitchClass, rootOctaveOffset + event.octaveOffset);
        return chooseInversionNearest (shape, rootMidi, lastCentroid);
    }

    SequencePlayer::Voiced SequencePlayer::voiceEvent (const progression::ChordEvent& event, int rootOctaveOffset,
                                                        int autoInversionIndex) const noexcept
    {
        using namespace musictheory;

        Voiced voiced;

        const auto shape = buildOverriddenShape (event.rootPitchClass, event.quality, event.extension);
        const int rootMidi = anchorMidiNote (event.rootPitchClass, rootOctaveOffset + event.octaveOffset);

        int inversion = 0;
        if (event.bassMode == progression::BassMode::Third) inversion = 1;
        else if (event.bassMode == progression::BassMode::Fifth) inversion = 2;
        else if (event.bassMode == progression::BassMode::InversionBass) inversion = autoInversionIndex;
        // Root forces root position; Custom adds a slash bass (below).

        voiced.closeCount = voiceShape (shape, rootMidi, inversion, voiced.close);

        std::array<int, maxVoicedNotes> styled {};
        const int styledCount = applyVoicingStyle (voiced.close, voiced.closeCount, rootMidi, getVoicingStyle(), styled);

        int out = 0;

        if (event.bassMode == progression::BassMode::Custom && styledCount > 0)
        {
            const int lowest = styled[0];
            int bassNote = ((event.customBassPitchClass % 12) + 12) % 12;
            bassNote += 12 * ((lowest / 12) - 1);

            while (bassNote >= lowest)
                bassNote -= 12;

            if (bassNote >= 0)
                voiced.notes[static_cast<size_t> (out++)] = bassNote;
        }

        for (int i = 0; i < styledCount; ++i)
            voiced.notes[static_cast<size_t> (out++)] = styled[static_cast<size_t> (i)];

        voiced.count = out;
        return voiced;
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

    // ---- note emission and the scheduler ------------------------------------

    void SequencePlayer::emitNoteOn (ActiveRuntime& runtime, Sink& sink, int note, juce::uint8 velocity, int sample) noexcept
    {
        sink.ledger.noteOn (sink.midi, sink.channel, note, velocity, juce::jlimit (0, juce::jmax (0, sink.blockSamples - 1), sample));

        if (runtime.currentNoteCount < static_cast<int> (runtime.currentNotes.size()))
            runtime.currentNotes[static_cast<size_t> (runtime.currentNoteCount++)] = note;
    }

    void SequencePlayer::scheduleNoteOn (ActiveRuntime& runtime, Sink& sink, int note, int velocity, int sample,
                                          bool jitterTiming) noexcept
    {
        int vel = velocity;
        const float velocityAmount = humanizeVelocity.load (std::memory_order_relaxed);

        if (velocityAmount > 0.0f)
            vel = juce::jlimit (1, 127, static_cast<int> (std::lround (static_cast<float> (velocity)
                                                                         * (1.0f + (nextRandom01() * 2.0f - 1.0f) * velocityAmount))));

        int when = sample;
        const float timingMs = humanizeTimingMs.load (std::memory_order_relaxed);

        if (jitterTiming && timingMs > 0.0f)
            when += static_cast<int> (nextRandom01() * timingMs * 0.001 * sampleRate);

        if (when < sink.blockSamples)
        {
            emitNoteOn (runtime, sink, note, static_cast<juce::uint8> (vel), juce::jmax (0, when));
            return;
        }

        for (auto& entry : pending)
        {
            if (! entry.used)
            {
                entry = { true, false, indexOf (runtime), note, static_cast<juce::uint8> (vel), 0, when };
                return;
            }
        }

        // Queue full (never in normal use): sound it at the end of this
        // block rather than lose it -- losing a Note On is safe, but a
        // late one is better than none.
        emitNoteOn (runtime, sink, note, static_cast<juce::uint8> (vel), sink.blockSamples - 1);
    }

    void SequencePlayer::scheduleArpGateOff (ActiveRuntime& runtime, int note, int delaySamples) noexcept
    {
        for (auto& entry : pending)
        {
            if (! entry.used)
            {
                entry = { true, true, indexOf (runtime), note, 0, runtime.arpStepIndex, delaySamples };
                return;
            }
        }
        // Queue full: the note simply keeps sounding until the next arp
        // step, which is what an ungated arp does anyway.
    }

    void SequencePlayer::runPending (const PendingNote& entry, Sink& sink, int sample) noexcept
    {
        auto& runtime = activeRuntimes[static_cast<size_t> (entry.runtimeIndex)];

        if (! runtime.inUse)
            return;

        if (! entry.isArpGateOff)
        {
            emitNoteOn (runtime, sink, entry.note, entry.velocity, sample);
            return;
        }

        // Only if the step this gate belongs to is still the one sounding.
        if (runtime.arpStepIndex == entry.arpSerial && runtime.currentNoteCount == 1 && runtime.currentNotes[0] == entry.note)
        {
            sink.ledger.noteOff (sink.midi, sink.channel, entry.note, juce::jlimit (0, juce::jmax (0, sink.blockSamples - 1), sample));
            runtime.currentNoteCount = 0;
        }
    }

    // Emits whatever this runtime had queued that falls before `beforeSample`
    // (it really did sound, however briefly) and discards the rest -- they
    // belong to something that is ending or being replaced.
    void SequencePlayer::flushPendingFor (ActiveRuntime& runtime, Sink& sink, int beforeSample) noexcept
    {
        const int index = indexOf (runtime);

        for (auto& entry : pending)
        {
            if (! entry.used || entry.runtimeIndex != index)
                continue;

            if (entry.delay < beforeSample)
                runPending (entry, sink, juce::jmax (0, entry.delay));

            entry.used = false;
        }
    }

    // End of block: everything now due is emitted at its sample; the rest
    // moves one block closer.
    void SequencePlayer::processPending (Sink& sink) noexcept
    {
        for (auto& entry : pending)
        {
            if (! entry.used)
                continue;

            if (entry.delay < sink.blockSamples)
            {
                runPending (entry, sink, juce::jmax (0, entry.delay));
                entry.used = false;
            }
            else
            {
                entry.delay -= sink.blockSamples;
            }
        }
    }

    void SequencePlayer::releaseCurrentNotes (ActiveRuntime& runtime, Sink& sink, int sampleOffset) noexcept
    {
        const int at = juce::jlimit (0, juce::jmax (0, sink.blockSamples - 1), sampleOffset);

        for (int i = 0; i < runtime.currentNoteCount; ++i)
            sink.ledger.noteOff (sink.midi, sink.channel, runtime.currentNotes[static_cast<size_t> (i)], at);

        runtime.currentNoteCount = 0;
    }

    void SequencePlayer::freeRuntime (ActiveRuntime& runtime, Sink& sink, int sampleOffset) noexcept
    {
        flushPendingFor (runtime, sink, sampleOffset);
        releaseCurrentNotes (runtime, sink, sampleOffset);
        runtime.inUse = false;
    }

    // ---- event playback -------------------------------------------------------

    void SequencePlayer::startEvent (const progression::ChordEvent& event, Sink& sink, int sampleOffset, int rootOctaveOffset,
                                      midiengine::ChordKeyboardEngine* voiceLeadingSink) noexcept
    {
        auto* runtime = allocateRuntimeFor (event.id);
        if (runtime == nullptr)
            return;

        runtime->inversion = chooseEventInversion (event, rootOctaveOffset);

        if (event.rhythm == progression::RhythmPattern::Arpeggio)
        {
            runtime->arpStepIndex = 0;
            runtime->nextPulseBeat = event.startBeats;   // fire the first step immediately, below
            advanceArp (event, *runtime, sink, sampleOffset, rootOctaveOffset);

            const auto voiced = voiceEvent (event, rootOctaveOffset, runtime->inversion);
            rememberChord (voiced, voiceLeadingSink);
            return;
        }

        const auto voiced = voiceEvent (event, rootOctaveOffset, runtime->inversion);
        const int velocity = musictheory::compensateVelocityForToneCount (event.velocity, voiced.count);

        if (event.rhythm == progression::RhythmPattern::Strum)
        {
            // A quick sequential onset (roughly 18ms between notes), low to
            // high, then held for the rest of the event's duration. Notes
            // due beyond this block wait in the scheduler.
            const int stepSamples = juce::jmax (1, static_cast<int> (std::lround (0.018 * sampleRate)));

            for (int i = 0; i < voiced.count; ++i)
                scheduleNoteOn (*runtime, sink, voiced.notes[static_cast<size_t> (i)], velocity, sampleOffset + i * stepSamples, true);

            runtime->nextPulseBeat = -1.0;   // strum doesn't repeat
        }
        else
        {
            for (int i = 0; i < voiced.count; ++i)
                scheduleNoteOn (*runtime, sink, voiced.notes[static_cast<size_t> (i)], velocity, sampleOffset, true);

            const auto interval = pulseIntervalBeats (event.rhythm, event.arpRateBeats);
            runtime->nextPulseBeat = interval > 0.0 ? event.startBeats + interval : -1.0;
        }

        rememberChord (voiced, voiceLeadingSink);
    }

    // Feeds the chord just started into both voice-leading memories: this
    // player's own (sequenced -> sequenced) and the live engine's
    // (sequenced -> live).
    void SequencePlayer::rememberChord (const Voiced& voiced, midiengine::ChordKeyboardEngine* voiceLeadingSink) noexcept
    {
        if (voiced.closeCount <= 0)
            return;

        float sum = 0.0f;
        for (int i = 0; i < voiced.closeCount; ++i)
            sum += static_cast<float> (voiced.close[static_cast<size_t> (i)]);

        lastCentroid = sum / static_cast<float> (voiced.closeCount);
        haveCentroid = true;

        if (voiceLeadingSink != nullptr)
            voiceLeadingSink->rememberExternallyPlayedNotes (voiced.close.data(), voiced.closeCount);
    }

    void SequencePlayer::retriggerEvent (const progression::ChordEvent& event, ActiveRuntime& runtime, Sink& sink,
                                          int sampleOffset, int rootOctaveOffset) noexcept
    {
        flushPendingFor (runtime, sink, sampleOffset);
        releaseCurrentNotes (runtime, sink, sampleOffset);

        const auto voiced = voiceEvent (event, rootOctaveOffset, runtime.inversion);
        const int velocity = musictheory::compensateVelocityForToneCount (event.velocity, voiced.count);

        for (int i = 0; i < voiced.count; ++i)
            scheduleNoteOn (runtime, sink, voiced.notes[static_cast<size_t> (i)], velocity, sampleOffset, true);

        const auto interval = pulseIntervalBeats (event.rhythm, event.arpRateBeats);
        runtime.nextPulseBeat += interval > 0.0 ? interval : 1.0e9;
    }

    void SequencePlayer::advanceArp (const progression::ChordEvent& event, ActiveRuntime& runtime, Sink& sink,
                                      int sampleOffset, int rootOctaveOffset) noexcept
    {
        flushPendingFor (runtime, sink, sampleOffset);
        releaseCurrentNotes (runtime, sink, sampleOffset);

        const double rateBeats = juce::jmax (0.03125, event.arpRateBeats);
        const auto voiced = voiceEvent (event, rootOctaveOffset, runtime.inversion);
        const int chordCount = voiced.count;

        if (chordCount <= 0)
        {
            runtime.nextPulseBeat += rateBeats;
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

        const int note = voiced.notes[static_cast<size_t> (step)];
        const int velocity = musictheory::compensateVelocityForToneCount (event.velocity, 1);

        scheduleNoteOn (runtime, sink, note, velocity, sampleOffset, false);
        ++runtime.arpStepIndex;

        // Gate: the fraction of each step the note is actually held. 1.0 (or
        // anything that rounds to it) is legato -- the next step's release
        // ends the note, exactly as before the gate existed.
        const double gate = juce::jlimit (0.05, 1.0, event.arpGate);

        if (gate < 0.999)
            scheduleArpGateOff (runtime, note, sampleOffset + juce::jmax (1, static_cast<int> (gate * rateBeats * samplesPerBeat)));

        runtime.nextPulseBeat += rateBeats;
    }

    // ---- block rendering ------------------------------------------------------

    void SequencePlayer::syncToHost (const HostTransport& host, juce::MidiBuffer& midiOut, midiengine::NoteLedger& ledger,
                                      int midiChannel) noexcept
    {
        if (! host.valid || ! host.playing)
        {
            if (playing.load (std::memory_order_relaxed))
            {
                playing.store (false, std::memory_order_relaxed);
                releaseAllNotes (midiOut, ledger, midiChannel);
            }

            return;
        }

        if (host.bpm > 0.0)
            setTempoBpm (host.bpm);

        const bool wasPlaying = playing.load (std::memory_order_relaxed);
        const double drift = std::abs (host.ppq - positionBeats.load (std::memory_order_relaxed));

        // Starting, or the host jumped (locate, loop, scrub): whatever was
        // sounding belongs to the old position.
        if (! wasPlaying || drift > 0.02)
        {
            releaseAllNotes (midiOut, ledger, midiChannel);
            setPositionBeats (host.ppq);
        }

        playing.store (true, std::memory_order_relaxed);
    }

    void SequencePlayer::advanceAndRender (const progression::ProgressionModel& model, juce::MidiBuffer& midiOut,
                                            midiengine::NoteLedger& ledger, int midiChannel, int numSamples, int rootOctaveOffset,
                                            midiengine::ChordKeyboardEngine* voiceLeadingSink) noexcept
    {
        if (! playing.load (std::memory_order_relaxed) || numSamples <= 0 || sampleRate <= 0.0)
            return;

        // The message thread may be mid-edit (drag, undo, state restore).
        // Never wait for it on the audio thread: skip this block's
        // sequencing instead. The playhead simply doesn't advance for one
        // block, which is inaudible and far better than a data race on the
        // events vector.
        const progression::ProgressionModel::Lock::ScopedTryLockType modelLock (model.getLock());
        if (! modelLock.isLocked())
            return;

        Sink sink { midiOut, ledger, midiChannel, numSamples };

        const double tempo = tempoBpm.load (std::memory_order_relaxed);
        const bool isLooping = looping.load (std::memory_order_relaxed);
        const double loopEnd = loopEndBeats.load (std::memory_order_relaxed);
        const double loopStart = loopStartBeats.load (std::memory_order_relaxed);
        const double beatsPerSample = (tempo / 60.0) / sampleRate;
        samplesPerBeat = 1.0 / beatsPerSample;

        double position = positionBeats.load (std::memory_order_relaxed);

        // Events starting at or after this (and before the span end) are
        // triggered. One sample of tolerance: after a loop wrap the playhead
        // sits a fraction of a sample past the loop start, and an event
        // exactly on the loop start must still fire. This can't double-
        // trigger anything -- an event already sounding is never restarted,
        // and events are far longer than one sample.
        double startBound = position - beatsPerSample * 1.01;
        int samplesDone = 0;

        // Usually one pass. A block that crosses the loop end is split in
        // two (or, for a very short loop, more): the first part renders up
        // to the loop end, then playback continues from the loop start for
        // the rest of the same block. Nothing is thrown away, so the loop
        // period is exact for any block size.
        for (int pass = 0; samplesDone < numSamples && pass < 8; ++pass)
        {
            const int remaining = numSamples - samplesDone;
            const double spanStart = position;
            double spanEnd = spanStart + static_cast<double> (remaining) * beatsPerSample;

            const bool willWrap = isLooping && spanEnd >= loopEnd;
            int segmentSamples = remaining;

            if (willWrap)
            {
                const double samplesToLoopEnd = (loopEnd - spanStart) / beatsPerSample;
                segmentSamples = juce::jlimit (0, remaining, static_cast<int> (std::ceil (samplesToLoopEnd - 1.0e-9)));
                spanEnd = loopEnd;
            }

            renderSegment (model, sink, rootOctaveOffset, voiceLeadingSink,
                           startBound, spanStart, spanEnd, beatsPerSample, samplesDone, segmentSamples);

            if (willWrap)
            {
                // Everything still sounding ends with the pass.
                const int releaseAt = juce::jmin (numSamples - 1, samplesDone + juce::jmax (0, segmentSamples - 1));

                for (auto& runtime : activeRuntimes)
                    if (runtime.inUse)
                        freeRuntime (runtime, sink, releaseAt);

                haveCentroid = false;   // each pass starts the voice-leading chain afresh

                // Carry the sub-sample overshoot into the next pass so the
                // average period is exactly the loop length.
                const double overshoot = juce::jmax (0.0, spanStart + static_cast<double> (segmentSamples) * beatsPerSample - loopEnd);
                position = loopStart + overshoot;
                startBound = loopStart;   // an event sitting exactly on the loop start must still fire
                samplesDone += segmentSamples;
            }
            else
            {
                position = spanEnd;
                samplesDone = numSamples;
            }
        }

        processPending (sink);
        positionBeats.store (position, std::memory_order_relaxed);
    }

    void SequencePlayer::renderSegment (const progression::ProgressionModel& model, Sink& sink, int rootOctaveOffset,
                                         midiengine::ChordKeyboardEngine* voiceLeadingSink,
                                         double startBound, double spanStart, double spanEnd, double beatsPerSample,
                                         int sampleBase, int segmentSamples) noexcept
    {
        const int lastOffset = juce::jmax (0, segmentSamples - 1);

        // Absolute sample position within the whole block for a beat inside
        // (or before/after) this segment, clamped to the segment.
        const auto beatToSample = [&] (double beat) noexcept -> int
        {
            const double raw = (beat - spanStart) / beatsPerSample;
            return sampleBase + static_cast<int> (std::llround (juce::jlimit (0.0, static_cast<double> (lastOffset), raw)));
        };

        // Start any event beginning in this span that isn't already active.
        for (int i = 0; i < model.size(); ++i)
        {
            const auto& event = model.getEvent (i);

            if (! event.enabled)
                continue;

            if (event.startBeats >= startBound && event.startBeats < spanEnd && findRuntimeFor (event.id) == nullptr)
                startEvent (event, sink, beatToSample (event.startBeats), rootOctaveOffset, voiceLeadingSink);
        }

        // Advance (pulse, then end) every currently-active event.
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
                freeRuntime (runtime, sink, sampleBase);
                continue;
            }

            // Repeating pulses (rhythm subdivisions, arp steps) land on
            // their exact sample. Bounded, and only before the event's own
            // end, so a pulse can't fire after (or on) its event's last beat.
            const double pulseLimit = juce::jmin (spanEnd, eventPtr->endBeats());

            for (int guard = 0; guard < 32 && runtime.nextPulseBeat > 0.0 && runtime.nextPulseBeat < pulseLimit; ++guard)
            {
                const int pulseSample = beatToSample (runtime.nextPulseBeat);

                if (eventPtr->rhythm == progression::RhythmPattern::Arpeggio)
                    advanceArp (*eventPtr, runtime, sink, pulseSample, rootOctaveOffset);
                else
                    retriggerEvent (*eventPtr, runtime, sink, pulseSample, rootOctaveOffset);
            }

            if (eventPtr->endBeats() <= spanEnd)
                freeRuntime (runtime, sink, beatToSample (eventPtr->endBeats()));
        }
    }

    void SequencePlayer::releaseAllNotes (juce::MidiBuffer& midiOut, midiengine::NoteLedger& ledger, int midiChannel) noexcept
    {
        Sink sink { midiOut, ledger, midiChannel, 1 };   // everything lands on sample 0

        for (auto& runtime : activeRuntimes)
            if (runtime.inUse)
                freeRuntime (runtime, sink, 0);

        for (auto& entry : pending)
            entry.used = false;

        haveCentroid = false;   // stop/seek: the next chord has nothing to lead from
    }
}
