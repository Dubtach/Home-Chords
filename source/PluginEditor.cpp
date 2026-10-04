#include "PluginEditor.h"
#include "MusicTheory/Presets.h"
#include <cmath>   // std::abs, used by updateSelectedEventToolbar

namespace
{
    // Colour follows chord quality, not an arbitrary per-slot rainbow --
    // see the ChordCard class comment in HomeSeriesUI.h.
    juce::Colour colourForQuality (musictheory::ChordQuality quality)
    {
        switch (quality)
        {
            case musictheory::ChordQuality::Major:      return homeUI::cyan;
            case musictheory::ChordQuality::Minor:      return homeUI::purple;
            case musictheory::ChordQuality::Diminished: return homeUI::warn;
            case musictheory::ChordQuality::Augmented:  return homeUI::pink;
            case musictheory::ChordQuality::Sus2:
            case musictheory::ChordQuality::Sus4:       return homeUI::green;
            default:                                    return homeUI::slotEdge;
        }
    }

    constexpr int trackedLetterKeyCodes[musictheory::maxDiatonicSlots] = { 'A', 'S', 'D', 'F', 'G', 'H', 'J' };
    constexpr int trackedNumberKeyCodes[musictheory::maxDiatonicSlots] = { '1', '2', '3', '4', '5', '6', '7' };
    const juce::String letterKeyCaps[musictheory::maxDiatonicSlots] = { "A", "S", "D", "F", "G", "H", "J" };
    const juce::String numberKeyCaps[musictheory::maxDiatonicSlots] = { "1", "2", "3", "4", "5", "6", "7" };

    constexpr double arpRateValues[4] = { 1.0, 0.5, 0.25, 0.125 };

    void styleComboBox (juce::ComboBox& box)
    {
        box.setColour (juce::ComboBox::backgroundColourId, homeUI::slot);
        box.setColour (juce::ComboBox::textColourId, juce::Colours::white);
        box.setColour (juce::ComboBox::outlineColourId, homeUI::slotEdge);
        box.setColour (juce::ComboBox::arrowColourId, homeUI::cyan);
    }

    void styleTextButton (juce::TextButton& button, juce::Colour accent)
    {
        button.setColour (juce::TextButton::buttonColourId, homeUI::slot);
        button.setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.85f));
        button.setColour (juce::TextButton::textColourOnId, homeUI::ink);
        button.setColour (juce::TextButton::buttonOnColourId, accent);
    }
}

HomeChordsAudioProcessorEditor::HomeChordsAudioProcessorEditor (HomeChordsAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      timelineComponent (p.progression, p.sequencePlayer, p.undoManager)
{
    setWantsKeyboardFocus (true);

    for (auto& card : chordCards)
        addAndMakeVisible (card);

    for (int i = 0; i < musictheory::maxDiatonicSlots; ++i)
    {
        chordCards[static_cast<size_t> (i)].onPressStart = [this, i] { processor.setChordSlotHeldFromMouse (i, true); };
        chordCards[static_cast<size_t> (i)].onPressEnd   = [this, i] { processor.setChordSlotHeldFromMouse (i, false); };
        chordCards[static_cast<size_t> (i)].onEditRequested = [this, i] { showChordEditor (i); };
    }

    keyBox.addItemList (musictheory::getAllKeyNames(), 1);
    scaleBox.addItemList (musictheory::getAllScaleNames(), 1);
    timeSigBox.addItemList ({ "4/4", "3/4", "6/8" }, 1);

    presetBox.addItemList ([] { juce::StringArray names; for (const auto& p : musictheory::getAllPresets()) names.add (p.name); return names; }(), 1);
    presetBox.setTextWhenNothingSelected ("Presets...");

    rhythmBox.addItemList ({ "Sustained", "Whole", "Half", "Quarter", "Eighth", "Sixteenth", "Strum", "Arpeggio" }, 1);
    bassBox.addItemList ({ "Root", "Third", "Fifth", "Inversion", "Custom" }, 1);
    arpModeBox.addItemList ({ "Off", "Up", "Down", "Up/Down", "Random" }, 1);
    arpRateBox.addItemList ({ "1/4", "1/8", "1/16", "1/32" }, 1);

    for (auto* box : { &keyBox, &scaleBox, &timeSigBox, &presetBox, &rhythmBox, &bassBox, &arpModeBox, &arpRateBox })
    {
        styleComboBox (*box);
        addAndMakeVisible (*box);
    }

    addAndMakeVisible (octaveDownButton);
    addAndMakeVisible (octaveUpButton);
    octaveDownButton.onClick = [this] { nudgeOctave (-1); };
    octaveUpButton.onClick   = [this] { nudgeOctave (1); };

    autoInversionCheckbox.setButtonText ("AUTO INVERSION");
    addModeCheckbox.setButtonText ("ADD MODE");
    loopCheckbox.setButtonText ("LOOP");
    addAndMakeVisible (autoInversionCheckbox);
    addAndMakeVisible (addModeCheckbox);
    addAndMakeVisible (loopCheckbox);

    addAndMakeVisible (velocityKnob);
    addAndMakeVisible (previewKnob);
    addAndMakeVisible (tempoKnob);
    velocityKnob.valueText = [] (double v) { return juce::String (static_cast<int> (v)); };
    previewKnob.valueText  = [] (double v) { return juce::String (v, 1) + " dB"; };
    tempoKnob.valueText    = [] (double v) { return juce::String (static_cast<int> (v)); };

    for (auto* button : { &settingsButton, &playStopButton, &restartButton, &undoButton, &redoButton,
                          &deleteEventButton, &duplicateEventButton })
    {
        styleTextButton (*button, homeUI::cyan);
        addAndMakeVisible (*button);
    }

    settingsButton.onClick = [this] { showSettingsPanel(); };

    playStopButton.onClick = [this]
    {
        processor.setSequencePlaying (! processor.sequencePlayer.isPlaying());
    };

    restartButton.onClick = [this] { processor.restartSequencer(); };

    undoButton.onClick = [this]
    {
        processor.undoManager.undo();
        timelineComponent.refreshFromModel();
        refreshChordCards();
    };

    redoButton.onClick = [this]
    {
        processor.undoManager.redo();
        timelineComponent.refreshFromModel();
        refreshChordCards();
    };

    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedItemIndex();
        if (index >= 0)
            applyPreset (index);
        presetBox.setSelectedId (0, juce::dontSendNotification);
    };

    addAndMakeVisible (timelineComponent);
    timelineComponent.onSelectionChanged = [this] (const juce::Uuid* id) { updateSelectedEventToolbar (id); };

    for (auto* box : { &rhythmBox, &bassBox, &arpModeBox, &arpRateBox })
        box->setVisible (false);
    deleteEventButton.setVisible (false);
    duplicateEventButton.setVisible (false);

    rhythmBox.onChange = [this] { pushSelectedEventChange(); };
    bassBox.onChange = [this] { pushSelectedEventChange(); };
    arpModeBox.onChange = [this] { pushSelectedEventChange(); };
    arpRateBox.onChange = [this] { pushSelectedEventChange(); };

    deleteEventButton.onClick = [this] { timelineComponent.deleteSelected(); };
    duplicateEventButton.onClick = [this] { timelineComponent.duplicateSelected(); timelineComponent.refreshFromModel(); };

    keyAttachment          = std::make_unique<ComboBoxAttachment> (processor.apvts, "KEY", keyBox);
    scaleAttachment        = std::make_unique<ComboBoxAttachment> (processor.apvts, "SCALE", scaleBox);
    timeSigAttachment      = std::make_unique<ComboBoxAttachment> (processor.apvts, "TIME_SIG", timeSigBox);
    velocityAttachment     = std::make_unique<SliderAttachment> (processor.apvts, "VELOCITY", velocityKnob);
    previewGainAttachment  = std::make_unique<SliderAttachment> (processor.apvts, "PREVIEW_GAIN", previewKnob);
    tempoAttachment        = std::make_unique<SliderAttachment> (processor.apvts, "TEMPO", tempoKnob);
    autoInversionAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "AUTO_INVERSION", autoInversionCheckbox);
    addModeAttachment       = std::make_unique<ButtonAttachment> (processor.apvts, "ADD_MODE", addModeCheckbox);
    loopAttachment          = std::make_unique<ButtonAttachment> (processor.apvts, "LOOP_ENABLED", loopCheckbox);

    processor.apvts.addParameterListener ("KEY", this);
    processor.apvts.addParameterListener ("SCALE", this);
    processor.apvts.addParameterListener ("NOTE_SPELLING", this);

    refreshChordCards();

    setResizable (true, true);
    setResizeLimits (minWidth, minHeight, 1800, 1100);
    setSize (defaultWidth, defaultHeight);

    startTimerHz (30);
}

HomeChordsAudioProcessorEditor::~HomeChordsAudioProcessorEditor()
{
    stopTimer();
    processor.apvts.removeParameterListener ("KEY", this);
    processor.apvts.removeParameterListener ("SCALE", this);
    processor.apvts.removeParameterListener ("NOTE_SPELLING", this);
    releaseAllHeldSlots();
}

void HomeChordsAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto header = area.removeFromTop (54);
    area.removeFromTop (6);
    auto footer = area.removeFromBottom (90);
    area.removeFromBottom (6);
    auto chordRow = area.removeFromTop (130);
    area.removeFromTop (6);
    auto transportRow = area.removeFromTop (30);
    area.removeFromTop (4);
    auto toolbarRow = area.removeFromTop (28);
    area.removeFromTop (4);
    auto timelineArea = area;   // whatever's left

    // ---- header: brand | lamp | settings | key | scale ----
    auto rightHeader = header.removeFromRight (344);
    keyBox.setBounds (rightHeader.removeFromLeft (158).reduced (6, 13));
    rightHeader.removeFromLeft (6);
    scaleBox.setBounds (rightHeader.reduced (6, 13));
    header.removeFromRight (8);
    settingsButton.setBounds (header.removeFromRight (34).reduced (0, 13));
    header.removeFromRight (8);
    lampBounds = header.removeFromRight (90).toFloat();
    brandBounds = header.toFloat();

    // ---- chord row ----
    const int shownCount = juce::jlimit (1, musictheory::maxDiatonicSlots, currentSlotCount);
    const int cardWidth = chordRow.getWidth() / shownCount;

    for (int i = 0; i < musictheory::maxDiatonicSlots; ++i)
    {
        auto& card = chordCards[static_cast<size_t> (i)];
        const bool shown = i < shownCount;
        card.setVisible (shown);

        if (shown)
        {
            const int x = chordRow.getX() + i * cardWidth;
            const int width = (i == shownCount - 1) ? (chordRow.getRight() - x) : cardWidth;
            card.setBounds (x, chordRow.getY(), width, chordRow.getHeight());
        }
    }

    // ---- transport row: play | restart | loop | tempo-ish | time sig | add mode | undo | redo | presets ----
    playStopButton.setBounds (transportRow.removeFromLeft (56));
    transportRow.removeFromLeft (4);
    restartButton.setBounds (transportRow.removeFromLeft (62));
    transportRow.removeFromLeft (10);
    loopCheckbox.setBounds (transportRow.removeFromLeft (64));
    transportRow.removeFromLeft (10);
    timeSigBox.setBounds (transportRow.removeFromLeft (64));
    transportRow.removeFromLeft (10);
    addModeCheckbox.setBounds (transportRow.removeFromLeft (90));
    transportRow.removeFromLeft (10);
    presetBox.setBounds (transportRow.removeFromLeft (140));
    transportRow.removeFromLeft (10);
    redoButton.setBounds (transportRow.removeFromRight (56));
    transportRow.removeFromRight (4);
    undoButton.setBounds (transportRow.removeFromRight (56));

    // ---- selected-event toolbar ----
    rhythmBox.setBounds (toolbarRow.removeFromLeft (110));
    toolbarRow.removeFromLeft (6);
    bassBox.setBounds (toolbarRow.removeFromLeft (90));
    toolbarRow.removeFromLeft (6);
    arpModeBox.setBounds (toolbarRow.removeFromLeft (90));
    toolbarRow.removeFromLeft (6);
    arpRateBox.setBounds (toolbarRow.removeFromLeft (70));
    toolbarRow.removeFromLeft (10);
    duplicateEventButton.setBounds (toolbarRow.removeFromRight (84));
    toolbarRow.removeFromRight (6);
    deleteEventButton.setBounds (toolbarRow.removeFromRight (70));

    // ---- timeline ----
    timelineComponent.setBounds (timelineArea);

    // ---- footer: octave stepper | auto inversion | velocity | tempo | preview volume ----
    footerCardBounds = footer.toFloat();
    auto footerContent = footer.reduced (14, 10);

    auto octaveArea = footerContent.removeFromLeft (150);
    octaveLabelBounds = octaveArea.removeFromTop (16).toFloat();
    octaveArea.removeFromTop (2);
    octaveDownButton.setBounds (octaveArea.removeFromLeft (26));
    octaveArea.removeFromLeft (4);
    octaveUpButton.setBounds (octaveArea.removeFromRight (26));
    octaveArea.removeFromRight (4);
    octaveValueWellBounds = octaveArea.toFloat();

    footerContent.removeFromLeft (16);

    auto autoInversionArea = footerContent.removeFromLeft (130);
    autoInversionCheckbox.setBounds (autoInversionArea.withSizeKeepingCentre (autoInversionArea.getWidth(), 22));

    footerContent.removeFromLeft (16);

    auto knobArea = footerContent;
    const int knobWidth = knobArea.getWidth() / 3;
    velocityKnob.setBounds (knobArea.removeFromLeft (knobWidth).reduced (10, 0));
    tempoKnob.setBounds (knobArea.removeFromLeft (knobWidth).reduced (10, 0));
    previewKnob.setBounds (knobArea.reduced (10, 0));
}

void HomeChordsAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (homeUI::chassis);

    auto faceBounds = getLocalBounds().toFloat().reduced (4.0f);
    g.setColour (homeUI::face);
    g.fillRoundedRectangle (faceBounds, 8.0f);
    g.setColour (homeUI::faceEdge);
    g.drawRoundedRectangle (faceBounds, 8.0f, 1.2f);

    if (! brandBounds.isEmpty())
        homeUI::drawBrand (g, "Chords", homeUI::cyan, brandBounds.getX(), brandBounds.getY(), brandBounds.getRight());

    if (! lampBounds.isEmpty())
        homeUI::drawLamp (g, lampBounds, "MIDI OUT", homeUI::green, processor.getMidiActivity(), false);

    if (! footerCardBounds.isEmpty())
    {
        const auto footerCardColour = homeUI::cyan.withSaturation (0.30f).withBrightness (0.50f);
        homeUI::drawCard (g, footerCardBounds, footerCardColour);

        homeUI::drawCardText (g, "OCTAVE", octaveLabelBounds, 9.5f, juce::Justification::centredLeft, 0.75f);

        homeUI::drawWell (g, octaveValueWellBounds, 4.0f);
        g.setFont (homeUI::font (15.0f));
        g.setColour (juce::Colours::white);
        const int octaveValue = static_cast<int> (processor.apvts.getRawParameterValue ("OCTAVE")->load());
        const auto octaveText = (octaveValue > 0 ? "+" : juce::String()) + juce::String (octaveValue);
        g.drawText (octaveText, octaveValueWellBounds, juce::Justification::centred, false);
    }
}

bool HomeChordsAudioProcessorEditor::keyStateChanged (bool /*isKeyDown*/)
{
    const auto mode = static_cast<int> (processor.apvts.getRawParameterValue ("KEYBOARD_MODE")->load());
    // 0 = Letters, 1 = Numbers, 2 = Both

    bool handledAny = false;

    for (int i = 0; i < musictheory::maxDiatonicSlots; ++i)
    {
        bool down = false;

        if (mode == 0 || mode == 2)
            down = down || juce::KeyPress::isKeyCurrentlyDown (trackedLetterKeyCodes[i]);
        if (mode == 1 || mode == 2)
            down = down || juce::KeyPress::isKeyCurrentlyDown (trackedNumberKeyCodes[i]);

        if (down != keyHeldState[static_cast<size_t> (i)])
        {
            keyHeldState[static_cast<size_t> (i)] = down;
            processor.setChordSlotHeld (i, down);
            handledAny = true;
        }
    }

    return handledAny;
}

void HomeChordsAudioProcessorEditor::visibilityChanged()
{
    if (isVisible())
        grabKeyboardFocus();
    else
        releaseAllHeldSlots();
}

void HomeChordsAudioProcessorEditor::mouseDown (const juce::MouseEvent&)
{
    grabKeyboardFocus();
}

void HomeChordsAudioProcessorEditor::timerCallback()
{
    if (chordsNeedRefresh.exchange (false, std::memory_order_relaxed))
        refreshChordCards();

    if (! hasKeyboardFocus (true))
        releaseAllHeldSlots();

    const auto activeMask = processor.getActiveSlotMask();

    for (int i = 0; i < musictheory::maxDiatonicSlots; ++i)
    {
        const bool active = (activeMask & (1u << static_cast<unsigned> (i))) != 0;
        auto& amount = litAmount[static_cast<size_t> (i)];
        amount = active ? 1.0f : amount * 0.80f;
        chordCards[static_cast<size_t> (i)].setLitAmount (amount);
    }

    // ADD mode: drain triggered slots every tick regardless of mode, so
    // switching into ADD mode later doesn't replay stale triggers from
    // while it was off; only act on them when ADD mode is actually on.
    const auto triggered = processor.consumeTriggeredSlotsForAddMode();
    const bool addMode = processor.apvts.getRawParameterValue ("ADD_MODE")->load() >= 0.5f;

    if (addMode && triggered != 0)
    {
        for (int i = 0; i < musictheory::maxDiatonicSlots; ++i)
            if ((triggered & (1u << static_cast<unsigned> (i))) != 0)
                processor.addChordFromSlotToTimeline (i);

        timelineComponent.refreshFromModel();
    }

    undoButton.setEnabled (processor.undoManager.canUndo());
    redoButton.setEnabled (processor.undoManager.canRedo());
    playStopButton.setButtonText (processor.sequencePlayer.isPlaying() ? "Stop" : "Play");

    repaint (lampBounds.getSmallestIntegerContainer());
    repaint (octaveValueWellBounds.getSmallestIntegerContainer());
}

void HomeChordsAudioProcessorEditor::parameterChanged (const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused (parameterID, newValue);
    // apvts.Listener callbacks can arrive off the message thread (host
    // automation), so this just raises a flag -- the actual rebuild
    // happens in timerCallback() on the message thread.
    chordsNeedRefresh.store (true, std::memory_order_relaxed);
}

void HomeChordsAudioProcessorEditor::refreshChordCards()
{
    const auto chords = processor.getCurrentDiatonicChords();
    currentSlotCount = juce::jlimit (1, musictheory::maxDiatonicSlots, static_cast<int> (chords.size()));

    const auto mode = static_cast<int> (processor.apvts.getRawParameterValue ("KEYBOARD_MODE")->load());
    const auto& caps = (mode == 1) ? numberKeyCaps : letterKeyCaps;   // Numbers-only shows digits; Letters/Both show letters

    for (int i = 0; i < musictheory::maxDiatonicSlots; ++i)
    {
        auto& card = chordCards[static_cast<size_t> (i)];

        if (i < static_cast<int> (chords.size()))
        {
            const auto& chord = chords[static_cast<size_t> (i)];
            card.setContent (caps[i], chord.romanNumeral, chord.chordName, colourForQuality (chord.quality));
            card.setHasOverride (processor.getSlotQualityOverride (i) >= 0
                                  || processor.getSlotExtensionOverride (i) != musictheory::Extension::None);
        }
        else
        {
            card.setEmpty();
        }
    }

    resized();
}

void HomeChordsAudioProcessorEditor::nudgeOctave (int delta)
{
    // Hardcoded to match the OCTAVE parameter's declared range in
    // PluginProcessor::createParameters() -- simpler and just as correct
    // as querying it back, since this editor owns no other source of
    // truth for that range.
    if (auto* param = dynamic_cast<juce::AudioParameterInt*> (processor.apvts.getParameter ("OCTAVE")))
        *param = juce::jlimit (-2, 2, param->get() + delta);
}

void HomeChordsAudioProcessorEditor::releaseAllHeldSlots()
{
    for (int i = 0; i < musictheory::maxDiatonicSlots; ++i)
    {
        keyHeldState[static_cast<size_t> (i)] = false;
        processor.setChordSlotHeld (i, false);
    }
}

void HomeChordsAudioProcessorEditor::showChordEditor (int slot)
{
    if (slot < 0 || slot >= currentSlotCount)
        return;

    const auto chords = processor.getCurrentDiatonicChords();

    if (slot >= static_cast<int> (chords.size()))
        return;

    const auto rootName = chords[static_cast<size_t> (slot)].rootName;
    const auto currentQuality = processor.getSlotQualityOverride (slot);
    const auto currentExtension = processor.getSlotExtensionOverride (slot);

    auto panel = std::make_unique<homeUI::ChordEditPanel> (rootName, currentQuality, currentExtension);

    panel->onQualityChosen = [this, slot] (int quality)
    {
        processor.setSlotQualityOverride (slot, quality);
        refreshChordCards();
    };

    panel->onExtensionChosen = [this, slot] (musictheory::Extension extension)
    {
        processor.setSlotExtensionOverride (slot, extension);
        refreshChordCards();
    };

    panel->onReset = [this, slot]
    {
        processor.clearSlotOverride (slot);
        refreshChordCards();
    };

    const auto targetBounds = chordCards[static_cast<size_t> (slot)].getScreenBounds();
    juce::CallOutBox::launchAsynchronously (std::move (panel), targetBounds, nullptr);
}

void HomeChordsAudioProcessorEditor::showSettingsPanel()
{
    const auto keyboardMode = static_cast<int> (processor.apvts.getRawParameterValue ("KEYBOARD_MODE")->load());
    const auto noteSpelling = static_cast<int> (processor.apvts.getRawParameterValue ("NOTE_SPELLING")->load());

    auto panel = std::make_unique<homeUI::SettingsPanel> (keyboardMode, noteSpelling);

    panel->onKeyboardModeChosen = [this] (int index)
    {
        if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter ("KEYBOARD_MODE")))
            *param = index;
        refreshChordCards();
    };

    panel->onNoteSpellingChosen = [this] (int index)
    {
        if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter ("NOTE_SPELLING")))
            *param = index;
        refreshChordCards();
    };

    const auto targetBounds = settingsButton.getScreenBounds();
    juce::CallOutBox::launchAsynchronously (std::move (panel), targetBounds, nullptr);
}

void HomeChordsAudioProcessorEditor::updateSelectedEventToolbar (const juce::Uuid* id)
{
    somethingSelected = (id != nullptr);
    selectedEventId = (id != nullptr) ? *id : juce::Uuid();

    for (auto* box : { &rhythmBox, &bassBox, &arpModeBox, &arpRateBox })
        box->setVisible (somethingSelected);
    deleteEventButton.setVisible (somethingSelected);
    duplicateEventButton.setVisible (somethingSelected);

    if (! somethingSelected)
        return;

    const auto index = processor.progression.indexOfEvent (selectedEventId);
    if (index < 0)
        return;

    const auto& event = processor.progression.getEvent (index);

    rhythmBox.setSelectedItemIndex (static_cast<int> (event.rhythm), juce::dontSendNotification);
    bassBox.setSelectedItemIndex (static_cast<int> (event.bassMode), juce::dontSendNotification);
    arpModeBox.setSelectedItemIndex (static_cast<int> (event.arpMode), juce::dontSendNotification);

    int closestRateIndex = 2;
    double closestDistance = 1.0e9;
    for (int i = 0; i < 4; ++i)
    {
        const double distance = std::abs (arpRateValues[i] - event.arpRateBeats);
        if (distance < closestDistance)
        {
            closestDistance = distance;
            closestRateIndex = i;
        }
    }
    arpRateBox.setSelectedItemIndex (closestRateIndex, juce::dontSendNotification);
}

void HomeChordsAudioProcessorEditor::pushSelectedEventChange()
{
    if (! somethingSelected)
        return;

    const auto index = processor.progression.indexOfEvent (selectedEventId);
    if (index < 0)
        return;

    auto updated = processor.progression.getEvent (index);
    updated.rhythm = static_cast<progression::RhythmPattern> (juce::jlimit (0, 7, rhythmBox.getSelectedItemIndex()));
    updated.bassMode = static_cast<progression::BassMode> (juce::jlimit (0, 4, bassBox.getSelectedItemIndex()));
    updated.arpMode = static_cast<progression::ArpMode> (juce::jlimit (0, 4, arpModeBox.getSelectedItemIndex()));
    updated.arpRateBeats = arpRateValues[juce::jlimit (0, 3, arpRateBox.getSelectedItemIndex())];

    timelineComponent.updateSelected (updated);
}

void HomeChordsAudioProcessorEditor::applyPreset (int presetIndex)
{
    const auto& presets = musictheory::getAllPresets();
    if (presetIndex < 0 || presetIndex >= static_cast<int> (presets.size()))
        return;

    const auto builtModel = musictheory::buildProgressionFromPreset (presets[static_cast<size_t> (presetIndex)],
                                                                      processor.getKeyTonicPitchClass(),
                                                                      processor.getScaleType(),
                                                                      processor.getBeatsPerBar());

    processor.undoManager.beginNewTransaction ("Apply preset");
    processor.undoManager.perform (new progression::ClearProgressionAction (processor.progression));

    for (int i = 0; i < builtModel.size(); ++i)
        processor.undoManager.perform (new progression::AddChordAction (processor.progression, builtModel.getEvent (i)));

    timelineComponent.refreshFromModel();
}
