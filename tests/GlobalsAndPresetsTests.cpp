#include "MusicTheory/ColorChords.h"
#include "MusicTheory/Presets.h"
#include "Progression/PlaybackSettings.h"
#include "Progression/ProgressionModel.h"
#include <catch2/catch_test_macros.hpp>
#include <set>

using namespace musictheory;

TEST_CASE ("The real-time colour-chord lookup matches the display palette", "[colour][rt]")
{
    for (int tonic = 0; tonic < 12; ++tonic)
    {
        for (int s = 0; s < numScaleTypes; ++s)
        {
            const auto scale = static_cast<ScaleType> (s);
            const auto palette = buildColorChords (tonic, scale);

            REQUIRE (numColorChords (scale) == static_cast<int> (palette.size()));

            for (int i = 0; i < static_cast<int> (palette.size()); ++i)
            {
                const auto shape = colorChordShape (i, tonic, scale);
                const auto& expected = palette[static_cast<size_t> (i)].shape;

                CHECK (shape.rootPitchClass == expected.rootPitchClass);
                CHECK (shape.toneCount == expected.toneCount);
                CHECK (shape.semitoneOffsets == expected.semitoneOffsets);
                CHECK (palette[static_cast<size_t> (i)].rootName.isNotEmpty());
                CHECK (palette[static_cast<size_t> (i)].chordName.startsWith (palette[static_cast<size_t> (i)].rootName));
            }
        }
    }

    SECTION ("out-of-range index falls back to the tonic triad")
    {
        const auto shape = colorChordShape (99, 5, ScaleType::Major);
        CHECK (shape.rootPitchClass == 5);
        CHECK (shape.toneCount == 3);
    }
}

TEST_CASE ("There are plenty of presets, in sensible groups", "[presets]")
{
    const auto& presets = getAllPresets();
    CHECK (presets.size() >= 25);

    std::set<std::string> names;
    std::set<std::string> categories;

    for (const auto& p : presets)
    {
        CHECK (p.name.isNotEmpty());
        CHECK (p.category.isNotEmpty());
        CHECK (! p.scaleDegrees.empty());
        if (! p.extensions.empty())
            CHECK (p.extensions.size() == p.scaleDegrees.size());

        names.insert (p.name.toStdString());
        categories.insert (p.category.toStdString());
    }

    CHECK (names.size() == presets.size());   // no duplicate names
    CHECK (categories.size() >= 4);
}

TEST_CASE ("Every preset builds valid, gap-free progressions wherever it is offered", "[presets]")
{
    for (const auto& preset : getAllPresets())
    {
        for (int tonic = 0; tonic < 12; ++tonic)
        {
            for (int s = 0; s < numScaleTypes; ++s)
            {
                const auto scale = static_cast<ScaleType> (s);

                if (! presetSuitsScale (preset, scale))
                    continue;

                const auto model = buildProgressionFromPreset (preset, tonic, scale, 4.0);

                // Short scales (pentatonics) may drop out-of-range degrees, but never produce nothing.
                CHECK (model.size() > 0);

                for (int i = 0; i + 1 < model.size(); ++i)
                    CHECK (model.getEvent (i).endBeats() == model.getEvent (i + 1).startBeats);
            }
        }
    }
}

TEST_CASE ("Major keys get major presets, minor keys get minor ones", "[presets]")
{
    CHECK (presetSideForScale (ScaleType::Major) == PresetSide::Major);
    CHECK (presetSideForScale (ScaleType::Mixolydian) == PresetSide::Major);
    CHECK (presetSideForScale (ScaleType::NaturalMinor) == PresetSide::Minor);
    CHECK (presetSideForScale (ScaleType::Dorian) == PresetSide::Minor);

    int majorOffered = 0, minorOffered = 0;
    for (const auto& p : getAllPresets())
    {
        majorOffered += presetSuitsScale (p, ScaleType::Major) ? 1 : 0;
        minorOffered += presetSuitsScale (p, ScaleType::NaturalMinor) ? 1 : 0;
    }

    CHECK (majorOffered >= 15);
    CHECK (minorOffered >= 8);
}

TEST_CASE ("Jazz presets carry their seventh chords", "[presets]")
{
    const PresetDefinition* iiVI = nullptr;

    for (const auto& p : getAllPresets())
        if (p.name.startsWith ("ii7 - V7"))
            iiVI = &p;

    REQUIRE (iiVI != nullptr);

    const auto model = buildProgressionFromPreset (*iiVI, 0, ScaleType::Major, 4.0);
    REQUIRE (model.size() == 3);
    CHECK (model.getEvent (0).rootPitchClass == 2);    // Dm7
    CHECK (model.getEvent (0).extension == Extension::MinorSeventh);
    CHECK (model.getEvent (1).rootPitchClass == 7);    // G7
    CHECK (model.getEvent (1).extension == Extension::MinorSeventh);
    CHECK (model.getEvent (2).rootPitchClass == 0);    // Cmaj7
    CHECK (model.getEvent (2).extension == Extension::MajorSeventh);
}

TEST_CASE ("Global playback settings stamp every chord", "[playback]")
{
    using namespace progression;

    SECTION ("choice indices map to values and clamp")
    {
        const auto s = PlaybackSettings::fromChoices (7, 2, 3, 0, 3);
        CHECK (s.rhythm == RhythmPattern::Arpeggio);
        CHECK (s.bassMode == BassMode::Fifth);
        CHECK (s.arpMode == ArpMode::UpDown);
        CHECK (s.arpRateBeats == 1.0);
        CHECK (s.arpGate == 1.0);

        const auto clamped = PlaybackSettings::fromChoices (-4, 99, 99, 99, -1);
        CHECK (clamped.rhythm == RhythmPattern::Sustained);
        CHECK (clamped.bassMode == BassMode::InversionBass);
        CHECK (clamped.arpMode == ArpMode::Random);
        CHECK (clamped.arpRateBeats == 0.125);
        CHECK (clamped.arpGate == 0.25);
    }

    SECTION ("applyTo / matches round trip")
    {
        const auto s = PlaybackSettings::fromChoices (3, 1, 1, 1, 1);
        ChordEvent e;
        CHECK (! s.matches (e));
        s.applyTo (e);
        CHECK (s.matches (e));
        CHECK (e.rhythm == RhythmPattern::Quarter);
        CHECK (e.arpRateBeats == 0.5);
    }
}

TEST_CASE ("findFreeStart keeps dropped chords off existing ones", "[model][drop]")
{
    using namespace progression;

    ProgressionModel model;
    ChordEvent a; a.startBeats = 0.0; a.lengthBeats = 4.0;
    ChordEvent b; b.startBeats = 4.0; b.lengthBeats = 4.0;
    const auto idA = model.addEvent (a);
    model.addEvent (b);

    CHECK (model.findFreeStart (8.0, 4.0) == 8.0);        // free space
    CHECK (model.findFreeStart (1.0, 4.0) == 8.0);        // lands on A and B -> pushed past both
    CHECK (model.findFreeStart (-3.0, 2.0) == 8.0);       // negative clamps to 0, then pushed
    CHECK (model.findFreeStart (4.0, 4.0) == 8.0);
    CHECK (model.findFreeStart (0.0, 4.0, { idA }) == 0.0);   // A is being moved, B starts at 4: 0..4 is free
    CHECK (model.findFreeStart (2.0, 4.0, { idA }) == 8.0);   // 2..6 hits B
}
