#include "PluginProcessor.h"
#include "PluginEditor.h"

HomeChordsAudioProcessor::HomeChordsAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameters())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout HomeChordsAudioProcessor::createParameters()
{
    using FloatAttributes = juce::AudioParameterFloatAttributes;
    using IntAttributes = juce::AudioParameterIntAttributes;
    using ChoiceAttributes = juce::AudioParameterChoiceAttributes;

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
    return musictheory::buildDiatonicChords (getKeyTonicPitchClass(), getScaleType());
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
    const auto shapes = musictheory::buildDiatonicShapes (tonicPitchClass, scaleType);

    keyboardEngine.renderBlockStart (midi, shapes, octaveOffset, velocity, 1);

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
