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
//
// Presets are grouped by category and tagged with the kind of key they suit
// (major-sounding, minor-sounding, or either), so the editor can offer only
// the ones that make musical sense for the current scale.
// =============================================================================

namespace musictheory
{
    enum class PresetSide
    {
        Any = 0,
        Major,
        Minor
    };

    struct PresetDefinition
    {
        juce::String name;
        juce::String category;               // "Pop", "Jazz", "Minor", ...
        PresetSide side = PresetSide::Any;
        std::vector<int> scaleDegrees;       // 0-based: I=0, ii=1, iii=2, IV=3, V=4, vi=5, vii=6
        std::vector<Extension> extensions;   // optional, parallel to scaleDegrees; empty = plain triads
        double beatsPerChord = 0.0;          // 0 = use the bar length the caller passes in
    };

    // Every preset, grouped by category in menu order.
    const std::vector<PresetDefinition>& getAllPresets();

    // Which side of the major/minor divide a scale sits on (by its own
    // third; the pentatonics and blues are classified by their character).
    PresetSide presetSideForScale (ScaleType scale) noexcept;

    // True if `preset` should be offered for `scale`.
    bool presetSuitsScale (const PresetDefinition& preset, ScaleType scale) noexcept;

    // Builds a fresh ProgressionModel from a preset in the given Key/Scale,
    // one ChordEvent per scale degree, each `beatsPerChord` beats long
    // (unless the preset sets its own), back to back starting at beat 0.
    // Degrees beyond the current scale's degree count (e.g. a preset using
    // vii on a 5-note pentatonic scale) are skipped rather than producing an
    // invalid event.
    progression::ProgressionModel buildProgressionFromPreset (const PresetDefinition& preset, int tonicPitchClass,
                                                               ScaleType scale, double beatsPerChord = 4.0);
}
