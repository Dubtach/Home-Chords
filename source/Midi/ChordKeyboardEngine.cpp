#include "ChordKeyboardEngine.h"

namespace midiengine
{
    namespace
    {
        // Places a pitch class in whichever octave lands closest to a
        // reference MIDI note, so all 7 slots' roots cluster together in
        // one comfortable register instead of spreading across nearly two
        // octaves (which a naive "always build upward from the pitch
        // class" placement would do). This is a simple, honest stand-in
        // for real voice-leading -- the Auto Voicing engine (spec
        // section 13) that actually minimises movement between
        // *neighbouring* chords in a progression is Phase 3 work.
        int placeNearReference (int pitchClass, int referenceMidiNote) noexcept
        {
            int note = pitchClass + 12 * (referenceMidiNote / 12);

            while (note < referenceMidiNote - 6)
                note += 12;
            while (note > referenceMidiNote + 6)
                note -= 12;

            return note;
        }
    }

    void ChordKeyboardEngine::setSlotHeld (int slotIndex, bool held) noexcept
    {
        if (slotIndex < 0 || slotIndex >= numSlots)
            return;

        const auto bit = static_cast<juce::uint32> (1u << static_cast<unsigned> (slotIndex));

        if (held)
            requestedMask.fetch_or (bit, std::memory_order_relaxed);
        else
            requestedMask.fetch_and (~bit, std::memory_order_relaxed);
    }

    void ChordKeyboardEngine::reset() noexcept
    {
        requestedMask.store (0, std::memory_order_relaxed);
        activeMaskForUi.store (0, std::memory_order_relaxed);
        lastRenderedMask = 0;

        for (auto& voices : heldVoices)
        {
            voices.count = 0;
            voices.notes.fill (-1);
        }

        noteRefCount.fill (0);
    }

    void ChordKeyboardEngine::noteOnSlot (juce::MidiBuffer& midiOut, int slot, const musictheory::RtChordShape& shape,
                                           int rootOctaveOffset, int velocity, int midiChannel) noexcept
    {
        const int referenceMidiNote = 60 + rootOctaveOffset * 12;
        const int rootMidiNote = placeNearReference (shape.rootPitchClass, referenceMidiNote);
        const auto clampedVelocity = static_cast<juce::uint8> (juce::jlimit (1, 127, velocity));

        auto& voices = heldVoices[static_cast<size_t> (slot)];
        voices.count = 0;

        const int toneCount = juce::jlimit (0, musictheory::maxChordTones, shape.toneCount);

        for (int i = 0; i < toneCount; ++i)
        {
            const int note = juce::jlimit (0, 127, rootMidiNote + shape.semitoneOffsets[static_cast<size_t> (i)]);

            voices.notes[static_cast<size_t> (voices.count)] = note;
            ++voices.count;

            auto& refCount = noteRefCount[static_cast<size_t> (note)];

            // Only the first thing to need this exact MIDI note number
            // actually triggers it -- if another slot already has it
            // sounding (voicings can overlap), we just add our own claim
            // on it via the ref count below.
            if (refCount == 0)
                midiOut.addEvent (juce::MidiMessage::noteOn (midiChannel, note, clampedVelocity), 0);

            ++refCount;
        }
    }

    void ChordKeyboardEngine::noteOffSlot (juce::MidiBuffer& midiOut, int slot, int midiChannel) noexcept
    {
        auto& voices = heldVoices[static_cast<size_t> (slot)];

        for (int i = 0; i < voices.count; ++i)
        {
            const int note = voices.notes[static_cast<size_t> (i)];

            if (note < 0 || note >= midiNoteCount)
                continue;

            auto& refCount = noteRefCount[static_cast<size_t> (note)];

            if (refCount > 0)
                --refCount;

            // Only actually release the note once nothing else still
            // claims it -- this is what stops a still-held slot from
            // losing a note just because a different slot that happened
            // to share it was released.
            if (refCount == 0)
                midiOut.addEvent (juce::MidiMessage::noteOff (midiChannel, note), 0);
        }

        voices.count = 0;
        voices.notes.fill (-1);
    }

    void ChordKeyboardEngine::renderBlockStart (juce::MidiBuffer& midiOut,
                                                 const musictheory::DiatonicShapeSet& currentShapes,
                                                 int rootOctaveOffset, int velocity, int midiChannel) noexcept
    {
        const auto requested = requestedMask.load (std::memory_order_relaxed);
        const auto changed = requested ^ lastRenderedMask;

        if (changed == 0)
            return;

        for (int slot = 0; slot < numSlots; ++slot)
        {
            const auto bit = static_cast<juce::uint32> (1u << static_cast<unsigned> (slot));

            if ((changed & bit) == 0)
                continue;

            const bool nowHeld = (requested & bit) != 0;

            if (nowHeld)
            {
                // A slot past the active scale's degree count (e.g. slots
                // 5 and 6 on a 5-note pentatonic scale) has no diatonic
                // chord to play -- pressing it is a no-op, not an error.
                if (slot < currentShapes.count)
                    noteOnSlot (midiOut, slot, currentShapes.shapes[static_cast<size_t> (slot)],
                                rootOctaveOffset, velocity, midiChannel);
            }
            else
            {
                noteOffSlot (midiOut, slot, midiChannel);
            }
        }

        lastRenderedMask = requested;

        juce::uint32 uiMask = 0;
        for (int slot = 0; slot < numSlots; ++slot)
            if (heldVoices[static_cast<size_t> (slot)].count > 0)
                uiMask |= static_cast<juce::uint32> (1u << static_cast<unsigned> (slot));

        activeMaskForUi.store (uiMask, std::memory_order_relaxed);
    }

    void ChordKeyboardEngine::allNotesOff (juce::MidiBuffer& midiOut) noexcept
    {
        constexpr int midiChannel = 1;

        for (int note = 0; note < midiNoteCount; ++note)
        {
            if (noteRefCount[static_cast<size_t> (note)] > 0)
                midiOut.addEvent (juce::MidiMessage::noteOff (midiChannel, note), 0);

            noteRefCount[static_cast<size_t> (note)] = 0;
        }

        for (auto& voices : heldVoices)
        {
            voices.count = 0;
            voices.notes.fill (-1);
        }

        requestedMask.store (0, std::memory_order_relaxed);
        lastRenderedMask = 0;
        activeMaskForUi.store (0, std::memory_order_relaxed);
    }
}
