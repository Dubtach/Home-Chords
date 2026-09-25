#pragma once

#include <array>
#include <cstdint>

// =============================================================================
// Shared vocabulary for the music theory layer.
//
// This header intentionally has zero JUCE dependency and zero allocation
// anywhere in it -- everything here is safe to use from the audio thread.
// String/juce::String-bearing types (chord names, roman numerals) live in
// Chord.h instead, which is message-thread only.
// =============================================================================

namespace musictheory
{
    // Keep in the exact order the spec lists them in, since the Key/Scale
    // combo boxes are built directly from this order.
    enum class ScaleType
    {
        Major = 0,
        NaturalMinor,
        HarmonicMinor,
        MelodicMinor,
        Dorian,
        Phrygian,
        Lydian,
        Mixolydian,
        Locrian,
        MajorPentatonic,
        MinorPentatonic,
        Blues,
        count
    };

    inline constexpr int numScaleTypes = static_cast<int> (ScaleType::count);

    enum class ChordQuality
    {
        Major = 0,
        Minor,
        Diminished,
        Augmented,
        Sus2,
        Sus4,
        Other   // fallback for non-tertian stacks (can occur on some pentatonic/blues degrees)
    };

    // The keyboard only ever drives 7 slots (A S D F G H J). Scales with
    // fewer than 7 degrees (pentatonic, blues) simply leave the remaining
    // slots inactive -- see MusicTheoryEngine::buildDiatonicShapes.
    inline constexpr int maxDiatonicSlots = 7;
    inline constexpr int maxChordTones = 4;

    // A chord, reduced to exactly what the audio thread needs to turn it
    // into MIDI notes: a root pitch class and a fixed set of semitone
    // offsets above that root. No heap, no juce::String -- safe to build
    // fresh every processBlock call.
    struct RtChordShape
    {
        int rootPitchClass = 0;                                  // 0-11, 0 = C
        int toneCount = 0;
        std::array<int, maxChordTones> semitoneOffsets { 0, 0, 0, 0 };   // ascending, offsets from rootPitchClass
    };

    // A fixed-capacity array of shapes for the 7 keyboard slots, plus how
    // many of them are actually in use this scale. Trivially copyable, safe
    // to pass by value or hold on the stack in the audio thread.
    struct DiatonicShapeSet
    {
        std::array<RtChordShape, maxDiatonicSlots> shapes {};
        int count = 0;
    };
}
