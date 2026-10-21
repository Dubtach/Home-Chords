#pragma once

#include <JuceHeader.h>
#include "MusicTheory/MusicTheoryEngine.h"
#include "MusicTheory/Presets.h"
#include "MusicTheory/ColorChords.h"
#include "MusicTheory/Suggestions.h"
#include "Midi/MidiExport.h"
#include "Midi/ChordKeyboardEngine.h"
#include "Midi/NoteLedger.h"
#include "Preview/PreviewSynth.h"
#include "Progression/ProgressionModel.h"
#include "Progression/ProgressionActions.h"
#include "Timeline/SequencePlayer.h"

// =============================================================================
// Home-Chords -- MIDI chord generator / songwriting plugin.
//
// Key + Scale drive algorithmic diatonic chord generation; the 7 chord
// slots trigger real, real-time-safe MIDI Note On/Off, playable from the
// computer keyboard (letters and/or numbers, see KEYBOARD_MODE), the
// mouse, or an external MIDI keyboard's white keys (mapped by ordinal
// position -- the 1st, 2nd, ...7th white key in an octave, regardless of
// its actual pitch, so they always mean "slot 1..7" of whatever Key/Scale
// is selected). A small internal preview synth makes the plugin audible
// on its own.
//
// PLAY mode sounds a slot live; ADD mode (keyboard/MIDI only, not mouse --
// see ChordKeyboardEngine) appends that slot's current chord to the end of
// `progression`, which SequencePlayer plays back on its own internal
// transport (Play/Stop/Loop) -- or, with SYNC_HOST on, in lockstep with the
// host's transport. Every progression edit goes through `undoManager`.
//
// Also here: voicing styles (Close/Open/Wide/Piano), humanize, borrowed/
// colour chords, next-chord suggestions (UI-side, see PluginEditor), and
// MIDI-file export of the progression.
// =============================================================================

class HomeChordsAudioProcessor : public juce::AudioProcessor
{
public:
    HomeChordsAudioProcessor();
    ~HomeChordsAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Home-Chords"; }

    // Incoming MIDI note on/off from a real MIDI keyboard's white keys
    // triggers chord slots (see the class comment above and
    // whiteKeySlotForPitchClass in the .cpp) -- so both accepting and
    // producing MIDI are genuinely true.
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::UndoManager undoManager;

    // The timeline's data (real, tested) and its transport-synced player.
    // Both public, matching apvts's own exposure style -- the editor reads
    // and mutates them directly rather than through processor passthroughs
    // for every single field.
    progression::ProgressionModel progression;
    timeline::SequencePlayer sequencePlayer;

    // ---- Called from the editor's message-thread key/mouse handling ----

    // Computer keyboard: sounds the chord live, and (in ADD mode) counts
    // as a trigger for the timeline. Slot index is 0-6 (first..last active
    // slot for the current scale).
    void setChordSlotHeld (int slotIndex, bool held) noexcept { keyboardEngine.setSlotHeld (slotIndex, held); }

    // Mouse: sounds the chord live, never triggers ADD mode.
    void setChordSlotHeldFromMouse (int slotIndex, bool held) noexcept { keyboardEngine.setSlotHeldFromMouse (slotIndex, held); }

    // Drains the set of slots triggered (by keyboard or MIDI) since the
    // last call -- what ADD mode acts on. Call once per UI tick regardless
    // of whether ADD mode is currently on, so switching into ADD mode
    // later doesn't replay stale triggers.
    juce::uint32 consumeTriggeredSlotsForAddMode() noexcept { return keyboardEngine.consumeTriggeredSlots(); }

    // Appends slot's current effective chord (quality/extension override
    // included) to the end of the progression, one bar long, through
    // undoManager. Message thread only -- mutates progression/undoManager,
    // neither of which is real-time safe.
    void addChordFromSlotToTimeline (int slot);

    // Appends a borrowed/colour chord (see MusicTheory/ColorChords.h) to the
    // end of the progression, one bar long, through undoManager. Message
    // thread only.
    void addColorChordToTimeline (const musictheory::ColorChord& chord);

    // Colour chords for the current Key/Scale, spelled per NOTE_SPELLING.
    std::vector<musictheory::ColorChord> getCurrentColorChords() const;

    // The current VOICING parameter.
    musictheory::VoicingStyle getVoicingStyle() const noexcept;

    // True if note names should be spelled with flats right now (the
    // NOTE_SPELLING preference, or the key's own bias under Auto).
    bool shouldSpellWithFlats() const noexcept;

    // Everything the MIDI exporter needs, read from the current parameters.
    midiexport::ExportSettings getExportSettings() const;

    int getKeyTonicPitchClass() const noexcept;
    musictheory::ScaleType getScaleType() const noexcept;

    // Message-thread only, allocates -- call when Key/Scale changes, not
    // once per frame. Returns the *effective* chord for each slot: the
    // plain diatonic chord, unless that slot has a quality/extension
    // override, in which case the override is already applied. Spelling
    // follows the NOTE_SPELLING parameter.
    std::vector<musictheory::ChordDefinition> getCurrentDiatonicChords() const;

    // How many beats one bar is, from the TIME_SIG parameter (4/4->4,
    // 3/4->3, 6/8->3, treating 6/8 as two dotted-quarter beats for bar-
    // length and grid purposes). Message thread only.
    double getBeatsPerBar() const noexcept;

    // ---- Per-slot quality/extension overrides ---------------------------
    //
    // Backed by real APVTS parameters (SLOT<n>_QUALITY / SLOT<n>_EXTENSION),
    // so they get host automation and state save/restore for free, same as
    // every other parameter here. -1 for quality means "no override, use
    // the diatonic quality"; Extension::None (0) means "no added tone".

    int getSlotQualityOverride (int slot) const noexcept;
    musictheory::Extension getSlotExtensionOverride (int slot) const noexcept;
    void setSlotQualityOverride (int slot, int qualityOrMinus1);
    void setSlotExtensionOverride (int slot, musictheory::Extension extension);
    void clearSlotOverride (int slot);

    // ---- Sequencer transport (message thread) ---------------------------
    // These go through request flags consumed at the top of the next
    // processBlock, rather than touching sequencePlayer's note-emitting
    // side directly, because stopping/seeking needs to emit Note Offs
    // through a MidiBuffer that only exists inside processBlock.

    void setSequencePlaying (bool shouldPlay);
    void restartSequencer();   // seeks to the loop start and releases notes

    // UI polling (message-thread Timer): which slots are currently
    // sounding, and a decaying MIDI-out activity level.
    juce::uint32 getActiveSlotMask() const noexcept { return keyboardEngine.getActiveSlotMaskForUi(); }
    float getMidiActivity() const noexcept { return midiActivityForUi.load (std::memory_order_relaxed); }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

    // Per-slot parameter IDs, e.g. "SLOT3_QUALITY" / "SLOT3_EXTENSION".
    // Used both when declaring the parameters and when caching pointers to
    // them below -- one place, so the two can never drift apart.
    static juce::String qualityParamId (int slot);
    static juce::String extensionParamId (int slot);

    // Computes the shape actually heard for one slot: the diatonic shape
    // with that slot's override (if any) applied. Real-time safe -- this
    // is the single place processBlock gets a slot's final shape from.
    musictheory::RtChordShape effectiveShape (int slot, const musictheory::RtChordShape& diatonicShape) const noexcept;

    midiengine::ChordKeyboardEngine keyboardEngine;
    midiengine::NoteLedger noteLedger;   // shared between live play and the sequencer -- see NoteLedger.h
    preview::PreviewSynth previewSynth;

    double sampleRate = 44100.0;
    std::atomic<float> midiActivityForUi { 0.0f };

    // See the "Sequencer transport" methods above.
    std::atomic<bool> sequencerReleaseRequested { false };
    std::atomic<double> sequencerSeekRequestBeats { -1.0 };

    // Raw pointers into apvts's own storage, cached once at construction so
    // processBlock never has to build a parameter-ID string (which would
    // allocate) just to read one of these every block.
    std::array<std::atomic<float>*, musictheory::maxDiatonicSlots> qualityOverrideParams {};
    std::array<std::atomic<float>*, musictheory::maxDiatonicSlots> extensionOverrideParams {};

    // Same idea for the parameters processBlock reads every block (a lookup
    // by string ID per block is avoidable work on the audio thread).
    std::atomic<float>* keyParam = nullptr;
    std::atomic<float>* scaleParam = nullptr;
    std::atomic<float>* octaveParam = nullptr;
    std::atomic<float>* velocityParam = nullptr;
    std::atomic<float>* previewGainParam = nullptr;
    std::atomic<float>* autoInversionParam = nullptr;
    std::atomic<float>* tempoParam = nullptr;
    std::atomic<float>* loopEnabledParam = nullptr;
    std::atomic<float>* voicingParam = nullptr;
    std::atomic<float>* humanizeTimeParam = nullptr;
    std::atomic<float>* humanizeVelParam = nullptr;
    std::atomic<float>* syncHostParam = nullptr;

    // Audio-thread-only: was the sequencer following the host last block?
    bool wasHostSyncing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HomeChordsAudioProcessor)
};
