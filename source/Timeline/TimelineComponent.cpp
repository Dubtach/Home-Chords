#include "TimelineComponent.h"
#include "../MusicTheory/Scale.h"
#include "../Progression/TimelineLayout.h"
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

    std::vector<progression::ChordEvent> TimelineComponent::shown() const
    {
        return previewActive ? previewEvents : model.getEvents();
    }

    double TimelineComponent::totalTimelineBeats() const noexcept
    {
        if (scaleLockBeats > 0.0)
            return scaleLockBeats;

        // Fit the chords, with one bar of room after the last one to drop onto.
        return juce::jmax (2.0 * beatsPerBar, model.getTotalLengthBeats() + beatsPerBar);
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

    bool TimelineComponent::eventAt (int x, int y, juce::Uuid& idOut, bool* onRightEdge) const
    {
        if (y < static_cast<int> (rulerHeight + loopMarkerHeight))
            return false;

        for (const auto& event : shown())
        {
            const auto bounds = blockBoundsFor (event);

            if (bounds.contains (static_cast<float> (x), static_cast<float> (y)))
            {
                idOut = event.id;

                if (onRightEdge != nullptr)
                    *onRightEdge = (static_cast<float> (x) > bounds.getRight() - 7.0f);

                return true;
            }
        }

        return false;
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

        // ---- ruler and grid: bar numbers, beat ticks, snap lines ----
        const float ppb = pixelsPerBeat();
        const float width = static_cast<float> (getWidth());
        const float numberLimit = width - static_cast<float> (cornerWidth) - 8.0f;   // keep clear of the time-signature chip

        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.fillRect (juce::Rectangle<float> (0.0f, loopMarkerHeight, width, rulerHeight));

        // Snap grid: faint lines at the current resolution (only if they'd be
        // far enough apart to read).
        if (static_cast<float> (snapBeats) * ppb >= 7.0f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.045f));

            for (double b = snapBeats; b < totalBeats; b += snapBeats)
            {
                const auto x = static_cast<float> (beatsToX (b));
                g.drawLine (x, gridTop, x, bottom, 0.6f);
            }
        }

        for (int beat = 0; static_cast<double> (beat) < totalBeats; ++beat)
        {
            const auto x = static_cast<float> (beatsToX (static_cast<double> (beat)));
            const double inBar = std::fmod (static_cast<double> (beat), beatsPerBar);
            const bool isBar = inBar < 1.0e-6;
            const int barNumber = static_cast<int> (static_cast<double> (beat) / beatsPerBar + 1.0e-6) + 1;

            if (isBar)
            {
                g.setColour (juce::Colours::white.withAlpha (barNumber % 4 == 1 ? 0.30f : 0.18f));
                g.drawLine (x, loopMarkerHeight, x, bottom, barNumber % 4 == 1 ? 1.3f : 1.0f);

                if (x + 18.0f < numberLimit)
                {
                    g.setFont (homeUI::font (11.0f, true));
                    g.setColour (juce::Colours::white.withAlpha (0.92f));
                    g.drawText (juce::String (barNumber), juce::Rectangle<float> (x + 4.0f, loopMarkerHeight, 26.0f, rulerHeight),
                                juce::Justification::centredLeft, false);
                }
            }
            else
            {
                g.setColour (juce::Colours::white.withAlpha (0.07f));
                g.drawLine (x, gridTop, x, bottom, 0.7f);

                g.setColour (juce::Colours::white.withAlpha (0.32f));
                g.drawLine (x, gridTop - 5.0f, x, gridTop, 1.0f);

                // Beat numbers (2, 3, 4 ...) once there's room for them.
                if (ppb >= 26.0f && x + 12.0f < numberLimit)
                {
                    g.setFont (homeUI::font (8.5f, false));
                    g.setColour (juce::Colours::white.withAlpha (0.42f));
                    g.drawText (juce::String (static_cast<int> (inBar + 1.5)),
                                juce::Rectangle<float> (x + 3.0f, loopMarkerHeight, 16.0f, rulerHeight),
                                juce::Justification::centredLeft, false);
                }
            }
        }

        // Loop region: always exactly the chords, so it is shaded, not draggable.
        {
            const auto loopX0 = static_cast<float> (beatsToX (0.0));
            const auto loopX1 = static_cast<float> (beatsToX (previewActive ? progression::layout::totalLength (previewEvents)
                                                                            : model.getTotalLengthBeats()));

            g.setColour (homeUI::cyan.withAlpha (0.07f));
            g.fillRect (juce::Rectangle<float> (loopX0, gridTop, loopX1 - loopX0, bottom - gridTop));

            g.setColour (homeUI::cyan.withAlpha (0.8f));
            g.fillRect (juce::Rectangle<float> (loopX0 - 1.5f, 0.0f, 3.0f, loopMarkerHeight));
            g.fillRect (juce::Rectangle<float> (loopX1 - 1.5f, 0.0f, 3.0f, loopMarkerHeight));
        }

        // Chord blocks. While blocks are being dragged, the ones that are not
        // moving close up around an open, dashed slot (where the drag will land),
        // and the dragged blocks float under the pointer.
        const bool removing = dragOutside && draggingBody;   // the preview already has the chords taken out
        const bool dragging = draggingBody && previewActive;

        for (const auto& event : shown())
        {
            const bool isGhost = (event.id == ghostId);
            const bool isSlot = dragging && ! dragOutside && progression::layout::containsId (dragIds, event.id);

            if (! replaceId.isNull() && event.id == replaceId)
            {
                // Card drop that would replace this chord: show the incoming chord in its place.
                const auto bounds = blockBoundsFor (event);
                drawBlock (g, progression::layout::withChordOf (event, dropEvent), bounds, false, false, false, true);

                const auto tag = juce::Rectangle<float> (bounds.getCentreX() - 30.0f, bounds.getBottom() - 22.0f, 60.0f, 14.0f);
                g.setColour (juce::Colours::black.withAlpha (0.65f));
                g.fillRoundedRectangle (tag, 7.0f);
                g.setColour (juce::Colours::white);
                g.setFont (homeUI::font (9.0f, true));
                g.drawText ("REPLACE", tag, juce::Justification::centred, false);
                continue;
            }

            drawBlock (g, event, blockBoundsFor (event), isSelected (event.id), false, isSlot, isGhost);

            if (isGhost)
            {
                const auto bounds = blockBoundsFor (event);
                const auto tag = juce::Rectangle<float> (bounds.getCentreX() - 28.0f, bounds.getBottom() - 22.0f, 56.0f, 14.0f);
                g.setColour (juce::Colours::black.withAlpha (0.65f));
                g.fillRoundedRectangle (tag, 7.0f);
                g.setColour (juce::Colours::white);
                g.setFont (homeUI::font (9.0f, true));
                g.drawText ("INSERT", tag, juce::Justification::centred, false);
            }
        }

        if (dragging)
        {
            for (const auto& original : dragOriginal)
                if (progression::layout::containsId (dragIds, original.id))
                    drawBlock (g, original, blockBoundsFor (original).translated (static_cast<float> (dragOffset.x),
                                                                                    static_cast<float> (dragOffset.y)),
                               true, true, false, false);
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
        else if (model.isEmpty() && ! previewActive)
        {
            g.setFont (homeUI::font (11.0f, false));
            g.setColour (juce::Colours::white.withAlpha (0.38f));
            g.drawText ("Drag chords here from the cards above   (or double-click a card)",
                        getLocalBounds(), juce::Justification::centred, false);
        }
    }

    void TimelineComponent::drawBlock (juce::Graphics& g, const progression::ChordEvent& event, juce::Rectangle<float> bounds,
                                       bool selected, bool floating, bool slot, bool ghost) const
    {
        juce::Colour accent = homeUI::green;
        switch (event.quality)
        {
            case musictheory::ChordQuality::Major:      accent = homeUI::cyan;   break;
            case musictheory::ChordQuality::Minor:      accent = homeUI::purple; break;
            case musictheory::ChordQuality::Diminished: accent = homeUI::warn;   break;
            case musictheory::ChordQuality::Augmented:  accent = homeUI::pink;   break;
            default:                                    accent = homeUI::green;  break;
        }

        const auto outline = bounds.reduced (1.5f);
        const auto dashedOutline = [&g, &outline]
        {
            juce::Path path;
            path.addRoundedRectangle (outline, 6.0f);
            juce::Path dashed;
            const float dashes[] = { 5.0f, 4.0f };
            juce::PathStrokeType (1.5f).createDashedStroke (dashed, path, dashes, 2);
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.fillPath (dashed);
        };

        g.saveState();

        if (slot)
        {
            // The open slot a dragged chord will land in: a faint, dashed place-holder.
            g.setColour (accent.withAlpha (0.16f));
            g.fillRoundedRectangle (outline, 6.0f);
            dashedOutline();
            g.restoreState();
            return;
        }

        if (ghost)
            g.setOpacity (0.62f);
        else if (floating)
            g.setOpacity (dragOutside ? 0.5f : 0.88f);

        homeUI::drawCard (g, outline, (selected || floating) ? accent.brighter (0.3f) : accent.darker (event.enabled ? 0.0f : 0.5f));

        if (ghost)
            dashedOutline();
        else if (floating)
        {
            g.setColour ((dragOutside ? homeUI::warn : juce::Colours::white).withAlpha (0.95f));
            g.drawRoundedRectangle (outline, 6.0f, 2.0f);
        }
        else if (selected)
        {
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawRoundedRectangle (outline, 6.0f, 2.0f);
        }

        const auto rootName = musictheory::noteName (event.rootPitchClass, useFlats);
        const auto chordName = rootName + musictheory::chordQualitySuffix (event.quality)
                                        + musictheory::extensionSuffix (event.extension);

        auto content = bounds.reduced (5.0f, 3.0f);
        homeUI::drawCardText (g, chordName, content, juce::jmin (15.0f, content.getHeight() * 0.5f));
        g.restoreState();
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

        juce::Uuid foundId;
        bool onRightEdge = false;
        const bool found = eventAt (x, y, foundId, &onRightEdge);
        const bool additive = event.mods.isShiftDown() || event.mods.isCommandDown() || event.mods.isCtrlDown();

        if (found)
        {
            if (additive)
            {
                toggleInSelection (foundId);

                if (! isSelected (foundId))
                    return;   // it was just removed from the selection: nothing to drag
            }
            else if (! isSelected (foundId))
            {
                setSelection ({ foundId });
            }

            dragIds = selection;
            dragOriginal = model.getEvents();
            dragGroupStart = 1.0e9;
            for (const auto& e : dragOriginal)
                if (progression::layout::containsId (dragIds, e.id))
                    dragGroupStart = juce::jmin (dragGroupStart, e.startBeats);
            dragStartMouseX = x;
            dragStartMouseY = y;
            dragOffset = {};
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
        if (marqueeActive)
        {
            const auto here = event.getPosition();

            if (! marqueeMoved && marqueeStart.getDistanceFrom (here) < 4)
                return;

            marqueeMoved = true;
            marqueeRect = juce::Rectangle<int> (marqueeStart, here);

            auto next = selectionBeforeMarquee;

            for (const auto& e : model.getEvents())
            {
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
            dragOutside = isOutside (event.getPosition());
            dragOffset = { event.x - dragStartMouseX, event.y - dragStartMouseY };

            if (dragOutside)
                previewEvents = progression::layout::without (dragOriginal, dragIds);
            else
                previewEvents = progression::layout::moveGroup (dragOriginal, dragIds,
                                                                 dragGroupStart + xToBeats (event.x) - xToBeats (dragStartMouseX));

            previewActive = true;
            repaint();
        }
        else if (draggingRightEdge)
        {
            const double delta = snapDelta (xToBeats (event.x) - xToBeats (dragStartMouseX));

            previewEvents = progression::layout::resized (dragOriginal, dragIds, [this, delta] (const progression::ChordEvent& e)
            {
                return juce::jmax (snapBeats, e.lengthBeats + delta);
            });

            previewActive = true;
            repaint();
        }
    }

    void TimelineComponent::commit (std::vector<progression::ChordEvent> events, const juce::String& undoName)
    {
        progression::layout::assignContiguous (events);

        // Nothing to do if neither the layout nor any chord changed (a replace keeps the layout).
        const auto current = model.getEvents();
        bool unchanged = progression::layout::sameLayout (events, current);

        for (size_t i = 0; unchanged && i < events.size(); ++i)
            unchanged = events[i].rootPitchClass == current[i].rootPitchClass
                     && events[i].quality == current[i].quality
                     && events[i].extension == current[i].extension;

        if (unchanged)
            return;

        undoManager.beginNewTransaction (undoName);
        undoManager.perform (new progression::SetEventsAction (model, std::move (events)));
    }

    void TimelineComponent::endGesture()
    {
        if (previewActive && (draggingBody || draggingRightEdge))
        {
            auto result = previewEvents;

            if (draggingBody && dragOutside)
            {
                commit (std::move (result), dragIds.size() > 1 ? "Delete chords" : "Delete chord");
                setSelection ({});
            }
            else
            {
                commit (std::move (result), draggingBody ? "Move chords" : "Resize chords");
            }
        }

        previewActive = false;
        previewEvents.clear();
        ghostId = juce::Uuid::null();
        draggingBody = false;
        draggingRightEdge = false;
        dragOutside = false;
        marqueeActive = false;
        marqueeMoved = false;
        marqueeRect = {};
        dragIds.clear();
        dragOriginal.clear();
        scaleLockBeats = 0.0;
        interacting = false;
    }

    void TimelineComponent::mouseUp (const juce::MouseEvent&)
    {
        endGesture();
        repaint();
    }

    void TimelineComponent::mouseMove (const juce::MouseEvent& event)
    {
        juce::Uuid id;
        bool onRightEdge = false;
        const bool found = eventAt (event.x, event.y, id, &onRightEdge);
        setMouseCursor (found && onRightEdge ? juce::MouseCursor::LeftRightResizeCursor
                                             : juce::MouseCursor::NormalCursor);
    }

    void TimelineComponent::showContextMenu (const juce::MouseEvent& event)
    {
        juce::Uuid foundId;
        if (eventAt (event.x, event.y, foundId, nullptr) && ! isSelected (foundId))
            setSelection ({ foundId });

        juce::PopupMenu menu;

        if (snapChoiceNames.size() > 0)
        {
            const int current = getSnapChoice != nullptr ? getSnapChoice() : -1;
            juce::PopupMenu grid;

            for (int i = 0; i < snapChoiceNames.size(); ++i)
                grid.addItem (100 + i, snapChoiceNames[i], true, i == current);

            menu.addSubMenu ("Grid snap", grid);
            menu.addSeparator();
        }

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
                default:
                    if (result >= 100 && safe->onSnapChoice != nullptr)
                        safe->onSnapChoice (result - 100);
                    break;
            }
        });
    }

    // ---- editing ------------------------------------------------------------

    void TimelineComponent::deleteSelected()
    {
        if (selection.empty())
            return;

        commit (progression::layout::without (model.getEvents(), selection),
                selection.size() > 1 ? "Delete chords" : "Delete chord");

        setSelection ({});
        repaint();
    }

    void TimelineComponent::duplicateSelected()
    {
        if (selection.empty())
            return;

        // The copies go right after the last selected chord.
        const auto events = model.getEvents();
        auto copies = progression::layout::extract (events, selection);

        if (copies.empty())
            return;

        int after = 0;
        for (size_t i = 0; i < events.size(); ++i)
            if (progression::layout::containsId (selection, events[i].id))
                after = static_cast<int> (i) + 1;

        std::vector<juce::Uuid> newSelection;

        for (auto& copy : copies)
        {
            copy.id = juce::Uuid();
            newSelection.push_back (copy.id);
        }

        commit (progression::layout::insertAt (events, copies, after),
                copies.size() > 1 ? "Duplicate chords" : "Duplicate chord");

        setSelection (std::move (newSelection));
        repaint();
    }

    void TimelineComponent::nudgeSelected (double deltaBeats)
    {
        if (selection.empty() || juce::approximatelyEqual (deltaBeats, 0.0))
            return;

        commit (progression::layout::shiftGroup (model.getEvents(), selection, deltaBeats < 0.0 ? -1 : 1), "Move chords");
        repaint();
    }

    void TimelineComponent::copySelected()
    {
        clipboard = progression::layout::extract (model.getEvents(), selection);
    }

    void TimelineComponent::pasteAtEnd()
    {
        if (clipboard.empty())
            return;

        auto pasted = clipboard;
        std::vector<juce::Uuid> newSelection;

        for (auto& copy : pasted)
        {
            copy.id = juce::Uuid();
            newSelection.push_back (copy.id);
        }

        auto events = model.getEvents();
        commit (progression::layout::insertAt (events, pasted, static_cast<int> (events.size())),
                pasted.size() > 1 ? "Paste chords" : "Paste chord");

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

        // Keep the real chord (not a default C) so the ghost shows what will land.
        dropEvent = {};
        dropLength = 4.0;

        if (makeEventFromSlot != nullptr && makeEventFromSlot (slotFromDescription (details.description), dropEvent))
            dropLength = dropEvent.lengthBeats;

        ghostId = juce::Uuid();
        itemDragMove (details);
    }

    void TimelineComponent::itemDragMove (const SourceDetails& details)
    {
        const auto events = model.getEvents();
        const auto target = progression::layout::dropTargetAt (events, xToBeats (details.localPosition.x),
                                                               juce::ModifierKeys::currentModifiers.isShiftDown());

        if (target.replace)
        {
            // Replace: nothing moves; the chord under the pointer shows what it would become.
            previewEvents = events;
            replaceId = events[static_cast<size_t> (target.index)].id;
        }
        else
        {
            auto ghost = dropEvent;
            ghost.id = ghostId;
            ghost.lengthBeats = dropLength;
            replaceId = juce::Uuid::null();

            // Insert: the chords after the gap move over to make room.
            previewEvents = progression::layout::insertAt (events, { ghost }, target.index);
        }

        previewActive = true;
        repaint();
    }

    void TimelineComponent::itemDragExit (const SourceDetails&)
    {
        replaceId = juce::Uuid::null();
        previewActive = false;
        previewEvents.clear();
        ghostId = juce::Uuid::null();
        scaleLockBeats = 0.0;
        interacting = false;
        repaint();
    }

    void TimelineComponent::itemDropped (const SourceDetails& details)
    {
        const auto events = model.getEvents();
        const auto target = progression::layout::dropTargetAt (events, xToBeats (details.localPosition.x),
                                                               juce::ModifierKeys::currentModifiers.isShiftDown());
        const int slot = slotFromDescription (details.description);

        previewActive = false;
        previewEvents.clear();
        replaceId = juce::Uuid::null();
        ghostId = juce::Uuid::null();
        scaleLockBeats = 0.0;
        interacting = false;

        progression::ChordEvent incoming;

        if (makeEventFromSlot != nullptr && makeEventFromSlot (slot, incoming))
        {
            if (target.replace)
            {
                auto updated = events;
                auto& chosen = updated[static_cast<size_t> (target.index)];
                chosen = progression::layout::withChordOf (chosen, incoming);
                commit (std::move (updated), "Replace chord");
                setSelection ({ chosen.id });
            }
            else
            {
                incoming.id = juce::Uuid();
                commit (progression::layout::insertAt (events, { incoming }, target.index), "Insert chord");
                setSelection ({ incoming.id });
            }
        }

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
