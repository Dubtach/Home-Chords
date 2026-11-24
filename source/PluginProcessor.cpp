#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

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
        colourOverrideParams[static_cast<size_t> (slot)] = apvts.getRawParameterValue (colourParamId (slot));
    }

    keyParam           = apvts.getRawParameterValue ("KEY");
    scaleParam         = apvts.getRawParameterValue ("SCALE");
    octaveParam        = apvts.getRawParameterValue ("OCTAVE");
    velocityParam      = apvts.getRawParameterValue ("VELOCITY");
    previewGainParam   = apvts.getRawParameterValue ("PREVIEW_GAIN");
    autoInversionParam = apvts.getRawParameterValue ("AUTO_INVERSION");
    tempoParam         = apvts.getRawParameterValue ("TEMPO");
    loopEnabledParam   = apvts.getRawParameterValue ("LOOP_ENABLED");
    voicingParam       = apvts.getRawParameterValue ("VOICING");
    humanizeTimeParam  = apvts.getRawParameterValue ("HUMANIZE_TIME");
    humanizeVelParam   = apvts.getRawParameterValue ("HUMANIZE_VEL");
    syncHostParam      = apvts.getRawParameterValue ("SYNC_HOST");

    sequencePlayer.setLoopRange (0.0, 16.0);
}

juce::String HomeChordsAudioProcessor::qualityParamId (int slot)   { return "SLOT" + juce::String (slot) + "_QUALITY"; }
juce::String HomeChordsAudioProcessor::extensionParamId (int slot) { return "SLOT" + juce::String (slot) + "_EXTENSION"; }
juce::String HomeChordsAudioProcessor::colourParamId (int slot)    { return "SLOT" + juce::String (slot) + "_COLOUR"; }

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

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "TEMPO", "Tempo", juce::NormalisableRange<float> (20.0f, 300.0f, 0.1f), 120.0f,
        FloatAttributes{}.withLabel ("BPM")));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "TIME_SIG", "Time Signature", juce::StringArray { "4/4", "3/4", "6/8" }, 0, ChoiceAttributes{}));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "LOOP_ENABLED", "Loop", true, BoolAttributes{}));

    // Follow the host's transport (start/stop, position, tempo) instead of
    // the plugin's own Play button and Tempo knob.
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "SYNC_HOST", "Sync to Host", false, BoolAttributes{}));

    // Index maps 1:1 to musictheory::VoicingStyle.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "VOICING", "Voicing", juce::StringArray { "Close", "Open", "Wide", "Piano" }, 0, ChoiceAttributes{}));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "HUMANIZE_TIME", "Humanize Timing", juce::NormalisableRange<float> (0.0f, 30.0f, 0.1f), 0.0f,
        FloatAttributes{}.withLabel ("ms")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "HUMANIZE_VEL", "Humanize Velocity", juce::NormalisableRange<float> (0.0f, 30.0f, 0.1f), 0.0f,
        FloatAttributes{}.withLabel ("%")));

    // Timeline snap grid, in the order the editor's snap box lists them:
    // 1/16, 1/8, 1/4, 1/2 note, one bar.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "SNAP", "Snap", juce::StringArray { "1/16", "1/8", "1/4", "1/2", "Bar" }, 0, ChoiceAttributes{}));

    // Show "what could come next" hints on the chord row.
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "HINTS", "Next-Chord Hints", true, BoolAttributes{}));

    // Default playback style for new chords (and the whole timeline when nothing is selected) (see
    // applyPlaybackField). The choice lists mirror the tables in
    // Progression/PlaybackSettings.h.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "RHYTHM", "Rhythm",
        juce::StringArray { "Sustained", "Whole", "Half", "Quarter", "Eighth", "Sixteenth", "Strum", "Arpeggio" }, 0, ChoiceAttributes{}));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "BASS_MODE", "Bass", juce::StringArray { "Root", "Third", "Fifth", "Inversion" }, 3, ChoiceAttributes{}));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "ARP_MODE", "Arp Mode", juce::StringArray { "Off", "Up", "Down", "Up/Down", "Random" }, 0, ChoiceAttributes{}));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "ARP_RATE", "Arp Rate", juce::StringArray { "1/4", "1/8", "1/16", "1/32" }, 2, ChoiceAttributes{}));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "GATE", "Gate", juce::StringArray { "25%", "50%", "80%", "Legato" }, 2, ChoiceAttributes{}));

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

        // 0 = the plain diatonic chord; 1..10 = borrowed/colour chord 1..10 for the key.
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            colourParamId (slot), "Slot " + juce::String (slot + 1) + " Colour Chord",
            juce::StringArray { "Diatonic", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" }, 0, ChoiceAttributes{}));
    }

    return { params.begin(), params.end() };
}

int HomeChordsAudioProcessor::getKeyTonicPitchClass() const noexcept
{
    return static_cast<int> (keyParam->load());
}

musictheory::ScaleType HomeChordsAudioProcessor::getScaleType() const noexcept
{
    const auto index = static_cast<int> (scaleParam->load());
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

musictheory::VoicingStyle HomeChordsAudioProcessor::getVoicingStyle() const noexcept
{
    return static_cast<musictheory::VoicingStyle> (
        juce::jlimit (0, musictheory::numVoicingStyles - 1, static_cast<int> (voicingParam->load())));
}

bool HomeChordsAudioProcessor::shouldSpellWithFlats() const noexcept
{
    const auto spelling = static_cast<musictheory::NoteSpelling> (
        juce::jlimit (0, 2, static_cast<int> (apvts.getRawParameterValue ("NOTE_SPELLING")->load())));

    switch (spelling)
    {
        case musictheory::NoteSpelling::Sharps: return false;
        case musictheory::NoteSpelling::Flats:  return true;
        default:                                return musictheory::prefersFlats (getKeyTonicPitchClass(), getScaleType());
    }
}

std::vector<musictheory::ColorChord> HomeChordsAudioProcessor::getCurrentColorChords() const
{
    const auto spelling = static_cast<musictheory::NoteSpelling> (
        juce::jlimit (0, 2, static_cast<int> (apvts.getRawParameterValue ("NOTE_SPELLING")->load())));

    return musictheory::buildColorChords (getKeyTonicPitchClass(), getScaleType(), spelling);
}

midiexport::ExportSettings HomeChordsAudioProcessor::getExportSettings() const
{
    midiexport::ExportSettings settings;
    settings.bpm = static_cast<double> (tempoParam->load());
    settings.beatsPerBar = static_cast<int> (getBeatsPerBar());
    settings.rootOctaveOffset = static_cast<int> (octaveParam->load());
    settings.voicing = getVoicingStyle();
    settings.autoInversion = autoInversionParam->load() >= 0.5f;
    settings.humanizeTimingMs = humanizeTimeParam->load();
    settings.humanizeVelocity = humanizeVelParam->load() * 0.01f;
    return settings;
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
    const auto colourChords = musictheory::buildColorChords (getKeyTonicPitchClass(), getScaleType(), spelling);

    for (int slot = 0; slot < static_cast<int> (chords.size()); ++slot)
    {
        auto& chord = chords[static_cast<size_t> (slot)];

        musictheory::SlotOverride slotOverride;
        slotOverride.qualityOverride = getSlotQualityOverride (slot);
        slotOverride.extension = getSlotExtensionOverride (slot);

        const int colour = getSlotColourIndex (slot);

        if (colour < 0 || colour >= static_cast<int> (colourChords.size()))
        {
            chord = musictheory::applyOverride (chord, slotOverride);
            continue;
        }

        // A borrowed chord replaces the diatonic one on this card.
        const auto& borrowed = colourChords[static_cast<size_t> (colour)];
        chord.rootPitchClass = borrowed.rootPitchClass;
        chord.rootName = borrowed.rootName;
        chord.quality = borrowed.quality;
        chord.romanNumeral = borrowed.label;
        chord.chordName = borrowed.chordName;
        chord.shape = borrowed.shape;

        if (slotOverride.isActive())
        {
            const auto quality = slotOverride.qualityOverride >= 0
                                     ? static_cast<musictheory::ChordQuality> (slotOverride.qualityOverride)
                                     : borrowed.quality;

            chord.quality = quality;
            chord.shape = musictheory::buildOverriddenShape (borrowed.rootPitchClass, quality, slotOverride.extension);
            chord.chordName = borrowed.rootName + musictheory::chordQualitySuffix (quality)
                                                + musictheory::extensionSuffix (slotOverride.extension);
            chord.romanNumeral = borrowed.label + musictheory::extensionSuffix (slotOverride.extension);
        }
    }

    return chords;
}

musictheory::RtChordShape HomeChordsAudioProcessor::effectiveShape (int slot, const musictheory::RtChordShape& diatonicShape,
                                                                     int tonicPitchClass, musictheory::ScaleType scale) const noexcept
{
    if (slot < 0 || slot >= musictheory::maxDiatonicSlots)
        return diatonicShape;

    auto base = diatonicShape;

    const int colour = static_cast<int> (colourOverrideParams[static_cast<size_t> (slot)]->load()) - 1;
    if (colour >= 0 && colour < musictheory::numColorChords (scale))
        base = musictheory::colorChordShape (colour, tonicPitchClass, scale);

    musictheory::SlotOverride slotOverride;
    slotOverride.qualityOverride = static_cast<int> (qualityOverrideParams[static_cast<size_t> (slot)]->load()) - 1;
    slotOverride.extension = static_cast<musictheory::Extension> (
        static_cast<int> (extensionOverrideParams[static_cast<size_t> (slot)]->load()));

    return musictheory::applyOverride (base, slotOverride);
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

int HomeChordsAudioProcessor::getSlotColourIndex (int slot) const noexcept
{
    if (slot < 0 || slot >= musictheory::maxDiatonicSlots)
        return -1;

    const int index = static_cast<int> (colourOverrideParams[static_cast<size_t> (slot)]->load()) - 1;
    return index < musictheory::numColorChords (getScaleType()) ? index : -1;   // a stale index from another scale reads as "none"
}

void HomeChordsAudioProcessor::setSlotColourIndex (int slot, int indexOrMinus1)
{
    if (slot < 0 || slot >= musictheory::maxDiatonicSlots)
        return;

    const int clamped = juce::jlimit (-1, 9, indexOrMinus1);

    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (colourParamId (slot))))
        *param = clamped + 1;
}

void HomeChordsAudioProcessor::clearSlotOverride (int slot)
{
    setSlotQualityOverride (slot, -1);
    setSlotExtensionOverride (slot, musictheory::Extension::None);
    setSlotColourIndex (slot, -1);
}

bool HomeChordsAudioProcessor::makeEventFromSlot (int slot, progression::ChordEvent& out) const
{
    const auto chords = getCurrentDiatonicChords();

    if (slot < 0 || slot >= static_cast<int> (chords.size()))
        return false;

    const auto& chord = chords[static_cast<size_t> (slot)];
    const int colour = getSlotColourIndex (slot);

    // The extension a card actually plays: its own override if set, else the
    // borrowed chord's own (e.g. the 7th in V/V), else none.
    auto extension = getSlotExtensionOverride (slot);

    if (extension == musictheory::Extension::None && colour >= 0 && getSlotQualityOverride (slot) < 0)
    {
        const auto palette = getCurrentColorChords();
        if (colour < static_cast<int> (palette.size()))
            extension = palette[static_cast<size_t> (colour)].extension;
    }

    progression::ChordEvent event;
    event.scaleDegree = colour >= 0 ? -1 : chord.scaleDegree;   // a borrowed chord isn't a diatonic degree; hints skip it
    event.rootPitchClass = chord.rootPitchClass;
    event.quality = chord.quality;
    event.extension = extension;
    event.velocity = static_cast<int> (velocityParam->load());
    event.startBeats = 0.0;
    event.lengthBeats = getBeatsPerBar();
    getPlaybackSettings().applyTo (event);

    out = event;
    return true;
}

void HomeChordsAudioProcessor::addChordFromSlotToTimeline (int slot)
{
    progression::ChordEvent event;

    if (! makeEventFromSlot (slot, event))
        return;

    event.startBeats = progression.getTotalLengthBeats();

    undoManager.beginNewTransaction ("Add chord");
    undoManager.perform (new progression::AddChordAction (progression, event));
    fitLoopToProgression();
}

progression::PlaybackSettings HomeChordsAudioProcessor::getPlaybackSettings() const
{
    const auto value = [this] (const char* id) { return static_cast<int> (apvts.getRawParameterValue (id)->load()); };

    return progression::PlaybackSettings::fromChoices (value ("RHYTHM"), value ("BASS_MODE"), value ("ARP_MODE"),
                                                       value ("ARP_RATE"), value ("GATE"));
}

void HomeChordsAudioProcessor::applyPlaybackField (progression::PlaybackField field, int choice, const std::vector<juce::Uuid>& scope)
{
    // With nothing selected the change is global: it also becomes the default
    // for chords added later. With a selection it touches only those chords.
    if (scope.empty())
    {
        static constexpr const char* ids[] = { "RHYTHM", "BASS_MODE", "ARP_MODE", "ARP_RATE", "GATE" };

        if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ids[static_cast<int> (field)])))
            *param = choice;
    }

    std::vector<progression::ChordEvent> changed;

    for (int i = 0; i < progression.size(); ++i)
    {
        auto event = progression.getEvent (i);

        if (! scope.empty() && ! progression::layout::containsId (scope, event.id))
            continue;

        if (progression::fieldChoice (event, field) == choice)
            continue;

        progression::applyField (event, field, choice);
        changed.push_back (event);
    }

    if (changed.empty())
        return;

    undoManager.beginNewTransaction (scope.empty() ? "Change playback style" : "Change playback style (selection)");

    for (const auto& event : changed)
        undoManager.perform (new progression::UpdateChordAction (progression, event.id, event));
}

void HomeChordsAudioProcessor::replaceTimeline (const progression::ProgressionModel& model, const juce::String& undoName)
{
    const auto settings = getPlaybackSettings();

    auto events = model.getEvents();
    progression::layout::pack (events);

    for (auto& event : events)
    {
        event.velocity = static_cast<int> (velocityParam->load());
        settings.applyTo (event);
    }

    undoManager.beginNewTransaction (undoName);
    undoManager.perform (new progression::SetEventsAction (progression, std::move (events)));
    fitLoopToProgression();
}

void HomeChordsAudioProcessor::fitLoopToProgression()
{
    // The loop always spans exactly the chords: it follows every add, delete,
    // reorder and resize, so there is never anything to drag by hand.
    const double end = progression.getTotalLengthBeats();
    const double wanted = end > 0.0 ? end : getBeatsPerBar() * 4.0;

    if (sequencePlayer.getLoopStartBeats() != 0.0 || sequencePlayer.getLoopEndBeats() != wanted)
        sequencePlayer.setLoopRange (0.0, wanted);
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
    noteLedger.reset();   // the engines above forgot their notes, so the shared reference counts must be forgotten too -- stale counts would suppress future Note Ons
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
    noteLedger.reset();
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
    const int octaveOffset = static_cast<int> (octaveParam->load());
    const int velocity = static_cast<int> (velocityParam->load());
    const float previewGainDb = previewGainParam->load();
    const bool autoInversion = autoInversionParam->load() >= 0.5f;

    // Recomputed fresh every block from the current Key/Scale parameters
    // rather than cached -- this is cheap (a handful of fixed-size array
    // writes) and sidesteps needing any cross-thread chord cache at all.
    const auto diatonicShapes = musictheory::buildDiatonicShapes (tonicPitchClass, scaleType);

    // Apply each slot's colour chord / quality / extension override (if any)
    // on top of the diatonic shape. This is the
    // one place processBlock and getCurrentDiatonicChords (the UI's view)
    // both go through effectiveShape(), so what's heard always matches
    // what's shown, override or not.
    musictheory::DiatonicShapeSet finalShapes;
    finalShapes.count = diatonicShapes.count;
    for (int slot = 0; slot < diatonicShapes.count; ++slot)
        finalShapes.shapes[static_cast<size_t> (slot)] = effectiveShape (slot, diatonicShapes.shapes[static_cast<size_t> (slot)],
                                                                          tonicPitchClass, scaleType);

    keyboardEngine.renderBlockStart (midi, noteLedger, finalShapes, octaveOffset, velocity, 1, autoInversion, getVoicingStyle(),
                                     tonicPitchClass);

    // Sequencer: sync its tempo/loop settings from the parameters, apply
    // any pending stop-release or seek request, then let it advance and
    // trigger this block's timeline events (if playing).
    sequencePlayer.setHumanize (humanizeTimeParam->load(), humanizeVelParam->load() * 0.01f);
    sequencePlayer.setVoicingStyle (getVoicingStyle());
    sequencePlayer.setAutoInversion (autoInversion);

    const bool hostSync = syncHostParam->load() >= 0.5f;

    if (hostSync)
    {
        // The host owns the clock, tempo and looping in this mode.
        timeline::SequencePlayer::HostTransport transport;

        if (auto* hostPlayHead = getPlayHead())
        {
            if (const auto position = hostPlayHead->getPosition())
            {
                if (const auto ppq = position->getPpqPosition())
                {
                    transport.valid = true;
                    transport.playing = position->getIsPlaying();
                    transport.ppq = *ppq;

                    if (const auto bpm = position->getBpm())
                        transport.bpm = *bpm;
                }
            }
        }

        sequencePlayer.setLooping (false);
        sequencePlayer.syncToHost (transport, midi, noteLedger, 1);
    }
    else
    {
        // Switching sync off while the host is rolling must not leave the
        // sequencer running on its own: hand control back stopped.
        if (wasHostSyncing)
        {
            sequencePlayer.setPlaying (false);
            sequencerReleaseRequested.store (true, std::memory_order_relaxed);
        }

        sequencePlayer.setTempoBpm (tempoParam->load());
        sequencePlayer.setLooping (loopEnabledParam->load() >= 0.5f);
    }

    wasHostSyncing = hostSync;

    if (sequencerReleaseRequested.exchange (false, std::memory_order_relaxed))
        sequencePlayer.releaseAllNotes (midi, noteLedger, 1);

    const double pendingSeek = sequencerSeekRequestBeats.exchange (-1.0, std::memory_order_relaxed);
    if (pendingSeek >= 0.0)
        sequencePlayer.setPositionBeats (pendingSeek);

    sequencePlayer.advanceAndRender (progression, midi, noteLedger, 1, numSamples, octaveOffset, &keyboardEngine);

    bool anyNoteThisBlock = false;

    previewSynth.setGain (juce::Decibels::decibelsToGain (previewGainDb));

    // Render up to each MIDI event and apply it exactly where it falls, so fast
    // arpeggios keep their timing instead of snapping to the block start.
    int rendered = 0;

    for (const auto metadata : midi)
    {
        const int at = juce::jlimit (rendered, numSamples, metadata.samplePosition);

        if (at > rendered)
        {
            previewSynth.renderBlock (buffer, rendered, at - rendered);
            rendered = at;
        }

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

    if (rendered < numSamples)
        previewSynth.renderBlock (buffer, rendered, numSamples - rendered);

    float activity = midiActivityForUi.load (std::memory_order_relaxed);
    activity = anyNoteThisBlock ? 1.0f : activity * 0.90f;
    midiActivityForUi.store (activity, std::memory_order_relaxed);
}

void HomeChordsAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    // The real progression, plus the loop range (session/editing state, not
    // something that needs host automation, so it isn't an APVTS
    // parameter). A state restored by setStateInformation leaves its
    // PROGRESSION child inside apvts.state, so drop any such stale copy
    // first -- otherwise every save appends another, and the *first* (oldest)
    // one wins on the next load.
    for (auto stale = state.getChildWithName ("PROGRESSION"); stale.isValid(); stale = state.getChildWithName ("PROGRESSION"))
        state.removeChild (stale, nullptr);

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

            // Undo history refers to the progression that was just replaced.
            undoManager.clearUndoHistory();

            // Sessions saved before Rhythm/Bass/Arp/Gate became global have no
            // such parameters: adopt the first chord's style as the global one
            // so opening the editor doesn't flatten the whole timeline to defaults.
            bool hasGlobalStyle = false;
            for (const auto& child : state)
                if (child.getProperty ("id").toString() == "RHYTHM")
                    hasGlobalStyle = true;

            if (! hasGlobalStyle && ! progression.isEmpty())
            {
                const auto& first = progression.getEvent (0);
                const auto nearest = [] (const auto& table, double value)
                {
                    int best = 0;
                    for (size_t i = 1; i < table.size(); ++i)
                        if (std::abs (table[i] - value) < std::abs (table[static_cast<size_t> (best)] - value))
                            best = static_cast<int> (i);
                    return best;
                };

                const auto setChoice = [this] (const char* id, int index)
                {
                    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id)))
                        *param = index;
                };

                setChoice ("RHYTHM", static_cast<int> (first.rhythm));
                setChoice ("BASS_MODE", juce::jmin (static_cast<int> (first.bassMode), progression::numBassChoices - 1));
                setChoice ("ARP_MODE", static_cast<int> (first.arpMode));
                setChoice ("ARP_RATE", nearest (progression::arpRateBeatsByChoice, first.arpRateBeats));
                setChoice ("GATE", nearest (progression::gateByChoice, first.arpGate));
            }

            // Sessions from before the timeline became gap-free may have overlaps
            // or holes: close them up, and span the loop over the result.
            auto restored = progression.getEvents();
            progression::layout::pack (restored);
            progression.setEvents (std::move (restored));
            fitLoopToProgression();
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
