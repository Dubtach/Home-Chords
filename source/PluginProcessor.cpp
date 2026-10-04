#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // An external MIDI keyboard's white keys map to the 7 chord slots by
    // ordinal position within an octave -- C=slot 1, D=slot 2, ... B=slot
    // 7 -- regardless of which Key/Scale is currently selected. The white
    // keys' pitch classes (0,2,4,5,7,9,11) happen to be exactly the major
    // scale pattern, so this reads as "play C major's white keys, get
    // degrees I-VII of whatever's selected", matching how most chord-pad
    // controllers treat a keyboard. Black keys aren't mapped to anything
    // (returns -1), so they pass through this plugin harmlessly ignored.
    int whiteKeySlotForPitchClass (int pitchClass) noexcept
    {
        switch (pitchClass)
        {
            case 0:  return 0;   // C
            case 2:  return 1;   // D
            case 4:  return 2;   // E
            case 5:  return 3;   // F
            case 7:  return 4;   // G
            case 9:  return 5;   // A
            case 11: return 6;   // B
            default: return -1;  // black key
        }
    }
}

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

    sequencePlayer.setLoopRange (0.0, 16.0);
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

    // Index maps 1:1 to musictheory::NoteSpelling (0 = Auto).
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "NOTE_SPELLING", "Note Spelling", juce::StringArray { "Auto", "Sharps", "Flats" }, 0, ChoiceAttributes{}));

    // Which computer-keyboard keys trigger the 7 chord slots: 0 = Letters
    // (A S D F G H J), 1 = Numbers (1-7, the number row), 2 = Both.
    // Defaults to Both so nothing that already worked stops working.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "KEYBOARD_MODE", "Keyboard Input", juce::StringArray { "Letters", "Numbers", "Both" }, 2, ChoiceAttributes{}));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "ADD_MODE", "Add Mode", false, BoolAttributes{}));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "TEMPO", "Tempo", juce::NormalisableRange<float> (20.0f, 300.0f, 0.1f), 120.0f,
        FloatAttributes{}.withLabel ("BPM")));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "TIME_SIG", "Time Signature", juce::StringArray { "4/4", "3/4", "6/8" }, 0, ChoiceAttributes{}));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "LOOP_ENABLED", "Loop", true, BoolAttributes{}));

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

double HomeChordsAudioProcessor::getBeatsPerBar() const noexcept
{
    const int index = static_cast<int> (apvts.getRawParameterValue ("TIME_SIG")->load());
    switch (index)
    {
        case 1:  return 3.0;   // 3/4
        case 2:  return 3.0;   // 6/8, treated as 2 dotted-quarter beats -> 3 quarter-note-equivalent beats
        default: return 4.0;   // 4/4
    }
}

std::vector<musictheory::ChordDefinition> HomeChordsAudioProcessor::getCurrentDiatonicChords() const
{
    // Spelling is purely a display concern (the audio thread deals in
    // pitch classes, never note names), so it only needs to be read here
    // on the message-thread path -- an override's chord name inherits the
    // already-spelled rootName from the diatonic definition it's built on.
    const auto spelling = static_cast<musictheory::NoteSpelling> (
        juce::jlimit (0, 2, static_cast<int> (apvts.getRawParameterValue ("NOTE_SPELLING")->load())));

    auto chords = musictheory::buildDiatonicChords (getKeyTonicPitchClass(), getScaleType(), spelling);

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

void HomeChordsAudioProcessor::addChordFromSlotToTimeline (int slot)
{
    const auto chords = getCurrentDiatonicChords();

    if (slot < 0 || slot >= static_cast<int> (chords.size()))
        return;

    const auto& chord = chords[static_cast<size_t> (slot)];

    progression::ChordEvent event;
    event.scaleDegree = chord.scaleDegree;
    event.rootPitchClass = chord.rootPitchClass;
    event.quality = chord.quality;
    event.extension = getSlotExtensionOverride (slot);
    event.velocity = static_cast<int> (apvts.getRawParameterValue ("VELOCITY")->load());
    event.startBeats = progression.getTotalLengthBeats();
    event.lengthBeats = getBeatsPerBar();

    undoManager.perform (new progression::AddChordAction (progression, event));
}

void HomeChordsAudioProcessor::setSequencePlaying (bool shouldPlay)
{
    sequencePlayer.setPlaying (shouldPlay);

    if (! shouldPlay)
        sequencerReleaseRequested.store (true, std::memory_order_relaxed);
}

void HomeChordsAudioProcessor::restartSequencer()
{
    sequencerReleaseRequested.store (true, std::memory_order_relaxed);
    sequencerSeekRequestBeats.store (sequencePlayer.getLoopStartBeats(), std::memory_order_relaxed);
}

void HomeChordsAudioProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    sampleRate = newSampleRate;
    keyboardEngine.reset();
    sequencePlayer.prepare (sampleRate);
    previewSynth.prepare (sampleRate, samplesPerBlock);
    midiActivityForUi.store (0.0f, std::memory_order_relaxed);
}

void HomeChordsAudioProcessor::releaseResources()
{
    // No MidiBuffer is available here to send final Note Offs through, so
    // this only clears internal state. The editor releases every held
    // keyboard/mouse slot when it loses keyboard focus or is closed --
    // see PluginEditor. A host that hard-kills the plugin mid-chord is the
    // one case this can't fully cover; hosts generally send their own
    // all-notes-off in that situation.
    keyboardEngine.reset();
    sequencePlayer.reset();
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

    if (numSamples <= 0)
    {
        midi.clear();
        return;
    }

    // Incoming MIDI keyboard input: white keys (by pitch class) trigger
    // the same slots a computer-keyboard key or mouse click would, mapped
    // by ordinal position -- see whiteKeySlotForPitchClass above. This is
    // audio-thread-safe: setSlotHeldFromMidi only touches plain members
    // this same thread owns, plus one atomic (triggeredMask) that's safe
    // to write from any thread.
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();

        if (message.isNoteOn() || message.isNoteOff())
        {
            const int slot = whiteKeySlotForPitchClass (message.getNoteNumber() % 12);
            if (slot >= 0)
                keyboardEngine.setSlotHeldFromMidi (slot, message.isNoteOn());
        }
    }

    midi.clear();   // don't pass the raw incoming notes through to output

    const int tonicPitchClass = getKeyTonicPitchClass();
    const auto scaleType = getScaleType();
    const int octaveOffset = static_cast<int> (apvts.getRawParameterValue ("OCTAVE")->load());
    const int velocity = static_cast<int> (apvts.getRawParameterValue ("VELOCITY")->load());
    const float previewGainDb = apvts.getRawParameterValue ("PREVIEW_GAIN")->load();
    const bool autoInversion = apvts.getRawParameterValue ("AUTO_INVERSION")->load() >= 0.5f;

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

    keyboardEngine.renderBlockStart (midi, noteLedger, finalShapes, octaveOffset, velocity, 1, autoInversion);

    // Sequencer: sync its tempo/loop settings from the parameters, apply
    // any pending stop-release or seek request, then let it advance and
    // trigger this block's timeline events (if playing).
    sequencePlayer.setTempoBpm (apvts.getRawParameterValue ("TEMPO")->load());
    sequencePlayer.setLooping (apvts.getRawParameterValue ("LOOP_ENABLED")->load() >= 0.5f);

    if (sequencerReleaseRequested.exchange (false, std::memory_order_relaxed))
        sequencePlayer.releaseAllNotes (midi, noteLedger, 1);

    const double pendingSeek = sequencerSeekRequestBeats.exchange (-1.0, std::memory_order_relaxed);
    if (pendingSeek >= 0.0)
        sequencePlayer.setPositionBeats (pendingSeek);

    sequencePlayer.advanceAndRender (progression, midi, noteLedger, 1, numSamples, octaveOffset, &keyboardEngine);

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

    // Phase 2 groundwork, now actually used: the real progression, plus
    // the loop range (session/editing state, not something that needs
    // host automation, so it isn't an APVTS parameter).
    state.appendChild (progression.toValueTree(), nullptr);
    state.setProperty ("loopStartBeats", sequencePlayer.getLoopStartBeats(), nullptr);
    state.setProperty ("loopEndBeats", sequencePlayer.getLoopEndBeats(), nullptr);

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

            const double loopStart = static_cast<double> (state.getProperty ("loopStartBeats", 0.0));
            const double loopEnd = static_cast<double> (state.getProperty ("loopEndBeats", 16.0));
            sequencePlayer.setLoopRange (loopStart, loopEnd);
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
