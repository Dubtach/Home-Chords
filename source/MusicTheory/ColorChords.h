#pragma once

#include "Chord.h"
#include "Scale.h"
#include <vector>

// =============================================================================
// Borrowed ("modal interchange") and colour chords -- the handful of
// non-diatonic chords that songwriters reach for most, laid out relative to
// the current Key/Scale.
//
// In a major-sounding key this offers chords borrowed from the parallel minor
// (iv, bIII, bVI, bVII), the Neapolitan (bII) and secondary dominants; in a
// minor-sounding key it offers the parallel major's I / IV / V (plus the
// dominant 7th that makes the minor key's cadence work), the Neapolitan and
// the leading-tone diminished 7th. Which side applies is read from the
// scale's own third, so Dorian/Phrygian/Locrian count as minor and Lydian/
// Mixolydian as major.
//
// Pure data + a builder: no UI, no audio, fully unit-testable.
// =============================================================================

namespace musictheory
{
    struct ColorChord
    {
        juce::String label;        // roman numeral in the current key, e.g. "bVII"
        juce::String rootName;     // e.g. "Bb" -- just the note
        juce::String chordName;    // e.g. "Bb" in C major (note + quality/extension suffix)
        juce::String description;  // one line on why you'd use it
        int rootPitchClass = 0;
        ChordQuality quality = ChordQuality::Major;
        Extension extension = Extension::None;
        RtChordShape shape;
    };

    // True if the scale's third is minor (so borrowing comes from the parallel major).
    bool isMinorSounding (ScaleType scale) noexcept;

    // How many colour chords the palette has for this scale (9 on the major
    // side, 7 on the minor side). Real-time safe.
    int numColorChords (ScaleType scale) noexcept;

    // The shape of colour chord `index` (0-based, same order as
    // buildColorChords) in this key. Real-time safe: no allocation, no
    // strings -- this is what lets a chord card hold a borrowed chord and
    // still be played from the audio thread. Out-of-range -> root-position
    // tonic triad.
    RtChordShape colorChordShape (int index, int tonicPitchClass, ScaleType scale) noexcept;

    // Builds the colour-chord palette for this key. Spelling follows `spelling`
    // exactly like buildDiatonicChords (Auto = the key's natural spelling).
    std::vector<ColorChord> buildColorChords (int tonicPitchClass, ScaleType scale,
                                              NoteSpelling spelling = NoteSpelling::Auto);
}
