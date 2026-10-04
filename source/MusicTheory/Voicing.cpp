#include "Voicing.h"
#include "Chord.h"
#include <algorithm>
#include <cmath>

namespace musictheory
{
    int anchorMidiNote (int anchorPitchClass, int rootOctaveOffset) noexcept
    {
        const int reference = 60 + rootOctaveOffset * 12;
        int note = (((anchorPitchClass % 12) + 12) % 12) + 12 * (reference / 12);

        while (note < reference - 6)
            note += 12;
        while (note > reference + 6)
            note -= 12;

        return note;
    }

    int placeRootAscending (int rootPitchClass, int anchorPitchClass, int anchorMidi) noexcept
    {
        const int offsetFromAnchor = ((rootPitchClass - anchorPitchClass) % 12 + 12) % 12;
        return anchorMidi + offsetFromAnchor;
    }

    int chooseInversionNearest (const RtChordShape& rootPositionShape, int rootMidiNote, float referenceCentroid) noexcept
    {
        if (rootPositionShape.toneCount <= 0)
            return 0;

        int bestInversion = 0;
        float bestDistance = 1.0e9f;

        for (int inversion = 0; inversion < rootPositionShape.toneCount; ++inversion)
        {
            const auto candidate = invertShape (rootPositionShape, inversion);

            float centroid = 0.0f;
            for (int i = 0; i < candidate.toneCount; ++i)
                centroid += static_cast<float> (rootMidiNote + candidate.semitoneOffsets[static_cast<size_t> (i)]);
            centroid /= static_cast<float> (candidate.toneCount);

            const float distance = std::abs (centroid - referenceCentroid);

            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestInversion = inversion;
            }
        }

        return bestInversion;
    }

    int voiceShape (const RtChordShape& rootPositionShape, int rootMidiNote, int inversion,
                    std::array<int, maxChordTones>& outNotes) noexcept
    {
        const auto inverted = invertShape (rootPositionShape, inversion);
        const int count = std::min (std::max (inverted.toneCount, 0), maxChordTones);

        for (int i = 0; i < count; ++i)
            outNotes[static_cast<size_t> (i)] = std::clamp (rootMidiNote + inverted.semitoneOffsets[static_cast<size_t> (i)], 0, 127);

        return count;
    }

    int compensateVelocityForToneCount (int velocity, int toneCount) noexcept
    {
        constexpr float referenceToneCount = 3.0f;
        const float scale = std::sqrt (referenceToneCount / static_cast<float> (std::max (1, toneCount)));
        return std::clamp (static_cast<int> (std::lround (static_cast<float> (velocity) * scale)), 1, 127);
    }
}
