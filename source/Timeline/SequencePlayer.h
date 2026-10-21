#pragma once

#include "../Progression/ProgressionModel.h"
#include "../Midi/NoteLedger.h"
#include "../Midi/ChordKeyboardEngine.h"
#include "../MusicTheory/MusicTheoryTypes.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

// =============================================================================
// Plays a ProgressionModel back as MIDI. Two clock modes:
//   * Internal: its own Play/Stop/Loop and tempo (the plugin's Tempo knob).
//   * Host: follows the host's transport -- see syncToHost().
//
// Thread safety: the setters are called from the message thread (UI
// controls); advanceAndRender runs on the audio thread and reads them every
// block -- so all of it is atomic, same pattern as ChordKeyboardEngine's
// masks. getPositionBeats is also read from the message thread, for the UI's
// playhead. Everything else (per-event runtime state, the note scheduler) is
// audio-thread-only, touched only from inside advanceAndRender/reset/
// releaseAllNotes/syncToHost.
//
// Timing: an event's start and end, and every rhythm subdivision / arpeggio
// step inside it, are placed at the exact interpolated sample within the
// block they fall in, so notes never drift and never get stuck. A block that
// crosses the loop end is split: it renders up to the loop end, then carries
// on from the loop start for the rest of the same block, so the loop period
// is exact for any block size.
//
// Note scheduler: a Note On can be due *after* the end of the current block
// (strum spread, humanize delay). Such notes wait in a small fixed-size
// queue and are emitted in whichever later block they fall in -- never
// written past the end of a block's MidiBuffer. A queued note is cancelled
// if its event ends first, so nothing can be stuck on.
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
        // What the host's transport said at the start of this block.
        struct HostTransport
        {
            bool valid = false;     // false: no playhead / no position info available
            bool playing = false;
            double ppq = 0.0;       // position in quarter notes
            double bpm = 0.0;       // <= 0: unknown, keep the current tempo
        };

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

        // Humanize: every chord onset is delayed by a random 0..timingMs
        // (never early, so it can't land before its own event), and every
        // note's velocity varies by +/- velocityAmount (0..1, as a fraction
        // of its velocity). The random sequence is deterministic -- the same
        // progression humanizes identically on every play and in an export.
        void setHumanize (float timingMs, float velocityAmount) noexcept
        {
            humanizeTimingMs.store (juce::jlimit (0.0f, 50.0f, timingMs), std::memory_order_relaxed);
            humanizeVelocity.store (juce::jlimit (0.0f, 1.0f, velocityAmount), std::memory_order_relaxed);
        }

        // Voice-leading between consecutive timeline chords: when on, an event
        // whose Bass is "Inversion" is voiced in whichever inversion sits
        // closest to the previous event's chord (Root/Third/Fifth always force
        // their own bass note). The chain restarts from root position after a
        // loop wrap, a stop or a seek, so every pass sounds identical.
        void setAutoInversion (bool shouldAutoInvert) noexcept { autoInversion.store (shouldAutoInvert, std::memory_order_relaxed); }
        bool isAutoInversion() const noexcept { return autoInversion.load (std::memory_order_relaxed); }

        void setVoicingStyle (musictheory::VoicingStyle style) noexcept
        {
            voicingStyle.store (static_cast<int> (style), std::memory_order_relaxed);
        }

        musictheory::VoicingStyle getVoicingStyle() const noexcept
        {
            return static_cast<musictheory::VoicingStyle> (juce::jlimit (0, musictheory::numVoicingStyles - 1,
                                                                         voicingStyle.load (std::memory_order_relaxed)));
        }

        // Follows the host transport: starts/stops with it, takes its tempo,
        // and jumps to its position whenever that disagrees with where this
        // player would be (a locate, a loop jump, a late start). Call once
        // per block, before advanceAndRender. The caller should have set
        // looping off -- the host owns looping in this mode. A host that
        // gives no position info counts as "stopped". Audio thread only.
        void syncToHost (const HostTransport& host, juce::MidiBuffer& midiOut, midiengine::NoteLedger& ledger,
                         int midiChannel) noexcept;

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
        // touch notes from other sources -- see NoteLedger), cancels queued
        // notes, and forgets all per-event runtime state. Does not change
        // positionBeats. Audio thread only.
        void releaseAllNotes (juce::MidiBuffer& midiOut, midiengine::NoteLedger& ledger, int midiChannel) noexcept;

        void reset() noexcept;

    private:
        static constexpr int maxActiveEvents = 8;
        static constexpr int maxPendingNotes = 64;

        struct ActiveRuntime
        {
            bool inUse = false;
            juce::Uuid eventId;
            double nextPulseBeat = 0.0;
            int arpStepIndex = 0;
            int inversion = 0;   // chosen once at the event's start, so retriggers/arp steps stay consistent
            std::array<int, musictheory::maxVoicedNotes + 1> currentNotes {};
            int currentNoteCount = 0;
        };

        // A note event waiting for a later sample than the current block
        // covers. `delay` counts samples from the start of the *current*
        // block (so it can exceed the block length).
        struct PendingNote
        {
            bool used = false;
            bool isArpGateOff = false;   // false: a Note On
            int runtimeIndex = 0;
            int note = 0;
            juce::uint8 velocity = 0;
            int arpSerial = 0;           // gate-off only: which arp step it belongs to
            int delay = 0;
        };

        // Everything an emit needs, bundled for the helpers.
        struct Sink
        {
            juce::MidiBuffer& midi;
            midiengine::NoteLedger& ledger;
            int channel;
            int blockSamples;
        };

        struct Voiced
        {
            std::array<int, musictheory::maxVoicedNotes + 1> notes {};   // ascending; includes a custom bass note
            int count = 0;
            std::array<int, musictheory::maxChordTones> close {};        // the un-styled chord, for voice-leading memory
            int closeCount = 0;
        };

        double sampleRate = 44100.0;
        double samplesPerBeat = 22050.0;   // refreshed every block, used for arp gate lengths

        std::atomic<double> tempoBpm { 120.0 };
        std::atomic<double> positionBeats { 0.0 };
        std::atomic<bool> playing { false };
        std::atomic<bool> looping { true };
        std::atomic<double> loopStartBeats { 0.0 };
        std::atomic<double> loopEndBeats { 16.0 };
        std::atomic<float> humanizeTimingMs { 0.0f };
        std::atomic<float> humanizeVelocity { 0.0f };
        std::atomic<int> voicingStyle { 0 };
        std::atomic<bool> autoInversion { false };

        // Audio-thread-only.
        std::array<ActiveRuntime, maxActiveEvents> activeRuntimes {};
        std::array<PendingNote, maxPendingNotes> pending {};
        juce::uint32 rngState = 0x9E3779B9u;
        float lastCentroid = 0.0f;       // mean MIDI note of the previous event's chord, for voice-leading
        bool haveCentroid = false;

        // Renders [spanStart, spanEnd) beats, which occupy `segmentSamples`
        // samples starting at `sampleBase` within the current block.
        // `startBound` is the lower bound for triggering event starts (it
        // equals spanStart, except right after a loop wrap where it is the
        // loop start so an event exactly on it still fires). Audio thread
        // only; the caller holds the model's lock.
        void renderSegment (const progression::ProgressionModel& model, Sink& sink, int rootOctaveOffset,
                            midiengine::ChordKeyboardEngine* voiceLeadingSink,
                            double startBound, double spanStart, double spanEnd, double beatsPerSample,
                            int sampleBase, int segmentSamples) noexcept;

        float nextRandom01() noexcept;
        int indexOf (const ActiveRuntime& runtime) const noexcept { return static_cast<int> (&runtime - activeRuntimes.data()); }

        void rememberChord (const Voiced& voiced, midiengine::ChordKeyboardEngine* voiceLeadingSink) noexcept;
        Voiced voiceEvent (const progression::ChordEvent& event, int rootOctaveOffset, int autoInversionIndex) const noexcept;
        int chooseEventInversion (const progression::ChordEvent& event, int rootOctaveOffset) const noexcept;

        ActiveRuntime* findRuntimeFor (const juce::Uuid& id) noexcept;
        ActiveRuntime* allocateRuntimeFor (const juce::Uuid& id) noexcept;
        void freeRuntime (ActiveRuntime& runtime, Sink& sink, int sampleOffset) noexcept;

        // Note emission / scheduling.
        void emitNoteOn (ActiveRuntime& runtime, Sink& sink, int note, juce::uint8 velocity, int sample) noexcept;
        void scheduleNoteOn (ActiveRuntime& runtime, Sink& sink, int note, int velocity, int sample, bool jitterTiming) noexcept;
        void scheduleArpGateOff (ActiveRuntime& runtime, int note, int delaySamples) noexcept;
        void runPending (const PendingNote& entry, Sink& sink, int sample) noexcept;
        void flushPendingFor (ActiveRuntime& runtime, Sink& sink, int beforeSample) noexcept;
        void processPending (Sink& sink) noexcept;
        void releaseCurrentNotes (ActiveRuntime& runtime, Sink& sink, int sampleOffset) noexcept;

        void startEvent (const progression::ChordEvent& event, Sink& sink, int sampleOffset, int rootOctaveOffset,
                         midiengine::ChordKeyboardEngine* voiceLeadingSink) noexcept;
        void retriggerEvent (const progression::ChordEvent& event, ActiveRuntime& runtime, Sink& sink,
                             int sampleOffset, int rootOctaveOffset) noexcept;
        void advanceArp (const progression::ChordEvent& event, ActiveRuntime& runtime, Sink& sink,
                         int sampleOffset, int rootOctaveOffset) noexcept;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencePlayer)
    };
}
