#include "MusicTheoryEngine.h"
#include <utility>   // std::move

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

    RtChordShape applyOverride (const RtChordShape& diatonicShape, const SlotOverride& slotOverride) noexcept
    {
        if (! slotOverride.isActive())
            return diatonicShape;

        ChordQuality quality;

        if (slotOverride.qualityOverride >= 0)
        {
            quality = static_cast<ChordQuality> (slotOverride.qualityOverride);
        }
        else
        {
            // Extension-only override: keep whatever quality the diatonic
            // triad already had.
            const int lower = diatonicShape.semitoneOffsets[1];
            const int upper = diatonicShape.semitoneOffsets[2] - diatonicShape.semitoneOffsets[1];
            quality = classifyTriad (lower, upper);
        }

        return buildOverriddenShape (diatonicShape.rootPitchClass, quality, slotOverride.extension);
    }

    ChordDefinition applyOverride (const ChordDefinition& diatonicDefinition, const SlotOverride& slotOverride)
    {
        if (! slotOverride.isActive())
            return diatonicDefinition;

        ChordDefinition result = diatonicDefinition;
        const auto quality = slotOverride.qualityOverride >= 0
                                ? static_cast<ChordQuality> (slotOverride.qualityOverride)
                                : diatonicDefinition.quality;

        result.quality = quality;
        result.shape = buildOverriddenShape (diatonicDefinition.rootPitchClass, quality, slotOverride.extension);
        result.chordName = diatonicDefinition.rootName + chordQualitySuffix (quality) + extensionSuffix (slotOverride.extension);
        result.romanNumeral = romanNumeralFor (diatonicDefinition.scaleDegree, quality) + extensionSuffix (slotOverride.extension);

        return result;
    }
}
