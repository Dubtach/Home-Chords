#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Shared/HomeSeriesUI.h"

// =============================================================================
// Home-Chords editor -- Phase 1: brand header with a Key/Scale picker and a
// MIDI-out activity lamp, the 7-slot chord row, and a footer strip for
// Octave/Velocity/Preview Volume. No progression timeline yet (Phase 2).
//
// Keyboard handling: A S D F G H J are polled every frame against
// juce::KeyPress::isKeyCurrentlyDown rather than handled as discrete
// keyPressed events, because that's the only reliable way to track
// several keys being held down at once. This editor is the component that
// wants keyboard focus; if a future control (e.g. a Phase 2 BPM box) needs
// text entry, JUCE's normal focus routing sends its key events to that
// control first and this editor's keyStateChanged simply never fires
// while it has focus -- no special-casing needed here for that to work.
// =============================================================================

class HomeChordsAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer,
                                        private juce::AudioProcessorValueTreeState::Listener
{
public:
    static constexpr int defaultWidth = 820;
    static constexpr int defaultHeight = 430;
    static constexpr int minWidth = 640;
    static constexpr int minHeight = 340;

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

    juce::ComboBox keyBox, scaleBox;
    homeUI::ChevronButton octaveDownButton { homeUI::ChevronButton::left, homeUI::cyan };
    homeUI::ChevronButton octaveUpButton   { homeUI::ChevronButton::right, homeUI::cyan };
    homeUI::Knob velocityKnob { "VELOCITY", homeUI::green };
    homeUI::Knob previewKnob  { "PREVIEW", homeUI::cyan };

    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;

    std::unique_ptr<ComboBoxAttachment> keyAttachment, scaleAttachment;
    std::unique_ptr<SliderAttachment> velocityAttachment, previewGainAttachment;

    juce::Rectangle<float> brandBounds, lampBounds, footerCardBounds, octaveLabelBounds, octaveValueWellBounds;

    std::array<bool, musictheory::maxDiatonicSlots> keyHeldState {};
    std::array<float, musictheory::maxDiatonicSlots> litAmount {};

    std::atomic<bool> chordsNeedRefresh { true };

    void timerCallback() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void refreshChordCards();
    void nudgeOctave (int delta);
    void releaseAllHeldSlots();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HomeChordsAudioProcessorEditor)
};
