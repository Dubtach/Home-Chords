#include "PluginEditor.h"
#include "MusicTheory/Presets.h"
#include <cmath>

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
    const int trackedNumpadKeyCodes[musictheory::maxDiatonicSlots] = {
        juce::KeyPress::numberPad1, juce::KeyPress::numberPad2, juce::KeyPress::numberPad3, juce::KeyPress::numberPad4,
        juce::KeyPress::numberPad5, juce::KeyPress::numberPad6, juce::KeyPress::numberPad7 };

    const juce::String letterKeyCaps[musictheory::maxDiatonicSlots] = { "A", "S", "D", "F", "G", "H", "J" };
    const juce::String numberKeyCaps[musictheory::maxDiatonicSlots] = { "1", "2", "3", "4", "5", "6", "7" };

    void styleComboBox (juce::ComboBox& box)
    {
        box.setColour (juce::ComboBox::backgroundColourId, homeUI::slot);
        box.setColour (juce::ComboBox::textColourId, juce::Colours::white);
        box.setColour (juce::ComboBox::outlineColourId, homeUI::slotEdge);
        box.setColour (juce::ComboBox::arrowColourId, homeUI::cyan);
        box.setJustificationType (juce::Justification::centredLeft);
    }

    void styleTextButton (juce::TextButton& button, juce::Colour accent)
    {
        button.setColour (juce::TextButton::buttonColourId, homeUI::slot);
        button.setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.85f));
        button.setColour (juce::TextButton::textColourOnId, homeUI::ink);
        button.setColour (juce::TextButton::buttonOnColourId, accent);
    }

    // Lays components out left to right in `row`, each `width` wide, then skips `gap`.
    void put (juce::Rectangle<int>& row, juce::Component& component, int width, int gap = 4)
    {
        component.setBounds (row.removeFromLeft (width));
        row.removeFromLeft (gap);
    }

    bool isLetter (const juce::KeyPress& key, char letter)
    {
        const int code = key.getKeyCode();
        return code == letter || code == (letter + ('a' - 'A'));
    }
}

HomeChordsAudioProcessorEditor::HomeChordsAudioProcessorEditor (HomeChordsAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      timelineComponent (p.progression, p.sequencePlayer, p.undoManager)
{
    setWantsKeyboardFocus (true);
    setLookAndFeel (&compactLookAndFeel);

    for (auto& card : chordCards)
        addAndMakeVisible (card);

    for (int i = 0; i < musictheory::maxDiatonicSlots; ++i)
    {
        auto& card = chordCards[static_cast<size_t> (i)];
        card.onPressStart     = [this, i] { processor.setChordSlotHeldFromMouse (i, true); };
        card.onPressEnd       = [this, i] { processor.setChordSlotHeldFromMouse (i, false); };
        card.onEditRequested  = [this, i] { showChordMenu (i); };
        card.onDragRequested  = [this, i] { startCardDrag (i); };
        card.onDoubleClicked  = [this, i] { processor.addChordFromSlotToTimeline (i); timelineComponent.refreshFromModel(); };
    }

    snapBox.addItemList ({ "1/16", "1/8", "1/4", "1/2", "Bar" }, 1);
    voicingBox.addItemList ({ "Close", "Open", "Wide", "Piano" }, 1);
    keyBox.addItemList (musictheory::getAllKeyNames(), 1);
    scaleBox.addItemList (musictheory::getAllScaleNames(), 1);
    timeSigBox.addItemList ({ "4/4", "3/4", "6/8" }, 1);

    rhythmBox.addItemList ({ "Sustained", "Whole", "Half", "Quarter", "Eighth", "Sixteenth", "Strum", "Arpeggio" }, 1);
    bassBox.addItemList ({ "Bass: Root", "Bass: Third", "Bass: Fifth", "Bass: Inversion" }, 1);
    arpModeBox.addItemList ({ "Arp: Off", "Arp: Up", "Arp: Down", "Arp: Up/Down", "Arp: Random" }, 1);
    arpRateBox.addItemList ({ "1/4", "1/8", "1/16", "1/32" }, 1);
    gateBox.addItemList ({ "Gate 25%", "Gate 50%", "Gate 80%", "Legato" }, 1);

    snapBox.setTooltip ("Timeline snap grid");
    voicingBox.setTooltip ("How chords are spread: Close, Open (drop-2), Wide (open + low root), Piano (low root + close chord)");
    timeSigBox.setTooltip ("Time signature (sets the bar length)");
    rhythmBox.setTooltip ("Rhythm for every chord on the timeline");
    bassBox.setTooltip ("Bass note for every chord on the timeline");
    arpModeBox.setTooltip ("Arpeggio direction for every chord on the timeline");
    arpRateBox.setTooltip ("Arpeggio speed for every chord on the timeline");
    gateBox.setTooltip ("Arpeggio gate: how much of each step the note is held");

    for (auto* box : { &keyBox, &scaleBox, &timeSigBox, &snapBox, &voicingBox,
                       &rhythmBox, &bassBox, &arpModeBox, &arpRateBox, &gateBox })
    {
        styleComboBox (*box);
        addAndMakeVisible (*box);
    }

    addAndMakeVisible (octaveDownButton);
    addAndMakeVisible (octaveUpButton);
    octaveDownButton.onClick = [this] { nudgeOctave (-1); };
    octaveUpButton.onClick   = [this] { nudgeOctave (1); };

    autoInversionCheckbox.setButtonText ("SMOOTH VOICING");
    loopCheckbox.setButtonText ("LOOP");
    syncCheckbox.setButtonText ("HOST SYNC");
    hintsCheckbox.setButtonText ("HINTS");
    autoInversionCheckbox.setTooltip ("Voice-lead: each new chord picks the inversion closest to the last one");
    syncCheckbox.setTooltip ("Follow the host: play/stop, position and tempo come from your DAW");
    hintsCheckbox.setTooltip ("Outline the chords that commonly follow the last one");
    loopCheckbox.setTooltip ("Loop the timeline (L)");

    for (auto* box : { &autoInversionCheckbox, &loopCheckbox, &syncCheckbox, &hintsCheckbox })
        addAndMakeVisible (*box);

    for (auto* knob : { &velocityKnob, &tempoKnob, &previewKnob, &humanizeTimeKnob, &humanizeVelKnob })
        addAndMakeVisible (*knob);

    humanizeTimeKnob.setTooltip ("Random timing looseness, in milliseconds");
    humanizeVelKnob.setTooltip ("Random velocity variation, in percent");
    humanizeTimeKnob.valueText = [] (double v) { return juce::String (v, 1) + " ms"; };
    humanizeVelKnob.valueText  = [] (double v) { return juce::String (v, 1) + " %"; };
    velocityKnob.valueText = [] (double v) { return juce::String (static_cast<int> (v)); };
    previewKnob.valueText  = [] (double v) { return juce::String (v, 1) + " dB"; };
    tempoKnob.valueText    = [] (double v) { return juce::String (static_cast<int> (v)); };

    for (auto* button : { &settingsButton, &helpButton, &playStopButton, &restartButton, &undoButton, &redoButton,
                          &presetsButton, &exportButton })
    {
        styleTextButton (*button, homeUI::cyan);
        addAndMakeVisible (*button);
    }

    settingsButton.setTooltip ("Keyboard input and note spelling");
    helpButton.setTooltip ("Shortcuts and gestures");
    playStopButton.setTooltip ("Play / stop the timeline (Space)");
    restartButton.setTooltip ("Back to the start (Home)");
    undoButton.setTooltip ("Undo (Ctrl/Cmd+Z)");
    redoButton.setTooltip ("Redo (Ctrl/Cmd+Shift+Z)");
    presetsButton.setTooltip ("Ready-made progressions for this key");
    exportButton.setTooltip ("Save the progression as a MIDI file");

    settingsButton.onClick = [this] { showSettingsPanel(); };
    helpButton.onClick = [this] { showHelpPanel(); };
    presetsButton.onClick = [this] { showPresetMenu(); };
    exportButton.onClick = [this] { exportMidiWithDialog(); };

    addAndMakeVisible (dragTile);
    dragTile.provideFile = [this] { return writeDragFile(); };

    playStopButton.onClick = [this] { processor.setSequencePlaying (! processor.sequencePlayer.isPlaying()); };
    restartButton.onClick = [this] { processor.restartSequencer(); };
    undoButton.onClick = [this] { performUndo(); };
    redoButton.onClick = [this] { performRedo(); };

    addAndMakeVisible (timelineComponent);
    timelineComponent.makeEventFromSlot = [this] (int slot, progression::ChordEvent& out)
    {
        return processor.makeEventFromSlot (slot, out);
    };

    keyAttachment          = std::make_unique<ComboBoxAttachment> (processor.apvts, "KEY", keyBox);
    scaleAttachment        = std::make_unique<ComboBoxAttachment> (processor.apvts, "SCALE", scaleBox);
    timeSigAttachment      = std::make_unique<ComboBoxAttachment> (processor.apvts, "TIME_SIG", timeSigBox);
    snapAttachment         = std::make_unique<ComboBoxAttachment> (processor.apvts, "SNAP", snapBox);
    voicingAttachment      = std::make_unique<ComboBoxAttachment> (processor.apvts, "VOICING", voicingBox);
    rhythmAttachment       = std::make_unique<ComboBoxAttachment> (processor.apvts, "RHYTHM", rhythmBox);
    bassAttachment         = std::make_unique<ComboBoxAttachment> (processor.apvts, "BASS_MODE", bassBox);
    arpModeAttachment      = std::make_unique<ComboBoxAttachment> (processor.apvts, "ARP_MODE", arpModeBox);
    arpRateAttachment      = std::make_unique<ComboBoxAttachment> (processor.apvts, "ARP_RATE", arpRateBox);
    gateAttachment         = std::make_unique<ComboBoxAttachment> (processor.apvts, "GATE", gateBox);
    velocityAttachment     = std::make_unique<SliderAttachment> (processor.apvts, "VELOCITY", velocityKnob);
    previewGainAttachment  = std::make_unique<SliderAttachment> (processor.apvts, "PREVIEW_GAIN", previewKnob);
    tempoAttachment        = std::make_unique<SliderAttachment> (processor.apvts, "TEMPO", tempoKnob);
    humanizeTimeAttachment = std::make_unique<SliderAttachment> (processor.apvts, "HUMANIZE_TIME", humanizeTimeKnob);
    humanizeVelAttachment  = std::make_unique<SliderAttachment> (processor.apvts, "HUMANIZE_VEL", humanizeVelKnob);
    autoInversionAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "AUTO_INVERSION", autoInversionCheckbox);
    loopAttachment         = std::make_unique<ButtonAttachment> (processor.apvts, "LOOP_ENABLED", loopCheckbox);
    syncAttachment         = std::make_unique<ButtonAttachment> (processor.apvts, "SYNC_HOST", syncCheckbox);
    hintsAttachment        = std::make_unique<ButtonAttachment> (processor.apvts, "HINTS", hintsCheckbox);

    // Rhythm / Bass / Arp / Gate are global: choosing one stamps it on every
    // chord already on the timeline (one undo step). The attachment has
    // already written the parameter by the time onChange runs.
    for (auto* box : { &rhythmBox, &bassBox, &arpModeBox, &arpRateBox, &gateBox })
        box->onChange = [this] { processor.applyPlaybackSettingsToTimeline(); timelineComponent.refreshFromModel(); };

    processor.apvts.addParameterListener ("KEY", this);
    processor.apvts.addParameterListener ("SCALE", this);
    processor.apvts.addParameterListener ("NOTE_SPELLING", this);

    refreshChordCards();

    // Clicking a control must not steal keyboard focus from the editor, or
    // the chord keys go dead until the next click on the background.
    for (auto* child : getChildren())
        child->setWantsKeyboardFocus (false);

    setResizable (true, true);
    setResizeLimits (minWidth, minHeight, 1500, 800);
    setSize (defaultWidth, defaultHeight);

    startTimerHz (30);
}

HomeChordsAudioProcessorEditor::~HomeChordsAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
    processor.apvts.removeParameterListener ("KEY", this);
    processor.apvts.removeParameterListener ("SCALE", this);
    processor.apvts.removeParameterListener ("NOTE_SPELLING", this);
    releaseAllHeldSlots();
    releaseAllMouseSlots();
}

void HomeChordsAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10, 8);

    auto header = area.removeFromTop (30);
    area.removeFromTop (4);
    auto chordRow = area.removeFromTop (80);
    area.removeFromTop (5);
    auto toolsRow = area.removeFromTop (24);
    area.removeFromTop (5);
    auto footer = area.removeFromBottom (64);
    area.removeFromBottom (5);
    auto playbackRow = area.removeFromBottom (22);
    area.removeFromBottom (5);
    auto timelineArea = area;   // whatever's left

    // ---- header: brand ... lamp | key | scale | settings | help ----
    helpButton.setBounds (header.removeFromRight (26).reduced (0, 3));
    header.removeFromRight (4);
    settingsButton.setBounds (header.removeFromRight (30).reduced (0, 3));
    header.removeFromRight (8);
    scaleBox.setBounds (header.removeFromRight (132).reduced (0, 3));
    header.removeFromRight (4);
    keyBox.setBounds (header.removeFromRight (78).reduced (0, 3));
    header.removeFromRight (10);
    lampBounds = header.removeFromRight (74).toFloat();
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

    // ---- tools row ----
    {
        auto right = toolsRow;
        dragTile.setBounds (right.removeFromRight (88));
        right.removeFromRight (4);
        exportButton.setBounds (right.removeFromRight (52));
        right.removeFromRight (10);
        redoButton.setBounds (right.removeFromRight (44));
        right.removeFromRight (4);
        undoButton.setBounds (right.removeFromRight (44));

        auto left = toolsRow.withRight (undoButton.getX() - 10);
        put (left, playStopButton, 50);
        put (left, restartButton, 28, 8);
        put (left, loopCheckbox, 58, 6);
        put (left, syncCheckbox, 92, 8);
        put (left, timeSigBox, 54);
        put (left, snapBox, 58, 8);
        put (left, presetsButton, 66, 4);
        put (left, voicingBox, 76);
    }

    // ---- timeline ----
    timelineComponent.setBounds (timelineArea);

    // ---- playback row: label | rhythm | bass | arp | rate | gate ----
    playbackLabelBounds = playbackRow.removeFromLeft (84).toFloat();
    put (playbackRow, rhythmBox, 104);
    put (playbackRow, bassBox, 118);
    put (playbackRow, arpModeBox, 112);
    put (playbackRow, arpRateBox, 58);
    put (playbackRow, gateBox, 94);

    // ---- footer: octave | checkboxes | five knobs ----
    footerCardBounds = footer.toFloat();
    auto footerContent = footer.reduced (12, 6);

    auto octaveArea = footerContent.removeFromLeft (96);
    octaveLabelBounds = octaveArea.removeFromTop (14).toFloat();
    octaveArea.removeFromTop (2);
    octaveDownButton.setBounds (octaveArea.removeFromLeft (24));
    octaveArea.removeFromLeft (3);
    octaveUpButton.setBounds (octaveArea.removeFromRight (24));
    octaveArea.removeFromRight (3);
    octaveValueWellBounds = octaveArea.toFloat();

    footerContent.removeFromLeft (14);

    auto checkArea = footerContent.removeFromLeft (122);
    autoInversionCheckbox.setBounds (checkArea.removeFromTop (checkArea.getHeight() / 2));
    hintsCheckbox.setBounds (checkArea);

    footerContent.removeFromLeft (8);

    const int knobWidth = footerContent.getWidth() / 5;
    velocityKnob.setBounds (footerContent.removeFromLeft (knobWidth).reduced (4, 0));
    tempoKnob.setBounds (footerContent.removeFromLeft (knobWidth).reduced (4, 0));
    previewKnob.setBounds (footerContent.removeFromLeft (knobWidth).reduced (4, 0));
    humanizeTimeKnob.setBounds (footerContent.removeFromLeft (knobWidth).reduced (4, 0));
    humanizeVelKnob.setBounds (footerContent.reduced (4, 0));
}

void HomeChordsAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (homeUI::chassis);

    auto faceBounds = getLocalBounds().toFloat().reduced (3.0f);
    g.setColour (homeUI::face);
    g.fillRoundedRectangle (faceBounds, 8.0f);
    g.setColour (homeUI::faceEdge);
    g.drawRoundedRectangle (faceBounds, 8.0f, 1.2f);

    // Compact one-line brand.
    if (! brandBounds.isEmpty())
    {
        const auto titleFont = homeUI::font (19.0f);
        g.setFont (titleFont);
        const juce::String head ("Home-"), tail ("Chords");
        const float headWidth = juce::TextLayout::getStringWidth (titleFont, head);
        const float tailWidth = juce::TextLayout::getStringWidth (titleFont, tail);
        const auto line = brandBounds.withHeight (30.0f);

        g.setColour (juce::Colours::white);
        g.drawText (head, line.withWidth (headWidth + 2.0f), juce::Justification::centredLeft, false);
        g.setColour (homeUI::cyan);
        g.drawText (tail, line.withX (line.getX() + headWidth).withWidth (tailWidth + 6.0f), juce::Justification::centredLeft, false);

        g.setColour (juce::Colours::white.withAlpha (0.4f));
        homeUI::drawTrackedText (g, "DUBTACH DSP", line.getX() + headWidth + tailWidth + 10.0f, line.getY() + 9.0f, 11.0f, 70.0f, 7.5f);
    }

    if (! lampBounds.isEmpty())
        homeUI::drawLamp (g, lampBounds, "MIDI OUT", homeUI::green, processor.getMidiActivity(), false);

    if (! playbackLabelBounds.isEmpty())
    {
        homeUI::drawTrackedText (g, "ALL CHORDS", playbackLabelBounds.getX() + 2.0f, playbackLabelBounds.getY(),
                                 playbackLabelBounds.getHeight(), 72.0f, 8.5f);
    }

    if (! footerCardBounds.isEmpty())
    {
        const auto footerCardColour = homeUI::cyan.withSaturation (0.30f).withBrightness (0.50f);
        homeUI::drawCard (g, footerCardBounds, footerCardColour);

        homeUI::drawCardText (g, "OCTAVE", octaveLabelBounds, 9.0f, juce::Justification::centredLeft, 0.75f);

        homeUI::drawWell (g, octaveValueWellBounds, 4.0f);
        g.setFont (homeUI::font (14.0f));
        g.setColour (juce::Colours::white);
        const int octaveValue = static_cast<int> (processor.apvts.getRawParameterValue ("OCTAVE")->load());
        const auto octaveText = (octaveValue > 0 ? "+" : juce::String()) + juce::String (octaveValue);
        g.drawText (octaveText, octaveValueWellBounds, juce::Justification::centred, false);
    }
}

bool HomeChordsAudioProcessorEditor::keyStateChanged (bool /*isKeyDown*/)
{
    const auto mode = static_cast<int> (processor.apvts.getRawParameterValue ("KEYBOARD_MODE")->load());
    // 0 = Letters, 1 = Numbers, 2 = Both. The numpad belongs to the number modes.

    // Ctrl/Cmd/Alt combos are editing shortcuts (undo, select all, ...), never chords.
    const auto mods = juce::ModifierKeys::currentModifiers;
    const bool chordKeysSuppressed = mods.isCommandDown() || mods.isAltDown() || mods.isCtrlDown();

    bool handledAny = false;

    for (int i = 0; i < musictheory::maxDiatonicSlots; ++i)
    {
        bool down = false;

        if (! chordKeysSuppressed)
        {
            if (mode == 0 || mode == 2)
                down = down || juce::KeyPress::isKeyCurrentlyDown (trackedLetterKeyCodes[i]);
            if (mode == 1 || mode == 2)
                down = down || juce::KeyPress::isKeyCurrentlyDown (trackedNumberKeyCodes[i])
                            || juce::KeyPress::isKeyCurrentlyDown (trackedNumpadKeyCodes[i]);
        }

        if (down != keyHeldState[static_cast<size_t> (i)])
        {
            keyHeldState[static_cast<size_t> (i)] = down;
            processor.setChordSlotHeld (i, down);
            handledAny = true;
        }
    }

    return handledAny;
}

void HomeChordsAudioProcessorEditor::performUndo()
{
    processor.undoManager.undo();
    timelineComponent.refreshFromModel();
    refreshChordCards();
}

void HomeChordsAudioProcessorEditor::performRedo()
{
    processor.undoManager.redo();
    timelineComponent.refreshFromModel();
    refreshChordCards();
}

bool HomeChordsAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const bool command = mods.isCommandDown() || mods.isCtrlDown();
    const bool shift = mods.isShiftDown();
    const bool hostSync = processor.apvts.getRawParameterValue ("SYNC_HOST")->load() >= 0.5f;

    if (command)
    {
        if (isLetter (key, 'Z'))  { if (shift) performRedo(); else performUndo(); return true; }
        if (isLetter (key, 'Y'))  { performRedo(); return true; }
        if (isLetter (key, 'A'))  { timelineComponent.selectAll(); return true; }
        if (isLetter (key, 'D'))  { timelineComponent.duplicateSelected(); return true; }
        if (isLetter (key, 'C'))  { timelineComponent.copySelected(); return true; }
        if (isLetter (key, 'V'))  { timelineComponent.pasteAtEnd(); return true; }
        if (isLetter (key, 'X'))  { timelineComponent.copySelected(); timelineComponent.deleteSelected(); return true; }
        return false;
    }

    if (mods.isAltDown())
        return false;

    if (key == juce::KeyPress::spaceKey)
    {
        if (! hostSync)
            processor.setSequencePlaying (! processor.sequencePlayer.isPlaying());
        return true;
    }

    if (key == juce::KeyPress::homeKey)
    {
        if (! hostSync)
            processor.restartSequencer();
        return true;
    }

    if (isLetter (key, 'L'))
    {
        if (! hostSync)
            loopCheckbox.setToggleState (! loopCheckbox.getToggleState(), juce::sendNotificationSync);
        return true;
    }

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        timelineComponent.deleteSelected();
        return true;
    }

    if (key == juce::KeyPress::escapeKey)
    {
        timelineComponent.clearSelection();
        return true;
    }

    if (key.isKeyCode (juce::KeyPress::leftKey) || key.isKeyCode (juce::KeyPress::rightKey))
    {
        const double step = shift ? processor.getBeatsPerBar() : timelineComponent.getSnapBeats();
        timelineComponent.nudgeSelected (key.isKeyCode (juce::KeyPress::leftKey) ? -step : step);
        return true;
    }

    if (key.isKeyCode (juce::KeyPress::upKey))   { nudgeOctave (1);  return true; }
    if (key.isKeyCode (juce::KeyPress::downKey)) { nudgeOctave (-1); return true; }

    if (key == juce::KeyPress::F1Key || key.getTextCharacter() == '?')
    {
        showHelpPanel();
        return true;
    }

    return false;
}

void HomeChordsAudioProcessorEditor::visibilityChanged()
{
    if (isVisible())
    {
        grabKeyboardFocus();
    }
    else
    {
        releaseAllHeldSlots();
        releaseAllMouseSlots();
    }
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

    // Drain the "pressed since last tick" slots so the hints can follow them.
    const auto triggered = processor.consumeTriggeredSlots();

    // Content added past the loop end would never be heard: grow the loop to
    // cover it (never shrinks it), once the gesture that changed it is over.
    const auto revision = processor.progression.getRevision();
    if (revision != lastLoopCheckRevision && ! timelineComponent.isInteracting())
    {
        lastLoopCheckRevision = revision;
        processor.extendLoopToCoverProgression();
    }

    undoButton.setEnabled (processor.undoManager.canUndo());
    redoButton.setEnabled (processor.undoManager.canRedo());

    // With host sync on, the host owns transport, position, tempo and looping.
    const bool hostSync = processor.apvts.getRawParameterValue ("SYNC_HOST")->load() >= 0.5f;
    playStopButton.setEnabled (! hostSync);
    restartButton.setEnabled (! hostSync);
    loopCheckbox.setEnabled (! hostSync);
    tempoKnob.setEnabled (! hostSync);
    playStopButton.setButtonText (hostSync ? "Host" : (processor.sequencePlayer.isPlaying() ? "Stop" : "Play"));

    // Things the timeline needs from parameters (cheap; the setters only
    // repaint on an actual change).
    timelineComponent.setSnapBeats (snapBeatsFromParam());
    timelineComponent.setBeatsPerBar (processor.getBeatsPerBar());
    timelineComponent.setUseFlats (processor.shouldSpellWithFlats());

    const bool hasProgression = ! processor.progression.isEmpty();
    exportButton.setEnabled (hasProgression);
    dragTile.setActive (hasProgression);

    updateHints (triggered, activeMask);

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
                                  || processor.getSlotExtensionOverride (i) != musictheory::Extension::None
                                  || processor.getSlotColourIndex (i) >= 0);
        }
        else
        {
            card.setEmpty();
        }
    }

    hintSignature = -1;   // the hint highlight may need to move with the cards
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

void HomeChordsAudioProcessorEditor::releaseAllMouseSlots()
{
    // A chord auditioned with the mouse never gets its mouseUp if the
    // editor is closed or hidden mid-click, which would leave it sounding.
    for (int i = 0; i < musictheory::maxDiatonicSlots; ++i)
        processor.setChordSlotHeldFromMouse (i, false);
}

void HomeChordsAudioProcessorEditor::showChordMenu (int slot)
{
    if (slot < 0 || slot >= currentSlotCount)
        return;

    const auto chords = processor.getCurrentDiatonicChords();

    if (slot >= static_cast<int> (chords.size()))
        return;

    const auto& chord = chords[static_cast<size_t> (slot)];
    const int currentQuality = processor.getSlotQualityOverride (slot);
    const auto currentExtension = processor.getSlotExtensionOverride (slot);
    const int currentColour = processor.getSlotColourIndex (slot);
    const auto palette = processor.getCurrentColorChords();

    static const char* const qualityNames[] = { "Major", "Minor", "Diminished", "Augmented", "Sus2", "Sus4" };
    static const char* const extensionNames[] = { "None", "6th", "7th", "Major 7th", "Add 9" };

    constexpr int qualityBase = 100, extensionBase = 200, borrowedBase = 300, resetId = 400, appendId = 500;

    juce::PopupMenu qualityMenu;
    qualityMenu.addItem (qualityBase, "Default", true, currentQuality < 0);
    for (int i = 0; i < 6; ++i)
        qualityMenu.addItem (qualityBase + 1 + i, qualityNames[i], true, currentQuality == i);

    juce::PopupMenu extensionMenu;
    for (int i = 0; i < musictheory::numExtensions; ++i)
        extensionMenu.addItem (extensionBase + i, extensionNames[i], true, static_cast<int> (currentExtension) == i);

    juce::PopupMenu borrowedMenu;
    borrowedMenu.addItem (borrowedBase, "Diatonic chord", true, currentColour < 0);
    borrowedMenu.addSeparator();
    for (int i = 0; i < static_cast<int> (palette.size()); ++i)
        borrowedMenu.addItem (borrowedBase + 1 + i,
                              palette[static_cast<size_t> (i)].label + "   " + palette[static_cast<size_t> (i)].chordName
                                  + "    " + palette[static_cast<size_t> (i)].description,
                              true, currentColour == i);

    const bool anyOverride = currentQuality >= 0 || currentExtension != musictheory::Extension::None || currentColour >= 0;

    juce::PopupMenu menu;
    menu.addSectionHeader (chord.chordName + "  (" + chord.romanNumeral + ")");
    menu.addSubMenu ("Quality", qualityMenu);
    menu.addSubMenu ("Extension", extensionMenu);
    menu.addSubMenu ("Borrowed chord", borrowedMenu);
    menu.addSeparator();
    menu.addItem (appendId, "Add to timeline");
    menu.addItem (resetId, "Reset to diatonic", anyOverride);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&chordCards[static_cast<size_t> (slot)]),
                        [safe = juce::Component::SafePointer<HomeChordsAudioProcessorEditor> (this), slot] (int result)
    {
        if (safe == nullptr || result == 0)
            return;

        auto& proc = safe->processor;

        if (result == appendId)
        {
            proc.addChordFromSlotToTimeline (slot);
            safe->timelineComponent.refreshFromModel();
            return;
        }

        if (result == resetId)
        {
            proc.clearSlotOverride (slot);
        }
        else if (result >= borrowedBase && result < borrowedBase + 100)
        {
            // Picking a borrowed chord replaces the card's chord outright, so
            // any earlier quality / extension tweak is cleared.
            proc.setSlotQualityOverride (slot, -1);
            proc.setSlotExtensionOverride (slot, musictheory::Extension::None);
            proc.setSlotColourIndex (slot, result - borrowedBase - 1);   // -1 = back to the diatonic chord
        }
        else if (result >= extensionBase && result < extensionBase + 100)
        {
            proc.setSlotExtensionOverride (slot, static_cast<musictheory::Extension> (result - extensionBase));
        }
        else if (result >= qualityBase && result < qualityBase + 100)
        {
            proc.setSlotQualityOverride (slot, result - qualityBase - 1);   // -1 = default
        }

        safe->refreshChordCards();
    });
}

void HomeChordsAudioProcessorEditor::startCardDrag (int slot)
{
    if (slot < 0 || slot >= currentSlotCount)
        return;

    auto& card = chordCards[static_cast<size_t> (slot)];
    auto snapshot = card.createComponentSnapshot (card.getLocalBounds().reduced (3));
    snapshot.multiplyAllAlphas (0.82f);

    startDragging ("slot:" + juce::String (slot), &card, juce::ScaledImage (snapshot), false);
}

void HomeChordsAudioProcessorEditor::showPresetMenu()
{
    const auto& presets = musictheory::getAllPresets();
    const auto scale = processor.getScaleType();

    juce::StringArray categories;
    for (const auto& preset : presets)
        if (musictheory::presetSuitsScale (preset, scale))
            categories.addIfNotAlreadyThere (preset.category);

    juce::PopupMenu menu;

    for (const auto& category : categories)
    {
        menu.addSectionHeader (category);

        for (size_t i = 0; i < presets.size(); ++i)
            if (presets[i].category == category && musictheory::presetSuitsScale (presets[i], scale))
                menu.addItem (static_cast<int> (i) + 1, presets[i].name);
    }

    menu.addSeparator();
    menu.addItem (1000, "Clear timeline", ! processor.progression.isEmpty());

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetsButton),
                        [safe = juce::Component::SafePointer<HomeChordsAudioProcessorEditor> (this)] (int result)
    {
        if (safe == nullptr || result == 0)
            return;

        if (result == 1000)
        {
            safe->processor.undoManager.beginNewTransaction ("Clear timeline");
            safe->processor.undoManager.perform (new progression::ClearProgressionAction (safe->processor.progression));
            safe->timelineComponent.clearSelection();
            safe->timelineComponent.refreshFromModel();
        }
        else
        {
            safe->applyPreset (result - 1);
        }
    });
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

    processor.replaceTimeline (builtModel, "Apply preset");
    timelineComponent.clearSelection();
    timelineComponent.refreshFromModel();
}

void HomeChordsAudioProcessorEditor::showHelpPanel()
{
    juce::CallOutBox::launchAsynchronously (std::make_unique<homeUI::HelpPanel>(), helpButton.getScreenBounds(), nullptr);
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

double HomeChordsAudioProcessorEditor::snapBeatsFromParam() const
{
    const int index = static_cast<int> (processor.apvts.getRawParameterValue ("SNAP")->load());

    switch (index)
    {
        case 1:  return 0.5;
        case 2:  return 1.0;
        case 3:  return 2.0;
        case 4:  return processor.getBeatsPerBar();
        default: return 0.25;
    }
}

void HomeChordsAudioProcessorEditor::exportMidiWithDialog()
{
    if (processor.progression.isEmpty())
        return;

    const auto keyNames = musictheory::getAllKeyNames();
    const auto keyName = keyNames[juce::jlimit (0, keyNames.size() - 1, processor.getKeyTonicPitchClass())];
    const auto fileName = juce::File::createLegalFileName ("Home-Chords " + keyName + " " + musictheory::getScaleName (processor.getScaleType()));

    fileChooser = std::make_unique<juce::FileChooser> ("Export progression as a MIDI file",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                           .getChildFile (fileName + ".mid"),
                                                       "*.mid;*.midi");

    const auto flags = juce::FileBrowserComponent::saveMode
                     | juce::FileBrowserComponent::canSelectFiles
                     | juce::FileBrowserComponent::warnAboutOverwriting;

    fileChooser->launchAsync (flags, [safe = juce::Component::SafePointer<HomeChordsAudioProcessorEditor> (this)]
                                      (const juce::FileChooser& chooser)
    {
        if (safe == nullptr)
            return;

        auto destination = chooser.getResult();

        if (destination == juce::File())
            return;   // cancelled

        if (! destination.hasFileExtension ("mid;midi"))
            destination = destination.withFileExtension ("mid");

        const auto midiFile = midiexport::buildMidiFile (safe->processor.progression, safe->processor.getExportSettings());

        if (! midiexport::writeMidiFile (midiFile, destination))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Export failed",
                                                    "Couldn't write " + destination.getFullPathName());
    });
}

// Called when the drag tile is pulled: writes the progression to a temp .mid
// and hands its path to the OS drag-and-drop. An empty File cancels the drag.
juce::File HomeChordsAudioProcessorEditor::writeDragFile()
{
    if (processor.progression.isEmpty())
        return {};

    const auto keyNames = musictheory::getAllKeyNames();
    const auto keyName = keyNames[juce::jlimit (0, keyNames.size() - 1, processor.getKeyTonicPitchClass())];
    const auto fileName = juce::File::createLegalFileName ("Home-Chords " + keyName + " " + musictheory::getScaleName (processor.getScaleType()));

    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("Home-Chords")
                          .getChildFile (fileName + ".mid");

    const auto midiFile = midiexport::buildMidiFile (processor.progression, processor.getExportSettings());

    return midiexport::writeMidiFile (midiFile, file) ? file : juce::File();
}

// Outlines the chords that commonly follow the last chord played or placed.
// "Last" is whichever happened most recently: a live key press, or the
// timeline changing (a dropped card, a preset, an undo).
void HomeChordsAudioProcessorEditor::updateHints (juce::uint32 triggeredSlots, juce::uint32 activeSlots)
{
    const auto freshSlots = triggeredSlots | (activeSlots & ~previousActiveMask);
    previousActiveMask = activeSlots;

    if (freshSlots != 0)
    {
        for (int i = musictheory::maxDiatonicSlots - 1; i >= 0; --i)
        {
            if ((freshSlots & (1u << static_cast<unsigned> (i))) != 0)
            {
                lastPlayedSlot = i;
                break;
            }
        }
    }

    const auto revision = processor.progression.getRevision();
    if (revision != lastSeenRevision)
    {
        lastSeenRevision = revision;
        lastHintSourceWasLive = false;
    }

    if (freshSlots != 0)
        lastHintSourceWasLive = true;

    int fromDegree = -1;

    if (lastHintSourceWasLive)
    {
        // A card holding a borrowed chord isn't a diatonic degree: no hints from it.
        fromDegree = processor.getSlotColourIndex (lastPlayedSlot) >= 0 ? -1 : lastPlayedSlot;
    }
    else if (! processor.progression.isEmpty())
    {
        // The chord that ends last is the one the next chord follows.
        double latestEnd = -1.0;
        for (int i = 0; i < processor.progression.size(); ++i)
        {
            const auto& event = processor.progression.getEvent (i);
            if (event.endBeats() > latestEnd)
            {
                latestEnd = event.endBeats();
                fromDegree = event.scaleDegree;   // -1 for a colour chord: no hints
            }
        }
    }
    else
    {
        fromDegree = processor.getSlotColourIndex (lastPlayedSlot) >= 0 ? -1 : lastPlayedSlot;
    }

    const bool hintsOn = processor.apvts.getRawParameterValue ("HINTS")->load() >= 0.5f;
    const auto scale = processor.getScaleType();

    // Only touch the cards when something that affects them changed.
    const int signature = ((hintsOn ? 1 : 0) * 1000 + (fromDegree + 1) * 20 + static_cast<int> (scale)) * 10 + currentSlotCount;
    if (signature == hintSignature)
        return;

    hintSignature = signature;

    for (auto& card : chordCards)
        card.setSuggestion (0, {});

    if (! hintsOn || fromDegree < 0)
        return;

    for (const auto& suggestion : musictheory::suggestNext (fromDegree, scale))
        if (suggestion.scaleDegree < currentSlotCount)
            chordCards[static_cast<size_t> (suggestion.scaleDegree)].setSuggestion (suggestion.rank, suggestion.reason);
}

