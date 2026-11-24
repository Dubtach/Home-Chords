#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Shared/HomeSeriesUI.h"
#include "Shared/SettingsPanel.h"
#include "Shared/HelpPanel.h"
#include "Shared/MidiDragTile.h"
#include "Timeline/TimelineComponent.h"

// =============================================================================
// Home-Chords editor -- compact layout, top to bottom:
//
//   header     brand | MIDI lamp | Key | Scale | settings | help
//   chords     the seven chord cards (play them, drag them onto the timeline,
//              right-click for quality / extension / borrowed chords)
//   tools      Play, Restart, Loop, Host sync, time signature, Snap, Presets,
//              Voicing ... Undo, Redo, Export, Drag-out
//   timeline   short strip; drop cards on it, select / move / resize / drag out
//   playback   Rhythm, Bass, Arp mode/rate, Gate -- global, for every chord
//   footer     Octave, Auto inversion, Hints, Velocity, Tempo, Preview,
//              Humanize time / velocity
//
// Keyboard: A-J and/or 1-7 and/or the numpad 1-7 play chords. Chord keys are
// polled every frame with KeyPress::isKeyCurrentlyDown (the only reliable way
// to track several keys held at once) and are ignored while Ctrl/Cmd/Alt is
// held, so the editing shortcuts (undo, select all, ...) never sound a chord.
// Those shortcuts are discrete keyPressed events -- see keyPressed().
// =============================================================================

class HomeChordsAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        public juce::DragAndDropContainer,
                                        private juce::Timer,
                                        private juce::AudioProcessorValueTreeState::Listener
{
public:
    static constexpr int defaultWidth = 820;
    static constexpr int defaultHeight = 346;
    static constexpr int minWidth = 780;
    static constexpr int minHeight = 322;

    explicit HomeChordsAudioProcessorEditor (HomeChordsAudioProcessor&);
    ~HomeChordsAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool keyStateChanged (bool isKeyDown) override;
    bool keyPressed (const juce::KeyPress&) override;
    void visibilityChanged() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    HomeChordsAudioProcessor& processor;
    homeUI::CompactLookAndFeel compactLookAndFeel;   // declared before the children that use it

    std::array<homeUI::ChordCard, musictheory::maxDiatonicSlots> chordCards;
    int currentSlotCount = 7;

    juce::ComboBox keyBox, scaleBox, timeSigBox, snapBox, voicingBox;
    juce::ComboBox rhythmBox, bassBox, arpModeBox, arpRateBox, gateBox;

    homeUI::Checkbox autoInversionCheckbox { homeUI::green };
    homeUI::Checkbox loopCheckbox { homeUI::cyan };
    homeUI::Checkbox syncCheckbox { homeUI::purple };
    homeUI::Checkbox hintsCheckbox { homeUI::green };
    homeUI::ChevronButton octaveDownButton { homeUI::ChevronButton::left, homeUI::cyan };
    homeUI::ChevronButton octaveUpButton   { homeUI::ChevronButton::right, homeUI::cyan };
    homeUI::Knob velocityKnob { "VELOCITY", homeUI::green };
    homeUI::Knob tempoKnob    { "TEMPO", homeUI::pink };
    homeUI::Knob previewKnob  { "PREVIEW", homeUI::cyan };
    homeUI::Knob humanizeTimeKnob { "HUMANIZE", homeUI::purple };
    homeUI::Knob humanizeVelKnob  { "VEL VAR", homeUI::purple };

    juce::TextButton settingsButton { "..." };
    juce::TextButton helpButton { "?" };
    juce::TextButton playStopButton { "Play" };
    juce::TextButton restartButton { "|<" };
    juce::TextButton presetsButton { "Presets" };
    juce::TextButton exportButton { "Export" };
    homeUI::MidiDragTile dragTile;

    timeline::TimelineComponent timelineComponent;

    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ComboBoxAttachment> keyAttachment, scaleAttachment, timeSigAttachment, snapAttachment, voicingAttachment;
    std::unique_ptr<SliderAttachment> velocityAttachment, previewGainAttachment, tempoAttachment;
    std::unique_ptr<SliderAttachment> humanizeTimeAttachment, humanizeVelAttachment;
    std::unique_ptr<ButtonAttachment> autoInversionAttachment, loopAttachment, syncAttachment, hintsAttachment;

    std::unique_ptr<juce::FileChooser> fileChooser;   // kept alive while its async dialog is open
    juce::TooltipWindow tooltipWindow { this, 500 };

    // Next-chord hint bookkeeping (message thread).
    int lastPlayedSlot = -1;
    juce::uint32 previousActiveMask = 0;
    juce::uint32 lastSeenRevision = 0;
    juce::uint32 lastLoopCheckRevision = 0;
    bool lastHintSourceWasLive = false;
    int hintSignature = -1;

    juce::Rectangle<float> brandBounds, lampBounds, footerCardBounds, octaveLabelBounds, octaveValueWellBounds, playbackLabelBounds;

    std::array<bool, musictheory::maxDiatonicSlots> keyHeldState {};
    std::array<float, musictheory::maxDiatonicSlots> litAmount {};

    std::atomic<bool> chordsNeedRefresh { true };

    void timerCallback() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void refreshChordCards();
    void nudgeOctave (int delta);
    void releaseAllHeldSlots();        // computer-keyboard slots only (e.g. on focus loss)
    void releaseAllMouseSlots();       // mouse-held slots (e.g. editor hidden/closed mid-click)

    void showChordMenu (int slot);
    void startCardDrag (int slot);
    void showPresetMenu();
    void applyPreset (int presetIndex);
    void showSettingsPanel();
    void showHelpPanel();
    void exportMidiWithDialog();
    juce::File writeDragFile();
    void updateHints (juce::uint32 triggeredSlots, juce::uint32 activeSlots);
    double snapBeatsFromParam() const;

    // Rhythm / Bass / Arp / Gate act on the selected chords, or on all of them
    // when nothing is selected; the boxes show what that scope currently has,
    // and the three arpeggio boxes only appear while they would do something.
    void playbackBoxChanged (progression::PlaybackField field, juce::ComboBox& box);
    void refreshPlaybackControls();
    std::array<int, 8> playbackSignature { -2, -2, -2, -2, -2, -2, -2, -2 };
    juce::String scopeLabel { "ALL CHORDS" };
    bool syncingPlaybackBoxes = false;

    void performUndo();
    void performRedo();
    bool handleEditingKey (const juce::KeyPress&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HomeChordsAudioProcessorEditor)
};
