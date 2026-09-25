#include "PluginEditor.h"

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

    constexpr int trackedKeyCodes[musictheory::maxDiatonicSlots] = { 'A', 'S', 'D', 'F', 'G', 'H', 'J' };
    const juce::String trackedKeyCaps[musictheory::maxDiatonicSlots] = { "A", "S", "D", "F", "G", "H", "J" };
}

HomeChordsAudioProcessorEditor::HomeChordsAudioProcessorEditor (HomeChordsAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setWantsKeyboardFocus (true);

    for (auto& card : chordCards)
        addAndMakeVisible (card);

    for (int i = 0; i < musictheory::maxDiatonicSlots; ++i)
    {
        chordCards[static_cast<size_t> (i)].onPressStart = [this, i] { processor.setChordSlotHeld (i, true); };
        chordCards[static_cast<size_t> (i)].onPressEnd   = [this, i] { processor.setChordSlotHeld (i, false); };
    }

    keyBox.addItemList (musictheory::getAllKeyNames(), 1);
    scaleBox.addItemList (musictheory::getAllScaleNames(), 1);

    for (auto* box : { &keyBox, &scaleBox })
    {
        box->setColour (juce::ComboBox::backgroundColourId, homeUI::slot);
        box->setColour (juce::ComboBox::textColourId, juce::Colours::white);
        box->setColour (juce::ComboBox::outlineColourId, homeUI::slotEdge);
        box->setColour (juce::ComboBox::arrowColourId, homeUI::cyan);
        addAndMakeVisible (*box);
    }

    addAndMakeVisible (octaveDownButton);
    addAndMakeVisible (octaveUpButton);
    octaveDownButton.onClick = [this] { nudgeOctave (-1); };
    octaveUpButton.onClick   = [this] { nudgeOctave (1); };

    addAndMakeVisible (velocityKnob);
    addAndMakeVisible (previewKnob);
    velocityKnob.valueText = [] (double v) { return juce::String (static_cast<int> (v)); };
    previewKnob.valueText  = [] (double v) { return juce::String (v, 1) + " dB"; };

    keyAttachment          = std::make_unique<ComboBoxAttachment> (processor.apvts, "KEY", keyBox);
    scaleAttachment        = std::make_unique<ComboBoxAttachment> (processor.apvts, "SCALE", scaleBox);
    velocityAttachment     = std::make_unique<SliderAttachment> (processor.apvts, "VELOCITY", velocityKnob);
    previewGainAttachment  = std::make_unique<SliderAttachment> (processor.apvts, "PREVIEW_GAIN", previewKnob);

    processor.apvts.addParameterListener ("KEY", this);
    processor.apvts.addParameterListener ("SCALE", this);

    refreshChordCards();

    setResizable (true, true);
    setResizeLimits (minWidth, minHeight, 1600, 900);
    setSize (defaultWidth, defaultHeight);

    startTimerHz (30);
}

HomeChordsAudioProcessorEditor::~HomeChordsAudioProcessorEditor()
{
    stopTimer();
    processor.apvts.removeParameterListener ("KEY", this);
    processor.apvts.removeParameterListener ("SCALE", this);
    releaseAllHeldSlots();
}

void HomeChordsAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto header = area.removeFromTop (54);
    area.removeFromTop (8);
    auto footer = area.removeFromBottom (90);
    // `area` is now exactly the chord row.

    // ---- header: brand | lamp | key | scale ----
    auto rightHeader = header.removeFromRight (344);
    keyBox.setBounds (rightHeader.removeFromLeft (158).reduced (6, 13));
    rightHeader.removeFromLeft (6);
    scaleBox.setBounds (rightHeader.reduced (6, 13));
    header.removeFromRight (10);
    lampBounds = header.removeFromRight (96).toFloat();
    brandBounds = header.toFloat();

    // ---- chord row ----
    const int shownCount = juce::jlimit (1, musictheory::maxDiatonicSlots, currentSlotCount);
    const int cardWidth = area.getWidth() / shownCount;

    for (int i = 0; i < musictheory::maxDiatonicSlots; ++i)
    {
        auto& card = chordCards[static_cast<size_t> (i)];
        const bool shown = i < shownCount;
        card.setVisible (shown);

        if (shown)
        {
            const int x = area.getX() + i * cardWidth;
            // The last visible card absorbs the integer-division remainder
            // so the row fills the full width with no gap on the right.
            const int width = (i == shownCount - 1) ? (area.getRight() - x) : cardWidth;
            card.setBounds (x, area.getY(), width, area.getHeight());
        }
    }

    // ---- footer: octave stepper | velocity | preview volume ----
    footerCardBounds = footer.toFloat();
    auto footerContent = footer.reduced (14, 10);

    auto octaveArea = footerContent.removeFromLeft (170);
    octaveLabelBounds = octaveArea.removeFromTop (16).toFloat();
    octaveArea.removeFromTop (2);
    octaveDownButton.setBounds (octaveArea.removeFromLeft (26));
    octaveArea.removeFromLeft (4);
    octaveUpButton.setBounds (octaveArea.removeFromRight (26));
    octaveArea.removeFromRight (4);
    octaveValueWellBounds = octaveArea.toFloat();   // whatever's left in the middle

    footerContent.removeFromLeft (20);

    auto knobArea = footerContent;
    const int knobWidth = knobArea.getWidth() / 2;
    velocityKnob.setBounds (knobArea.removeFromLeft (knobWidth).reduced (10, 0));
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
    bool handledAny = false;

    for (int i = 0; i < musictheory::maxDiatonicSlots; ++i)
    {
        const bool down = juce::KeyPress::isKeyCurrentlyDown (trackedKeyCodes[i]);

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
    // Clicking empty background area (not a card/knob/combo box, which
    // would consume the click themselves) returns keyboard focus to the
    // chord-playing surface.
    grabKeyboardFocus();
}

void HomeChordsAudioProcessorEditor::timerCallback()
{
    if (chordsNeedRefresh.exchange (false, std::memory_order_relaxed))
        refreshChordCards();

    // Safety net: if the whole plugin window has lost OS focus, we can no
    // longer reliably observe physical key-up events, so treat every
    // tracked key as released rather than risk a stuck note. This does
    // NOT fire just because a child control (e.g. a combo box popup) has
    // focus -- hasKeyboardFocus(true) is true as long as focus is
    // anywhere inside this editor.
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

    for (int i = 0; i < musictheory::maxDiatonicSlots; ++i)
    {
        auto& card = chordCards[static_cast<size_t> (i)];

        if (i < static_cast<int> (chords.size()))
        {
            const auto& chord = chords[static_cast<size_t> (i)];
            card.setContent (trackedKeyCaps[i], chord.romanNumeral, chord.chordName, colourForQuality (chord.quality));
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
