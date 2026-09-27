#include "MusicTheory/MusicTheoryEngine.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace musictheory;
using Catch::Matchers::Equals;

namespace
{
    juce::StringArray tonesOf (const RtChordShape& shape, bool useFlats = false)
    {
        return chordToneNames (shape, useFlats);
    }
}

TEST_CASE ("buildOverriddenShape builds named chord types correctly on a fixed root", "[musictheory][override]")
{
    SECTION ("C Major + MinorSeventh = dominant 7th: C E G Bb")
    {
        const auto shape = buildOverriddenShape (0, ChordQuality::Major, Extension::MinorSeventh);
        REQUIRE (shape.toneCount == 4);
        const auto tones = tonesOf (shape, true);
        CHECK_THAT (tones[0].toStdString(), Equals ("C"));
        CHECK_THAT (tones[1].toStdString(), Equals ("E"));
        CHECK_THAT (tones[2].toStdString(), Equals ("G"));
        CHECK_THAT (tones[3].toStdString(), Equals ("Bb"));
    }

    SECTION ("C Minor + MinorSeventh = plain m7: C Eb G Bb")
    {
        const auto shape = buildOverriddenShape (0, ChordQuality::Minor, Extension::MinorSeventh);
        const auto tones = tonesOf (shape, true);
        CHECK_THAT (tones[1].toStdString(), Equals ("Eb"));
        CHECK_THAT (tones[3].toStdString(), Equals ("Bb"));
    }

    SECTION ("C Minor + MajorSeventh = minor-major 7th: C Eb G B")
    {
        const auto shape = buildOverriddenShape (0, ChordQuality::Minor, Extension::MajorSeventh);
        const auto tones = tonesOf (shape, true);
        CHECK_THAT (tones[1].toStdString(), Equals ("Eb"));
        CHECK_THAT (tones[3].toStdString(), Equals ("B"));
    }

    SECTION ("Extension::None keeps a plain triad (toneCount stays 3)")
    {
        const auto shape = buildOverriddenShape (0, ChordQuality::Major, Extension::None);
        CHECK (shape.toneCount == 3);
    }

    SECTION ("Root pitch class is preserved and normalised into 0-11")
    {
        const auto shape = buildOverriddenShape (13 /* out of range on purpose */, ChordQuality::Major, Extension::None);
        CHECK (shape.rootPitchClass == 1);   // 13 mod 12
    }
}

TEST_CASE ("invertShape rotates the lowest N tones up an octave", "[musictheory][inversion]")
{
    const auto cMajor = buildOverriddenShape (0, ChordQuality::Major, Extension::None);   // C E G -> {0,4,7}

    SECTION ("Inversion 0 is root position, unchanged")
    {
        const auto shape = invertShape (cMajor, 0);
        CHECK (shape.semitoneOffsets[0] == 0);
        CHECK (shape.semitoneOffsets[1] == 4);
        CHECK (shape.semitoneOffsets[2] == 7);
    }

    SECTION ("1st inversion: E G C(+12) -> {4, 7, 12}")
    {
        const auto shape = invertShape (cMajor, 1);
        CHECK (shape.semitoneOffsets[0] == 4);
        CHECK (shape.semitoneOffsets[1] == 7);
        CHECK (shape.semitoneOffsets[2] == 12);
    }

    SECTION ("2nd inversion: G C(+12) E(+12) -> {7, 12, 16}")
    {
        const auto shape = invertShape (cMajor, 2);
        CHECK (shape.semitoneOffsets[0] == 7);
        CHECK (shape.semitoneOffsets[1] == 12);
        CHECK (shape.semitoneOffsets[2] == 16);
    }

    SECTION ("Inversion index wraps modulo toneCount (3 == 0 for a triad)")
    {
        const auto wrapped = invertShape (cMajor, 3);
        const auto rootPosition = invertShape (cMajor, 0);
        CHECK (wrapped.semitoneOffsets[0] == rootPosition.semitoneOffsets[0]);
        CHECK (wrapped.semitoneOffsets[1] == rootPosition.semitoneOffsets[1]);
        CHECK (wrapped.semitoneOffsets[2] == rootPosition.semitoneOffsets[2]);
    }

    SECTION ("A 4-note 7th chord has a 3rd inversion (the 7th in the bass)")
    {
        const auto c7 = buildOverriddenShape (0, ChordQuality::Major, Extension::MinorSeventh);   // {0,4,7,10}
        const auto thirdInversion = invertShape (c7, 3);
        CHECK (thirdInversion.semitoneOffsets[0] == 10);   // the 7th, now the lowest tone
        CHECK (thirdInversion.semitoneOffsets[1] == 12);
        CHECK (thirdInversion.semitoneOffsets[2] == 16);
        CHECK (thirdInversion.semitoneOffsets[3] == 19);
    }

    SECTION ("toneCount and rootPitchClass are unchanged by inversion")
    {
        const auto shape = invertShape (cMajor, 2);
        CHECK (shape.toneCount == cMajor.toneCount);
        CHECK (shape.rootPitchClass == cMajor.rootPitchClass);
    }
}

TEST_CASE ("applyOverride (shape) leaves the shape untouched when inactive", "[musictheory][override]")
{
    const auto diatonic = buildDiatonicShapes (0, ScaleType::Major).shapes[1];   // C major's ii = Dm
    SlotOverride inactive;   // qualityOverride == -1, extension == None

    const auto result = applyOverride (diatonic, inactive);

    CHECK (result.rootPitchClass == diatonic.rootPitchClass);
    CHECK (result.toneCount == diatonic.toneCount);
    for (int i = 0; i < diatonic.toneCount; ++i)
        CHECK (result.semitoneOffsets[static_cast<size_t> (i)] == diatonic.semitoneOffsets[static_cast<size_t> (i)]);
}

TEST_CASE ("applyOverride (shape) keeps the diatonic root but replaces quality/extension", "[musictheory][override]")
{
    const auto diatonic = buildDiatonicShapes (0, ScaleType::Major).shapes[1];   // Dm (root D, pitch class 2)

    SECTION ("Quality-only override: Dm becomes D major, same root")
    {
        SlotOverride ov;
        ov.qualityOverride = static_cast<int> (ChordQuality::Major);

        const auto result = applyOverride (diatonic, ov);
        CHECK (result.rootPitchClass == diatonic.rootPitchClass);
        CHECK (result.semitoneOffsets[1] == 4);   // major third, not minor
        CHECK (result.toneCount == 3);            // extension untouched -> still a triad
    }

    SECTION ("Extension-only override: Dm becomes Dm7, quality unchanged")
    {
        SlotOverride ov;
        ov.extension = Extension::MinorSeventh;

        const auto result = applyOverride (diatonic, ov);
        REQUIRE (result.toneCount == 4);
        CHECK (result.semitoneOffsets[1] == 3);    // still minor -- quality wasn't touched
        CHECK (result.semitoneOffsets[3] == 10);   // minor 7th added
    }

    SECTION ("Both overridden at once: Dm becomes D major 7 (Dmaj7)")
    {
        SlotOverride ov;
        ov.qualityOverride = static_cast<int> (ChordQuality::Major);
        ov.extension = Extension::MajorSeventh;

        const auto result = applyOverride (diatonic, ov);
        REQUIRE (result.toneCount == 4);
        CHECK (result.semitoneOffsets[1] == 4);
        CHECK (result.semitoneOffsets[3] == 11);
    }
}

TEST_CASE ("applyOverride (ChordDefinition) updates chordName and romanNumeral consistently", "[musictheory][override]")
{
    const auto chords = buildDiatonicChords (0, ScaleType::Major);
    const auto& diatonicIi = chords[1];   // "ii" / "Dm"

    REQUIRE_THAT (diatonicIi.chordName.toStdString(), Equals ("Dm"));
    REQUIRE_THAT (diatonicIi.romanNumeral.toStdString(), Equals ("ii"));

    SECTION ("Overriding quality to Major flips the roman numeral case too")
    {
        SlotOverride ov;
        ov.qualityOverride = static_cast<int> (ChordQuality::Major);

        const auto result = applyOverride (diatonicIi, ov);
        CHECK_THAT (result.chordName.toStdString(), Equals ("D"));
        CHECK_THAT (result.romanNumeral.toStdString(), Equals ("II"));   // uppercase now -- it's major
        CHECK (result.rootPitchClass == diatonicIi.rootPitchClass);      // root never changes
    }

    SECTION ("Adding a MinorSeventh extension appends '7' to both name and numeral")
    {
        SlotOverride ov;
        ov.extension = Extension::MinorSeventh;

        const auto result = applyOverride (diatonicIi, ov);
        CHECK_THAT (result.chordName.toStdString(), Equals ("Dm7"));
        CHECK_THAT (result.romanNumeral.toStdString(), Equals ("ii7"));
    }

    SECTION ("Inactive override returns the definition unchanged")
    {
        SlotOverride inactive;
        const auto result = applyOverride (diatonicIi, inactive);
        CHECK_THAT (result.chordName.toStdString(), Equals (diatonicIi.chordName.toStdString()));
        CHECK_THAT (result.romanNumeral.toStdString(), Equals (diatonicIi.romanNumeral.toStdString()));
    }
}
