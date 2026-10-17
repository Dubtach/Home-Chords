#include "Progression/ProgressionActions.h"
#include <catch2/catch_test_macros.hpp>

using namespace progression;

TEST_CASE ("AddChordAction adds on perform and removes on undo", "[progression][undo]")
{
    ProgressionModel model;
    juce::UndoManager undoManager;

    ChordEvent event;
    event.rootPitchClass = 7;
    event.startBeats = 4.0;

    REQUIRE (undoManager.perform (new AddChordAction (model, event)));
    REQUIRE (model.size() == 1);
    CHECK (model.getEvent (0).rootPitchClass == 7);

    REQUIRE (undoManager.undo());
    CHECK (model.isEmpty());

    REQUIRE (undoManager.redo());
    REQUIRE (model.size() == 1);
    CHECK (model.getEvent (0).rootPitchClass == 7);
}

TEST_CASE ("RemoveChordAction removes on perform and restores the exact event on undo", "[progression][undo]")
{
    ProgressionModel model;
    juce::UndoManager undoManager;

    ChordEvent event;
    event.rootPitchClass = 4;
    event.velocity = 77;
    const auto id = model.addEvent (event);   // added directly, not through the undo manager

    REQUIRE (undoManager.perform (new RemoveChordAction (model, id)));
    CHECK (model.isEmpty());

    REQUIRE (undoManager.undo());
    REQUIRE (model.size() == 1);
    CHECK (model.getEvent (0).id == id);
    CHECK (model.getEvent (0).rootPitchClass == 4);
    CHECK (model.getEvent (0).velocity == 77);
}

TEST_CASE ("UpdateChordAction covers move/resize/property changes uniformly, restored exactly on undo", "[progression][undo]")
{
    ProgressionModel model;
    juce::UndoManager undoManager;

    ChordEvent original;
    original.rootPitchClass = 0;
    original.startBeats = 0.0;
    original.lengthBeats = 4.0;
    original.quality = musictheory::ChordQuality::Major;
    const auto id = model.addEvent (original);

    ChordEvent updated = model.getEvent (0);
    updated.startBeats = 8.0;       // a "move"
    updated.lengthBeats = 2.0;      // a "resize"
    updated.quality = musictheory::ChordQuality::Minor;   // a quality edit -- all in one action

    REQUIRE (undoManager.perform (new UpdateChordAction (model, id, updated)));

    const auto afterUpdate = model.getEvent (model.indexOfEvent (id));
    CHECK (afterUpdate.startBeats == 8.0);
    CHECK (afterUpdate.lengthBeats == 2.0);
    CHECK (afterUpdate.quality == musictheory::ChordQuality::Minor);

    REQUIRE (undoManager.undo());
    const auto afterUndo = model.getEvent (model.indexOfEvent (id));
    CHECK (afterUndo.startBeats == 0.0);
    CHECK (afterUndo.lengthBeats == 4.0);
    CHECK (afterUndo.quality == musictheory::ChordQuality::Major);
}

TEST_CASE ("Multiple actions undo in reverse order, redo in forward order", "[progression][undo]")
{
    ProgressionModel model;
    juce::UndoManager undoManager;

    ChordEvent a, b, c;
    a.rootPitchClass = 0;
    b.rootPitchClass = 2;
    c.rootPitchClass = 4;

    // Each user gesture is its own transaction, exactly as the editor does
    // it -- without beginNewTransaction(), juce::UndoManager merges
    // consecutive perform() calls into one undo step.
    undoManager.beginNewTransaction();
    undoManager.perform (new AddChordAction (model, a));
    undoManager.beginNewTransaction();
    undoManager.perform (new AddChordAction (model, b));
    undoManager.beginNewTransaction();
    undoManager.perform (new AddChordAction (model, c));
    REQUIRE (model.size() == 3);

    undoManager.undo();
    CHECK (model.size() == 2);
    undoManager.undo();
    CHECK (model.size() == 1);

    undoManager.redo();
    CHECK (model.size() == 2);
    undoManager.redo();
    CHECK (model.size() == 3);
}

TEST_CASE ("ClearProgressionAction restores every event on undo", "[progression][undo]")
{
    ProgressionModel model;
    juce::UndoManager undoManager;

    ChordEvent a, b;
    a.rootPitchClass = 0;
    a.startBeats = 0.0;
    b.rootPitchClass = 5;
    b.startBeats = 4.0;
    model.addEvent (a);
    model.addEvent (b);

    REQUIRE (undoManager.perform (new ClearProgressionAction (model)));
    CHECK (model.isEmpty());

    REQUIRE (undoManager.undo());
    REQUIRE (model.size() == 2);
    CHECK (model.getEvent (0).rootPitchClass == 0);
    CHECK (model.getEvent (1).rootPitchClass == 5);
}

TEST_CASE ("Actions in one transaction undo together", "[progression][undo]")
{
    // Guards the behaviour applyPreset() relies on (Clear + N adds as a
    // single undo step), and documents why every other edit path must call
    // beginNewTransaction() first.
    ProgressionModel model;
    juce::UndoManager undoManager;

    ChordEvent a, b;
    a.rootPitchClass = 0;
    b.rootPitchClass = 2;

    undoManager.beginNewTransaction();
    undoManager.perform (new AddChordAction (model, a));
    undoManager.perform (new AddChordAction (model, b));
    REQUIRE (model.size() == 2);

    undoManager.undo();
    CHECK (model.size() == 0);

    undoManager.redo();
    CHECK (model.size() == 2);
}
