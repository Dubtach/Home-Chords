#pragma once

#include "ChordEvent.h"
#include <algorithm>
#include <vector>

// =============================================================================
// The timeline is a "magnetic" track: chords always sit one after another from
// beat 0 -- never overlapping, never with a gap. Every edit (add, remove,
// reorder, resize) is therefore expressed as "change the ordered list, then
// pack it", and these pure functions are the one place that logic lives. The
// editor, the processor and the tests all use them.
// =============================================================================

namespace progression::layout
{
    // Lay the events out back to back from beat 0 in their current order.
    inline void assignContiguous (std::vector<ChordEvent>& events) noexcept
    {
        double cursor = 0.0;

        for (auto& e : events)
        {
            e.startBeats = cursor;
            cursor += e.lengthBeats;
        }
    }

    // Order by start position, then lay out back to back.
    inline void pack (std::vector<ChordEvent>& events)
    {
        std::stable_sort (events.begin(), events.end(),
                          [] (const ChordEvent& a, const ChordEvent& b) { return a.startBeats < b.startBeats; });
        assignContiguous (events);
    }

    inline bool containsId (const std::vector<juce::Uuid>& ids, const juce::Uuid& id)
    {
        return std::find (ids.begin(), ids.end(), id) != ids.end();
    }

    // The events whose ids are in `ids`, in timeline order.
    inline std::vector<ChordEvent> extract (const std::vector<ChordEvent>& events, const std::vector<juce::Uuid>& ids)
    {
        std::vector<ChordEvent> out;

        for (const auto& e : events)
            if (containsId (ids, e.id))
                out.push_back (e);

        return out;
    }

    // Everything except `ids`, closed up so no gap is left behind.
    inline std::vector<ChordEvent> without (const std::vector<ChordEvent>& events, const std::vector<juce::Uuid>& ids)
    {
        std::vector<ChordEvent> out;

        for (const auto& e : events)
            if (! containsId (ids, e.id))
                out.push_back (e);

        assignContiguous (out);
        return out;
    }

    // Where a chord dropped at `beat` belongs among `others` (which must be
    // packed): after every chord whose midpoint lies before the pointer.
    inline int insertionIndex (const std::vector<ChordEvent>& others, double beat) noexcept
    {
        int index = 0;

        for (const auto& e : others)
            if (e.startBeats + e.lengthBeats * 0.5 < beat)
                ++index;

        return index;
    }

    // Insert `items` (kept in order) at `index` and close everything up.
    inline std::vector<ChordEvent> insertAt (std::vector<ChordEvent> events, const std::vector<ChordEvent>& items, int index)
    {
        index = std::max (0, std::min (index, static_cast<int> (events.size())));
        events.insert (events.begin() + index, items.begin(), items.end());
        assignContiguous (events);
        return events;
    }

    // Take the chords in `ids` out and drop them, as a group, at `pointerBeat`
    // (measured against the chords that stay behind).
    inline std::vector<ChordEvent> moveGroup (const std::vector<ChordEvent>& events, const std::vector<juce::Uuid>& ids, double pointerBeat)
    {
        const auto group = extract (events, ids);
        const auto rest = without (events, ids);
        return insertAt (rest, group, insertionIndex (rest, pointerBeat));
    }

    // Shift the group `slots` places earlier (negative) or later (positive) among the other chords.
    inline std::vector<ChordEvent> shiftGroup (const std::vector<ChordEvent>& events, const std::vector<juce::Uuid>& ids, int slots)
    {
        const auto group = extract (events, ids);
        const auto rest = without (events, ids);

        int before = 0;   // how many of the staying chords currently precede the first selected one
        for (const auto& e : events)
        {
            if (containsId (ids, e.id))
                break;
            ++before;
        }

        return insertAt (rest, group, std::max (0, std::min (before + slots, static_cast<int> (rest.size()))));
    }

    // Give each chord in `ids` the length `lengthFor(event)` returns, closing up around it.
    template <typename LengthFn>
    std::vector<ChordEvent> resized (std::vector<ChordEvent> events, const std::vector<juce::Uuid>& ids, LengthFn lengthFor)
    {
        for (auto& e : events)
            if (containsId (ids, e.id))
                e.lengthBeats = lengthFor (e);

        assignContiguous (events);
        return events;
    }

    inline double totalLength (const std::vector<ChordEvent>& events) noexcept
    {
        return events.empty() ? 0.0 : events.back().endBeats();
    }

    // True if the two lists have the same chords in the same places.
    inline bool sameLayout (const std::vector<ChordEvent>& a, const std::vector<ChordEvent>& b) noexcept
    {
        if (a.size() != b.size())
            return false;

        for (size_t i = 0; i < a.size(); ++i)
            if (a[i].id != b[i].id || a[i].startBeats != b[i].startBeats || a[i].lengthBeats != b[i].lengthBeats)
                return false;

        return true;
    }
}
