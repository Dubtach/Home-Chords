#pragma once

#include "../MusicTheory/MusicTheoryTypes.h"
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>   // juce::ValueTree

// =============================================================================
// One entry in a chord progression: which chord, where, for how long, and
// how it's played back (rhythm, arpeggiator, bass).
// =============================================================================

namespace progression
{
    // How a chord's notes are spread across its duration.
    enum class RhythmPattern
    {
        Sustained = 0,   // one Note On at the start, held for the full duration
        Whole,
        Half,
        Quarter,
        Eighth,
        Sixteenth,
        Strum,           // notes staggered slightly, low to high, then sustained
        Arpeggio         // see ArpMode/arpRate below
    };

    enum class ArpMode
    {
        Off = 0,
        Up,
        Down,
        UpDown,
        Random
    };

    // Which chord tone sits in the bass (the lowest sounding note).
    enum class BassMode
    {
        Root = 0,
        Third,
        Fifth,
        InversionBass,   // whatever note the current inversion already puts lowest -- i.e. don't override it
        Custom           // customBassPitchClass below
    };

    struct ChordEvent
    {
        juce::Uuid id;
        int scaleDegree = 0;
        int rootPitchClass = 0;
        musictheory::ChordQuality quality = musictheory::ChordQuality::Major;
        musictheory::Extension extension = musictheory::Extension::None;
        double startBeats = 0.0;
        double lengthBeats = 4.0;
        int velocity = 100;
        int octaveOffset = 0;
        bool enabled = true;

        RhythmPattern rhythm = RhythmPattern::Sustained;
        ArpMode arpMode = ArpMode::Off;
        double arpRateBeats = 0.25;    // 1/16 note at 0.25 beats/quarter-note-per-beat convention
        double arpGate = 0.8;          // 0-1, fraction of each arp step that's actually held

        BassMode bassMode = BassMode::InversionBass;
        int customBassPitchClass = 0;   // used only when bassMode == Custom

        double endBeats() const noexcept { return startBeats + lengthBeats; }

        juce::ValueTree toValueTree() const;
        static ChordEvent fromValueTree (const juce::ValueTree& tree);
    };
}
