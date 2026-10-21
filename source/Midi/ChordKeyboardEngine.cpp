#include "ChordKeyboardEngine.h"
#include "../MusicTheory/Chord.h"
#include "../MusicTheory/Voicing.h"

namespace midiengine
{
    void ChordKeyboardEngine::setSlotHeld (int slotIndex, bool held) noexcept
    {
        if (slotIndex < 0 || slotIndex >= numSlots)
            return;

        const auto bit = static_cast<juce::uint32> (1u << static_cast<unsigned> (slotIndex));

        if (held)
        {
            const auto previous = requestedMask.fetch_or (bit, std::memory_order_relaxed);
            if ((previous & bit) == 0)
                triggeredMask.fetch_or (bit, std::memory_order_relaxed);
        }
        else
        {
            requestedMask.fetch_and (~bit, std::memory_order_relaxed);
        }
    }

    void ChordKeyboardEngine::setSlotHeldFromMouse (int slotIndex, bool held) noexcept
    {
        if (slotIndex < 0 || slotIndex >= numSlots)
            return;

        const auto bit = static_cast<juce::uint32> (1u << static_cast<unsigned> (slotIndex));

        if (held)
            mouseMask.fetch_or (bit, std::memory_order_relaxed);
        else
            mouseMask.fetch_and (~bit, std::memory_order_relaxed);
    }

    void ChordKeyboardEngine::setSlotHeldFromMidi (int slotIndex, bool held) noexcept
    {
        if (slotIndex < 0 || slotIndex >= numSlots)
            return;

        const auto bit = static_cast<juce::uint32> (1u << static_cast<unsigned> (slotIndex));

        if (held)
        {
            if ((midiRequestedMask & bit) == 0)
                triggeredMask.fetch_or (bit, std::memory_order_relaxed);   // atomic write is safe from this thread too

            midiRequestedMask |= bit;
        }
        else
        {
            midiRequestedMask &= ~bit;
        }
    }

    void ChordKeyboardEngine::releaseAllMidiHeldSlots() noexcept
    {
        midiRequestedMask = 0;
    }

    void ChordKeyboardEngine::reset() noexcept
    {
        requestedMask.store (0, std::memory_order_relaxed);
        mouseMask.store (0, std::memory_order_relaxed);
        activeMaskForUi.store (0, std::memory_order_relaxed);
        triggeredMask.store (0, std::memory_order_relaxed);

        midiRequestedMask = 0;
        lastRenderedMask = 0;

        for (auto& voices : heldVoices)
        {
            voices.count = 0;
            voices.notes.fill (-1);
        }

        lastPlayedNotes.fill (-1);
        lastPlayedCount = 0;
    }

    void ChordKeyboardEngine::rememberExternallyPlayedNotes (const int* notes, int count) noexcept
    {
        lastPlayedCount = juce::jlimit (0, musictheory::maxChordTones, count);

        for (int i = 0; i < lastPlayedCount; ++i)
            lastPlayedNotes[static_cast<size_t> (i)] = notes[i];
    }

    float ChordKeyboardEngine::lastPlayedCentroid() const noexcept
    {
        if (lastPlayedCount <= 0)
            return 0.0f;

        float sum = 0.0f;
        for (int i = 0; i < lastPlayedCount; ++i)
            sum += static_cast<float> (lastPlayedNotes[static_cast<size_t> (i)]);

        return sum / static_cast<float> (lastPlayedCount);
    }

    void ChordKeyboardEngine::noteOnSlot (juce::MidiBuffer& midiOut, NoteLedger& ledger, int slot,
                                           const musictheory::RtChordShape& rootPositionShape, int anchorPitchClass,
                                           int rootOctaveOffset, int velocity, int midiChannel, bool autoInversion,
                                           musictheory::VoicingStyle style) noexcept
    {
        const int anchorMidi = musictheory::anchorMidiNote (anchorPitchClass, rootOctaveOffset);
        const int rootMidiNote = musictheory::placeRootAscending (rootPositionShape.rootPitchClass, anchorPitchClass, anchorMidi);

        const int inversion = (autoInversion && lastPlayedCount > 0)
                                 ? musictheory::chooseInversionNearest (rootPositionShape, rootMidiNote, lastPlayedCentroid())
                                 : 0;

        // The close voicing is what voice-leading remembers and compares
        // against (so a spread style can't make later chords drift
        // register); the styled voicing is what actually sounds.
        std::array<int, musictheory::maxChordTones> closeNotes {};
        const int closeCount = musictheory::voiceShape (rootPositionShape, rootMidiNote, inversion, closeNotes);

        auto& voices = heldVoices[static_cast<size_t> (slot)];
        voices.count = musictheory::applyVoicingStyle (closeNotes, closeCount, rootMidiNote, style, voices.notes);

        const auto clampedVelocity = static_cast<juce::uint8> (musictheory::compensateVelocityForToneCount (velocity, voices.count));

        for (int i = 0; i < voices.count; ++i)
            ledger.noteOn (midiOut, midiChannel, voices.notes[static_cast<size_t> (i)], clampedVelocity, 0);

        // Remember what was actually played so the *next* chord (whichever
        // slot, whenever it's triggered) can voice-lead from it.
        lastPlayedCount = closeCount;
        for (int i = 0; i < closeCount; ++i)
            lastPlayedNotes[static_cast<size_t> (i)] = closeNotes[static_cast<size_t> (i)];
    }

    void ChordKeyboardEngine::noteOffSlot (juce::MidiBuffer& midiOut, NoteLedger& ledger, int slot, int midiChannel) noexcept
    {
        auto& voices = heldVoices[static_cast<size_t> (slot)];

        for (int i = 0; i < voices.count; ++i)
            ledger.noteOff (midiOut, midiChannel, voices.notes[static_cast<size_t> (i)], 0);

        voices.count = 0;
        voices.notes.fill (-1);
    }

    void ChordKeyboardEngine::renderBlockStart (juce::MidiBuffer& midiOut, NoteLedger& ledger,
                                                 const musictheory::DiatonicShapeSet& currentShapes,
                                                 int rootOctaveOffset, int velocity, int midiChannel, bool autoInversion,
                                                 musictheory::VoicingStyle style) noexcept
    {
        const auto combined = requestedMask.load (std::memory_order_relaxed)
                             | mouseMask.load (std::memory_order_relaxed)
                             | midiRequestedMask;

        const auto changed = combined ^ lastRenderedMask;

        if (changed == 0)
            return;

        // Slot 0's root is always the scale tonic -- an override never
        // changes a slot's root, only what's built on it -- so it's the
        // fixed point every other slot's root is placed relative to.
        const int anchorPitchClass = currentShapes.count > 0 ? currentShapes.shapes[0].rootPitchClass : 0;

        for (int slot = 0; slot < numSlots; ++slot)
        {
            const auto bit = static_cast<juce::uint32> (1u << static_cast<unsigned> (slot));

            if ((changed & bit) == 0)
                continue;

            const bool nowHeld = (combined & bit) != 0;

            if (nowHeld)
            {
                // A slot past the active scale's degree count (e.g. slots
                // 5-6 on a 5-note pentatonic scale) has no diatonic chord
                // to play -- pressing it is a no-op, not an error.
                if (slot < currentShapes.count)
                    noteOnSlot (midiOut, ledger, slot, currentShapes.shapes[static_cast<size_t> (slot)],
                                anchorPitchClass, rootOctaveOffset, velocity, midiChannel, autoInversion, style);
            }
            else
            {
                noteOffSlot (midiOut, ledger, slot, midiChannel);
            }
        }

        lastRenderedMask = combined;

        juce::uint32 uiMask = 0;
        for (int slot = 0; slot < numSlots; ++slot)
            if (heldVoices[static_cast<size_t> (slot)].count > 0)
                uiMask |= static_cast<juce::uint32> (1u << static_cast<unsigned> (slot));

        activeMaskForUi.store (uiMask, std::memory_order_relaxed);
    }

    void ChordKeyboardEngine::allNotesOff (juce::MidiBuffer& midiOut, NoteLedger& ledger, int midiChannel) noexcept
    {
        for (int slot = 0; slot < numSlots; ++slot)
            noteOffSlot (midiOut, ledger, slot, midiChannel);

        requestedMask.store (0, std::memory_order_relaxed);
        mouseMask.store (0, std::memory_order_relaxed);
        midiRequestedMask = 0;
        lastRenderedMask = 0;
        activeMaskForUi.store (0, std::memory_order_relaxed);
        lastPlayedNotes.fill (-1);
        lastPlayedCount = 0;
    }
}
