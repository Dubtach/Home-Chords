#include "PluginProcessor.h"
#include "PluginEditor.h"

HomeChordsAudioProcessor::HomeChordsAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameters())
{
    // Cached once here (message thread, construction time -- string
    // building is fine) so processBlock never has to build a parameter-ID
    // string just to read one of these every block.
    for (int slot = 0; slot < musictheory::maxDiatonicSlots; ++slot)
    {
        qualityOverrideParams[static_cast<size_t> (slot)] = apvts.getRawParameterValue (qualityParamId (slot));
        extensionOverrideParams[static_cast<size_t> (slot)] = apvts.getRawParameterValue (extensionParamId (slot));
    }
}

juce::String HomeChordsAudioProcessor::qualityParamId (int slot)   { return "SLOT" + juce::String (slot) + "_QUALITY"; }
juce::String HomeChordsAudioProcessor::extensionParamId (int slot) { return "SLOT" + juce::String (slot) + "_EXTENSION"; }

juce::AudioProcessorValueTreeState::ParameterLayout HomeChordsAudioProcessor::createParameters()
{
    using FloatAttributes = juce::AudioParameterFloatAttributes;
    using IntAttributes = juce::AudioParameterIntAttributes;
    using ChoiceAttributes = juce::AudioParameterChoiceAttributes;
    using BoolAttributes = juce::AudioParameterBoolAttributes;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "KEY", "Key", musictheory::getAllKeyNames(), 0, ChoiceAttributes{}));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "SCALE", "Scale", musictheory::getAllScaleNames(), 0, ChoiceAttributes{}));

    params.push_back (std::make_unique<juce::AudioParameterInt> (
        "OCTAVE", "Octave", -2, 2, 0, IntAttributes{}));

    params.push_back (std::make_unique<juce::AudioParameterInt> (
        "VELOCITY", "Velocity", 1, 127, 100, IntAttributes{}));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "PREVIEW_GAIN", "Preview Volume",
        juce::NormalisableRange<float> (-48.0f, 6.0f, 0.1f), -6.0f,
        FloatAttributes{}.withLabel ("dB")));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "AUTO_INVERSION", "Auto Inversion", true, BoolAttributes{}));

    // Index 0 = "Diatonic" (no override); indices 1-6 map to
    // ChordQuality::Major..Sus4 (see getSlotQualityOverride/
    // setSlotQualityOverride for the -1 offset that encodes this).
    static const juce::StringArray qualityChoices { "Diatonic", "Major", "Minor", "Diminished", "Augmented", "Sus2", "Sus4" };
    // Index maps 1:1 to the Extension enum (0 = None).
    static const juce::StringArray extensionChoices { "None", "6", "7", "Maj7", "Add9" };

    for (int slot = 0; slot < musictheory::maxDiatonicSlots; ++slot)
    {
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            qualityParamId (slot), "Slot " + juce::String (slot + 1) + " Quality", qualityChoices, 0, ChoiceAttributes{}));

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            extensionParamId (slot), "Slot " + juce::String (slot + 1) + " Extension", extensionChoices, 0, ChoiceAttributes{}));
    }

    return { params.begin(), params.end() };
}

int HomeChordsAudioProcessor::getKeyTonicPitchClass() const noexcept
{
    return static_cast<int> (apvts.getRawParameterValue ("KEY")->load());
}

musictheory::ScaleType HomeChordsAudioProcessor::getScaleType() const noexcept
{
    const auto index = static_cast<int> (apvts.getRawParameterValue ("SCALE")->load());
    return static_cast<musictheory::ScaleType> (juce::jlimit (0, musictheory::numScaleTypes - 1, index));
}

std::vector<musictheory::ChordDefinition> HomeChordsAudioProcessor::getCurrentDiatonicChords() const
{
    auto chords = musictheory::buildDiatonicChords (getKeyTonicPitchClass(), getScaleType());

    for (int slot = 0; slot < static_cast<int> (chords.size()); ++slot)
    {
        musictheory::SlotOverride slotOverride;
        slotOverride.qualityOverride = getSlotQualityOverride (slot);
        slotOverride.extension = getSlotExtensionOverride (slot);

        chords[static_cast<size_t> (slot)] = musictheory::applyOverride (chords[static_cast<size_t> (slot)], slotOverride);
    }

    return chords;
}

musictheory::RtChordShape HomeChordsAudioProcessor::effectiveShape (int slot, const musictheory::RtChordShape& diatonicShape) const noexcept
{
    if (slot < 0 || slot >= musictheory::maxDiatonicSlots)
        return diatonicShape;

    musictheory::SlotOverride slotOverride;
    slotOverride.qualityOverride = static_cast<int> (qualityOverrideParams[static_cast<size_t> (slot)]->load()) - 1;
    slotOverride.extension = static_cast<musictheory::Extension> (
        static_cast<int> (extensionOverrideParams[static_cast<size_t> (slot)]->load()));

    return musictheory::applyOverride (diatonicShape, slotOverride);
}

int HomeChordsAudioProcessor::getSlotQualityOverride (int slot) const noexcept
{
    if (slot < 0 || slot >= musictheory::maxDiatonicSlots)
        return -1;

    return static_cast<int> (qualityOverrideParams[static_cast<size_t> (slot)]->load()) - 1;
}

musictheory::Extension HomeChordsAudioProcessor::getSlotExtensionOverride (int slot) const noexcept
{
    if (slot < 0 || slot >= musictheory::maxDiatonicSlots)
        return musictheory::Extension::None;

    return static_cast<musictheory::Extension> (static_cast<int> (extensionOverrideParams[static_cast<size_t> (slot)]->load()));
}

void HomeChordsAudioProcessor::setSlotQualityOverride (int slot, int qualityOrMinus1)
{
    if (slot < 0 || slot >= musictheory::maxDiatonicSlots)
        return;

    // -1 = Diatonic (choice index 0), 0..5 = Major..Sus4 (choice index 1..6).
    const int clampedQuality = juce::jlimit (-1, 5, qualityOrMinus1);

    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (qualityParamId (slot))))
        *param = clampedQuality + 1;
}

void HomeChordsAudioProcessor::setSlotExtensionOverride (int slot, musictheory::Extension extension)
{
    if (slot < 0 || slot >= musictheory::maxDiatonicSlots)
        return;

    const int index = juce::jlimit (0, musictheory::numExtensions - 1, static_cast<int> (extension));

    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (extensionParamId (slot))))
        *param = index;
}

void HomeChordsAudioProcessor::clearSlotOverride (int slot)
{
    setSlotQualityOverride (slot, -1);
    setSlotExtensionOverride (slot, musictheory::Extension::None);
}

void HomeChordsAudioProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    sampleRate = newSampleRate;
    keyboardEngine.reset();
    previewSynth.prepare (sampleRate, samplesPerBlock);
    midiActivityForUi.store (0.0f, std::memory_order_relaxed);
}

void HomeChordsAudioProcessor::releaseResources()
{
    // No MidiBuffer is available here to send final Note Offs through, so
    // this only clears internal state. The editor releases every held
    // slot (generating real Note Offs on the next block) when it loses
    // keyboard focus or is closed -- see PluginEditor. A host that hard-
    // kills the plugin mid-chord is the one case this can't fully cover;
    // hosts generally send their own all-notes-off in that situation.
    keyboardEngine.reset();
    previewSynth.reset();
}

bool HomeChordsAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    return mainOut == juce::AudioChannelSet::mono() || mainOut == juce::AudioChannelSet::stereo();
}

void HomeChordsAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();

    buffer.clear();
    // This plugin generates its own output rather than processing an
    // incoming signal, and incoming MIDI is currently ignored (reserved
    // for MIDI chord detection -- see the header comment), so nothing
    // from the host should reach the output here.
    midi.clear();

    if (numSamples <= 0)
        return;

    const int tonicPitchClass = getKeyTonicPitchClass();
    const auto scaleType = getScaleType();
    const int octaveOffset = static_cast<int> (apvts.getRawParameterValue ("OCTAVE")->load());
    const int velocity = static_cast<int> (apvts.getRawParameterValue ("VELOCITY")->load());
    const float previewGainDb = apvts.getRawParameterValue ("PREVIEW_GAIN")->load();

    // Recomputed fresh every block from the current Key/Scale parameters
    // rather than cached -- this is cheap (a handful of fixed-size array
    // writes) and sidesteps needing any cross-thread chord cache at all.
    const auto diatonicShapes = musictheory::buildDiatonicShapes (tonicPitchClass, scaleType);

    // Apply each slot's quality/extension override (if any) on top of the
    // diatonic shape -- same root, possibly different tones. This is the
    // one place processBlock and getCurrentDiatonicChords (the UI's view)
    // both go through effectiveShape(), so what's heard always matches
    // what's shown, override or not.
    musictheory::DiatonicShapeSet finalShapes;
    finalShapes.count = diatonicShapes.count;
    for (int slot = 0; slot < diatonicShapes.count; ++slot)
        finalShapes.shapes[static_cast<size_t> (slot)] = effectiveShape (slot, diatonicShapes.shapes[static_cast<size_t> (slot)]);

    const bool autoInversion = apvts.getRawParameterValue ("AUTO_INVERSION")->load() >= 0.5f;

    keyboardEngine.renderBlockStart (midi, finalShapes, octaveOffset, velocity, 1, autoInversion);

    bool anyNoteThisBlock = false;

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();

        if (message.isNoteOn())
        {
            previewSynth.noteOn (message.getNoteNumber(), message.getFloatVelocity());
            anyNoteThisBlock = true;
        }
        else if (message.isNoteOff())
        {
            previewSynth.noteOff (message.getNoteNumber());
        }
    }

    previewSynth.setGain (juce::Decibels::decibelsToGain (previewGainDb));
    previewSynth.renderBlock (buffer, 0, numSamples);

    float activity = midiActivityForUi.load (std::memory_order_relaxed);
    activity = anyNoteThisBlock ? 1.0f : activity * 0.90f;
    midiActivityForUi.store (activity, std::memory_order_relaxed);
}

void HomeChordsAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    // Phase 2 groundwork: an empty <PROGRESSION> tree round-trips cleanly
    // today, and will carry real chord events once the timeline exists.
    state.appendChild (progression.toValueTree(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void HomeChordsAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            const auto state = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (state);
            progression = progression::ProgressionModel::fromValueTree (state.getChildWithName ("PROGRESSION"));
        }
    }
}

juce::AudioProcessorEditor* HomeChordsAudioProcessor::createEditor()
{
    return new HomeChordsAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HomeChordsAudioProcessor();
}
