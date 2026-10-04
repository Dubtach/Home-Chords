#include "MusicTheory/Voicing.h"
#include "MusicTheory/Presets.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace musictheory;
using Catch::Matchers::Equals;

TEST_CASE ("anchorMidiNote places a pitch class near the reference for its octave offset", "[voicing]")
{
    CHECK (anchorMidiNote (0 /* C */, 0) == 60);

    SECTION ("Octave offset shifts by exactly 12 semitones per step")
    {
        CHECK (anchorMidiNote (0, 1) - anchorMidiNote (0, 0) == 12);
        CHECK (anchorMidiNote (0, -1) - anchorMidiNote (0, 0) == -12);
        CHECK (anchorMidiNote (0, -2) == 36);
    }
}

TEST_CASE ("placeRootAscending never drops below the anchor within one octave", "[voicing]")
{
    // This is the specific bug that was reported and fixed: chords used to
    // independently snap to their own nearest octave, causing roughly half
    // the scale to drop an octave versus the other half.
    const int anchorPc = 0;   // C
    const int anchorMidi = anchorMidiNote (anchorPc, 0);   // 60

    const int pcs[] = { 0, 2, 4, 5, 7, 9, 11 };   // C D E F G A B
    int previous = -1;

    for (const int pc : pcs)
    {
        const int note = placeRootAscending (pc, anchorPc, anchorMidi);
        CHECK (note >= anchorMidi);
        CHECK (note <= anchorMidi + 11);
        CHECK (note > previous);   // strictly ascending -- no octave drop anywhere in the row
        previous = note;
    }
}

TEST_CASE ("voiceShape produces root-position and inverted notes correctly", "[voicing]")
{
    const auto cMajor = buildOverriddenShape (0, ChordQuality::Major, Extension::None);

    SECTION ("Root position: C E G ascending from the given root MIDI note")
    {
        std::array<int, maxChordTones> notes {};
        const int count = voiceShape (cMajor, 60, 0, notes);
        REQUIRE (count == 3);
        CHECK (notes[0] == 60);
        CHECK (notes[1] == 64);
        CHECK (notes[2] == 67);
    }

    SECTION ("1st inversion: E G C(+12)")
    {
        std::array<int, maxChordTones> notes {};
        const int count = voiceShape (cMajor, 60, 1, notes);
        REQUIRE (count == 3);
        CHECK (notes[0] == 64);
        CHECK (notes[1] == 67);
        CHECK (notes[2] == 72);
    }

    SECTION ("Notes are always clamped to the valid MIDI range")
    {
        std::array<int, maxChordTones> notes {};
        const int count = voiceShape (cMajor, 126, 2, notes);   // deliberately near the top of the range
        REQUIRE (count == 3);
        for (int i = 0; i < count; ++i)
        {
            CHECK (notes[i] >= 0);
            CHECK (notes[i] <= 127);
        }
    }
}

TEST_CASE ("compensateVelocityForToneCount", "[voicing]")
{
    SECTION ("3 notes (a plain triad) leaves velocity unchanged")
    {
        CHECK (compensateVelocityForToneCount (100, 3) == 100);
    }

    SECTION ("More notes reduces velocity, never increases it")
    {
        CHECK (compensateVelocityForToneCount (100, 4) < 100);
        CHECK (compensateVelocityForToneCount (100, 4) > 0);
    }

    SECTION ("Result is always clamped to the valid MIDI velocity range")
    {
        CHECK (compensateVelocityForToneCount (127, 1) <= 127);
        CHECK (compensateVelocityForToneCount (1, 4) >= 1);
    }
}

TEST_CASE ("chooseInversionNearest picks the inversion with the closest centroid", "[voicing]")
{
    const auto cMajor = buildOverriddenShape (0, ChordQuality::Major, Extension::None);   // root-position notes at 60,64,67 (centroid 63.67)

    SECTION ("A reference centroid far above should prefer a higher inversion")
    {
        // 2nd inversion (7,12,16 relative -> 67,72,76, centroid ~71.67) is
        // the closest available option to a reference centroid of 72.
        const int inversion = chooseInversionNearest (cMajor, 60, 72.0f);
        CHECK (inversion == 2);
    }

    SECTION ("A reference centroid matching root position picks root position")
    {
        const int inversion = chooseInversionNearest (cMajor, 60, 63.0f);
        CHECK (inversion == 0);
    }
}

TEST_CASE ("Presets apply scale-aware, not as fixed note names", "[presets]")
{
    const auto& presets = getAllPresets();
    REQUIRE (presets.size() >= 5);

    const auto& iVviIV = presets[0];
    REQUIRE_THAT (iVviIV.name.toStdString(), Equals ("I - V - vi - IV"));

    SECTION ("In C Major, I-V-vi-IV is C G Am F")
    {
        const auto model = buildProgressionFromPreset (iVviIV, 0, ScaleType::Major);
        REQUIRE (model.size() == 4);
        CHECK (model.getEvent (0).rootPitchClass == 0);   // C
        CHECK (model.getEvent (0).quality == ChordQuality::Major);
        CHECK (model.getEvent (1).rootPitchClass == 7);   // G
        CHECK (model.getEvent (2).rootPitchClass == 9);   // A
        CHECK (model.getEvent (2).quality == ChordQuality::Minor);
        CHECK (model.getEvent (3).rootPitchClass == 5);   // F
    }

    SECTION ("In D Major, the same preset is D A Bm G -- not transposed note names")
    {
        const auto model = buildProgressionFromPreset (iVviIV, 2 /* D */, ScaleType::Major);
        REQUIRE (model.size() == 4);
        CHECK (model.getEvent (0).rootPitchClass == 2);    // D
        CHECK (model.getEvent (1).rootPitchClass == 9);    // A
        CHECK (model.getEvent (2).rootPitchClass == 11);   // B
        CHECK (model.getEvent (2).quality == ChordQuality::Minor);
        CHECK (model.getEvent (3).rootPitchClass == 7);    // G
    }

    SECTION ("Events are placed back to back with no gaps or overlaps")
    {
        const auto model = buildProgressionFromPreset (iVviIV, 0, ScaleType::Major, 4.0);
        REQUIRE (model.size() == 4);
        for (int i = 0; i < 3; ++i)
            CHECK (model.getEvent (i).endBeats() == model.getEvent (i + 1).startBeats);
    }
}
