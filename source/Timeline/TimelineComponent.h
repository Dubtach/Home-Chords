#pragma once
#include <map>

#include "../Progression/ProgressionModel.h"
#include "../Progression/ProgressionActions.h"
#include "SequencePlayer.h"
#include "../Shared/HomeSeriesUI.h"
#include <JuceHeader.h>
#include <functional>
#include <vector>

// =============================================================================
// The progression timeline: a short strip of chord blocks with a bar ruler,
// playhead and loop shading. It is a "magnetic" track (see TimelineLayout.h):
// chords always sit back to back from beat 0 -- they can't overlap and can't
// leave a gap -- and the loop always spans exactly the chords.
//
//   * Drop a chord card on the middle of a chord to REPLACE it (it keeps its
//     length and style), or near a chord's edge / after the last chord to INSERT
//     it there -- the chords after it make room. Hold Shift to always insert.
//   * Click a block to select it; shift/ctrl/cmd-click adds to the selection;
//     drag on empty space to rubber-band select several.
//   * Drag the selection sideways to reorder it (it lands in the gap its left
//     edge is nearest, so a long chord needs no extra travel); drag the right edge of a block
//     to resize the whole selection (later chords shift to follow); drag it out
//     of the strip (above, below or off the side) and let go to delete it --
//     the gap closes up.
//   * Right-click anywhere for Grid snap (the snap resolution lives here, not
//     in the toolbar) plus Delete / Duplicate / Copy / Paste / Select all.
//
// Drags are previewed without touching the model and applied as one undo step
// when released. There is no horizontal scrolling: the visible width is scaled
// to fit the progression; the scale is frozen while a drag is in progress so
// blocks don't slide under the mouse.
//
// Rhythm / Bass / Arp / Gate live on the chords but are edited from the
// editor's playback row (selection, or all chords), not on the blocks.
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
        void mouseDoubleClick (const juce::MouseEvent&) override;
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
        void nudgeSelected (double deltaBeats);   // moves the selection one place earlier (<0) or later (>0)
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

        // The snap choices shown under right-click > Grid snap (supplied by the
        // editor, which owns the SNAP parameter), the index currently in force,
        // and what to call when one is picked.
        juce::StringArray snapChoiceNames;
        std::function<int()> getSnapChoice;
        std::function<void (int)> onSnapChoice;

        // The top-right corner of the ruler is kept clear for the editor's
        // time-signature chip.
        static constexpr int cornerWidth = 68;

        static constexpr int idealHeight = 130;

    private:
        double snapBeats = 0.25;
        double beatsPerBar = 4.0;
        bool useFlats = false;

        static constexpr float rulerHeight = 18.0f;
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

        // Live preview of a gesture in progress (the model is untouched until release).
        bool previewActive = false;
        std::vector<progression::ChordEvent> previewEvents;
        juce::Uuid ghostId;   // the card being dragged in, drawn dashed

        // Block drag state.
        std::vector<juce::Uuid> dragIds;
        std::vector<progression::ChordEvent> dragOriginal;
        bool draggingBody = false;
        bool draggingRightEdge = false;
        bool dragOutside = false;
        int dragStartMouseX = 0;
        int dragStartMouseY = 0;
        double scaleLockBeats = 0.0;   // > 0: freeze the pixel scale during a drag

        // Rubber band.
        bool marqueeActive = false;
        bool marqueeMoved = false;
        juce::Point<int> marqueeStart;
        juce::Rectangle<int> marqueeRect;
        std::vector<juce::Uuid> selectionBeforeMarquee;

        // Card drop: the chord being dragged in (shown in its slot as a ghost).
        progression::ChordEvent dropEvent;
        double dropLength = 4.0;

        // While dragging blocks they float under the pointer; the slot they will land in stays open.
        juce::Point<int> dragOffset;
        double dragGroupStart = 0.0;      // where the dragged group's left edge started, in beats
        juce::Uuid replaceId;             // card drop: the chord that would be replaced (null = inserting)

        // Card drop: the dragged card floats under the pointer, like a block being reordered.
        bool cardActive = false;
        juce::Point<float> cardPointer;

        // Shift+click toggles selection, Shift+drag duplicates the selected chords.
        bool duplicateArmed = false;
        bool duplicating = false;
        bool shiftWasSelected = false;
        bool shiftMoved = false;
        juce::Uuid shiftId;

        // Smooth shifting: each block eases toward where the layout says it belongs.
        struct Anim { double start = 0.0, length = 0.0; };
        std::map<juce::Uuid, Anim> anim;
        bool stepAnimation();
        juce::Rectangle<float> animatedBoundsFor (const progression::ChordEvent& event) const;

        std::vector<progression::ChordEvent> shown() const;   // the preview while a gesture runs, else the model
        double totalTimelineBeats() const noexcept;
        float pixelsPerBeat() const noexcept;
        double xToBeats (int x) const noexcept;
        int beatsToX (double beats) const noexcept;
        double snapDelta (double beats) const noexcept;
        bool isOutside (juce::Point<int> p) const noexcept;

        juce::Rectangle<float> blockBoundsFor (const progression::ChordEvent& event) const noexcept;
        bool eventAt (int x, int y, juce::Uuid& idOut, bool* onRightEdge) const;

        bool isSelected (const juce::Uuid& id) const noexcept;
        void setSelection (std::vector<juce::Uuid> newSelection);
        void toggleInSelection (const juce::Uuid& id);
        void commit (std::vector<progression::ChordEvent> events, const juce::String& undoName);
        void endGesture();
        void showContextMenu (const juce::MouseEvent&);
        void drawBlock (juce::Graphics&, const progression::ChordEvent&, juce::Rectangle<float> bounds, bool selected, bool floating, bool slot, bool ghost, bool replacing = false) const;
        static int slotFromDescription (const juce::var& description);

    void timerCallback() override;   // repaints for the moving playhead, and when the model changed underneath us

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimelineComponent)
    };
}
