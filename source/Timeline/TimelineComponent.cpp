#include "TimelineComponent.h"
#include "../MusicTheory/Scale.h"
#include <algorithm>
#include <cmath>

namespace timeline
{
    TimelineComponent::TimelineComponent (progression::ProgressionModel& modelIn, SequencePlayer& playerIn,
                                           juce::UndoManager& undoManagerIn)
        : model (modelIn), player (playerIn), undoManager (undoManagerIn)
    {
        setWantsKeyboardFocus (false);
        lastSeenModelRevision = model.getRevision();
        startTimerHz (30);
    }

    TimelineComponent::~TimelineComponent()
    {
        stopTimer();
    }

    // ---- geometry -----------------------------------------------------------

    double TimelineComponent::totalTimelineBeats() const noexcept
    {
        if (scaleLockBeats > 0.0)
            return scaleLockBeats;

        return juce::jmax (16.0, model.getTotalLengthBeats(), player.getLoopEndBeats()) + 2.0;
    }

    float TimelineComponent::pixelsPerBeat() const noexcept
    {
        return juce::jmax (4.0f, static_cast<float> (getWidth()) / static_cast<float> (totalTimelineBeats()));
    }

    double TimelineComponent::xToBeats (int x) const noexcept
    {
        return static_cast<double> (x) / static_cast<double> (pixelsPerBeat());
    }

    int TimelineComponent::beatsToX (double beats) const noexcept
    {
        return static_cast<int> (beats * static_cast<double> (pixelsPerBeat()));
    }

    double TimelineComponent::snap (double beats) const noexcept
    {
        return juce::jmax (0.0, std::round (beats / snapBeats) * snapBeats);
    }

    double TimelineComponent::snapDelta (double beats) const noexcept
    {
        return std::round (beats / snapBeats) * snapBeats;
    }

    bool TimelineComponent::isOutside (juce::Point<int> p) const noexcept
    {
        constexpr int margin = 14;
        return p.y < -margin || p.y > getHeight() + margin || p.x < -margin || p.x > getWidth() + margin;
    }

    juce::Rectangle<float> TimelineComponent::blockBoundsFor (const progression::ChordEvent& event) const noexcept
    {
        const float top = rulerHeight + loopMarkerHeight + 3.0f;
        const float x = static_cast<float> (beatsToX (event.startBeats));
        const float width = juce::jmax (4.0f, static_cast<float> (beatsToX (event.endBeats())) - x);
        const float height = juce::jlimit (10.0f, blockHeightMax, static_cast<float> (getHeight()) - top - 4.0f);
        return { x, top, width, height };
    }

    const progression::ChordEvent* TimelineComponent::eventAt (int x, int y, bool* onRightEdge) const noexcept
    {
        if (y < static_cast<int> (rulerHeight + loopMarkerHeight))
            return nullptr;

        for (int i = model.size() - 1; i >= 0; --i)   // topmost (latest) first
        {
            const auto& event = model.getEvent (i);
            const auto bounds = blockBoundsFor (event);

            if (bounds.contains (static_cast<float> (x), static_cast<float> (y)))
            {
                if (onRightEdge != nullptr)
                    *onRightEdge = (static_cast<float> (x) > bounds.getRight() - 7.0f);

                return &event;
            }
        }

        return nullptr;
    }

    // ---- selection ----------------------------------------------------------

    bool TimelineComponent::isSelected (const juce::Uuid& id) const noexcept
    {
        return std::find (selection.begin(), selection.end(), id) != selection.end();
    }

    void TimelineComponent::setSelection (std::vector<juce::Uuid> newSelection)
    {
        if (newSelection == selection)
            return;

        selection = std::move (newSelection);

        if (onSelectionChanged != nullptr)
            onSelectionChanged();

        repaint();
    }

    void TimelineComponent::toggleInSelection (const juce::Uuid& id)
    {
        auto next = selection;
        const auto it = std::find (next.begin(), next.end(), id);

        if (it != next.end())
            next.erase (it);
        else
            next.push_back (id);

        setSelection (std::move (next));
    }

    void TimelineComponent::selectAll()
    {
        std::vector<juce::Uuid> all;

        for (int i = 0; i < model.size(); ++i)
            all.push_back (model.getEvent (i).id);

        setSelection (std::move (all));
    }

    void TimelineComponent::clearSelection()
    {
        setSelection ({});
    }

    // ---- painting -----------------------------------------------------------

    void TimelineComponent::paint (juce::Graphics& g)
    {
        homeUI::drawWell (g, getLocalBounds().toFloat(), 6.0f);

        const double totalBeats = totalTimelineBeats();
        const float bottom = static_cast<float> (getHeight());
        const float gridTop = rulerHeight + loopMarkerHeight;

        // Bar lines and numbers follow the plugin's time signature.
        for (int bar = 0; static_cast<double> (bar) * beatsPerBar < totalBeats; ++bar)
        {
            const auto x = static_cast<float> (beatsToX (static_cast<double> (bar) * beatsPerBar));
            g.setColour (juce::Colours::white.withAlpha (bar % 4 == 0 ? 0.28f : 0.13f));
            g.drawLine (x, gridTop, x, bottom, bar % 4 == 0 ? 1.2f : 0.7f);

            g.setFont (homeUI::font (8.5f, false));
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.drawText (juce::String (bar + 1), juce::Rectangle<float> (x + 3.0f, loopMarkerHeight, 30.0f, rulerHeight),
                        juce::Justification::centredLeft, false);
        }

        // Loop region shading + handles.
        {
            const auto loopX0 = static_cast<float> (beatsToX (player.getLoopStartBeats()));
            const auto loopX1 = static_cast<float> (beatsToX (player.getLoopEndBeats()));

            g.setColour (homeUI::cyan.withAlpha (0.07f));
            g.fillRect (juce::Rectangle<float> (loopX0, gridTop, loopX1 - loopX0, bottom - gridTop));

            g.setColour (homeUI::cyan.withAlpha (0.8f));
            g.fillRect (juce::Rectangle<float> (loopX0 - 1.5f, 0.0f, 3.0f, loopMarkerHeight));
            g.fillRect (juce::Rectangle<float> (loopX1 - 1.5f, 0.0f, 3.0f, loopMarkerHeight));
        }

        // Chord blocks.
        const bool removing = dragOutside && draggingBody;

        for (int i = 0; i < model.size(); ++i)
        {
            const auto& event = model.getEvent (i);
            const auto bounds = blockBoundsFor (event);

            juce::Colour accent = homeUI::green;
            switch (event.quality)
            {
                case musictheory::ChordQuality::Major:      accent = homeUI::cyan;   break;
                case musictheory::ChordQuality::Minor:      accent = homeUI::purple; break;
                case musictheory::ChordQuality::Diminished: accent = homeUI::warn;   break;
                case musictheory::ChordQuality::Augmented:  accent = homeUI::pink;   break;
                default:                                    accent = homeUI::green;  break;
            }

            const bool isSel = isSelected (event.id);
            const bool doomed = removing && isSel;

            if (doomed)
                accent = homeUI::warn;

            g.saveState();
            if (doomed)
                g.setOpacity (0.55f);

            homeUI::drawCard (g, bounds.reduced (1.5f), isSel ? accent.brighter (0.3f) : accent.darker (event.enabled ? 0.0f : 0.5f));

            if (isSel)
            {
                g.setColour (juce::Colours::white.withAlpha (0.9f));
                g.drawRoundedRectangle (bounds.reduced (1.5f), 6.0f, 2.0f);
            }

            const auto rootName = musictheory::noteName (event.rootPitchClass, useFlats);
            const auto chordName = rootName + musictheory::chordQualitySuffix (event.quality)
                                            + musictheory::extensionSuffix (event.extension);

            auto content = bounds.reduced (5.0f, 3.0f);
            homeUI::drawCardText (g, chordName, content, juce::jmin (15.0f, content.getHeight() * 0.5f));
            g.restoreState();
        }

        // Card-drop preview: a ghost block where the chord would land.
        if (dropBeat >= 0.0)
        {
            const double start = model.findFreeStart (dropBeat, dropLength);
            progression::ChordEvent ghost;
            ghost.startBeats = start;
            ghost.lengthBeats = dropLength;

            const auto bounds = blockBoundsFor (ghost).reduced (1.5f);
            g.setColour (juce::Colours::white.withAlpha (0.14f));
            g.fillRoundedRectangle (bounds, 6.0f);

            juce::Path outline;
            outline.addRoundedRectangle (bounds, 6.0f);
            juce::Path dashed;
            const float dashes[] = { 5.0f, 4.0f };
            juce::PathStrokeType (1.5f).createDashedStroke (dashed, outline, dashes, 2);
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.fillPath (dashed);
        }

        // Playhead.
        if (player.isPlaying())
        {
            const auto x = static_cast<float> (beatsToX (player.getPositionBeats()));
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawLine (x, loopMarkerHeight, x, bottom, 2.0f);
        }

        // Rubber band.
        if (marqueeActive && marqueeMoved)
        {
            const auto r = marqueeRect.toFloat();
            g.setColour (homeUI::cyan.withAlpha (0.12f));
            g.fillRect (r);
            g.setColour (homeUI::cyan.withAlpha (0.85f));
            g.drawRect (r, 1.0f);
        }

        if (removing)
        {
            const auto pill = juce::Rectangle<float> (0.0f, 0.0f, 132.0f, 20.0f).withCentre (getLocalBounds().toFloat().getCentre());
            g.setColour (juce::Colours::black.withAlpha (0.78f));
            g.fillRoundedRectangle (pill, 10.0f);
            g.setColour (homeUI::warn);
            g.drawRoundedRectangle (pill, 10.0f, 1.0f);
            g.setFont (homeUI::font (11.0f));
            g.drawText ("Release to remove", pill, juce::Justification::centred, false);
        }
        else if (model.isEmpty() && dropBeat < 0.0)
        {
            g.setFont (homeUI::font (11.0f, false));
            g.setColour (juce::Colours::white.withAlpha (0.38f));
            g.drawText ("Drag chords here from the cards above   (double-click a card to append it)",
                        getLocalBounds(), juce::Justification::centred, false);
        }
    }

    // ---- mouse --------------------------------------------------------------

    void TimelineComponent::mouseDown (const juce::MouseEvent& event)
    {
        const int x = event.x;
        const int y = event.y;

        // A click here shouldn't leave the chord keys dead after a combo box had focus.
        if (auto* parent = getParentComponent())
            parent->grabKeyboardFocus();

        if (event.mods.isPopupMenu())
        {
            showContextMenu (event);
            return;
        }

        const auto loopX0 = static_cast<float> (beatsToX (player.getLoopStartBeats()));
        const auto loopX1 = static_cast<float> (beatsToX (player.getLoopEndBeats()));

        if (y < static_cast<int> (loopMarkerHeight + 4.0f))
        {
            if (std::abs (static_cast<float> (x) - loopX0) < 7.0f)
            {
                draggingLoopStart = true;
                return;
            }

            if (std::abs (static_cast<float> (x) - loopX1) < 7.0f)
            {
                draggingLoopEnd = true;
                return;
            }
        }

        bool onRightEdge = false;
        const auto* found = eventAt (x, y, &onRightEdge);
        const bool additive = event.mods.isShiftDown() || event.mods.isCommandDown() || event.mods.isCtrlDown();

        if (found != nullptr)
        {
            const auto id = found->id;

            if (additive)
            {
                toggleInSelection (id);

                if (! isSelected (id))
                    return;   // it was just removed from the selection: nothing to drag
            }
            else if (! isSelected (id))
            {
                setSelection ({ id });
            }

            dragItems.clear();

            for (const auto& selectedId : selection)
            {
                const auto index = model.indexOfEvent (selectedId);

                if (index >= 0)
                {
                    const auto& e = model.getEvent (index);
                    dragItems.push_back ({ e.id, e.startBeats, e.lengthBeats });
                }
            }

            dragStartMouseX = x;
            dragDeltaBeats = 0.0;
            dragOutside = false;
            draggingRightEdge = onRightEdge;
            draggingBody = ! onRightEdge;
            scaleLockBeats = totalTimelineBeats();
            interacting = true;
        }
        else if (y >= static_cast<int> (loopMarkerHeight))
        {
            selectionBeforeMarquee = additive ? selection : std::vector<juce::Uuid>();

            if (! additive)
                setSelection ({});

            marqueeActive = true;
            marqueeMoved = false;
            marqueeStart = { x, y };
            marqueeRect = {};
            scaleLockBeats = totalTimelineBeats();
            interacting = true;
        }

        repaint();
    }

    void TimelineComponent::mouseDrag (const juce::MouseEvent& event)
    {
        if (draggingLoopStart)
        {
            const double beats = juce::jmin (snap (xToBeats (event.x)), player.getLoopEndBeats() - snapBeats);
            player.setLoopRange (juce::jmax (0.0, beats), player.getLoopEndBeats());
            repaint();
            return;
        }

        if (draggingLoopEnd)
        {
            const double minEnd = player.getLoopStartBeats() + snapBeats;
            player.setLoopRange (player.getLoopStartBeats(), juce::jmax (minEnd, snap (xToBeats (event.x))));
            repaint();
            return;
        }

        if (marqueeActive)
        {
            const auto here = event.getPosition();

            if (! marqueeMoved && marqueeStart.getDistanceFrom (here) < 4)
                return;

            marqueeMoved = true;
            marqueeRect = juce::Rectangle<int> (marqueeStart, here);

            auto next = selectionBeforeMarquee;

            for (int i = 0; i < model.size(); ++i)
            {
                const auto& e = model.getEvent (i);

                if (blockBoundsFor (e).intersects (marqueeRect.toFloat())
                    && std::find (next.begin(), next.end(), e.id) == next.end())
                    next.push_back (e.id);
            }

            setSelection (std::move (next));
            repaint();
            return;
        }

        if (draggingBody)
        {
            double delta = snapDelta (xToBeats (event.x) - xToBeats (dragStartMouseX));

            // Don't let the earliest chord go before beat 0.
            double earliest = std::numeric_limits<double>::max();
            for (const auto& item : dragItems)
                earliest = juce::jmin (earliest, item.startBeats);

            delta = juce::jmax (delta, -earliest);
            dragDeltaBeats = delta;
            dragOutside = isOutside (event.getPosition());

            for (const auto& item : dragItems)
                model.moveEvent (item.id, item.startBeats + delta);

            repaint();
        }
        else if (draggingRightEdge)
        {
            const double delta = snapDelta (xToBeats (event.x) - xToBeats (dragStartMouseX));
            dragDeltaBeats = delta;

            for (const auto& item : dragItems)
                model.resizeEvent (item.id, juce::jmax (snapBeats, item.lengthBeats + delta));

            repaint();
        }
    }

    void TimelineComponent::finishBlockDrag()
    {
        // Put every block back where it started (bypassing undo) so the
        // actions below capture the true "before" state, then apply the whole
        // gesture as one undo step.
        std::vector<progression::ChordEvent> finals;

        for (const auto& item : dragItems)
        {
            const auto index = model.indexOfEvent (item.id);

            if (index < 0)
                continue;

            auto liveState = model.getEvent (index);
            model.moveEvent (item.id, item.startBeats);
            model.resizeEvent (item.id, item.lengthBeats);
            finals.push_back (liveState);
        }

        if (finals.empty())
            return;

        if (draggingBody && dragOutside)
        {
            undoManager.beginNewTransaction (finals.size() > 1 ? "Delete chords" : "Delete chord");

            for (const auto& e : finals)
                undoManager.perform (new progression::RemoveChordAction (model, e.id));

            setSelection ({});
            return;
        }

        bool changed = false;

        for (size_t i = 0; i < finals.size(); ++i)
        {
            const auto& item = dragItems[i];
            changed = changed || ! juce::approximatelyEqual (finals[i].startBeats, item.startBeats)
                              || ! juce::approximatelyEqual (finals[i].lengthBeats, item.lengthBeats);
        }

        if (! changed)
            return;

        undoManager.beginNewTransaction (draggingBody ? "Move chords" : "Resize chords");

        for (const auto& e : finals)
            undoManager.perform (new progression::UpdateChordAction (model, e.id, e));
    }

    void TimelineComponent::mouseUp (const juce::MouseEvent&)
    {
        if (draggingBody || draggingRightEdge)
            finishBlockDrag();

        draggingBody = false;
        draggingRightEdge = false;
        draggingLoopStart = false;
        draggingLoopEnd = false;
        dragOutside = false;
        marqueeActive = false;
        marqueeMoved = false;
        marqueeRect = {};
        dragItems.clear();
        scaleLockBeats = 0.0;
        interacting = false;
        repaint();
    }

    void TimelineComponent::mouseMove (const juce::MouseEvent& event)
    {
        bool onRightEdge = false;
        const auto* found = eventAt (event.x, event.y, &onRightEdge);
        setMouseCursor (found != nullptr && onRightEdge ? juce::MouseCursor::LeftRightResizeCursor
                                                         : juce::MouseCursor::NormalCursor);
    }

    void TimelineComponent::showContextMenu (const juce::MouseEvent& event)
    {
        if (const auto* found = eventAt (event.x, event.y, nullptr))
            if (! isSelected (found->id))
                setSelection ({ found->id });

        juce::PopupMenu menu;
        menu.addItem (1, "Delete", hasSelection());
        menu.addItem (2, "Duplicate", hasSelection());
        menu.addItem (3, "Copy", hasSelection());
        menu.addItem (4, "Paste at end", canPaste());
        menu.addSeparator();
        menu.addItem (5, "Select all", ! model.isEmpty());

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (
                                juce::Rectangle<int> (event.getScreenX(), event.getScreenY(), 1, 1)),
                            [safe = juce::Component::SafePointer<TimelineComponent> (this)] (int result)
        {
            if (safe == nullptr)
                return;

            switch (result)
            {
                case 1: safe->deleteSelected();    break;
                case 2: safe->duplicateSelected(); break;
                case 3: safe->copySelected();      break;
                case 4: safe->pasteAtEnd();        break;
                case 5: safe->selectAll();         break;
                default: break;
            }
        });
    }

    // ---- editing ------------------------------------------------------------

    void TimelineComponent::deleteSelected()
    {
        if (selection.empty())
            return;

        undoManager.beginNewTransaction (selection.size() > 1 ? "Delete chords" : "Delete chord");

        for (const auto& id : selection)
            undoManager.perform (new progression::RemoveChordAction (model, id));

        setSelection ({});
        repaint();
    }

    void TimelineComponent::duplicateSelected()
    {
        if (selection.empty())
            return;

        // The copies go right after the selected group, keeping their spacing.
        double groupStart = std::numeric_limits<double>::max(), groupEnd = 0.0;
        std::vector<progression::ChordEvent> originals;

        for (const auto& id : selection)
        {
            const auto index = model.indexOfEvent (id);

            if (index < 0)
                continue;

            originals.push_back (model.getEvent (index));
            groupStart = juce::jmin (groupStart, originals.back().startBeats);
            groupEnd = juce::jmax (groupEnd, originals.back().endBeats());
        }

        if (originals.empty())
            return;

        const double shift = groupEnd - groupStart;
        std::vector<juce::Uuid> newSelection;

        undoManager.beginNewTransaction (originals.size() > 1 ? "Duplicate chords" : "Duplicate chord");

        for (auto copy : originals)
        {
            copy.id = juce::Uuid();
            copy.startBeats += shift;
            newSelection.push_back (copy.id);
            undoManager.perform (new progression::AddChordAction (model, copy));
        }

        // AddChordAction keeps a non-null id, so the new ids are the ones we made.
        setSelection (std::move (newSelection));
        repaint();
    }

    void TimelineComponent::nudgeSelected (double deltaBeats)
    {
        if (selection.empty() || juce::approximatelyEqual (deltaBeats, 0.0))
            return;

        std::vector<progression::ChordEvent> states;
        double earliest = std::numeric_limits<double>::max();

        for (const auto& id : selection)
        {
            const auto index = model.indexOfEvent (id);

            if (index >= 0)
            {
                states.push_back (model.getEvent (index));
                earliest = juce::jmin (earliest, states.back().startBeats);
            }
        }

        if (states.empty())
            return;

        deltaBeats = juce::jmax (deltaBeats, -earliest);

        if (juce::approximatelyEqual (deltaBeats, 0.0))
            return;

        undoManager.beginNewTransaction ("Nudge chords");

        for (auto e : states)
        {
            e.startBeats += deltaBeats;
            undoManager.perform (new progression::UpdateChordAction (model, e.id, e));
        }

        repaint();
    }

    void TimelineComponent::copySelected()
    {
        clipboard.clear();

        for (const auto& id : selection)
        {
            const auto index = model.indexOfEvent (id);

            if (index >= 0)
                clipboard.push_back (model.getEvent (index));
        }

        std::sort (clipboard.begin(), clipboard.end(),
                   [] (const auto& a, const auto& b) { return a.startBeats < b.startBeats; });
    }

    void TimelineComponent::pasteAtEnd()
    {
        if (clipboard.empty())
            return;

        const double origin = clipboard.front().startBeats;
        const double base = model.getTotalLengthBeats();
        std::vector<juce::Uuid> newSelection;

        undoManager.beginNewTransaction (clipboard.size() > 1 ? "Paste chords" : "Paste chord");

        for (auto copy : clipboard)
        {
            copy.id = juce::Uuid();
            copy.startBeats = base + (copy.startBeats - origin);
            newSelection.push_back (copy.id);
            undoManager.perform (new progression::AddChordAction (model, copy));
        }

        setSelection (std::move (newSelection));
        repaint();
    }

    void TimelineComponent::setUseFlats (bool shouldUseFlats)
    {
        if (useFlats != shouldUseFlats)
        {
            useFlats = shouldUseFlats;
            repaint();
        }
    }

    // ---- chord cards dropped on the strip -----------------------------------

    int TimelineComponent::slotFromDescription (const juce::var& description)
    {
        const auto text = description.toString();
        return text.startsWith ("slot:") ? text.fromFirstOccurrenceOf ("slot:", false, false).getIntValue() : -1;
    }

    bool TimelineComponent::isInterestedInDragSource (const SourceDetails& details)
    {
        return slotFromDescription (details.description) >= 0 && makeEventFromSlot != nullptr;
    }

    void TimelineComponent::itemDragEnter (const SourceDetails& details)
    {
        interacting = true;
        scaleLockBeats = totalTimelineBeats();

        progression::ChordEvent preview;
        if (makeEventFromSlot != nullptr && makeEventFromSlot (slotFromDescription (details.description), preview))
            dropLength = preview.lengthBeats;

        itemDragMove (details);
    }

    void TimelineComponent::itemDragMove (const SourceDetails& details)
    {
        // The ghost's left edge sits under the cursor, snapped to the grid.
        dropBeat = snap (xToBeats (details.localPosition.x));
        repaint();
    }

    void TimelineComponent::itemDragExit (const SourceDetails&)
    {
        dropBeat = -1.0;
        scaleLockBeats = 0.0;
        interacting = false;
        repaint();
    }

    void TimelineComponent::addSlotEvent (int slot, double desiredStart)
    {
        progression::ChordEvent event;

        if (makeEventFromSlot == nullptr || ! makeEventFromSlot (slot, event))
            return;

        event.startBeats = model.findFreeStart (desiredStart, event.lengthBeats);

        undoManager.beginNewTransaction ("Add chord");
        undoManager.perform (new progression::AddChordAction (model, event));
        setSelection ({ event.id });
    }

    void TimelineComponent::itemDropped (const SourceDetails& details)
    {
        const double start = snap (xToBeats (details.localPosition.x));
        dropBeat = -1.0;
        scaleLockBeats = 0.0;
        interacting = false;

        addSlotEvent (slotFromDescription (details.description), start);
        repaint();
    }

    // ---- housekeeping -------------------------------------------------------

    void TimelineComponent::timerCallback()
    {
        // Repaint while the playhead moves, and whenever the model changed
        // without going through this component (host state restore, undo,
        // presets) -- cheaper and more reliable than every caller remembering
        // to call refreshFromModel().
        const auto revision = model.getRevision();
        const bool modelChanged = (revision != lastSeenModelRevision);
        lastSeenModelRevision = revision;

        if (modelChanged && ! interacting)
        {
            // Drop any selected ids that an undo or a restore removed.
            std::vector<juce::Uuid> alive;

            for (const auto& id : selection)
                if (model.indexOfEvent (id) >= 0)
                    alive.push_back (id);

            if (alive.size() != selection.size())
                setSelection (std::move (alive));
        }

        if (modelChanged || player.isPlaying())
            repaint();
    }
}
