#pragma once

#include "HomeSeriesUI.h"

// =============================================================================
// A small cheat-sheet popup (the "?" button): every shortcut and gesture in
// one place. Plain painted text -- no controls.
// =============================================================================

namespace homeUI
{
    class HelpPanel : public juce::Component
    {
    public:
        HelpPanel()
        {
            setSize (470, 14 + static_cast<int> (rows().size()) * lineHeight + 8);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (homeUI::face);
            g.setColour (homeUI::faceEdge);
            g.drawRect (getLocalBounds(), 1);

            auto area = getLocalBounds().reduced (12, 8).toFloat();

            for (const auto& row : rows())
            {
                auto line = area.removeFromTop (static_cast<float> (lineHeight));

                if (row.second.isEmpty())
                {
                    g.setFont (homeUI::font (9.5f));
                    g.setColour (homeUI::cyan.withAlpha (0.9f));
                    g.drawText (row.first.toUpperCase(), line.withTrimmedTop (4.0f), juce::Justification::centredLeft, false);
                    continue;
                }

                g.setFont (homeUI::font (10.5f));
                g.setColour (juce::Colours::white);
                g.drawText (row.first, line.removeFromLeft (190.0f), juce::Justification::centredLeft, false);

                g.setFont (homeUI::font (10.5f, false));
                g.setColour (juce::Colours::white.withAlpha (0.72f));
                g.drawText (row.second, line, juce::Justification::centredLeft, false);
            }
        }

    private:
        static constexpr int lineHeight = 16;

        static const std::vector<std::pair<juce::String, juce::String>>& rows()
        {
            static const std::vector<std::pair<juce::String, juce::String>> list = {
                { "Play chords", {} },
                { "A S D F G H J  /  1 - 7", "computer keyboard (see the ... settings)" },
                { "Numpad 1 - 7", "same chords, from the number pad" },
                { "MIDI keyboard", "white keys C D E F G A B = chords 1-7, any octave" },
                { "Click a card", "play it with the mouse" },
                { "Build the progression", {} },
                { "Drag a card onto the timeline", "add that chord where you drop it" },
                { "Double-click a card", "append it to the end" },
                { "Right-click a card", "quality, extension, borrowed chords" },
                { "Drag blocks / drag right edge", "move / resize (all selected)" },
                { "Drag blocks off the timeline", "delete them" },
                { "Drag on empty space", "select several chords" },
                { "Keys", {} },
                { "Space  /  Home  /  L", "play-stop / restart / loop" },
                { "Ctrl or Cmd + Z  /  Shift+Z  or  Y", "undo / redo" },
                { "Ctrl or Cmd + A  C  X  V  D", "select all / copy / cut / paste / duplicate" },
                { "Delete  /  Esc", "delete selected / deselect" },
                { "Left  Right  (+Shift)", "nudge selected by the grid (by a bar)" },
                { "Up  Down", "octave" },
            };

            return list;
        }
    };
}
