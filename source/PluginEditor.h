#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Shared/HomeSeriesUI.h"
#include "Shared/ChordEditPanel.h"
#include "Shared/SettingsPanel.h"
#include "Timeline/TimelineComponent.h"

// =============================================================================
// Home-Chords editor. Header (brand, MIDI lamp, settings, Key/Scale),
// chord row, transport/preset strip, the progression timeline, a toolbar
// for the selected timeline event's Rhythm/Bass/Arp (shown only when
// something is selected), and a footer (Auto Inversion, Octave, Velocity,
// Preview Volume).
//
// Keyboard handling: A-J and/or 1-7 (see KEYBOARD_MODE) are polled every
// frame against juce::KeyPress::isKeyCurrentlyDown rather than handled as
// discrete keyPressed events, because that's the only reliable way to
// track several keys held at once. This editor is the component that
// wants keyboard focus; a future text-entry control would naturally take
// focus (and therefore its own key events) away from this via JUCE's
// normal focus routing, same as the existing combo boxes already do.
//
// Right-clicking a chord card opens a quality/extension override popup
// for that slot. In ADD mode, pressing a keyboard/MIDI key also appends
// that slot's current chord to the timeline (mouse clicks never do).
// =============================================================================

class HomeChordsAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer,
                                        private juce::AudioProcessorValueTreeState::Listener
{
public:
    static constexpr int defaultWidth = 920;
    static constexpr int defaultHeight = 700;
    static constexpr int minWidth = 760;
    static constexpr int minHeight = 560;

    explicit HomeChordsAudioProcessorEditor (HomeChordsAudioProcessor&);
    ~HomeChordsAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool keyStateChanged (bool isKeyDown) override;
    void visibilityChanged() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    HomeChordsAudioProcessor& processor;

    std::array<homeUI::ChordCard, musictheory::maxDiatonicSlots> chordCards;
    int currentSlotCount = 7;

    juce::ComboBox keyBox, scaleBox, timeSigBox, presetBox;
    homeUI::Checkbox autoInversionCheckbox { homeUI::green };
    homeUI::Checkbox addModeCheckbox { homeUI::pink };
    homeUI::Checkbox loopCheckbox { homeUI::cyan };
    homeUI::ChevronButton octaveDownButton { homeUI::ChevronButton::left, homeUI::cyan };
    homeUI::ChevronButton octaveUpButton   { homeUI::ChevronButton::right, homeUI::cyan };
    homeUI::Knob velocityKnob { "VELOCITY", homeUI::green };
    homeUI::Knob previewKnob  { "PREVIEW", homeUI::cyan };
    homeUI::Knob tempoKnob    { "TEMPO", homeUI::pink };

    juce::TextButton settingsButton { "..." };
    juce::TextButton playStopButton { "Play" };
    juce::TextButton restartButton { "Restart" };
    juce::TextButton undoButton { "Undo" };
    juce::TextButton redoButton { "Redo" };

    timeline::TimelineComponent timelineComponent;

    // Selected-event toolbar -- only visible while a timeline block is selected.
    juce::ComboBox rhythmBox, bassBox, arpModeBox, arpRateBox;
    juce::TextButton deleteEventButton { "Delete" };
    juce::TextButton duplicateEventButton { "Duplicate" };
    bool somethingSelected = false;
    juce::Uuid selectedEventId;

    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ComboBoxAttachment> keyAttachment, scaleAttachment, timeSigAttachment;
    std::unique_ptr<SliderAttachment> velocityAttachment, previewGainAttachment, tempoAttachment;
    std::unique_ptr<ButtonAttachment> autoInversionAttachment, addModeAttachment, loopAttachment;

    juce::Rectangle<float> brandBounds, lampBounds, footerCardBounds, octaveLabelBounds, octaveValueWellBounds;

    std::array<bool, musictheory::maxDiatonicSlots> keyHeldState {};
    std::array<float, musictheory::maxDiatonicSlots> litAmount {};

    std::atomic<bool> chordsNeedRefresh { true };

    void timerCallback() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void refreshChordCards();
    void nudgeOctave (int delta);
    void releaseAllHeldSlots();
    void showChordEditor (int slot);
    void showSettingsPanel();
    void updateSelectedEventToolbar (const juce::Uuid* id);
    void pushSelectedEventChange();   // reads the toolbar's combos and pushes one UpdateChordAction
    void applyPreset (int presetIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HomeChordsAudioProcessorEditor)
};
