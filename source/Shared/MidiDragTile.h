#pragma once

#include "HomeSeriesUI.h"
#include <functional>

// =============================================================================
// A small "grab me" tile: drag it out of the plugin window and drop it onto a
// DAW track or into a folder to deliver the progression as a .mid file.
//
// The tile doesn't know about MIDI. When a drag starts it calls
// `provideFile`, which should write the file and return it (or return a
// non-existent File to cancel, e.g. when the progression is empty).
// =============================================================================

namespace homeUI
{
    class MidiDragTile : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        MidiDragTile()
        {
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
            setTooltip ("Drag this out to a DAW track or a folder to drop the progression as a MIDI file");
        }

        std::function<juce::File()> provideFile;

        void setActive (bool shouldBeActive)
        {
            if (active != shouldBeActive)
            {
                active = shouldBeActive;
                setMouseCursor (active ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
                repaint();
            }
        }

        void paint (juce::Graphics& g) override
        {
            const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
            const float alpha = active ? 1.0f : 0.35f;

            g.setColour (juce::Colours::black.withAlpha (0.30f * alpha + (hover && active ? 0.15f : 0.0f)));
            g.fillRoundedRectangle (bounds, 5.0f);
            g.setColour (homeUI::green.withAlpha (0.55f * alpha));
            g.drawRoundedRectangle (bounds, 5.0f, 1.1f);

            // A small grip (two columns of dots) then the label.
            g.setColour (homeUI::green.withAlpha (0.8f * alpha));
            const float cy = bounds.getCentreY();
            for (int col = 0; col < 2; ++col)
                for (int row = -1; row <= 1; ++row)
                    g.fillEllipse (bounds.getX() + 8.0f + static_cast<float> (col) * 5.0f, cy + static_cast<float> (row) * 5.0f - 1.5f, 3.0f, 3.0f);

            g.setFont (homeUI::font (10.5f));
            g.setColour (juce::Colours::white.withAlpha (0.85f * alpha));
            g.drawText ("DRAG MIDI", bounds.withTrimmedLeft (24.0f), juce::Justification::centredLeft, false);
        }

        void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
        void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }

        void mouseDrag (const juce::MouseEvent& event) override
        {
            if (! active || dragging || event.getDistanceFromDragStart() < 6 || provideFile == nullptr)
                return;

            const auto file = provideFile();

            if (! file.existsAsFile())
                return;

            dragging = true;

            // Static call: works without the editor being a DragAndDropContainer.
            juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this,
                                                                        [safe = juce::Component::SafePointer<MidiDragTile> (this)]
                                                                        {
                                                                            if (safe != nullptr)
                                                                                safe->dragging = false;
                                                                        });
        }

    private:
        bool active = false;
        bool hover = false;
        bool dragging = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiDragTile)
    };
}
