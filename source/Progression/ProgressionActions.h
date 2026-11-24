#pragma once

#include "ProgressionModel.h"
#include <juce_data_structures/juce_data_structures.h>   // juce::UndoableAction, juce::UndoManager

// =============================================================================
// juce::UndoableAction wrappers around ProgressionModel's own add/remove/
// replace methods. Three actions cover every kind of timeline edit (add,
// remove, and "change anything about an existing event" -- move, resize,
// quality, rhythm, bass, ...) so there's one place each direction's logic
// lives, rather than a growing family of single-field action classes.
//
// Usage: construct the action, then either call undoManager.perform(new
// ...Action(...)) to run it and push it onto the stack in one step, or use
// TimelineEditor's helper methods, which do exactly that.
// =============================================================================

namespace progression
{
    class AddChordAction : public juce::UndoableAction
    {
    public:
        AddChordAction (ProgressionModel& modelIn, ChordEvent eventIn)
            : model (modelIn), event (std::move (eventIn))
        {
        }

        bool perform() override
        {
            addedId = model.addEvent (event);
            return true;
        }

        bool undo() override
        {
            return model.removeEvent (addedId);
        }

    private:
        ProgressionModel& model;
        ChordEvent event;
        juce::Uuid addedId;
    };

    class RemoveChordAction : public juce::UndoableAction
    {
    public:
        RemoveChordAction (ProgressionModel& modelIn, juce::Uuid idIn)
            : model (modelIn), id (idIn)
        {
        }

        bool perform() override
        {
            const auto index = model.indexOfEvent (id);
            if (index < 0)
                return false;

            removedEvent = model.getEvent (index);
            return model.removeEvent (id);
        }

        bool undo() override
        {
            model.addEvent (removedEvent);   // removedEvent.id is already set, so it's restored under the same id
            return true;
        }

    private:
        ProgressionModel& model;
        juce::Uuid id;
        ChordEvent removedEvent;
    };

    class UpdateChordAction : public juce::UndoableAction
    {
    public:
        UpdateChordAction (ProgressionModel& modelIn, juce::Uuid idIn, ChordEvent newStateIn)
            : model (modelIn), id (idIn), newState (std::move (newStateIn))
        {
        }

        bool perform() override
        {
            const auto index = model.indexOfEvent (id);
            if (index < 0)
                return false;

            oldState = model.getEvent (index);
            return model.replaceEvent (id, newState);
        }

        bool undo() override
        {
            return model.replaceEvent (id, oldState);
        }

    private:
        ProgressionModel& model;
        juce::Uuid id;
        ChordEvent newState, oldState;
    };

    // Replaces the whole event list (and restores it on undo). Every layout
    // edit -- reorder, resize, delete with the gap closing up -- is one of these,
    // so a gesture is always exactly one undo step.
    class SetEventsAction : public juce::UndoableAction
    {
    public:
        SetEventsAction (ProgressionModel& modelIn, std::vector<ChordEvent> newEventsIn)
            : model (modelIn), newEvents (std::move (newEventsIn))
        {
        }

        bool perform() override
        {
            oldEvents = model.getEvents();
            model.setEvents (newEvents);
            return true;
        }

        bool undo() override
        {
            model.setEvents (oldEvents);
            return true;
        }

    private:
        ProgressionModel& model;
        std::vector<ChordEvent> newEvents, oldEvents;
    };

    class ClearProgressionAction : public juce::UndoableAction
    {
    public:
        explicit ClearProgressionAction (ProgressionModel& modelIn) : model (modelIn) {}

        bool perform() override
        {
            savedEvents.clear();
            for (int i = 0; i < model.size(); ++i)
                savedEvents.push_back (model.getEvent (i));

            model.clear();
            return true;
        }

        bool undo() override
        {
            model.clear();
            for (const auto& e : savedEvents)
                model.addEvent (e);
            return true;
        }

    private:
        ProgressionModel& model;
        std::vector<ChordEvent> savedEvents;
    };
}
