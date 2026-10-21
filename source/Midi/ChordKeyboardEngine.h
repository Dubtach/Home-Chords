#pragma once

#include "NoteLedger.h"
#include "../MusicTheory/MusicTheoryTypes.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

// =============================================================================
// Turns "which of the 7 chord slots are currently held" into real MIDI Note
// On / Note Off, safely across the message thread (computer keyboard, mouse)
// and the audio thread (where MIDI is actually sent, and where an external
// MIDI keyboard's notes arrive).
//
// Design notes, because getting this wrong is exactly how you get stuck notes:
//
// 1. Other threads never touch a juce::MidiBuffer or this engine's audio-
//    thread state. They only flip bits in atomic masks.
//
// 2. The engine remembers the *actual* notes it played for a slot at the
//    moment it went down, and releases exactly those notes -- even if Key,
//    Scale or Octave change while the chord is held.
//
// 3. Every note goes through the shared NoteLedger, so slots (and the
//    progression sequencer, via a different NoteLedger caller) that happen
//    to share a MIDI note number can't cut each other off or double-release.
//
// 4. Three input sources are tracked as separate held-levels -- computer
//    keyboard, mouse, external MIDI -- and OR'd together at render time, so
//    releasing one can't release a chord another source is still holding.
//    Mouse is kept separate from the "triggered" notification below because
//    ADD mode appends a chord to the timeline on a keyboard/MIDI press but
//    not on a mouse click (cards are clicked to preview or dragged to add).
//
// 5. "Triggered" (for ADD mode) is detected at the moment each setter sees
//    its own rising edge, not by comparing held-levels once per block --
//    the latter would miss a press that's released again before the next
//    block gets a chance to look.
//
// Everything here is fixed-size and allocation-free.
// =============================================================================

namespace midiengine
{
    class ChordKeyboardEngine
    {
    public:
        static constexpr int numSlots = musictheory::maxDiatonicSlots;

        ChordKeyboardEngine() noexcept { reset(); }

        // ---- Message thread -------------------------------------------------

        // Computer keyboard. Lock-free. Counts as an ADD-mode trigger.
        void setSlotHeld (int slotIndex, bool held) noexcept;

        // Mouse. Sounds the chord but never counts as an ADD-mode trigger.
        void setSlotHeldFromMouse (int slotIndex, bool held) noexcept;

        // Slots currently sounding, for the UI's highlight animation.
        juce::uint32 getActiveSlotMaskForUi() const noexcept
        {
            return activeMaskForUi.load (std::memory_order_relaxed);
        }

        // Slots pressed (by keyboard or MIDI) since the last call -- what
        // ADD mode appends to the timeline. Never misses a quick tap.
        juce::uint32 consumeTriggeredSlots() noexcept
        {
            return triggeredMask.exchange (0, std::memory_order_relaxed);
        }

        // ---- Audio thread ---------------------------------------------------

        // External MIDI keyboard input (see PluginProcessor for the white-key
        // mapping). Plain members, not atomics: only ever touched from the
        // audio thread, same thread renderBlockStart runs on.
        void setSlotHeldFromMidi (int slotIndex, bool held) noexcept;
        void releaseAllMidiHeldSlots() noexcept;

        // Call once per block, before other MIDI is added to midiOut. Emits
        // Note On/Off for every slot whose combined (keyboard | mouse |
        // MIDI) held state changed since the last call. All notes land at
        // sample 0 -- keyboard input has no sample-accurate timestamp to
        // honour, so this is a deliberate simplification. Each slot's root
        // is placed by its distance above slot 0's root (the scale tonic --
        // an override never changes a root), so the row ascends smoothly.
        // With autoInversion, a new chord is re-voiced into whichever
        // inversion sits closest to the last chord played (by any source,
        // live or sequenced -- see MusicTheory/Voicing.h).
        void renderBlockStart (juce::MidiBuffer& midiOut, NoteLedger& ledger,
                                const musictheory::DiatonicShapeSet& currentShapes,
                                int rootOctaveOffset, int velocity, int midiChannel, bool autoInversion,
                                musictheory::VoicingStyle style = musictheory::VoicingStyle::Close) noexcept;

        // Releases everything this engine is holding (not other emitters'
        // notes -- see NoteLedger), and forgets all held-key state.
        void allNotesOff (juce::MidiBuffer& midiOut, NoteLedger& ledger, int midiChannel) noexcept;

        // Feeds an externally-voiced chord (e.g. from the sequencer) into
        // this engine's voice-leading memory, so the *next live-played*
        // chord leads smoothly from whatever the timeline just played.
        // Real-time safe; does not itself emit MIDI.
        void rememberExternallyPlayedNotes (const int* notes, int count) noexcept;

        // Resets internal state without emitting MIDI (prepareToPlay).
        void reset() noexcept;

    private:
        struct SlotVoices
        {
            std::array<int, musictheory::maxVoicedNotes> notes { -1, -1, -1, -1, -1, -1 };
            int count = 0;
        };

        // Written by the message thread, read by the audio thread.
        std::atomic<juce::uint32> requestedMask { 0 };   // computer keyboard, held level
        std::atomic<juce::uint32> mouseMask { 0 };        // mouse, held level

        // Written by the audio thread, read by the message thread.
        std::atomic<juce::uint32> activeMaskForUi { 0 };
        std::atomic<juce::uint32> triggeredMask { 0 };

        // Audio-thread-only.
        juce::uint32 midiRequestedMask = 0;
        juce::uint32 lastRenderedMask = 0;
        std::array<SlotVoices, numSlots> heldVoices {};

        // The actual notes of the most recently triggered chord (live or
        // sequenced), purely as the voice-leading reference for whichever
        // chord is triggered next.
        std::array<int, musictheory::maxChordTones> lastPlayedNotes {};
        int lastPlayedCount = 0;

        void noteOnSlot (juce::MidiBuffer& midiOut, NoteLedger& ledger, int slot,
                          const musictheory::RtChordShape& rootPositionShape, int anchorPitchClass,
                          int rootOctaveOffset, int velocity, int midiChannel, bool autoInversion,
                          musictheory::VoicingStyle style) noexcept;
        void noteOffSlot (juce::MidiBuffer& midiOut, NoteLedger& ledger, int slot, int midiChannel) noexcept;
        float lastPlayedCentroid() const noexcept;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordKeyboardEngine)
    };
}
