#pragma once

#include "../Progression/ProgressionModel.h"
#include "../Progression/ProgressionActions.h"
#include "SequencePlayer.h"
#include "../Shared/HomeSeriesUI.h"
#include <JuceHeader.h>
#include <functional>
#include <vector>

// =============================================================================
// The progression timeline: a short strip of chord blocks positioned by beat,
// with a bar ruler, playhead and loop-range shading.
//
//   * Drop a chord card on it to add that chord where you let go (snapped,
//     and never on top of another chord).
//   * Click a block to select it; shift/ctrl/cmd-click adds to the selection;
//     drag on empty space to rubber-band select several.
//   * Drag the selection to move it; drag the right edge of a block to
//     resize the whole selection; drag it out of the strip (above, below or
//     off the side) and let go to delete it.
//   * Right-click a block for Delete / Duplicate / Copy / Paste / Select all.
//
// Every edit is one UndoManager transaction per gesture. There is no
// horizontal scrolling: the visible width is scaled to fit the progression (or
// the loop range, whichever is longer); the scale is frozen while a drag is in
// progress so blocks don't slide under the mouse.
//
// Rhythm / Bass / Arp / Gate are global (see PlaybackSettings.h), so the
// blocks carry no per-chord controls -- just position, length and the chord.
// =============================================================================

namespace timeline
{
    class TimelineComponent : public juce::Component,
                              public juce::DragAndDropTarget,
                              private juce::Timer
    {
    public:
        TimelineComponent (progression::ProgressionModel& modelIn, SequencePlayer& playerIn, juce::UndoManager& undoManagerIn);
        ~TimelineComponent() override;

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;

        // Chord cards dropped on the strip (description string "slot:<n>").
        bool isInterestedInDragSource (const SourceDetails&) override;
        void itemDragEnter (const SourceDetails&) override;
        void itemDragMove (const SourceDetails&) override;
        void itemDragExit (const SourceDetails&) override;
        void itemDropped (const SourceDetails&) override;

        // Supplied by the editor: builds the event for a chord card (slot).
        std::function<bool (int slot, progression::ChordEvent& out)> makeEventFromSlot;

        // Called whenever the selection changes.
        std::function<void()> onSelectionChanged;

        // ---- Selection & editing (the editor's shortcuts call these) ----
        const std::vector<juce::Uuid>& getSelection() const noexcept { return selection; }
        bool hasSelection() const noexcept { return ! selection.empty(); }
        void selectAll();
        void clearSelection();
        void deleteSelected();
        void duplicateSelected();
        void nudgeSelected (double deltaBeats);
        void copySelected();
        void pasteAtEnd();
        bool canPaste() const noexcept { return ! clipboard.empty(); }

        // True while a drag (move / resize / marquee / card drop) is in progress.
        bool isInteracting() const noexcept { return interacting; }

        // Grid that drags snap to, in beats (clamped to a sane minimum).
        void setSnapBeats (double beats) noexcept { snapBeats = juce::jmax (0.0625, beats); }
        double getSnapBeats() const noexcept { return snapBeats; }

        // Beats per bar, for the ruler's bar lines and numbers.
        void setBeatsPerBar (double beats) noexcept { beatsPerBar = juce::jmax (1.0, beats); }

        // Whether chord names spell black-key roots with flats (Bb) or sharps (A#).
        void setUseFlats (bool shouldUseFlats);

        void refreshFromModel() { repaint(); }

        static constexpr int idealHeight = 84;

    private:
        struct DragItem
        {
            juce::Uuid id;
            double startBeats = 0.0;
            double lengthBeats = 0.0;
        };

        double snapBeats = 0.25;
        double beatsPerBar = 4.0;
        bool useFlats = false;

        static constexpr float rulerHeight = 14.0f;
        static constexpr float loopMarkerHeight = 6.0f;
        static constexpr float blockHeightMax = 140.0f;

        progression::ProgressionModel& model;
        SequencePlayer& player;
        juce::UndoManager& undoManager;

        // NB: juce::Uuid() generates a fresh *random* id; "nothing" is Uuid::null().
        std::vector<juce::Uuid> selection;
        std::vector<progression::ChordEvent> clipboard;

        juce::uint32 lastSeenModelRevision = 0;
        bool interacting = false;

        // Block drag state.
        std::vector<DragItem> dragItems;
        bool draggingBody = false;
        bool draggingRightEdge = false;
        bool dragOutside = false;
        double dragDeltaBeats = 0.0;
        int dragStartMouseX = 0;
        double scaleLockBeats = 0.0;   // > 0: freeze the pixel scale during a drag

        // Loop handle drag state.
        bool draggingLoopStart = false;
        bool draggingLoopEnd = false;

        // Rubber band.
        bool marqueeActive = false;
        bool marqueeMoved = false;
        juce::Point<int> marqueeStart;
        juce::Rectangle<int> marqueeRect;
        std::vector<juce::Uuid> selectionBeforeMarquee;

        // Card-drop preview.
        double dropBeat = -1.0;
        double dropLength = 4.0;

        double totalTimelineBeats() const noexcept;
        float pixelsPerBeat() const noexcept;
        double xToBeats (int x) const noexcept;
        int beatsToX (double beats) const noexcept;
        double snap (double beats) const noexcept;
        double snapDelta (double beats) const noexcept;
        bool isOutside (juce::Point<int> p) const noexcept;

        juce::Rectangle<float> blockBoundsFor (const progression::ChordEvent& event) const noexcept;
        const progression::ChordEvent* eventAt (int x, int y, bool* onRightEdge) const noexcept;

        bool isSelected (const juce::Uuid& id) const noexcept;
        void setSelection (std::vector<juce::Uuid> newSelection);
        void toggleInSelection (const juce::Uuid& id);
        void finishBlockDrag();
        void showContextMenu (const juce::MouseEvent&);
        void addSlotEvent (int slot, double desiredStart);
        static int slotFromDescription (const juce::var& description);

        void timerCallback() override;   // repaints for the moving playhead, and when the model changed underneath us

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimelineComponent)
    };
}
