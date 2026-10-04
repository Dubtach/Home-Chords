#pragma once

#include "MusicTheoryEngine.h"
#include "../Progression/ProgressionModel.h"
#include <vector>

// =============================================================================
// Named progression templates (spec section 19), applied scale-aware: "I-V-
// vi-IV" in D Major becomes D-A-Bm-G by looking up each roman numeral's
// actual chord in the current Key/Scale, never by transposing fixed note
// names. Message-thread only (builds a ProgressionModel, which owns
// juce::String-bearing/allocating data).
// =============================================================================

namespace musictheory
{
    struct PresetDefinition
    {
        juce::String name;
        std::vector<int> scaleDegrees;   // 0-based: I=0, ii=1, iii=2, IV=3, V=4, vi=5, vii=6
    };

    // The presets named in spec section 19, in that order.
    const std::vector<PresetDefinition>& getAllPresets();

    // Builds a fresh ProgressionModel from a preset in the given Key/Scale,
    // one ChordEvent per scale degree, each `beatsPerChord` beats long,
    // back to back starting at beat 0. Degrees beyond the current scale's
    // degree count (e.g. a preset using vii on a 5-note pentatonic scale)
    // are skipped rather than producing an invalid event.
    progression::ProgressionModel buildProgressionFromPreset (const PresetDefinition& preset, int tonicPitchClass,
                                                               ScaleType scale, double beatsPerChord = 4.0);
}
