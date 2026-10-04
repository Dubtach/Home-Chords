#pragma once

#include "HomeSeriesUI.h"
#include <functional>
#include <vector>
#include <memory>

// =============================================================================
// Compact popup for plugin-wide preferences: which computer-keyboard keys
// trigger chords (Letters/Numbers/Both), and note-name spelling (Auto/
// Sharps/Flats). Opened via the header's settings button, shown in a
// juce::CallOutBox -- same pattern as ChordEditPanel, but this file
// defines its own small option-toggle class rather than sharing
// ChordEditPanel's, to keep the two panels independent of each other.
// =============================================================================

namespace homeUI
{
    class SettingsPanel : public juce::Component
    {
    public:
        SettingsPanel (int currentKeyboardMode, int currentNoteSpelling)
        {
            setSize (240, 132);

            static const juce::StringArray keyboardLabels { "Letters", "Numbers", "Both" };
            buildRow (keyboardButtons, keyboardLabels, currentKeyboardMode,
                      [this] (int index) { if (onKeyboardModeChosen != nullptr) onKeyboardModeChosen (index); });

            static const juce::StringArray spellingLabels { "Auto", "Sharps", "Flats" };
            buildRow (spellingButtons, spellingLabels, currentNoteSpelling,
                      [this] (int index) { if (onNoteSpellingChosen != nullptr) onNoteSpellingChosen (index); });
        }

        std::function<void (int)> onKeyboardModeChosen;
        std::function<void (int)> onNoteSpellingChosen;

        void resized() override
        {
            auto area = getLocalBounds().reduced (10);

            area.removeFromTop (16);   // "KEYBOARD INPUT" label, drawn in paint()
            layoutRow (keyboardButtons, area.removeFromTop (26));

            area.removeFromTop (18);   // "NOTE SPELLING" label
            layoutRow (spellingButtons, area.removeFromTop (26));
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (homeUI::face);
            g.setColour (homeUI::faceEdge);
            g.drawRect (getLocalBounds(), 1);

            auto area = getLocalBounds().reduced (10).toFloat();
            drawCardText (g, "KEYBOARD INPUT", area.removeFromTop (16.0f), 9.5f, juce::Justification::centredLeft, 0.7f);

            area.removeFromTop (26.0f + 18.0f);
            drawCardText (g, "NOTE SPELLING", area.removeFromTop (16.0f), 9.5f, juce::Justification::centredLeft, 0.7f);
        }

    private:
        class ToggleButton : public juce::Button
        {
        public:
            explicit ToggleButton (const juce::String& label) : juce::Button (label)
            {
                setClickingTogglesState (false);
                setButtonText (label);
            }

            void paintButton (juce::Graphics& g, bool over, bool down) override
            {
                const auto bounds = getLocalBounds().toFloat();
                const bool selected = getToggleState();

                g.setColour (selected ? homeUI::cyan.withAlpha (0.85f)
                                       : juce::Colours::black.withAlpha (down ? 0.55f : (over ? 0.42f : 0.30f)));
                g.fillRoundedRectangle (bounds, 4.0f);
                g.setColour (selected ? juce::Colours::white : juce::Colours::white.withAlpha (0.5f));
                g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

                g.setFont (homeUI::font (10.5f, selected));
                g.setColour (selected ? homeUI::ink : juce::Colours::white.withAlpha (0.8f));
                g.drawText (getButtonText(), bounds, juce::Justification::centred, false);
            }

        private:
            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToggleButton)
        };

        void buildRow (std::vector<std::unique_ptr<ToggleButton>>& row, const juce::StringArray& labels,
                       int selectedIndex, std::function<void (int)> onChosen)
        {
            for (int i = 0; i < labels.size(); ++i)
            {
                auto button = std::make_unique<ToggleButton> (labels[i]);
                button->setToggleState (i == selectedIndex, juce::dontSendNotification);

                auto* rowPtr = &row;
                button->onClick = [this, rowPtr, i, onChosen]
                {
                    for (auto& b : *rowPtr)
                        b->setToggleState (false, juce::dontSendNotification);
                    (*rowPtr)[static_cast<size_t> (i)]->setToggleState (true, juce::dontSendNotification);
                    onChosen (i);
                };

                addAndMakeVisible (*button);
                row.push_back (std::move (button));
            }
        }

        static void layoutRow (std::vector<std::unique_ptr<ToggleButton>>& row, juce::Rectangle<int> area)
        {
            const int count = static_cast<int> (row.size());
            const int width = area.getWidth() / juce::jmax (1, count);

            for (int i = 0; i < count; ++i)
                row[static_cast<size_t> (i)]->setBounds (area.getX() + i * width, area.getY(), width - 4, area.getHeight());
        }

        std::vector<std::unique_ptr<ToggleButton>> keyboardButtons, spellingButtons;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsPanel)
    };
}
