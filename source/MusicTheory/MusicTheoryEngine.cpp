#include "MusicTheoryEngine.h"

namespace musictheory
{
    DiatonicShapeSet buildDiatonicShapes (int tonicPitchClass, ScaleType scale) noexcept
    {
        DiatonicShapeSet result;

        std::array<int, 8> semitones {};
        const int degreeCount = getScaleIntervals (scale, semitones);
        result.count = degreeCount;   // always <= maxDiatonicSlots (7)

        for (int degree = 0; degree < result.count; ++degree)
        {
            RtChordShape shape = buildTriadShape (semitones, degreeCount, degree);
            shape.rootPitchClass = ((tonicPitchClass + shape.rootPitchClass) % 12 + 12) % 12;
            result.shapes[static_cast<size_t> (degree)] = shape;
        }

        return result;
    }

    std::vector<ChordDefinition> buildDiatonicChords (int tonicPitchClass, ScaleType scale)
    {
        std::vector<ChordDefinition> result;

        std::array<int, 8> semitones {};
        const int degreeCount = getScaleIntervals (scale, semitones);
        const bool useFlats = prefersFlats (tonicPitchClass, scale);

        result.reserve (static_cast<size_t> (degreeCount));

        for (int degree = 0; degree < degreeCount; ++degree)
        {
            RtChordShape shape = buildTriadShape (semitones, degreeCount, degree);
            shape.rootPitchClass = ((tonicPitchClass + shape.rootPitchClass) % 12 + 12) % 12;

            const int lowerInterval = shape.semitoneOffsets[1];
            const int upperInterval = shape.semitoneOffsets[2] - shape.semitoneOffsets[1];
            const auto quality = classifyTriad (lowerInterval, upperInterval);

            ChordDefinition def;
            def.scaleDegree = degree;
            def.rootPitchClass = shape.rootPitchClass;
            def.quality = quality;
            def.romanNumeral = romanNumeralFor (degree, quality);
            def.rootName = noteName (shape.rootPitchClass, useFlats);
            def.chordName = def.rootName + chordQualitySuffix (quality);
            def.shape = shape;

            result.push_back (std::move (def));
        }

        return result;
    }
}
