#pragma once

#include "HomeSeriesUI.h"
#include "../MusicTheory/MusicTheoryTypes.h"
#include <memory>
#include <vector>

// =============================================================================
// Compact popup editor for one chord slot's quality/extension override,
// opened via right-click on a ChordCard (see ChordCard::onEditRequested)
// and shown in a juce::CallOutBox. Always keeps the slot's existing root
// note -- this only changes quality/extension, per the "same note,
// different chord" request.
// =============================================================================

namespace homeUI
{
    class ChordEditPanel : public juce::Component
    {
    public:
        ChordEditPanel (const juce::String& rootName, int currentQualityOverride, musictheory::Extension currentExtension)
            : titleText (rootName)
        {
            setSize (256, 132);

            static const juce::StringArray qualityLabels { "Maj", "Min", "Dim", "Aug", "Sus2", "Sus4" };

            for (int i = 0; i < qualityLabels.size(); ++i)
            {
                auto button = std::make_unique<OptionButton> (qualityLabels[i]);
                button->setToggleState (currentQualityOverride == i, juce::dontSendNotification);
                button->onClick = [this, i] { selectQuality (i); };
                addAndMakeVisible (*button);
                qualityButtons.push_back (std::move (button));
            }

            static const juce::StringArray extensionLabels { "None", "6", "7", "Maj7", "Add9" };

            for (int i = 0; i < extensionLabels.size(); ++i)
            {
                auto button = std::make_unique<OptionButton> (extensionLabels[i]);
                button->setToggleState (static_cast<int> (currentExtension) == i, juce::dontSendNotification);
                button->onClick = [this, i] { selectExtension (i); };
                addAndMakeVisible (*button);
                extensionButtons.push_back (std::move (button));
            }

            resetButton.setButtonText ("Reset to Diatonic");
            resetButton.setColour (juce::TextButton::buttonColourId, homeUI::slot);
            resetButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.8f));
            resetButton.onClick = [this] { if (onReset != nullptr) onReset(); };
            addAndMakeVisible (resetButton);
        }

        // -1 for onQualityChosen means "Diatonic" was picked (same as
        // pressing Reset for the quality half only, extension unchanged).
        std::function<void (int)> onQualityChosen;
        std::function<void (musictheory::Extension)> onExtensionChosen;
        std::function<void()> onReset;

        void resized() override
        {
            auto area = getLocalBounds().reduced (10);
            area.removeFromTop (18);   // title row, drawn in paint()

            auto qualityRow = area.removeFromTop (28);
            const int qCount = static_cast<int> (qualityButtons.size());
            const int qWidth = qualityRow.getWidth() / juce::jmax (1, qCount);

            for (int i = 0; i < qCount; ++i)
                qualityButtons[static_cast<size_t> (i)]->setBounds (qualityRow.getX() + i * qWidth, qualityRow.getY(),
                                                                    qWidth - 4, qualityRow.getHeight());

            area.removeFromTop (8);

            auto extensionRow = area.removeFromTop (28);
            const int eCount = static_cast<int> (extensionButtons.size());
            const int eWidth = extensionRow.getWidth() / juce::jmax (1, eCount);

            for (int i = 0; i < eCount; ++i)
                extensionButtons[static_cast<size_t> (i)]->setBounds (extensionRow.getX() + i * eWidth, extensionRow.getY(),
                                                                      eWidth - 4, extensionRow.getHeight());

            area.removeFromTop (10);
            resetButton.setBounds (area.removeFromTop (24));
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (homeUI::face);
            g.setColour (homeUI::faceEdge);
            g.drawRect (getLocalBounds(), 1);

            const auto titleArea = getLocalBounds().reduced (10).removeFromTop (18).toFloat();
            g.setFont (homeUI::font (12.0f));
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.drawText (titleText, titleArea, juce::Justification::centredLeft, false);
        }

    private:
        // Small flat toggle shared by both rows; selection (which one is
        // "on") is managed externally, radio-style, by selectQuality/
        // selectExtension rather than each button toggling itself.
        class OptionButton : public juce::Button
        {
        public:
            explicit OptionButton (const juce::String& label) : juce::Button (label)
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
            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OptionButton)
        };

        void selectQuality (int index)
        {
            for (int i = 0; i < static_cast<int> (qualityButtons.size()); ++i)
                qualityButtons[static_cast<size_t> (i)]->setToggleState (i == index, juce::dontSendNotification);

            if (onQualityChosen != nullptr)
                onQualityChosen (index);
        }

        void selectExtension (int index)
        {
            for (int i = 0; i < static_cast<int> (extensionButtons.size()); ++i)
                extensionButtons[static_cast<size_t> (i)]->setToggleState (i == index, juce::dontSendNotification);

            if (onExtensionChosen != nullptr)
                onExtensionChosen (static_cast<musictheory::Extension> (index));
        }

        juce::String titleText;
        std::vector<std::unique_ptr<OptionButton>> qualityButtons, extensionButtons;
        juce::TextButton resetButton;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordEditPanel)
    };
}
