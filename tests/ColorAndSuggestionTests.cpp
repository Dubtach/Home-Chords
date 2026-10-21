#include "MusicTheory/ColorChords.h"
#include "MusicTheory/Suggestions.h"
#include "MusicTheory/MusicTheoryEngine.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <set>

using namespace musictheory;

namespace
{
    const ColorChord* find (const std::vector<ColorChord>& chords, const char* label)
    {
        for (const auto& c : chords)
            if (c.label == label)
                return &c;
        return nullptr;
    }

    std::set<int> pitchClassesOf (const ColorChord& chord)
    {
        std::set<int> pcs;
        for (int i = 0; i < chord.shape.toneCount; ++i)
            pcs.insert ((chord.shape.rootPitchClass + chord.shape.semitoneOffsets[static_cast<size_t> (i)]) % 12);
        return pcs;
    }
}

TEST_CASE ("Scales are classified minor- or major-sounding by their third", "[color]")
{
    CHECK_FALSE (isMinorSounding (ScaleType::Major));
    CHECK_FALSE (isMinorSounding (ScaleType::Lydian));
    CHECK_FALSE (isMinorSounding (ScaleType::Mixolydian));
    CHECK_FALSE (isMinorSounding (ScaleType::MajorPentatonic));

    CHECK (isMinorSounding (ScaleType::NaturalMinor));
    CHECK (isMinorSounding (ScaleType::HarmonicMinor));
    CHECK (isMinorSounding (ScaleType::Dorian));
    CHECK (isMinorSounding (ScaleType::Phrygian));
    CHECK (isMinorSounding (ScaleType::Locrian));
}

TEST_CASE ("C Major borrowed chords are the textbook set", "[color]")
{
    const auto chords = buildColorChords (0, ScaleType::Major);

    const auto check = [&] (const char* label, const char* name, std::set<int> pcs)
    {
        INFO (label);
        const auto* c = find (chords, label);
        REQUIRE (c != nullptr);
        CHECK (c->chordName.toStdString() == name);
        CHECK (pitchClassesOf (*c) == pcs);
    };

    // C = 0, so pitch classes are plain semitone numbers.
    check ("bIII", "Eb",  { 3, 7, 10 });      // Eb G Bb
    check ("iv",   "Fm",  { 5, 8, 0 });       // F Ab C
    check ("bVI",  "Ab",  { 8, 0, 3 });       // Ab C Eb
    check ("bVII", "Bb",  { 10, 2, 5 });      // Bb D F
    check ("bII",  "Db",  { 1, 5, 8 });       // Db F Ab
    check ("V/V",  "D7",  { 2, 6, 9, 0 });    // D F# A C
    check ("V/vi", "E7",  { 4, 8, 11, 2 });   // E G# B D
    check ("V/ii", "A7",  { 9, 1, 4, 7 });    // A C# E G
    check ("I7",   "C7",  { 0, 4, 7, 10 });   // C E G Bb
}

TEST_CASE ("A minor gets the parallel-major / dominant palette", "[color]")
{
    const auto chords = buildColorChords (9, ScaleType::NaturalMinor);   // A minor

    const auto check = [&] (const char* label, const char* name, std::set<int> pcs)
    {
        INFO (label);
        const auto* c = find (chords, label);
        REQUIRE (c != nullptr);
        CHECK (c->chordName.toStdString() == name);
        CHECK (pitchClassesOf (*c) == pcs);
    };

    check ("I",     "A",     { 9, 1, 4 });          // A C# E (Picardy)
    check ("IV",    "D",     { 2, 6, 9 });          // D F# A
    check ("V",     "E",     { 4, 8, 11 });         // E G# B
    check ("V7",    "E7",    { 4, 8, 11, 2 });      // E G# B D
    check ("bII",   "Bb",    { 10, 2, 5 });         // Bb D F (Neapolitan)
    check ("viio7", "G#dim7", { 8, 11, 2, 5 });     // G# B D F

    // Flat/sharp spelling follows the key by default (A minor -> sharps),
    // and the override wins.
    CHECK (find (chords, "bII")->chordName.toStdString() == "Bb");
    const auto sharps = buildColorChords (9, ScaleType::NaturalMinor, NoteSpelling::Sharps);
    CHECK (find (sharps, "bII")->chordName.toStdString() == "A#");
}

TEST_CASE ("Colour chords follow the key and are not diatonic duplicates", "[color]")
{
    // Every key: roots are the right offsets from the tonic, and none of
    // the major-key colour chords is a plain diatonic triad of that key.
    for (int tonic = 0; tonic < 12; ++tonic)
    {
        const auto chords = buildColorChords (tonic, ScaleType::Major);
        const auto diatonic = buildDiatonicChords (tonic, ScaleType::Major);

        REQUIRE (chords.size() == 9);
        CHECK (find (chords, "bVII")->rootPitchClass == (tonic + 10) % 12);
        CHECK (find (chords, "iv")->rootPitchClass == (tonic + 5) % 12);

        for (const auto& c : chords)
        {
            // A "colour" chord must differ from the diatonic chord on the same root.
            for (const auto& d : diatonic)
                if (d.rootPitchClass == c.rootPitchClass)
                    CHECK ((d.quality != c.quality || c.extension != Extension::None));
        }
    }
}

TEST_CASE ("Suggestions: tendencies are sensible and well-formed", "[suggestions]")
{
    SECTION ("I leads to IV, V, vi")
    {
        const auto s = suggestNext (0, ScaleType::Major);
        REQUIRE (s.size() == 3);
        CHECK (s[0].scaleDegree == 3);
        CHECK (s[1].scaleDegree == 4);
        CHECK (s[2].scaleDegree == 5);
    }

    SECTION ("V resolves to I first")
    {
        CHECK (suggestNext (4, ScaleType::Major)[0].scaleDegree == 0);
        CHECK (suggestNext (4, ScaleType::NaturalMinor)[0].scaleDegree == 0);
    }

    SECTION ("ii goes to V first")
    {
        CHECK (suggestNext (1, ScaleType::Major)[0].scaleDegree == 4);
    }

    SECTION ("minor keys use the minor table: VI climbs to VII")
    {
        CHECK (suggestNext (5, ScaleType::NaturalMinor)[0].scaleDegree == 6);
        CHECK (suggestNext (5, ScaleType::Dorian)[0].scaleDegree == 6);
    }

    SECTION ("out of range yields nothing")
    {
        CHECK (suggestNext (-1, ScaleType::Major).empty());
        CHECK (suggestNext (7, ScaleType::Major).empty());
        CHECK (suggestNext (5, ScaleType::MajorPentatonic).empty());   // pentatonic has 5 degrees (0-4)
    }
}

TEST_CASE ("Suggestions stay inside the scale for every scale and degree", "[suggestions]")
{
    for (int s = 0; s < numScaleTypes; ++s)
    {
        const auto scale = static_cast<ScaleType> (s);
        const int degrees = getDegreeCount (scale);

        for (int from = 0; from < degrees; ++from)
        {
            const auto suggestions = suggestNext (from, scale);
            INFO ("scale " << s << " degree " << from);

            CHECK_FALSE (suggestions.empty());
            CHECK (suggestions.size() <= 3);

            std::set<int> seen;
            for (size_t i = 0; i < suggestions.size(); ++i)
            {
                CHECK (suggestions[i].scaleDegree >= 0);
                CHECK (suggestions[i].scaleDegree < degrees);
                CHECK (suggestions[i].rank == static_cast<int> (i) + 1);
                CHECK (suggestions[i].reason.isNotEmpty());
                CHECK (seen.insert (suggestions[i].scaleDegree).second);   // no duplicates
            }
        }
    }
}
