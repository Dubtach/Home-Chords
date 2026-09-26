#include "Chord.h"
#include "Scale.h"   // noteName() -- used by chordToneNames() below

namespace musictheory
{
    ChordQuality classifyTriad (int lowerInterval, int upperInterval) noexcept
    {
        if (lowerInterval == 4 && upperInterval == 3) return ChordQuality::Major;
        if (lowerInterval == 3 && upperInterval == 4) return ChordQuality::Minor;
        if (lowerInterval == 3 && upperInterval == 3) return ChordQuality::Diminished;
        if (lowerInterval == 4 && upperInterval == 4) return ChordQuality::Augmented;
        if (lowerInterval == 2 && upperInterval == 5) return ChordQuality::Sus2;
        if (lowerInterval == 5 && upperInterval == 2) return ChordQuality::Sus4;

        return ChordQuality::Other;
    }

    RtChordShape buildTriadShape (const std::array<int, 8>& semitones, int degreeCount, int degreeIndex) noexcept
    {
        RtChordShape shape;

        if (degreeCount <= 0)
            return shape;

        degreeIndex = ((degreeIndex % degreeCount) + degreeCount) % degreeCount;

        // Every-other-scale-tone stacking, wrapping through as many octaves
        // as needed. For a 7-note scale, +2/+4 steps is exactly "the third"
        // and "the fifth" above the root -- for shorter scales it's the
        // same idea applied to however many tones there are.
        const auto absoluteSemitoneFor = [&semitones, degreeCount] (int stepsFromZero) noexcept -> int
        {
            const int wraps = stepsFromZero / degreeCount;
            const int idx = stepsFromZero % degreeCount;
            return semitones[static_cast<size_t> (idx)] + 12 * wraps;
        };

        const int rootAbs  = absoluteSemitoneFor (degreeIndex);
        const int thirdAbs = absoluteSemitoneFor (degreeIndex + 2);
        const int fifthAbs = absoluteSemitoneFor (degreeIndex + 4);

        shape.rootPitchClass = ((rootAbs % 12) + 12) % 12;
        shape.toneCount = 3;
        shape.semitoneOffsets[0] = 0;
        shape.semitoneOffsets[1] = thirdAbs - rootAbs;
        shape.semitoneOffsets[2] = fifthAbs - rootAbs;
        shape.semitoneOffsets[3] = 0;

        return shape;
    }

    juce::String romanNumeralFor (int degreeIndex, ChordQuality quality)
    {
        static const juce::StringArray bases { "I", "II", "III", "IV", "V", "VI", "VII" };

        if (degreeIndex < 0 || degreeIndex >= bases.size())
            return {};

        const auto base = bases[degreeIndex];
        const auto degreeSign = juce::String::fromUTF8 ("\xC2\xB0");   // U+00B0 DEGREE SIGN

        switch (quality)
        {
            case ChordQuality::Major:      return base;
            case ChordQuality::Minor:      return base.toLowerCase();
            case ChordQuality::Diminished: return base.toLowerCase() + degreeSign;
            case ChordQuality::Augmented:  return base + "+";
            case ChordQuality::Sus2:       return base + "sus2";
            case ChordQuality::Sus4:       return base + "sus4";
            default:                       return base;
        }
    }

    juce::StringArray chordToneNames (const RtChordShape& shape, bool useFlats)
    {
        juce::StringArray names;

        for (int i = 0; i < shape.toneCount && i < maxChordTones; ++i)
        {
            const int pc = ((shape.rootPitchClass + shape.semitoneOffsets[static_cast<size_t> (i)]) % 12 + 12) % 12;
            names.add (noteName (pc, useFlats));
        }

        return names;
    }
}
