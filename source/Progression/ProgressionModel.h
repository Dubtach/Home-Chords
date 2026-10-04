#pragma once

#include "ChordEvent.h"
#include <vector>

// =============================================================================
// An ordered collection of ChordEvents -- the data model the progression
// timeline renders and edits, and what SequencePlayer reads during playback.
//
// This class itself has no undo/redo -- ProgressionActions.h supplies
// juce::UndoableAction wrappers around it (Add/Remove/Update), so undo/redo
// is layered on top rather than woven through every method here. Loop range
// is owned by SequencePlayer (it's playback state, not progression content);
// sections are not implemented.
// =============================================================================

namespace progression
{
    class ProgressionModel
    {
    public:
        int size() const noexcept { return static_cast<int> (events.size()); }
        bool isEmpty() const noexcept { return events.empty(); }
        const ChordEvent& getEvent (int index) const;

        // Returns the new event's id. Events are kept sorted by startBeats.
        // If event.id is already set (non-null), it's kept as-is -- this is
        // what lets undo re-add a removed event under its original id.
        juce::Uuid addEvent (ChordEvent event);

        bool removeEvent (const juce::Uuid& id);
        bool moveEvent (const juce::Uuid& id, double newStartBeats);
        bool resizeEvent (const juce::Uuid& id, double newLengthBeats);

        // Replaces an event's entire state in place (id is preserved
        // regardless of what newState.id says), re-sorting if startBeats
        // changed. The one primitive ProgressionActions' UpdateChordAction
        // needs to cover every kind of edit (move, resize, quality,
        // rhythm, bass, ...) with a single undoable action type.
        bool replaceEvent (const juce::Uuid& id, ChordEvent newState);

        int indexOfEvent (const juce::Uuid& id) const;
        void clear() noexcept;

        double getTotalLengthBeats() const noexcept;

        juce::ValueTree toValueTree() const;
        static ProgressionModel fromValueTree (const juce::ValueTree& tree);

    private:
        std::vector<ChordEvent> events;

        void sortEvents();
    };
}
