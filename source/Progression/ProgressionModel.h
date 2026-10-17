#pragma once

#include "ChordEvent.h"
#include <atomic>
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
//
// Thread safety: the message thread is the only writer; SequencePlayer reads
// the events from the audio thread. Every mutating method therefore takes
// `getLock()`, and the audio thread holds it (via a try-lock -- it skips the
// block's sequencing rather than ever waiting) for the duration of its read.
// The message thread reads without the lock, which is safe because nothing
// else writes. `getRevision()` increments on every change so UI code can
// cheaply detect "the model changed" (undo/redo, state restore, presets)
// without being told.
// =============================================================================

namespace progression
{
    class ProgressionModel
    {
    public:
        ProgressionModel() = default;

        // Copy/assign the events only -- the lock and revision counter are
        // per-instance and deliberately not copied.
        ProgressionModel (const ProgressionModel& other);
        ProgressionModel& operator= (const ProgressionModel& other);

        using Lock = juce::SpinLock;
        const Lock& getLock() const noexcept { return lock; }

        // Bumped by every mutation (including assignment). Safe to read from any thread.
        juce::uint32 getRevision() const noexcept { return revision.load (std::memory_order_relaxed); }

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
        mutable Lock lock;
        std::atomic<juce::uint32> revision { 0 };

        void sortEvents();
        void markChanged() noexcept { revision.fetch_add (1, std::memory_order_relaxed); }
    };
}
