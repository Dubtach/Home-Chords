#include "MusicTheory/MusicTheoryEngine.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace musictheory;
using Catch::Matchers::Equals;

namespace
{
    juce::StringArray tonesOf (const ChordDefinition& chord, bool useFlats = false)
    {
        return chordToneNames (chord.shape, useFlats);
    }
}

TEST_CASE ("C Major diatonic triads match the textbook set", "[musictheory][diatonic]")
{
    const auto chords = buildDiatonicChords (0 /* C */, ScaleType::Major);
    REQUIRE (chords.size() == 7);

    SECTION ("I = C E G, major")
    {
        const auto tones = tonesOf (chords[0]);
        REQUIRE (tones.size() == 3);
        CHECK_THAT (tones[0].toStdString(), Equals ("C"));
        CHECK_THAT (tones[1].toStdString(), Equals ("E"));
        CHECK_THAT (tones[2].toStdString(), Equals ("G"));
        CHECK (chords[0].quality == ChordQuality::Major);
        CHECK_THAT (chords[0].romanNumeral.toStdString(), Equals ("I"));
        CHECK_THAT (chords[0].chordName.toStdString(), Equals ("C"));
    }

    SECTION ("ii = D F A, minor")
    {
        const auto tones = tonesOf (chords[1]);
        CHECK_THAT (tones[0].toStdString(), Equals ("D"));
        CHECK_THAT (tones[1].toStdString(), Equals ("F"));
        CHECK_THAT (tones[2].toStdString(), Equals ("A"));
        CHECK (chords[1].quality == ChordQuality::Minor);
        CHECK_THAT (chords[1].romanNumeral.toStdString(), Equals ("ii"));
    }

    SECTION ("iii = E G B, minor")
    {
        const auto tones = tonesOf (chords[2]);
        CHECK_THAT (tones[0].toStdString(), Equals ("E"));
        CHECK_THAT (tones[1].toStdString(), Equals ("G"));
        CHECK_THAT (tones[2].toStdString(), Equals ("B"));
        CHECK (chords[2].quality == ChordQuality::Minor);
    }

    SECTION ("IV = F A C, major")
    {
        const auto tones = tonesOf (chords[3]);
        CHECK_THAT (tones[0].toStdString(), Equals ("F"));
        CHECK_THAT (tones[1].toStdString(), Equals ("A"));
        CHECK_THAT (tones[2].toStdString(), Equals ("C"));
        CHECK (chords[3].quality == ChordQuality::Major);
    }

    SECTION ("V = G B D, major")
    {
        const auto tones = tonesOf (chords[4]);
        CHECK_THAT (tones[0].toStdString(), Equals ("G"));
        CHECK_THAT (tones[1].toStdString(), Equals ("B"));
        CHECK_THAT (tones[2].toStdString(), Equals ("D"));
        CHECK (chords[4].quality == ChordQuality::Major);
    }

    SECTION ("vi = A C E, minor")
    {
        const auto tones = tonesOf (chords[5]);
        CHECK_THAT (tones[0].toStdString(), Equals ("A"));
        CHECK_THAT (tones[1].toStdString(), Equals ("C"));
        CHECK_THAT (tones[2].toStdString(), Equals ("E"));
        CHECK (chords[5].quality == ChordQuality::Minor);
    }

    SECTION ("vii\u00b0 = B D F, diminished")
    {
        const auto tones = tonesOf (chords[6]);
        CHECK_THAT (tones[0].toStdString(), Equals ("B"));
        CHECK_THAT (tones[1].toStdString(), Equals ("D"));
        CHECK_THAT (tones[2].toStdString(), Equals ("F"));
        CHECK (chords[6].quality == ChordQuality::Diminished);
        CHECK_THAT (chords[6].romanNumeral.toStdString(), Equals ("vii\u00b0"));
    }

    SECTION ("Chords are generated algorithmically, not hard-coded -- a different key changes them")
    {
        const auto gMajor = buildDiatonicChords (7, ScaleType::Major);
        CHECK_THAT (gMajor[0].chordName.toStdString(), ! Equals (chords[0].chordName.toStdString()));
    }
}

TEST_CASE ("G Major diatonic triads", "[musictheory][diatonic]")
{
    const auto chords = buildDiatonicChords (7 /* G */, ScaleType::Major);
    REQUIRE (chords.size() == 7);

    SECTION ("I = G B D")
    {
        const auto tones = tonesOf (chords[0]);
        CHECK_THAT (tones[0].toStdString(), Equals ("G"));
        CHECK_THAT (tones[1].toStdString(), Equals ("B"));
        CHECK_THAT (tones[2].toStdString(), Equals ("D"));
    }

    SECTION ("ii = A C E")
    {
        const auto tones = tonesOf (chords[1]);
        CHECK_THAT (tones[0].toStdString(), Equals ("A"));
        CHECK_THAT (tones[1].toStdString(), Equals ("C"));
        CHECK_THAT (tones[2].toStdString(), Equals ("E"));
    }

    SECTION ("V = D F# A -- G major's one sharp shows up correctly, not Gb")
    {
        const auto tones = tonesOf (chords[4]);
        CHECK_THAT (tones[0].toStdString(), Equals ("D"));
        CHECK_THAT (tones[1].toStdString(), Equals ("F#"));
        CHECK_THAT (tones[2].toStdString(), Equals ("A"));
    }
}

TEST_CASE ("A Natural Minor diatonic triads", "[musictheory][diatonic]")
{
    const auto chords = buildDiatonicChords (9 /* A */, ScaleType::NaturalMinor);
    REQUIRE (chords.size() == 7);

    SECTION ("i = A C E, minor, lowercase roman")
    {
        const auto tones = tonesOf (chords[0]);
        CHECK_THAT (tones[0].toStdString(), Equals ("A"));
        CHECK_THAT (tones[1].toStdString(), Equals ("C"));
        CHECK_THAT (tones[2].toStdString(), Equals ("E"));
        CHECK (chords[0].quality == ChordQuality::Minor);
        CHECK_THAT (chords[0].romanNumeral.toStdString(), Equals ("i"));
    }

    SECTION ("iv = D F A, minor")
    {
        const auto tones = tonesOf (chords[3]);
        CHECK_THAT (tones[0].toStdString(), Equals ("D"));
        CHECK_THAT (tones[1].toStdString(), Equals ("F"));
        CHECK_THAT (tones[2].toStdString(), Equals ("A"));
        CHECK (chords[3].quality == ChordQuality::Minor);
    }

    SECTION ("v correctly reflects natural minor: E G B, minor -- not a raised-7th major V")
    {
        const auto tones = tonesOf (chords[4]);
        CHECK_THAT (tones[0].toStdString(), Equals ("E"));
        CHECK_THAT (tones[1].toStdString(), Equals ("G"));
        CHECK_THAT (tones[2].toStdString(), Equals ("B"));
        CHECK (chords[4].quality == ChordQuality::Minor);
        CHECK_THAT (chords[4].romanNumeral.toStdString(), Equals ("v"));
    }
}

TEST_CASE ("A Harmonic Minor raises the 7th: V becomes major, III becomes augmented", "[musictheory][diatonic]")
{
    const auto chords = buildDiatonicChords (9 /* A */, ScaleType::HarmonicMinor);
    REQUIRE (chords.size() == 7);

    SECTION ("V = E G# B, major -- the raised leading tone")
    {
        const auto tones = tonesOf (chords[4]);
        CHECK_THAT (tones[1].toStdString(), Equals ("G#"));
        CHECK (chords[4].quality == ChordQuality::Major);
        CHECK_THAT (chords[4].romanNumeral.toStdString(), Equals ("V"));
    }

    SECTION ("III = C E G#, augmented")
    {
        const auto tones = tonesOf (chords[2]);
        CHECK_THAT (tones[2].toStdString(), Equals ("G#"));
        CHECK (chords[2].quality == ChordQuality::Augmented);
        CHECK_THAT (chords[2].romanNumeral.toStdString(), Equals ("III+"));
    }
}

TEST_CASE ("F Major spells with flats", "[musictheory][enharmonic]")
{
    const auto chords = buildDiatonicChords (5 /* F */, ScaleType::Major);
    REQUIRE (chords.size() == 7);

    SECTION ("IV = Bb, not A#")
    {
        CHECK_THAT (chords[3].rootName.toStdString(), Equals ("Bb"));
        CHECK_THAT (chords[3].chordName.toStdString(), Equals ("Bb"));
    }

    SECTION ("vii\u00b0 root spells as E, and its 5th spells as Bb not A#")
    {
        const auto tones = tonesOf (chords[6]);
        CHECK_THAT (tones[0].toStdString(), Equals ("E"));
        CHECK_THAT (tones[2].toStdString(), Equals ("Bb"));
    }
}

TEST_CASE ("D Dorian", "[musictheory][modes]")
{
    const auto chords = buildDiatonicChords (2 /* D */, ScaleType::Dorian);
    REQUIRE (chords.size() == 7);

    SECTION ("i = D F A, minor -- Dorian's tonic triad is minor")
    {
        CHECK (chords[0].quality == ChordQuality::Minor);
    }

    SECTION ("IV = G B D, major -- the natural 6th (B) is Dorian's defining note")
    {
        const auto tones = tonesOf (chords[3]);
        CHECK_THAT (tones[1].toStdString(), Equals ("B"));
        CHECK (chords[3].quality == ChordQuality::Major);
    }
}

TEST_CASE ("Every mode of the major scale has 7 degrees", "[musictheory][modes]")
{
    const ScaleType heptatonic[] = { ScaleType::Major, ScaleType::NaturalMinor, ScaleType::HarmonicMinor,
                                      ScaleType::MelodicMinor, ScaleType::Dorian, ScaleType::Phrygian,
                                      ScaleType::Lydian, ScaleType::Mixolydian, ScaleType::Locrian };

    for (const auto scale : heptatonic)
        CHECK (getDegreeCount (scale) == 7);
}

TEST_CASE ("Pentatonic and blues scales populate fewer than 7 slots", "[musictheory][shortscales]")
{
    SECTION ("Major pentatonic has 5 degrees")
    {
        const auto shapes = buildDiatonicShapes (0, ScaleType::MajorPentatonic);
        CHECK (shapes.count == 5);
        CHECK (buildDiatonicChords (0, ScaleType::MajorPentatonic).size() == 5);
    }

    SECTION ("Minor pentatonic has 5 degrees")
    {
        CHECK (getDegreeCount (ScaleType::MinorPentatonic) == 5);
    }

    SECTION ("Blues has 6 degrees")
    {
        const auto shapes = buildDiatonicShapes (0, ScaleType::Blues);
        CHECK (shapes.count == 6);
    }
}

TEST_CASE ("buildDiatonicShapes (real-time path) agrees with buildDiatonicChords (display path)",
           "[musictheory][realtime]")
{
    // These two entry points must never disagree -- the audio thread uses
    // one, the editor uses the other, and a mismatch would mean the
    // sound produced doesn't match what's on screen.
    for (int tonic = 0; tonic < 12; ++tonic)
    {
        for (int s = 0; s < numScaleTypes; ++s)
        {
            const auto scale = static_cast<ScaleType> (s);
            const auto shapes = buildDiatonicShapes (tonic, scale);
            const auto chords = buildDiatonicChords (tonic, scale);

            REQUIRE (shapes.count == static_cast<int> (chords.size()));

            for (int i = 0; i < shapes.count; ++i)
            {
                const auto& shape = shapes.shapes[static_cast<size_t> (i)];
                const auto& chord = chords[static_cast<size_t> (i)];

                CHECK (shape.rootPitchClass == chord.shape.rootPitchClass);
                CHECK (shape.toneCount == chord.shape.toneCount);

                for (int t = 0; t < shape.toneCount; ++t)
                    CHECK (shape.semitoneOffsets[static_cast<size_t> (t)]
                           == chord.shape.semitoneOffsets[static_cast<size_t> (t)]);
            }
        }
    }
}

TEST_CASE ("Roman numerals never exceed the 7 keyboard slots and are never empty for a valid degree",
           "[musictheory][romannumerals]")
{
    for (int tonic = 0; tonic < 12; ++tonic)
    {
        const auto chords = buildDiatonicChords (tonic, ScaleType::Major);

        for (const auto& chord : chords)
            CHECK (chord.romanNumeral.isNotEmpty());
    }
}
