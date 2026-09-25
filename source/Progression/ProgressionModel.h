#pragma once

#include "ChordEvent.h"
#include <vector>

// =============================================================================
// An ordered collection of ChordEvents -- the data model the progression
// timeline (spec section 8) will eventually render and edit.
//
// Deliberately minimal for now: no undo/redo, no sections, no loop range
// (spec sections 30, 32, 11) -- those are built on top of this once the
// timeline UI itself exists. What's here (add/remove/move/resize/
// serialize) is fully implemented and unit tested, just not wired into the
// processor's audio path or the editor yet.
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
        juce::Uuid addEvent (ChordEvent event);

        bool removeEvent (const juce::Uuid& id);
        bool moveEvent (const juce::Uuid& id, double newStartBeats);
        bool resizeEvent (const juce::Uuid& id, double newLengthBeats);
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
