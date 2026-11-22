#pragma once

#include "ChordEvent.h"
#include <array>

// =============================================================================
// The "global" playback style: Rhythm, Bass, Arp mode/rate and Gate are set
// once for the whole timeline rather than per chord. The values still live on
// each ChordEvent (the sequencer and the MIDI exporter read them from there),
// so changing a global control stamps the new value onto every event, and any
// chord added later is stamped with the current settings.
//
// This header also owns the choice-list <-> value tables, so the editor's
// combo boxes, the processor's parameters and the tests all agree.
// =============================================================================

namespace progression
{
    inline constexpr int numRhythmChoices = 8;
    inline constexpr int numBassChoices = 4;       // Root, Third, Fifth, Inversion (Custom needs a pitch, so it isn't global)
    inline constexpr int numArpModeChoices = 5;
    inline constexpr int numArpRateChoices = 4;
    inline constexpr int numGateChoices = 4;

    inline constexpr std::array<double, numArpRateChoices> arpRateBeatsByChoice { 1.0, 0.5, 0.25, 0.125 };   // 1/4 .. 1/32
    inline constexpr std::array<double, numGateChoices> gateByChoice { 0.25, 0.5, 0.8, 1.0 };                // 25%, 50%, 80%, legato

    struct PlaybackSettings
    {
        RhythmPattern rhythm = RhythmPattern::Sustained;
        BassMode bassMode = BassMode::InversionBass;
        ArpMode arpMode = ArpMode::Off;
        double arpRateBeats = 0.25;
        double arpGate = 0.8;

        // Builds settings from the parameters' choice indices (each clamped).
        static PlaybackSettings fromChoices (int rhythmIndex, int bassIndex, int arpModeIndex, int arpRateIndex, int gateIndex) noexcept
        {
            const auto clampIndex = [] (int v, int count) { return v < 0 ? 0 : (v >= count ? count - 1 : v); };

            PlaybackSettings s;
            s.rhythm = static_cast<RhythmPattern> (clampIndex (rhythmIndex, numRhythmChoices));
            s.bassMode = static_cast<BassMode> (clampIndex (bassIndex, numBassChoices));
            s.arpMode = static_cast<ArpMode> (clampIndex (arpModeIndex, numArpModeChoices));
            s.arpRateBeats = arpRateBeatsByChoice[static_cast<size_t> (clampIndex (arpRateIndex, numArpRateChoices))];
            s.arpGate = gateByChoice[static_cast<size_t> (clampIndex (gateIndex, numGateChoices))];
            return s;
        }

        void applyTo (ChordEvent& event) const noexcept
        {
            event.rhythm = rhythm;
            event.bassMode = bassMode;
            event.arpMode = arpMode;
            event.arpRateBeats = arpRateBeats;
            event.arpGate = arpGate;
        }

        // True if `event` already carries exactly these settings.
        bool matches (const ChordEvent& event) const noexcept
        {
            return event.rhythm == rhythm && event.bassMode == bassMode && event.arpMode == arpMode
                && event.arpRateBeats == arpRateBeats && event.arpGate == arpGate;
        }
    };
}
