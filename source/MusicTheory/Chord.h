#pragma once

#include "MusicTheoryTypes.h"
#include <juce_core/juce_core.h>

// =============================================================================
// Turning "scale degree N" into an actual chord.
//
// The stacking algorithm (buildTriadShape) is generic over scale size: it
// takes every other note of whatever scale it's given, wrapping through
// octaves as needed. For a 7-note scale that's the textbook definition of a
// diatonic triad. For the 5/6-note pentatonic and blues scales it produces
// the same "skip a scale tone" chords those scales are traditionally
// harmonised with -- which don't always land on a plain major/minor/
// diminished triad, hence ChordQuality::Other as an honest fallback rather
// than forcing every result into a bucket that doesn't fit.
// =============================================================================

namespace musictheory
{
    // Classifies a triad from its two stacked intervals (root->middle
    // tone, middle tone->top tone), both in semitones. Real-time safe.
    ChordQuality classifyTriad (int lowerInterval, int upperInterval) noexcept;

    // Stacks a triad starting at `degreeIndex` (0-based) within a scale of
    // `degreeCount` tones described by `semitones` (as filled by
    // Scale::getScaleIntervals). The returned shape's rootPitchClass is
    // relative to the scale's own tonic (0 = the tonic itself) -- the
    // caller adds the actual tonic pitch class. Real-time safe.
    RtChordShape buildTriadShape (const std::array<int, 8>& semitones, int degreeCount, int degreeIndex) noexcept;

    // "I", "ii", "vii\u00b0"... -- case and suffix follow the quality, not
    // just the degree, so a naturally-minor v reads as lowercase even
    // though a major-scale V would not. Message-thread only.
    juce::String romanNumeralFor (int degreeIndex, ChordQuality quality);

    // The actual note name of every tone in a built shape, root first
    // (e.g. {"C", "E", "G"}). `shape.rootPitchClass` must already be the
    // absolute pitch class (tonic added in), as MusicTheoryEngine produces.
    // Message-thread only.
    juce::StringArray chordToneNames (const RtChordShape& shape, bool useFlats);

    // One diatonic degree, fully described for display and export.
    // Message-thread only (owns juce::String members).
    struct ChordDefinition
    {
        int scaleDegree = 0;
        int rootPitchClass = 0;
        ChordQuality quality = ChordQuality::Major;
        juce::String romanNumeral;
        juce::String rootName;
        juce::String chordName;
        RtChordShape shape;
    };
}
