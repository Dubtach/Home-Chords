#include "ProgressionModel.h"
#include <algorithm>
#include <utility>   // std::move

namespace progression
{
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
        events.push_back (std::move (event));
        sortEvents();
        return id;
    }

    bool ProgressionModel::removeEvent (const juce::Uuid& id)
    {
        const auto it = std::find_if (events.begin(), events.end(),
                                       [&id] (const ChordEvent& e) { return e.id == id; });

        if (it == events.end())
            return false;

        events.erase (it);
        return true;
    }

    bool ProgressionModel::moveEvent (const juce::Uuid& id, double newStartBeats)
    {
        const auto index = indexOfEvent (id);
        if (index < 0)
            return false;

        events[static_cast<size_t> (index)].startBeats = juce::jmax (0.0, newStartBeats);
        sortEvents();
        return true;
    }

    bool ProgressionModel::resizeEvent (const juce::Uuid& id, double newLengthBeats)
    {
        const auto index = indexOfEvent (id);
        if (index < 0)
            return false;

        // Floor at a 1/16 note (at 4/4, a quarter note = 1 beat) so a drag
        // can never collapse an event to zero or negative length.
        events[static_cast<size_t> (index)].lengthBeats = juce::jmax (0.0625, newLengthBeats);
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
        events.clear();
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
