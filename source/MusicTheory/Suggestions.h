#pragma once

#include "MusicTheoryTypes.h"
#include <juce_core/juce_core.h>
#include <vector>

// =============================================================================
// "What could come next?" -- a small, explainable functional-harmony guide.
//
// Given the scale degree of the chord just played/placed, returns up to three
// diatonic degrees that commonly follow it, best first, each with a one-line
// reason. It is deliberately a tendency table (tonic -> subdominant/dominant,
// dominant -> tonic, ii -> V, ...), not a model of "correct" harmony: the UI
// presents it as a nudge, never a rule.
//
// Degrees are 0-based indices into the scale's diatonic chords (the same
// numbering as ChordEvent::scaleDegree and the chord-card slots), so the
// result maps directly onto the chord row.
// =============================================================================

namespace musictheory
{
    struct Suggestion
    {
        int scaleDegree = 0;     // 0-based
        int rank = 1;            // 1 = strongest
        juce::String reason;
    };

    // Empty if `fromDegree` is out of range for the scale.
    std::vector<Suggestion> suggestNext (int fromDegree, ScaleType scale);
}
