#include "ChordKeyboardEngine.h"
#include "../MusicTheory/Chord.h"   // musictheory::invertShape, used by chooseBestInversion
#include <cmath>

namespace midiengine
{
    namespace
    {
        // Places a pitch class in whichever octave lands closest to a
        // reference MIDI note, so all 7 slots' roots cluster together in
        // one comfortable register instead of spreading across nearly two
        // octaves (which a naive "always build upward from the pitch
        // class" placement would do).
        int placeNearReference (int pitchClass, int referenceMidiNote) noexcept
        {
            int note = pitchClass + 12 * (referenceMidiNote / 12);

            while (note < referenceMidiNote - 6)
                note += 12;
            while (note > referenceMidiNote + 6)
                note -= 12;

            return note;
        }

        // Treats each simultaneous note as an independent, incoherent
        // sound source (power sums, not amplitude), so scaling velocity by
        // roughly sqrt(reference / actual) keeps a 4-note 7th chord from
        // sounding noticeably louder than a plain 3-note triad just
        // because it has one more note ringing.
        int compensateVelocityForToneCount (int velocity, int toneCount) noexcept
        {
            constexpr float referenceToneCount = 3.0f;
            const float scale = std::sqrt (referenceToneCount / static_cast<float> (juce::jmax (1, toneCount)));
            return juce::jlimit (1, 127, static_cast<int> (std::lround (static_cast<float> (velocity) * scale)));
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
        lastPlayedNotes.fill (-1);
        lastPlayedCount = 0;
    }

    int ChordKeyboardEngine::chooseBestInversion (const musictheory::RtChordShape& rootPositionShape, int rootMidiNote) const noexcept
    {
        if (lastPlayedCount <= 0 || rootPositionShape.toneCount <= 0)
            return 0;   // nothing to voice-lead from yet -- root position

        float lastCentroid = 0.0f;
        for (int i = 0; i < lastPlayedCount; ++i)
            lastCentroid += static_cast<float> (lastPlayedNotes[static_cast<size_t> (i)]);
        lastCentroid /= static_cast<float> (lastPlayedCount);

        int bestInversion = 0;
        float bestDistance = 1.0e9f;

        for (int inversion = 0; inversion < rootPositionShape.toneCount; ++inversion)
        {
            const auto candidate = musictheory::invertShape (rootPositionShape, inversion);

            float candidateCentroid = 0.0f;
            for (int i = 0; i < candidate.toneCount; ++i)
                candidateCentroid += static_cast<float> (rootMidiNote + candidate.semitoneOffsets[static_cast<size_t> (i)]);
            candidateCentroid /= static_cast<float> (candidate.toneCount);

            const float distance = std::abs (candidateCentroid - lastCentroid);

            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestInversion = inversion;
            }
        }

        return bestInversion;
    }

    void ChordKeyboardEngine::noteOnSlot (juce::MidiBuffer& midiOut, int slot, const musictheory::RtChordShape& rootPositionShape,
                                           int rootOctaveOffset, int velocity, int midiChannel, bool autoInversion) noexcept
    {
        const int referenceMidiNote = 60 + rootOctaveOffset * 12;
        const int rootMidiNote = placeNearReference (rootPositionShape.rootPitchClass, referenceMidiNote);

        const auto shape = autoInversion
                              ? musictheory::invertShape (rootPositionShape, chooseBestInversion (rootPositionShape, rootMidiNote))
                              : rootPositionShape;

        const int toneCount = juce::jlimit (0, musictheory::maxChordTones, shape.toneCount);
        const auto clampedVelocity = static_cast<juce::uint8> (compensateVelocityForToneCount (velocity, toneCount));

        auto& voices = heldVoices[static_cast<size_t> (slot)];
        voices.count = 0;

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

        // Remember what was actually played so the *next* chord (whichever
        // slot that turns out to be) can voice-lead from it, regardless of
        // whether this one is still held when that happens.
        lastPlayedCount = voices.count;
        for (int i = 0; i < voices.count; ++i)
            lastPlayedNotes[static_cast<size_t> (i)] = voices.notes[static_cast<size_t> (i)];
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
                                                 int rootOctaveOffset, int velocity, int midiChannel, bool autoInversion) noexcept
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
                                rootOctaveOffset, velocity, midiChannel, autoInversion);
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
        lastPlayedNotes.fill (-1);
        lastPlayedCount = 0;
    }
}
