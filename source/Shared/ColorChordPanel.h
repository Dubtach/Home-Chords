#pragma once

#include "HomeSeriesUI.h"
#include "../MusicTheory/ColorChords.h"
#include <functional>
#include <memory>
#include <vector>

// =============================================================================
// Popup listing the borrowed/colour chords for the current Key/Scale (see
// MusicTheory/ColorChords.h), shown in a juce::CallOutBox. Clicking a chord
// reports its index; the panel stays open so several can be added in a row.
// Hovering shows why you'd reach for it.
// =============================================================================

namespace homeUI
{
    class ColorChordPanel : public juce::Component
    {
    public:
        explicit ColorChordPanel (const std::vector<musictheory::ColorChord>& chordsIn, const juce::String& heading)
            : headingText (heading)
        {
            for (int i = 0; i < static_cast<int> (chordsIn.size()); ++i)
            {
                const auto& chord = chordsIn[static_cast<size_t> (i)];

                auto button = std::make_unique<ChordButton> (chord.label, chord.chordName);
                button->setTooltip (chord.description);
                button->onClick = [this, i] { if (onChosen != nullptr) onChosen (i); };
                addAndMakeVisible (*button);
                buttons.push_back (std::move (button));
            }

            const int rows = juce::jmax (1, (static_cast<int> (buttons.size()) + columns - 1) / columns);
            setSize (columns * cellWidth + 20, 52 + rows * (cellHeight + 6) + 8);
        }

        // Index into the vector the panel was built with.
        std::function<void (int)> onChosen;

        void resized() override
        {
            auto area = getLocalBounds().reduced (10);
            area.removeFromTop (42);

            for (size_t i = 0; i < buttons.size(); ++i)
            {
                const int col = static_cast<int> (i) % columns;
                const int row = static_cast<int> (i) / columns;
                buttons[i]->setBounds (area.getX() + col * cellWidth, area.getY() + row * (cellHeight + 6),
                                       cellWidth - 6, cellHeight);
            }
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (homeUI::face);
            g.setColour (homeUI::faceEdge);
            g.drawRect (getLocalBounds(), 1);

            auto top = getLocalBounds().reduced (10).removeFromTop (38).toFloat();
            g.setFont (homeUI::font (12.0f));
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawText (headingText, top.removeFromTop (18.0f), juce::Justification::centredLeft, false);

            g.setFont (homeUI::font (9.5f, false));
            g.setColour (juce::Colours::white.withAlpha (0.5f));
            g.drawText ("Click to add to the end of the timeline. Hover for why.", top, juce::Justification::centredLeft, false);
        }

    private:
        static constexpr int columns = 3;
        static constexpr int cellWidth = 106;
        static constexpr int cellHeight = 40;

        class ChordButton : public juce::Button
        {
        public:
            ChordButton (const juce::String& labelIn, const juce::String& nameIn)
                : juce::Button (nameIn), label (labelIn), name (nameIn)
            {
            }

            void paintButton (juce::Graphics& g, bool over, bool down) override
            {
                const auto bounds = getLocalBounds().toFloat().reduced (1.0f);

                g.setColour (juce::Colours::black.withAlpha (down ? 0.60f : (over ? 0.45f : 0.32f)));
                g.fillRoundedRectangle (bounds, 5.0f);
                g.setColour (homeUI::pink.withAlpha (over ? 0.85f : 0.45f));
                g.drawRoundedRectangle (bounds, 5.0f, 1.1f);

                auto inner = bounds.reduced (7.0f, 3.0f);
                g.setFont (homeUI::font (9.5f, false));
                g.setColour (juce::Colours::white.withAlpha (0.55f));
                g.drawText (label, inner.removeFromTop (12.0f), juce::Justification::centredLeft, false);

                g.setFont (homeUI::font (14.0f));
                g.setColour (juce::Colours::white);
                g.drawText (name, inner, juce::Justification::centredLeft, false);
            }

        private:
            juce::String label, name;

            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordButton)
        };

        juce::String headingText;
        std::vector<std::unique_ptr<ChordButton>> buttons;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ColorChordPanel)
    };
}
