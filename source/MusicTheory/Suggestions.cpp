#include "Suggestions.h"
#include <algorithm>
#include "ColorChords.h"
#include "Scale.h"

namespace musictheory
{
    namespace
    {
        struct Rule
        {
            int to;
            const char* reason;
        };

        // Seven-note scales, major-sounding: I ii iii IV V vi vii.
        const Rule majorRules[7][3] = {
            { { 3, "Leave home for the subdominant" },        { 4, "Build tension toward the dominant" },   { 5, "Slip to the relative minor" } },
            { { 4, "The classic ii - V" },                    { 6, "Leading-tone path back to I" },         { 3, "Subdominant to subdominant" } },
            { { 5, "iii - vi, a falling-fifths chain" },      { 3, "Step up to the subdominant" },          { 1, "On to ii, heading for V" } },
            { { 4, "IV - V, the standard approach" },         { 0, "Plagal return (the 'amen' cadence)" },  { 1, "IV - ii, a softer pre-dominant" } },
            { { 0, "Resolve home" },                          { 5, "Deceptive cadence to vi" },             { 3, "Back-step to IV" } },
            { { 3, "vi - IV, the pop move" },                 { 1, "vi - ii, falling fifths" },             { 4, "vi - V, lift to the dominant" } },
            { { 0, "Resolve up to I" },                       { 2, "Resolve to iii" },                      { 5, "Resolve to vi" } },
        };

        // Seven-note scales, minor-sounding: i ii III iv v VI VII.
        const Rule minorRules[7][3] = {
            { { 3, "Leave home for the subdominant" },        { 4, "Build tension toward the dominant" },   { 5, "Drop to the relative major's neighbour" } },
            { { 4, "ii - v, the minor-key pre-dominant" },    { 6, "On to the subtonic" },                  { 0, "Resolve home" } },
            { { 5, "III - VI, falling fifths" },              { 3, "Step to the subdominant" },             { 6, "III - VII" } },
            { { 4, "iv - v, the standard approach" },         { 0, "Plagal return" },                       { 6, "iv - VII, a modal lift" } },
            { { 0, "Resolve home" },                          { 5, "Deceptive cadence to VI" },             { 3, "Back-step to iv" } },
            { { 6, "VI - VII - i, the epic climb" },          { 3, "VI - iv, a falling step" },             { 4, "VI - v" } },
            { { 2, "VII - III, the 'Andalusian' turn" },      { 0, "Resolve home" },                        { 5, "VII - VI" } },
        };
    }

    std::vector<Suggestion> suggestNext (int fromDegree, ScaleType scale)
    {
        std::vector<Suggestion> result;

        const int degreeCount = getDegreeCount (scale);

        if (fromDegree < 0 || fromDegree >= degreeCount)
            return result;

        if (degreeCount == 7)
        {
            const auto& rules = isMinorSounding (scale) ? minorRules : majorRules;

            for (int i = 0; i < 3; ++i)
            {
                const auto& rule = rules[fromDegree][i];
                result.push_back ({ rule.to, i + 1, rule.reason });
            }

            return result;
        }

        // Fewer-than-seven-note scales (pentatonics, blues): no functional
        // table applies, so fall back to the movements that work in any
        // such scale -- up a step, down a step, and the fifth-ish leap.
        const int steps[3] = { 1, degreeCount - 1, degreeCount / 2 + 1 };
        const char* reasons[3] = { "Step up the scale", "Step back down", "A bigger leap for contrast" };

        for (int i = 0; i < 3; ++i)
        {
            const int target = (fromDegree + steps[i]) % degreeCount;
            const bool duplicate = target == fromDegree
                || std::any_of (result.begin(), result.end(), [target] (const Suggestion& s) { return s.scaleDegree == target; });

            if (! duplicate)
                result.push_back ({ target, static_cast<int> (result.size()) + 1, reasons[i] });
        }

        return result;
    }
}
