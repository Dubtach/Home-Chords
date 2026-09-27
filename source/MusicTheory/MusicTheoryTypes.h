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

    // An optional 4th tone added on top of a triad. Deliberately decoupled
    // from ChordQuality (rather than named chord types like "dom7"/"maj7")
    // so any quality can combine with any extension -- e.g. Major+MinorSeventh
    // is a dominant 7th, Minor+MinorSeventh is a plain m7, Minor+MajorSeventh
    // is a minor-major 7th. See Scale.h's extensionSuffix/extensionLabel for
    // how a combination gets named.
    enum class Extension
    {
        None = 0,
        Sixth,          // +9 semitones
        MinorSeventh,   // +10 semitones
        MajorSeventh,   // +11 semitones
        Ninth,          // +14 semitones (an added 9th, no 7th -- "add9", not a full 9th chord)
        count
    };

    inline constexpr int numExtensions = static_cast<int> (Extension::count);

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

    // A user override for one keyboard slot: replace the diatonic quality
    // and/or add a 4th-tone extension, keeping the same root note. Both
    // parts are independent -- overriding just the extension keeps the
    // diatonic quality, overriding just the quality keeps it a plain triad.
    struct SlotOverride
    {
        int qualityOverride = -1;             // -1 = none, else a ChordQuality enum value
        Extension extension = Extension::None;

        bool isActive() const noexcept { return qualityOverride >= 0 || extension != Extension::None; }
    };
}
