#pragma once

#include "Chord.h"
#include "Scale.h"
#include <vector>

// =============================================================================
// The single entry point the rest of the plugin uses for "given this Key
// and Scale, what are the 7 keyboard-slot chords?"
//
// buildDiatonicShapes is the real-time-safe path: no allocation, callable
// every processBlock. buildDiatonicChords is the rich, message-thread path
// used by the editor (allocates juce::String/std::vector freely -- only
// ever called when Key or Scale actually changes, not per frame).
//
// Free functions rather than a class, matching Scale.h/Chord.h: there's no
// instance state here, just Key+Scale in, chords out.
// =============================================================================

namespace musictheory
{
    // Real-time safe. tonicPitchClass is 0-11 (0 = C).
    DiatonicShapeSet buildDiatonicShapes (int tonicPitchClass, ScaleType scale) noexcept;

    // Message-thread only. One ChordDefinition per active scale degree (5
    // for pentatonic scales, 6 for blues, 7 otherwise), in slot order
    // (slot 0 = A, ... slot 6 = J).
    std::vector<ChordDefinition> buildDiatonicChords (int tonicPitchClass, ScaleType scale);

    // Applies a per-slot override to an already-computed diatonic shape,
    // keeping the same root. Returns the shape unchanged if the override
    // isn't active. Real-time safe -- this is what processBlock calls.
    RtChordShape applyOverride (const RtChordShape& diatonicShape, const SlotOverride& slotOverride) noexcept;

    // Same idea for the rich display type: updates quality, chordName, and
    // romanNumeral to reflect the override (rootName/rootPitchClass/
    // scaleDegree stay the same, since an override never changes the
    // root). Message-thread only -- this is what the editor calls.
    ChordDefinition applyOverride (const ChordDefinition& diatonicDefinition, const SlotOverride& slotOverride);
}
