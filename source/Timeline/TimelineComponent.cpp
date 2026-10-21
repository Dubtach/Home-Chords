#include "TimelineComponent.h"
#include "../MusicTheory/Scale.h"
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

    double TimelineComponent::totalTimelineBeats() const noexcept
    {
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

    juce::Rectangle<float> TimelineComponent::blockBoundsFor (const progression::ChordEvent& event) const noexcept
    {
        const float top = rulerHeight + loopMarkerHeight;
        const float x = static_cast<float> (beatsToX (event.startBeats));
        const float width = juce::jmax (4.0f, static_cast<float> (beatsToX (event.endBeats())) - x);
        const float height = juce::jmax (10.0f, static_cast<float> (getHeight()) - top);
        return { x, top, width, height };
    }

    const progression::ChordEvent* TimelineComponent::eventAt (int x, int y, bool* onRightEdge) const noexcept
    {
        if (y < static_cast<int> (rulerHeight + loopMarkerHeight))
            return nullptr;

        for (int i = 0; i < model.size(); ++i)
        {
            const auto& event = model.getEvent (i);
            const auto bounds = blockBoundsFor (event);

            if (bounds.contains (static_cast<float> (x), static_cast<float> (y)))
            {
                if (onRightEdge != nullptr)
                    *onRightEdge = (static_cast<float> (x) > bounds.getRight() - 8.0f);

                return &event;
            }
        }

        return nullptr;
    }

    void TimelineComponent::paint (juce::Graphics& g)
    {
        homeUI::drawWell (g, getLocalBounds().toFloat(), 6.0f);

        const double totalBeats = totalTimelineBeats();

        // Bar ruler (drawn on a 4-beats-per-bar grid regardless of the
        // actual time signature -- a reasonable simplification for a
        // visual guide; playback itself doesn't depend on this grid).
        for (int bar = 0; static_cast<double> (bar) * 4.0 < totalBeats; ++bar)
        {
            const auto x = static_cast<float> (beatsToX (static_cast<double> (bar) * 4.0));
            g.setColour (juce::Colours::white.withAlpha (bar % 4 == 0 ? 0.30f : 0.14f));
            g.drawLine (x, rulerHeight + loopMarkerHeight, x, static_cast<float> (getHeight()), bar % 4 == 0 ? 1.3f : 0.7f);

            g.setFont (homeUI::font (9.0f, false));
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.drawText (juce::String (bar + 1), juce::Rectangle<float> (x + 3.0f, loopMarkerHeight, 30.0f, rulerHeight),
                        juce::Justification::centredLeft, false);
        }

        // Loop region shading + handles.
        {
            const auto loopX0 = static_cast<float> (beatsToX (player.getLoopStartBeats()));
            const auto loopX1 = static_cast<float> (beatsToX (player.getLoopEndBeats()));

            g.setColour (homeUI::cyan.withAlpha (0.07f));
            g.fillRect (juce::Rectangle<float> (loopX0, rulerHeight + loopMarkerHeight, loopX1 - loopX0,
                                                static_cast<float> (getHeight()) - rulerHeight - loopMarkerHeight));

            g.setColour (homeUI::cyan.withAlpha (0.8f));
            g.fillRect (juce::Rectangle<float> (loopX0 - 1.5f, 0.0f, 3.0f, loopMarkerHeight));
            g.fillRect (juce::Rectangle<float> (loopX1 - 1.5f, 0.0f, 3.0f, loopMarkerHeight));
        }

        // Chord blocks.
        for (int i = 0; i < model.size(); ++i)
        {
            const auto& event = model.getEvent (i);
            const auto bounds = blockBoundsFor (event);

            juce::Colour accent = homeUI::green;
            switch (event.quality)
            {
                case musictheory::ChordQuality::Major:      accent = homeUI::cyan;  break;
                case musictheory::ChordQuality::Minor:      accent = homeUI::purple; break;
                case musictheory::ChordQuality::Diminished: accent = homeUI::warn;   break;
                case musictheory::ChordQuality::Augmented:  accent = homeUI::pink;   break;
                default:                                    accent = homeUI::green;  break;
            }

            const bool isSelected = event.id == selectedId;
            homeUI::drawCard (g, bounds.reduced (1.5f), isSelected ? accent.brighter (0.3f) : accent.darker (event.enabled ? 0.0f : 0.5f));

            if (isSelected)
            {
                g.setColour (juce::Colours::white.withAlpha (0.85f));
                g.drawRoundedRectangle (bounds.reduced (1.5f), 6.0f, 2.0f);
            }

            const auto rootName = musictheory::noteName (event.rootPitchClass, useFlats);
            const auto chordName = rootName + musictheory::chordQualitySuffix (event.quality)
                                            + musictheory::extensionSuffix (event.extension);

            auto content = bounds.reduced (6.0f, 4.0f);
            homeUI::drawCardText (g, chordName, content, juce::jmin (16.0f, content.getHeight() * 0.5f));

            if (event.rhythm != progression::RhythmPattern::Sustained)
            {
                static const juce::StringArray rhythmGlyphs { "sus", "1", "1/2", "1/4", "1/8", "1/16", "strum", "arp" };
                const auto label = rhythmGlyphs[juce::jlimit (0, rhythmGlyphs.size() - 1, static_cast<int> (event.rhythm))];
                auto tagSource = bounds;   // bounds is const; removeFromBottom mutates its receiver
                auto tag = tagSource.removeFromBottom (juce::jmin (12.0f, bounds.getHeight() * 0.3f)).reduced (4.0f, 0.0f);
                homeUI::drawCardText (g, label, tag, 8.5f, juce::Justification::centredLeft, 0.7f);
            }
        }

        // Playhead.
        if (player.isPlaying())
        {
            const auto x = static_cast<float> (beatsToX (player.getPositionBeats()));
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawLine (x, loopMarkerHeight, x, static_cast<float> (getHeight()), 2.0f);
        }

        if (model.isEmpty())
        {
            g.setFont (homeUI::font (12.0f, false));
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.drawText ("Turn on Add Mode and play a chord to start building a progression",
                        getLocalBounds(), juce::Justification::centred, false);
        }
    }

    void TimelineComponent::resized()
    {
        // Nothing to lay out -- everything is drawn directly in paint()
        // against getLocalBounds(), recomputed from current beats/pixel
        // scale on every resize.
    }

    void TimelineComponent::mouseDown (const juce::MouseEvent& event)
    {
        const int x = event.x;
        const int y = event.y;

        const auto loopX0 = static_cast<float> (beatsToX (player.getLoopStartBeats()));
        const auto loopX1 = static_cast<float> (beatsToX (player.getLoopEndBeats()));

        if (y < static_cast<int> (loopMarkerHeight) && std::abs (static_cast<float> (x) - loopX0) < 6.0f)
        {
            draggingLoopStart = true;
            return;
        }

        if (y < static_cast<int> (loopMarkerHeight) && std::abs (static_cast<float> (x) - loopX1) < 6.0f)
        {
            draggingLoopEnd = true;
            return;
        }

        bool onRightEdge = false;

        if (const auto* found = eventAt (x, y, &onRightEdge))
        {
            selectedId = found->id;
            dragEventId = found->id;
            dragStartBeats = found->startBeats;
            dragStartLength = found->lengthBeats;
            dragStartMouseX = x;
            draggingRightEdge = onRightEdge;
            draggingBody = ! onRightEdge;
        }
        else
        {
            selectedId = juce::Uuid::null();
            draggingBody = false;
            draggingRightEdge = false;
        }

        if (onSelectionChanged != nullptr)
            onSelectionChanged (selectedId.isNull() ? nullptr : &selectedId);

        // This component doesn't take keyboard focus itself, but clicking in
        // it shouldn't leave the chord keys dead after a combo box had focus.
        if (auto* parent = getParentComponent())
            parent->grabKeyboardFocus();

        repaint();
    }

    void TimelineComponent::mouseDrag (const juce::MouseEvent& event)
    {
        if (draggingLoopStart)
        {
            const double beats = snap (xToBeats (event.x));
            player.setLoopRange (beats, player.getLoopEndBeats());
            repaint();
            return;
        }

        if (draggingLoopEnd)
        {
            const double minEnd = player.getLoopStartBeats() + snapBeats;
            const double beats = juce::jmax (minEnd, snap (xToBeats (event.x)));
            player.setLoopRange (player.getLoopStartBeats(), beats);
            repaint();
            return;
        }

        if (draggingBody)
        {
            const double deltaBeats = snap (xToBeats (event.x)) - snap (xToBeats (dragStartMouseX));
            model.moveEvent (dragEventId, juce::jmax (0.0, dragStartBeats + deltaBeats));
            repaint();
        }
        else if (draggingRightEdge)
        {
            const double newLength = snap (xToBeats (event.x) - dragStartBeats);
            model.resizeEvent (dragEventId, juce::jmax (snapBeats, newLength));
            repaint();
        }
    }

    void TimelineComponent::mouseUp (const juce::MouseEvent&)
    {
        if (draggingBody || draggingRightEdge)
        {
            const auto index = model.indexOfEvent (dragEventId);

            if (index >= 0)
            {
                const auto liveState = model.getEvent (index);   // wherever the live-preview drag left it
                const bool changed = ! juce::approximatelyEqual (liveState.startBeats, dragStartBeats)
                                   || ! juce::approximatelyEqual (liveState.lengthBeats, dragStartLength);

                if (changed)
                {
                    // Revert the live-preview edit (bypassing undo) so
                    // UpdateChordAction::perform() below captures the
                    // correct "before" state, then apply the real change
                    // through undo in one step -- one undo entry per whole
                    // drag gesture, not one per mouseDrag callback.
                    model.moveEvent (dragEventId, dragStartBeats);
                    model.resizeEvent (dragEventId, dragStartLength);
                    undoManager.beginNewTransaction ("Edit chord");
                    undoManager.perform (new progression::UpdateChordAction (model, dragEventId, liveState));
                }
            }
        }

        draggingBody = false;
        draggingRightEdge = false;
        draggingLoopStart = false;
        draggingLoopEnd = false;
        repaint();
    }

    void TimelineComponent::mouseMove (const juce::MouseEvent& event)
    {
        bool onRightEdge = false;
        const auto* found = eventAt (event.x, event.y, &onRightEdge);
        setMouseCursor (found != nullptr && onRightEdge ? juce::MouseCursor::LeftRightResizeCursor
                                                         : juce::MouseCursor::NormalCursor);
    }

    void TimelineComponent::deleteSelected()
    {
        if (selectedId.isNull())
            return;

        undoManager.beginNewTransaction ("Delete chord");
        undoManager.perform (new progression::RemoveChordAction (model, selectedId));
        selectedId = juce::Uuid::null();

        if (onSelectionChanged != nullptr)
            onSelectionChanged (nullptr);

        repaint();
    }

    void TimelineComponent::duplicateSelected()
    {
        const auto index = model.indexOfEvent (selectedId);
        if (index < 0)
            return;

        auto copy = model.getEvent (index);
        copy.id = juce::Uuid();
        copy.startBeats = copy.endBeats();   // place immediately after the original

        undoManager.beginNewTransaction ("Duplicate chord");
        undoManager.perform (new progression::AddChordAction (model, copy));
        repaint();
    }

    void TimelineComponent::updateSelected (const progression::ChordEvent& newState)
    {
        if (selectedId.isNull())
            return;

        undoManager.beginNewTransaction ("Edit chord");
        undoManager.perform (new progression::UpdateChordAction (model, selectedId, newState));
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

    void TimelineComponent::refreshFromModel()
    {
        repaint();
    }

    void TimelineComponent::timerCallback()
    {
        // Repaint while the playhead moves, and whenever the model changed
        // without going through this component (host state restore, undo
        // via a shortcut, ADD mode, presets) -- cheaper and more reliable
        // than every caller remembering to call refreshFromModel().
        const auto revision = model.getRevision();
        const bool modelChanged = (revision != lastSeenModelRevision);
        lastSeenModelRevision = revision;

        if (modelChanged)
        {
            // The selection may have been deleted by an undo or a restore.
            if (! selectedId.isNull() && model.indexOfEvent (selectedId) < 0)
            {
                selectedId = juce::Uuid::null();

                if (onSelectionChanged != nullptr)
                    onSelectionChanged (nullptr);
            }
        }

        if (modelChanged || player.isPlaying())
            repaint();
    }
}
