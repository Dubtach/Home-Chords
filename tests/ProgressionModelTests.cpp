#include "Progression/ProgressionModel.h"
#include <catch2/catch_test_macros.hpp>

using namespace progression;
using musictheory::ChordQuality;

TEST_CASE ("ProgressionModel starts empty", "[progression]")
{
    ProgressionModel model;
    CHECK (model.size() == 0);
    CHECK (model.isEmpty());
    CHECK (model.getTotalLengthBeats() == 0.0);
}

TEST_CASE ("addEvent grows the model and returns a usable id", "[progression]")
{
    ProgressionModel model;

    ChordEvent event;
    event.rootPitchClass = 0;
    event.quality = ChordQuality::Major;
    event.startBeats = 0.0;
    event.lengthBeats = 4.0;

    const auto id = model.addEvent (event);

    REQUIRE (model.size() == 1);
    CHECK_FALSE (id.isNull());
    CHECK (model.indexOfEvent (id) == 0);
    CHECK (model.getEvent (0).rootPitchClass == 0);
}

TEST_CASE ("Events are kept sorted by start position regardless of insertion order", "[progression]")
{
    ProgressionModel model;

    ChordEvent third;
    third.startBeats = 8.0;
    third.rootPitchClass = 2;   // D, arbitrary marker to identify the event

    ChordEvent first;
    first.startBeats = 0.0;
    first.rootPitchClass = 0;   // C

    ChordEvent second;
    second.startBeats = 4.0;
    second.rootPitchClass = 7;   // G

    model.addEvent (third);
    model.addEvent (first);
    model.addEvent (second);

    REQUIRE (model.size() == 3);
    CHECK (model.getEvent (0).rootPitchClass == 0);
    CHECK (model.getEvent (1).rootPitchClass == 7);
    CHECK (model.getEvent (2).rootPitchClass == 2);
}

TEST_CASE ("removeEvent removes exactly the matching event and reports success", "[progression]")
{
    ProgressionModel model;

    ChordEvent a, b;
    a.startBeats = 0.0;
    b.startBeats = 4.0;

    const auto idA = model.addEvent (a);
    model.addEvent (b);

    CHECK (model.removeEvent (idA));
    REQUIRE (model.size() == 1);
    CHECK (model.indexOfEvent (idA) == -1);

    CHECK_FALSE (model.removeEvent (idA));            // already gone
    CHECK_FALSE (model.removeEvent (juce::Uuid()));   // never existed
}

TEST_CASE ("moveEvent updates position and re-sorts", "[progression]")
{
    ProgressionModel model;

    ChordEvent a, b;
    a.startBeats = 0.0;
    a.rootPitchClass = 0;
    b.startBeats = 4.0;
    b.rootPitchClass = 7;

    const auto idA = model.addEvent (a);
    model.addEvent (b);

    REQUIRE (model.moveEvent (idA, 10.0));

    CHECK (model.getEvent (0).rootPitchClass == 7);   // b is now first
    CHECK (model.getEvent (1).rootPitchClass == 0);   // a moved to the end
    CHECK (model.getEvent (1).startBeats == 10.0);
}

TEST_CASE ("moveEvent clamps to non-negative start positions", "[progression]")
{
    ProgressionModel model;
    ChordEvent a;
    a.startBeats = 4.0;
    const auto id = model.addEvent (a);

    model.moveEvent (id, -10.0);
    CHECK (model.getEvent (0).startBeats == 0.0);
}

TEST_CASE ("resizeEvent floors length at a 1/16 note and never goes to zero or negative", "[progression]")
{
    ProgressionModel model;
    ChordEvent a;
    a.lengthBeats = 4.0;
    const auto id = model.addEvent (a);

    REQUIRE (model.resizeEvent (id, 2.0));
    CHECK (model.getEvent (0).lengthBeats == 2.0);

    model.resizeEvent (id, -5.0);
    CHECK (model.getEvent (0).lengthBeats > 0.0);
}

TEST_CASE ("getTotalLengthBeats reflects the furthest-reaching event, not insertion order", "[progression]")
{
    ProgressionModel model;

    ChordEvent early;
    early.startBeats = 0.0;
    early.lengthBeats = 2.0;   // ends at beat 2

    ChordEvent longButEarlier;
    longButEarlier.startBeats = 1.0;
    longButEarlier.lengthBeats = 20.0;   // ends at beat 21 -- should win

    model.addEvent (early);
    model.addEvent (longButEarlier);

    CHECK (model.getTotalLengthBeats() == 21.0);
}

TEST_CASE ("clear empties the model", "[progression]")
{
    ProgressionModel model;
    model.addEvent (ChordEvent {});
    model.addEvent (ChordEvent {});
    REQUIRE (model.size() == 2);

    model.clear();
    CHECK (model.size() == 0);
    CHECK (model.isEmpty());
}

TEST_CASE ("ValueTree round-trip preserves every field", "[progression][serialization]")
{
    ProgressionModel model;

    ChordEvent event;
    event.scaleDegree = 4;
    event.rootPitchClass = 7;
    event.quality = ChordQuality::Diminished;
    event.startBeats = 12.5;
    event.lengthBeats = 3.25;
    event.velocity = 87;
    event.octaveOffset = -1;
    event.enabled = false;

    const auto id = model.addEvent (event);

    const auto tree = model.toValueTree();
    const auto restored = ProgressionModel::fromValueTree (tree);

    REQUIRE (restored.size() == 1);
    const auto& r = restored.getEvent (0);

    CHECK (r.id == id);
    CHECK (r.scaleDegree == 4);
    CHECK (r.rootPitchClass == 7);
    CHECK (r.quality == ChordQuality::Diminished);
    CHECK (r.startBeats == 12.5);
    CHECK (r.lengthBeats == 3.25);
    CHECK (r.velocity == 87);
    CHECK (r.octaveOffset == -1);
    CHECK (r.enabled == false);
}

TEST_CASE ("An empty progression round-trips to an empty progression", "[progression][serialization]")
{
    ProgressionModel model;
    const auto restored = ProgressionModel::fromValueTree (model.toValueTree());
    CHECK (restored.isEmpty());
}

TEST_CASE ("fromValueTree on an invalid tree yields an empty model rather than crashing",
           "[progression][serialization]")
{
    const auto restored = ProgressionModel::fromValueTree (juce::ValueTree());
    CHECK (restored.isEmpty());
}
