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

    int applyVoicingStyle (const std::array<int, maxChordTones>& closeNotes, int count, int rootMidiNote,
                           VoicingStyle style, std::array<int, maxVoicedNotes>& outNotes) noexcept
    {
        std::array<int, maxVoicedNotes> work {};
        int n = std::min (std::max (count, 0), maxChordTones);

        for (int i = 0; i < n; ++i)
            work[static_cast<size_t> (i)] = closeNotes[static_cast<size_t> (i)];

        const auto addNote = [&] (int note)
        {
            if (n < maxVoicedNotes)
                work[static_cast<size_t> (n++)] = note;
        };

        const bool dropSecondHighest = (style == VoicingStyle::Open || style == VoicingStyle::Wide) && n >= 3;

        if (dropSecondHighest)
            work[static_cast<size_t> (n - 2)] -= 12;

        if (style == VoicingStyle::Wide && n >= 1)
        {
            // Double the lowest tone an octave down (after the drop, so the
            // double sits below the whole voicing).
            const int lowest = *std::min_element (work.begin(), work.begin() + n);
            addNote (lowest - 12);
        }

        if (style == VoicingStyle::Piano && n >= 1)
            addNote (rootMidiNote - 12);

        std::sort (work.begin(), work.begin() + n);

        int out = 0;
        for (int i = 0; i < n; ++i)
        {
            const int note = std::clamp (work[static_cast<size_t> (i)], 0, 127);

            if (out > 0 && outNotes[static_cast<size_t> (out - 1)] == note)
                continue;   // clamping or doubling can land two voices on one note

            outNotes[static_cast<size_t> (out++)] = note;
        }

        return out;
    }

    int compensateVelocityForToneCount (int velocity, int toneCount) noexcept
    {
        constexpr float referenceToneCount = 3.0f;
        const float scale = std::sqrt (referenceToneCount / static_cast<float> (std::max (1, toneCount)));
        return std::clamp (static_cast<int> (std::lround (static_cast<float> (velocity) * scale)), 1, 127);
    }
}
