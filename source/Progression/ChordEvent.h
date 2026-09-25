#pragma once

#include "../MusicTheory/MusicTheoryTypes.h"
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>   // juce::ValueTree

// =============================================================================
// One entry in a chord progression: which chord, where, and for how long.
//
// This is Phase 2 groundwork. It's fully implemented and unit tested, but
// nothing in the processor or editor creates, reads, or plays one of these
// yet -- there is no timeline UI in this pass. See STATUS.md.
// =============================================================================

namespace progression
{
    struct ChordEvent
    {
        juce::Uuid id;
        int scaleDegree = 0;
        int rootPitchClass = 0;
        musictheory::ChordQuality quality = musictheory::ChordQuality::Major;
        double startBeats = 0.0;
        double lengthBeats = 4.0;
        int velocity = 100;
        int octaveOffset = 0;
        bool enabled = true;

        double endBeats() const noexcept { return startBeats + lengthBeats; }

        juce::ValueTree toValueTree() const;
        static ChordEvent fromValueTree (const juce::ValueTree& tree);
    };
}
