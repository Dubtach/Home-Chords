#include "ProgressionModel.h"
#include <algorithm>
#include <utility>   // std::move

namespace progression
{
    ProgressionModel::ProgressionModel (const ProgressionModel& other)
        : events (other.events)
    {
    }

    ProgressionModel& ProgressionModel::operator= (const ProgressionModel& other)
    {
        if (this != &other)
        {
            auto copy = other.events;   // allocate outside the lock
            {
                const Lock::ScopedLockType sl (lock);
                events.swap (copy);
            }   // `copy` (the old events) is freed after the lock is released
            markChanged();
        }

        return *this;
    }

    const ChordEvent& ProgressionModel::getEvent (int index) const
    {
        jassert (index >= 0 && index < size());
        return events[static_cast<size_t> (index)];
    }

    juce::Uuid ProgressionModel::addEvent (ChordEvent event)
    {
        if (event.id.isNull())
            event.id = juce::Uuid();

        const auto id = event.id;

        {
            const Lock::ScopedLockType sl (lock);
            events.push_back (std::move (event));
            sortEvents();
        }

        markChanged();
        return id;
    }

    bool ProgressionModel::removeEvent (const juce::Uuid& id)
    {
        const auto it = std::find_if (events.begin(), events.end(),
                                       [&id] (const ChordEvent& e) { return e.id == id; });

        if (it == events.end())
            return false;

        {
            const Lock::ScopedLockType sl (lock);
            events.erase (it);
        }

        markChanged();
        return true;
    }

    bool ProgressionModel::moveEvent (const juce::Uuid& id, double newStartBeats)
    {
        const auto index = indexOfEvent (id);
        if (index < 0)
            return false;

        {
            const Lock::ScopedLockType sl (lock);
            events[static_cast<size_t> (index)].startBeats = juce::jmax (0.0, newStartBeats);
            sortEvents();
        }

        markChanged();
        return true;
    }

    bool ProgressionModel::resizeEvent (const juce::Uuid& id, double newLengthBeats)
    {
        const auto index = indexOfEvent (id);
        if (index < 0)
            return false;

        // Floor at a 1/16 note (at 4/4, a quarter note = 1 beat) so a drag
        // can never collapse an event to zero or negative length.
        {
            const Lock::ScopedLockType sl (lock);
            events[static_cast<size_t> (index)].lengthBeats = juce::jmax (0.0625, newLengthBeats);
        }

        markChanged();
        return true;
    }

    bool ProgressionModel::replaceEvent (const juce::Uuid& id, ChordEvent newState)
    {
        const auto index = indexOfEvent (id);
        if (index < 0)
            return false;

        newState.id = id;   // never let a caller accidentally reassign the id via this path
        {
            const Lock::ScopedLockType sl (lock);
            events[static_cast<size_t> (index)] = std::move (newState);
            sortEvents();
        }

        markChanged();
        return true;
    }

    int ProgressionModel::indexOfEvent (const juce::Uuid& id) const
    {
        for (int i = 0; i < size(); ++i)
            if (events[static_cast<size_t> (i)].id == id)
                return i;

        return -1;
    }

    void ProgressionModel::clear() noexcept
    {
        {
            const Lock::ScopedLockType sl (lock);
            events.clear();
        }

        markChanged();
    }

    double ProgressionModel::getTotalLengthBeats() const noexcept
    {
        double total = 0.0;
        for (const auto& e : events)
            total = juce::jmax (total, e.endBeats());
        return total;
    }

    void ProgressionModel::sortEvents()
    {
        std::stable_sort (events.begin(), events.end(),
                           [] (const ChordEvent& a, const ChordEvent& b) { return a.startBeats < b.startBeats; });
    }

    juce::ValueTree ProgressionModel::toValueTree() const
    {
        juce::ValueTree tree ("PROGRESSION");

        for (const auto& e : events)
            tree.appendChild (e.toValueTree(), nullptr);

        return tree;
    }

    ProgressionModel ProgressionModel::fromValueTree (const juce::ValueTree& tree)
    {
        ProgressionModel model;

        if (! tree.isValid())
            return model;

        for (int i = 0; i < tree.getNumChildren(); ++i)
            model.events.push_back (ChordEvent::fromValueTree (tree.getChild (i)));

        model.sortEvents();
        return model;
    }
}

namespace progression
{
    double ProgressionModel::findFreeStart (double desiredStart, double length, const std::vector<juce::Uuid>& ignore) const
    {
        double start = desiredStart < 0.0 ? 0.0 : desiredStart;
        const double epsilon = 1.0e-6;

        // Events are sorted by start, so one forward pass that pushes the
        // candidate past every event it overlaps settles it.
        for (const auto& event : events)
        {
            if (std::find (ignore.begin(), ignore.end(), event.id) != ignore.end())
                continue;

            const bool overlaps = start < event.endBeats() - epsilon && start + length > event.startBeats + epsilon;

            if (overlaps)
                start = event.endBeats();
        }

        return start;
    }
}
