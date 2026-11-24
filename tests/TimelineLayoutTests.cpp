#include <catch2/catch_test_macros.hpp>
#include "Progression/ProgressionModel.h"
#include "Progression/ProgressionActions.h"
#include "Progression/PlaybackSettings.h"
#include "Progression/TimelineLayout.h"

using namespace progression;

namespace
{
    std::vector<ChordEvent> makeRow (std::initializer_list<double> lengths)
    {
        std::vector<ChordEvent> events;
        int root = 0;

        for (double length : lengths)
        {
            ChordEvent e;
            e.id = juce::Uuid();
            e.rootPitchClass = root++;
            e.lengthBeats = length;
            events.push_back (e);
        }

        layout::assignContiguous (events);
        return events;
    }

    bool isPacked (const std::vector<ChordEvent>& events)
    {
        double cursor = 0.0;

        for (const auto& e : events)
        {
            if (std::abs (e.startBeats - cursor) > 1.0e-9)
                return false;
            cursor += e.lengthBeats;
        }

        return true;
    }

    std::vector<int> roots (const std::vector<ChordEvent>& events)
    {
        std::vector<int> out;
        for (const auto& e : events)
            out.push_back (e.rootPitchClass);
        return out;
    }
}

TEST_CASE ("pack removes overlaps and gaps, keeping start order", "[layout]")
{
    auto events = makeRow ({ 4, 4, 4 });
    events[0].startBeats = 10.0;   // gap before everything
    events[1].startBeats = 11.0;   // overlaps the first
    events[2].startBeats = 40.0;   // far away

    layout::pack (events);

    CHECK (isPacked (events));
    CHECK (roots (events) == std::vector<int> { 0, 1, 2 });
    CHECK (layout::totalLength (events) == 12.0);
}

TEST_CASE ("removing a middle chord closes the gap", "[layout]")
{
    const auto events = makeRow ({ 4, 2, 4, 2 });
    const auto after = layout::without (events, { events[1].id });

    CHECK (after.size() == 3);
    CHECK (isPacked (after));
    CHECK (after[1].startBeats == 4.0);
    CHECK (layout::totalLength (after) == 10.0);
}

TEST_CASE ("moveGroup reorders by pointer position", "[layout]")
{
    const auto events = makeRow ({ 4, 4, 4, 4 });   // midpoints 2, 6, 10, 14

    SECTION ("drop past the last midpoint puts the chord last")
    {
        const auto moved = layout::moveGroup (events, { events[0].id }, 15.0);
        CHECK (roots (moved) == std::vector<int> { 1, 2, 3, 0 });
        CHECK (isPacked (moved));
    }

    SECTION ("drop before the first midpoint puts the chord first")
    {
        const auto moved = layout::moveGroup (events, { events[3].id }, 1.0);
        CHECK (roots (moved) == std::vector<int> { 3, 0, 1, 2 });
    }

    SECTION ("a non-adjacent selection gathers into one block")
    {
        const auto moved = layout::moveGroup (events, { events[0].id, events[2].id }, 15.0);
        CHECK (roots (moved) == std::vector<int> { 1, 3, 0, 2 });
        CHECK (isPacked (moved));
    }

    SECTION ("dropping where it already is changes nothing")
    {
        const auto moved = layout::moveGroup (events, { events[1].id }, 6.0);
        CHECK (layout::sameLayout (moved, events));
    }
}

TEST_CASE ("shiftGroup moves a chord one place and stops at the ends", "[layout]")
{
    const auto events = makeRow ({ 4, 4, 4 });

    CHECK (roots (layout::shiftGroup (events, { events[0].id }, 1)) == std::vector<int> { 1, 0, 2 });
    CHECK (roots (layout::shiftGroup (events, { events[2].id }, -1)) == std::vector<int> { 0, 2, 1 });
    CHECK (layout::sameLayout (layout::shiftGroup (events, { events[0].id }, -1), events));
    CHECK (layout::sameLayout (layout::shiftGroup (events, { events[2].id }, 1), events));
}

TEST_CASE ("resizing pushes later chords along instead of overlapping them", "[layout]")
{
    const auto events = makeRow ({ 4, 4, 4 });
    const auto grown = layout::resized (events, { events[0].id }, [] (const ChordEvent& e) { return e.lengthBeats + 2.0; });

    CHECK (isPacked (grown));
    CHECK (grown[0].lengthBeats == 6.0);
    CHECK (grown[1].startBeats == 6.0);
    CHECK (layout::totalLength (grown) == 14.0);
}

TEST_CASE ("insertAt makes room and clamps the index", "[layout]")
{
    const auto events = makeRow ({ 4, 4 });
    ChordEvent extra;
    extra.id = juce::Uuid();
    extra.rootPitchClass = 9;
    extra.lengthBeats = 2.0;

    CHECK (roots (layout::insertAt (events, { extra }, 1)) == std::vector<int> { 0, 9, 1 });
    CHECK (roots (layout::insertAt (events, { extra }, 99)) == std::vector<int> { 0, 1, 9 });
    CHECK (roots (layout::insertAt (events, { extra }, -5)) == std::vector<int> { 9, 0, 1 });
    CHECK (isPacked (layout::insertAt (events, { extra }, 1)));
}

TEST_CASE ("SetEventsAction is one undo step", "[layout][undo]")
{
    ProgressionModel model;
    for (const auto& e : makeRow ({ 4, 4, 4 }))
        model.addEvent (e);

    juce::UndoManager undo;
    const auto before = model.getEvents();

    undo.beginNewTransaction ("delete");
    undo.perform (new SetEventsAction (model, layout::without (before, { before[1].id })));

    CHECK (model.size() == 2);
    CHECK (model.getTotalLengthBeats() == 8.0);

    undo.undo();
    CHECK (model.size() == 3);
    CHECK (layout::sameLayout (model.getEvents(), before));

    undo.redo();
    CHECK (model.size() == 2);
}

TEST_CASE ("playback fields round-trip and report mixed values", "[playback]")
{
    ChordEvent e;

    applyField (e, PlaybackField::Rhythm, 7);
    applyField (e, PlaybackField::ArpMode, 3);
    applyField (e, PlaybackField::ArpRate, 1);
    applyField (e, PlaybackField::Gate, 2);
    applyField (e, PlaybackField::Bass, 2);

    CHECK (e.rhythm == RhythmPattern::Arpeggio);
    CHECK (fieldChoice (e, PlaybackField::Rhythm) == 7);
    CHECK (fieldChoice (e, PlaybackField::ArpMode) == 3);
    CHECK (fieldChoice (e, PlaybackField::ArpRate) == 1);
    CHECK (fieldChoice (e, PlaybackField::Gate) == 2);
    CHECK (fieldChoice (e, PlaybackField::Bass) == 2);

    e.bassMode = BassMode::Custom;
    CHECK (fieldChoice (e, PlaybackField::Bass) == -1);   // not offered by the combo

    e.arpRateBeats = 0.3;
    CHECK (fieldChoice (e, PlaybackField::ArpRate) == -1);

    applyField (e, PlaybackField::Rhythm, 99);   // clamped
    CHECK (e.rhythm == RhythmPattern::Arpeggio);
}
