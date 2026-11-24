#pragma once

#include "ChordEvent.h"
#include <array>
#include <cmath>

// =============================================================================
// The playback style: Rhythm, Bass, Arp mode/rate and Gate apply to the
// selected chords, or to every chord on the timeline when nothing is selected.
// The values live on each ChordEvent (the sequencer and the MIDI exporter read
// them from there). The parameters hold the default stamped onto new chords
// and are updated whenever the whole timeline is changed at once.
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

    // One of the five playback controls, for changing a single field.
    enum class PlaybackField { Rhythm = 0, Bass, ArpMode, ArpRate, Gate };

    // Writes one control's choice index onto an event.
    inline void applyField (ChordEvent& event, PlaybackField field, int choice) noexcept
    {
        const auto clampIndex = [] (int v, int count) { return v < 0 ? 0 : (v >= count ? count - 1 : v); };

        switch (field)
        {
            case PlaybackField::Rhythm:  event.rhythm = static_cast<RhythmPattern> (clampIndex (choice, numRhythmChoices)); break;
            case PlaybackField::Bass:    event.bassMode = static_cast<BassMode> (clampIndex (choice, numBassChoices)); break;
            case PlaybackField::ArpMode: event.arpMode = static_cast<ArpMode> (clampIndex (choice, numArpModeChoices)); break;
            case PlaybackField::ArpRate: event.arpRateBeats = arpRateBeatsByChoice[static_cast<size_t> (clampIndex (choice, numArpRateChoices))]; break;
            case PlaybackField::Gate:    event.arpGate = gateByChoice[static_cast<size_t> (clampIndex (choice, numGateChoices))]; break;
        }
    }

    // The choice index an event currently has for one control, or -1 when the
    // value isn't one the combo box offers (e.g. a custom bass note).
    inline int fieldChoice (const ChordEvent& event, PlaybackField field) noexcept
    {
        const auto nearest = [] (const auto& table, double value)
        {
            for (size_t i = 0; i < table.size(); ++i)
                if (std::abs (table[i] - value) < 1.0e-6)
                    return static_cast<int> (i);
            return -1;
        };

        switch (field)
        {
            case PlaybackField::Rhythm:  return static_cast<int> (event.rhythm);
            case PlaybackField::Bass:    return static_cast<int> (event.bassMode) < numBassChoices ? static_cast<int> (event.bassMode) : -1;
            case PlaybackField::ArpMode: return static_cast<int> (event.arpMode);
            case PlaybackField::ArpRate: return nearest (arpRateBeatsByChoice, event.arpRateBeats);
            case PlaybackField::Gate:    return nearest (gateByChoice, event.arpGate);
        }

        return -1;
    }

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
