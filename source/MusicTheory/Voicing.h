#pragma once

#include "MusicTheoryTypes.h"
#include <array>

// =============================================================================
// Where a chord's notes actually land on the keyboard.
//
// Shared by live play (ChordKeyboardEngine) and the progression timeline
// (SequenceBuilder), so a chord sounds identical whether it was played or
// sequenced. All real-time safe: no allocation, no JUCE types.
// =============================================================================

namespace musictheory
{
    // The MIDI note the scale tonic (the "anchor") lands on for a given
    // keyboard octave shift: whichever octave puts it nearest 60 + 12*shift.
    int anchorMidiNote (int anchorPitchClass, int rootOctaveOffset) noexcept;

    // Places a chord root by its semitone distance *above* the anchor
    // (always 0-11), not by independently snapping each root to its own
    // nearest octave. Independent snapping is what used to make chords on
    // one side of the scale drop an octave versus the other; anchoring
    // makes the whole row ascend smoothly in every key.
    int placeRootAscending (int rootPitchClass, int anchorPitchClass, int anchorMidi) noexcept;

    // Which inversion (0..toneCount-1) of a root-position shape, with its
    // root at rootMidiNote, has a centroid closest to referenceCentroid.
    // Ties go to the lower inversion.
    int chooseInversionNearest (const RtChordShape& rootPositionShape, int rootMidiNote, float referenceCentroid) noexcept;

    // Turns a shape into absolute MIDI notes for a given inversion:
    // outNotes[0..count-1], ascending, clamped to 0-127. Returns count.
    int voiceShape (const RtChordShape& rootPositionShape, int rootMidiNote, int inversion,
                    std::array<int, maxChordTones>& outNotes) noexcept;

    // Scales velocity by roughly sqrt(3 / toneCount) so a 4-note chord
    // doesn't ring louder than a 3-note one (power of independent notes
    // sums, not amplitude). 3 notes = unchanged. Result is clamped 1-127.
    int compensateVelocityForToneCount (int velocity, int toneCount) noexcept;
}
