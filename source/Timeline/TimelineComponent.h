#pragma once

#include "../Progression/ProgressionModel.h"
#include "../Progression/ProgressionActions.h"
#include "SequencePlayer.h"
#include "../Shared/HomeSeriesUI.h"
#include <JuceHeader.h>
#include <functional>

// =============================================================================
// The progression timeline: a horizontal strip of chord blocks positioned
// by beat, with a beat ruler, playhead, and loop-range shading. Drag a
// block to move it, drag its right edge to resize it, click to select it
// (which shows a small toolbar elsewhere in the editor for Rhythm/Bass/
// Arp/Delete/Duplicate -- see PluginEditor). Every edit goes through the
// supplied UndoManager via ProgressionActions.
//
// No horizontal scrolling: the visible width is scaled to fit however
// long the progression (or the loop range, whichever is longer) currently
// is, rather than a fixed pixels-per-beat with a scrollbar. Simpler, and
// fine for the progression lengths a songwriting sketch tool actually
// produces; long progressions just render with narrower blocks.
//
// Snap is fixed at a quarter-beat grid for this pass (spec section 10's
// configurable snap-grid picker isn't implemented).
// =============================================================================

namespace timeline
{
    class TimelineComponent : public juce::Component, private juce::Timer
    {
    public:
        TimelineComponent (progression::ProgressionModel& modelIn, SequencePlayer& playerIn, juce::UndoManager& undoManagerIn);
        ~TimelineComponent() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;

        // Called whenever the selected event changes (nullptr = nothing
        // selected), so the host editor can show/hide the properties
        // toolbar and populate it from the newly-selected event.
        std::function<void (const juce::Uuid*)> onSelectionChanged;

        juce::Uuid getSelectedId() const noexcept { return selectedId; }
        void deleteSelected();
        void duplicateSelected();
        void updateSelected (const progression::ChordEvent& newState);   // for the toolbar to push Rhythm/Bass/Arp edits through

        void refreshFromModel();   // call after any external model change (undo/redo, preset applied, ADD-mode append)

    private:
        static constexpr double snapBeats = 0.25;
        static constexpr float rulerHeight = 22.0f;
        static constexpr float loopMarkerHeight = 10.0f;

        progression::ProgressionModel& model;
        SequencePlayer& player;
        juce::UndoManager& undoManager;

        // NB: juce::Uuid() generates a fresh *random* id; "nothing" is
        // Uuid::null(). Mixing the two up leaves a phantom selection.
        juce::Uuid selectedId = juce::Uuid::null();
        juce::Uuid dragEventId = juce::Uuid::null();
        juce::uint32 lastSeenModelRevision = 0;
        bool draggingBody = false;
        bool draggingRightEdge = false;
        bool draggingLoopStart = false;
        bool draggingLoopEnd = false;
        double dragStartBeats = 0.0;
        double dragStartLength = 0.0;
        int dragStartMouseX = 0;

        double totalTimelineBeats() const noexcept;
        float pixelsPerBeat() const noexcept;
        double xToBeats (int x) const noexcept;
        int beatsToX (double beats) const noexcept;
        double snap (double beats) const noexcept;

        juce::Rectangle<float> blockBoundsFor (const progression::ChordEvent& event) const noexcept;
        const progression::ChordEvent* eventAt (int x, int y, bool* onRightEdge) const noexcept;

        void timerCallback() override;   // repaints for the moving playhead, and when the model changed underneath us

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimelineComponent)
    };
}
