#pragma once

#include "MusicTheoryTypes.h"
#include <juce_core/juce_core.h>

// =============================================================================
// Scale interval tables, plus enharmonic-aware note spelling.
//
// The interval-table side (getIntervals/getDegreeCount) is real-time safe:
// fixed-size arrays, no allocation. The naming side (noteName, scaleName,
// chordQualitySuffix) builds juce::String and is message-thread/UI only.
// =============================================================================

namespace musictheory
{
    // Semitone offsets from the tonic, ascending, NOT including the octave
    // repeat of the tonic. E.g. Major = {0,2,4,5,7,9,11}.
    // Real-time safe: fixed array, no allocation.
    const std::array<int, 7>& getHeptatonicIntervals (ScaleType scale) noexcept;

    // Pentatonic/Blues scales don't have 7 degrees, so they get their own
    // small fixed arrays. getDegreeCount tells you how many entries in
    // whichever table is actually populated are valid.
    const std::array<int, 6>& getShortScaleIntervals (ScaleType scale) noexcept;

    // How many scale degrees this scale actually has (5 for pentatonic, 6
    // for blues, 7 for everything else). This is also how many of the 7
    // keyboard slots end up active for this scale.
    int getDegreeCount (ScaleType scale) noexcept;

    // Fills `outSemitones` (must have room for at least getDegreeCount(scale)
    // entries) with the ascending semitone offsets from the tonic for this
    // scale, regardless of which underlying table it comes from. Real-time
    // safe. Returns the degree count (same as getDegreeCount).
    int getScaleIntervals (ScaleType scale, std::array<int, 8>& outSemitones) noexcept;

    // ---- Message-thread / UI helpers (juce::String, may allocate) --------

    juce::String getScaleName (ScaleType scale);

    // Every scale name, in enum order -- exactly what the Scale combo box
    // is populated from.
    juce::StringArray getAllScaleNames();

    // The 12 chromatic root names, sharp-spelled, in the exact order the
    // spec lists them -- exactly what the Key combo box is populated from.
    juce::StringArray getAllKeyNames();

    // Whether a key built on this tonic conventionally reads with flats
    // (F, Bb, Eb, Ab, Db) rather than sharps. Minor keys and modes are
    // resolved via their relative/parent major so "the notes in Eb Dorian"
    // read the same way "the notes in Eb major" would.
    bool prefersFlats (int tonicPitchClass, ScaleType scale) noexcept;

    // Spells a single pitch class (0-11) as a note name, honouring the
    // sharp/flat preference above.
    juce::String noteName (int pitchClass, bool useFlats);

    juce::String chordQualitySuffix (ChordQuality quality);
    juce::String chordQualityLabel (ChordQuality quality);   // "Major", "Minor", "Diminished"...
}
