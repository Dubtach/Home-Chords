#pragma once

#include "../MusicTheory/MusicTheoryTypes.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

// =============================================================================
// Turns "which of the 7 chord slots are currently held" into real MIDI
// Note On / Note Off events, safely across the message thread (where key
// presses happen) and the audio thread (where MIDI actually gets sent).
//
// Design notes, because getting this wrong is exactly how you get stuck
// notes and wrong note-offs (see spec section 39):
//
// 1. The message thread never touches a juce::MidiBuffer or the audio
//    thread's state directly. It only flips bits in an atomic mask.
//
// 2. The audio thread captures the *actual* MIDI notes it played for a slot
//    at the moment that slot went down, and releases exactly those notes
//    when it goes up -- even if Key/Scale/Octave change while the chord is
//    still held. Otherwise changing key mid-chord would either leave notes
//    stuck on or turn off the wrong notes.
//
// 3. Two slots can legitimately share a MIDI note number (e.g. two chords
//    that both contain the same pitch class in different octaves collide
//    at the note-number level surprisingly often once voicing/inversions
//    exist). A per-note reference count means a note only actually gets a
//    Note Off once nothing still needs it.
//
// 4. Everything here is fixed-size and allocation-free, so it's safe to
//    call renderBlockStart() from processBlock() every single block.
// =============================================================================

namespace midiengine
{
    class ChordKeyboardEngine
    {
    public:
        static constexpr int numSlots = musictheory::maxDiatonicSlots;

        ChordKeyboardEngine() noexcept { reset(); }

        // ---- Message thread ------------------------------------------------

        // Called from the editor's key-handling whenever a tracked key's
        // held state changes. Lock-free.
        void setSlotHeld (int slotIndex, bool held) noexcept;

        // Snapshot of which slots are currently sounding, for UI highlight
        // animation. Safe to poll from a Timer.
        juce::uint32 getActiveSlotMaskForUi() const noexcept
        {
            return activeMaskForUi.load (std::memory_order_relaxed);
        }

        // ---- Audio thread ---------------------------------------------------

        // Call once, at the very start of processBlock, before any other
        // MIDI is added to `midiOut` this block. Diffs the requested mask
        // against what was last acted on and emits Note On/Off for every
        // slot that changed. All notes land at sample 0 of the block --
        // keyboard input has no sample-accurate host timestamp to honour
        // anyway, so this is a deliberate, simple, and safe choice rather
        // than a compromise around a real constraint. When `autoInversion`
        // is true, a newly-pressed chord is re-voiced into whichever
        // inversion keeps it closest to the last chord that was played
        // (see chooseBestInversion); when false, every chord plays in
        // root position, as Phase 1 always did.
        void renderBlockStart (juce::MidiBuffer& midiOut,
                                const musictheory::DiatonicShapeSet& currentShapes,
                                int rootOctaveOffset, int velocity, int midiChannel, bool autoInversion) noexcept;

        // Force every currently-sounding note off immediately (block
        // start/stop, prepareToPlay, or the editor losing keyboard focus
        // mid-chord). Safe to call from the audio thread.
        void allNotesOff (juce::MidiBuffer& midiOut) noexcept;

        // Resets all internal state (used from prepareToPlay). Does not
        // emit MIDI -- callers that need a guaranteed-clean host state
        // should call allNotesOff() first if any notes might be sounding.
        void reset() noexcept;

    private:
        struct SlotVoices
        {
            std::array<int, musictheory::maxChordTones> notes { -1, -1, -1, -1 };
            int count = 0;
        };

        static constexpr int midiNoteCount = 128;

        std::atomic<juce::uint32> requestedMask { 0 };     // bit i = slot i held, written by message thread
        std::atomic<juce::uint32> activeMaskForUi { 0 };   // bit i = slot i sounding, written by audio thread

        juce::uint32 lastRenderedMask = 0;                          // audio-thread-only
        std::array<SlotVoices, numSlots> heldVoices {};             // audio-thread-only
        std::array<int, midiNoteCount> noteRefCount {};             // audio-thread-only

        // The actual MIDI notes of the most recently *triggered* chord
        // (any slot, whether or not it's still held), used purely as the
        // voice-leading reference point for whichever chord gets pressed
        // next. Audio-thread-only, like everything else in this section.
        std::array<int, musictheory::maxChordTones> lastPlayedNotes {};
        int lastPlayedCount = 0;

        void noteOnSlot (juce::MidiBuffer& midiOut, int slot, const musictheory::RtChordShape& rootPositionShape,
                          int rootOctaveOffset, int velocity, int midiChannel, bool autoInversion) noexcept;
        void noteOffSlot (juce::MidiBuffer& midiOut, int slot, int midiChannel) noexcept;

        // Evaluates every inversion of `rootPositionShape` (already placed
        // with its root at `rootMidiNote`) and returns whichever one's
        // centroid lands closest to the last chord's centroid -- a simple,
        // deliberately bounded stand-in for full voice-leading: it never
        // considers shifting the whole chord by an extra octave, only
        // which of its own tones sit an octave up, so it can't wander into
        // an extreme register even after a long chord sequence.
        int chooseBestInversion (const musictheory::RtChordShape& rootPositionShape, int rootMidiNote) const noexcept;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordKeyboardEngine)
    };
}
