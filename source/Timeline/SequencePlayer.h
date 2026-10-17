#pragma once

#include "../Progression/ProgressionModel.h"
#include "../Midi/NoteLedger.h"
#include "../Midi/ChordKeyboardEngine.h"
#include "../MusicTheory/MusicTheoryTypes.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

// =============================================================================
// Plays a ProgressionModel back on its own internal clock (Play/Stop/Loop,
// driven by a tempo the user sets in this plugin -- NOT synced to host
// transport position; see STATUS.md for why that's a deliberate scope
// choice for this pass, not an oversight).
//
// Thread safety: setPlaying/setLooping/setLoopRange/setTempoBpm/
// setPositionBeats are called from the message thread (UI controls);
// advanceAndRender runs on the audio thread and reads them every block --
// so all of it is atomic, same pattern as ChordKeyboardEngine's masks.
// getPositionBeats is also read from the message thread, for the UI's
// playhead. Everything else (per-event runtime state) is audio-thread-only,
// touched only from inside advanceAndRender/reset.
//
// Timing: an event's start and end, and every rhythm subdivision / arpeggio
// step inside it, are placed at the exact interpolated sample within the
// block they fall in, so notes never drift and never get stuck. A block that
// crosses the loop end is split: it renders up to the loop end, then carries
// on from the loop start for the rest of the same block, so the loop period
// is exact for any block size.
//
// The model is read under its spin lock via a *try*-lock: if the message
// thread is mid-edit the block's sequencing is skipped rather than waited
// for (see ProgressionModel.h).
//
// Real-time safe: fixed-size, no allocation once prepare() has run.
// =============================================================================

namespace timeline
{
    class SequencePlayer
    {
    public:
        SequencePlayer() noexcept { reset(); }

        void prepare (double newSampleRate) noexcept { sampleRate = newSampleRate; }

        void setPlaying (bool shouldPlay) noexcept { playing.store (shouldPlay, std::memory_order_relaxed); }
        bool isPlaying() const noexcept { return playing.load (std::memory_order_relaxed); }

        void setLooping (bool shouldLoop) noexcept { looping.store (shouldLoop, std::memory_order_relaxed); }
        bool isLooping() const noexcept { return looping.load (std::memory_order_relaxed); }

        void setLoopRange (double startBeats, double endBeats) noexcept
        {
            const double clampedStart = juce::jmax (0.0, startBeats);
            loopStartBeats.store (clampedStart, std::memory_order_relaxed);
            loopEndBeats.store (juce::jmax (clampedStart + 0.25, endBeats), std::memory_order_relaxed);
        }

        double getLoopStartBeats() const noexcept { return loopStartBeats.load (std::memory_order_relaxed); }
        double getLoopEndBeats() const noexcept { return loopEndBeats.load (std::memory_order_relaxed); }

        void setTempoBpm (double bpm) noexcept { tempoBpm.store (juce::jlimit (20.0, 300.0, bpm), std::memory_order_relaxed); }
        double getTempoBpm() const noexcept { return tempoBpm.load (std::memory_order_relaxed); }

        // Seeks without emitting MIDI -- caller should release notes first
        // (via releaseAllNotes) if anything might be sounding.
        void setPositionBeats (double beats) noexcept { positionBeats.store (juce::jmax (0.0, beats), std::memory_order_relaxed); }
        double getPositionBeats() const noexcept { return positionBeats.load (std::memory_order_relaxed); }

        // Advances playback by numSamples (if playing) and emits Note On/
        // Off for every event boundary and rhythm/arp pulse crossed, via
        // the shared ledger. `model` is read-only here. `voiceLeadingSink`,
        // if non-null, has its live voice-leading memory updated with
        // whatever this call last played, so a chord played on the
        // keyboard right after can lead smoothly from it -- purely a nice-
        // to-have link between sequenced and live playback, not required
        // for correct sequencing. Audio thread only.
        void advanceAndRender (const progression::ProgressionModel& model, juce::MidiBuffer& midiOut,
                                midiengine::NoteLedger& ledger, int midiChannel, int numSamples, int rootOctaveOffset,
                                midiengine::ChordKeyboardEngine* voiceLeadingSink) noexcept;

        // Stops every event this player currently has sounding (does not
        // touch notes from other sources -- see NoteLedger) and forgets
        // all per-event runtime state. Does not change positionBeats.
        // Audio thread only.
        void releaseAllNotes (juce::MidiBuffer& midiOut, midiengine::NoteLedger& ledger, int midiChannel) noexcept;

        void reset() noexcept;

    private:
        static constexpr int maxActiveEvents = 8;

        struct ActiveRuntime
        {
            bool inUse = false;
            juce::Uuid eventId;
            double nextPulseBeat = 0.0;
            int arpStepIndex = 0;
            std::array<int, musictheory::maxChordTones> currentNotes {};
            int currentNoteCount = 0;
        };

        double sampleRate = 44100.0;

        std::atomic<double> tempoBpm { 120.0 };
        std::atomic<double> positionBeats { 0.0 };
        std::atomic<bool> playing { false };
        std::atomic<bool> looping { true };
        std::atomic<double> loopStartBeats { 0.0 };
        std::atomic<double> loopEndBeats { 16.0 };

        // Audio-thread-only.
        std::array<ActiveRuntime, maxActiveEvents> activeRuntimes {};

        // Renders [spanStart, spanEnd) beats, which occupy `segmentSamples`
        // samples starting at `sampleBase` within the current block.
        // `startBound` is the lower bound for triggering event starts (it
        // equals spanStart, except right after a loop wrap where it is the
        // loop start so an event exactly on it still fires). Audio thread
        // only; the caller holds the model's lock.
        void renderSegment (const progression::ProgressionModel& model, juce::MidiBuffer& midiOut,
                            midiengine::NoteLedger& ledger, int midiChannel, int rootOctaveOffset,
                            midiengine::ChordKeyboardEngine* voiceLeadingSink,
                            double startBound, double spanStart, double spanEnd, double beatsPerSample,
                            int sampleBase, int segmentSamples) noexcept;

        ActiveRuntime* findRuntimeFor (const juce::Uuid& id) noexcept;
        ActiveRuntime* allocateRuntimeFor (const juce::Uuid& id) noexcept;
        void freeRuntime (ActiveRuntime& runtime, juce::MidiBuffer& midiOut, midiengine::NoteLedger& ledger, int midiChannel,
                          int sampleOffset) noexcept;

        void startEvent (const progression::ChordEvent& event, juce::MidiBuffer& midiOut, midiengine::NoteLedger& ledger,
                          int midiChannel, int sampleOffset, int rootOctaveOffset,
                          midiengine::ChordKeyboardEngine* voiceLeadingSink) noexcept;
        void retriggerEvent (const progression::ChordEvent& event, ActiveRuntime& runtime, juce::MidiBuffer& midiOut,
                             midiengine::NoteLedger& ledger, int midiChannel, int sampleOffset, int rootOctaveOffset) noexcept;
        void advanceArp (const progression::ChordEvent& event, ActiveRuntime& runtime, juce::MidiBuffer& midiOut,
                         midiengine::NoteLedger& ledger, int midiChannel, int sampleOffset, int rootOctaveOffset) noexcept;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencePlayer)
    };
}
