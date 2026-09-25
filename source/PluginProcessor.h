#pragma once

#include <JuceHeader.h>
#include "MusicTheory/MusicTheoryEngine.h"
#include "Midi/ChordKeyboardEngine.h"
#include "Preview/PreviewSynth.h"
#include "Progression/ProgressionModel.h"

// =============================================================================
// Home-Chords -- MIDI chord generator / songwriting plugin.
//
// Phase 1 scope (see STATUS.md for the full breakdown): Key + Scale drive
// algorithmic diatonic chord generation; the 7 keyboard slots (A S D F G H
// J) trigger real, real-time-safe MIDI Note On/Off; a small internal
// preview synth makes the plugin audible on its own. `progression` is
// Phase 2 groundwork -- it's a real, tested data model, but nothing here
// reads from or writes to it yet.
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

    // Incoming MIDI is currently ignored (reserved for the MIDI chord-
    // detection feature in spec section 34); the plugin does generate MIDI
    // from the keyboard, so producesMidi() is genuinely true.
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

    // ---- Called from the editor's message-thread key/mouse handling ----

    // Thread-safe: just flips a bit the audio thread reads at the next
    // block. Slot index is 0-6 (A through J).
    void setChordSlotHeld (int slotIndex, bool held) noexcept { keyboardEngine.setSlotHeld (slotIndex, held); }

    int getKeyTonicPitchClass() const noexcept;
    musictheory::ScaleType getScaleType() const noexcept;

    // Message-thread only, allocates -- call when Key/Scale changes, not
    // once per frame.
    std::vector<musictheory::ChordDefinition> getCurrentDiatonicChords() const;

    // UI polling (message-thread Timer): which slots are currently
    // sounding, and a decaying MIDI-out activity level.
    juce::uint32 getActiveSlotMask() const noexcept { return keyboardEngine.getActiveSlotMaskForUi(); }
    float getMidiActivity() const noexcept { return midiActivityForUi.load (std::memory_order_relaxed); }

    // Phase 2 groundwork -- not yet read from or written to by
    // processBlock or the editor. See the class comment above.
    progression::ProgressionModel progression;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

    midiengine::ChordKeyboardEngine keyboardEngine;
    preview::PreviewSynth previewSynth;

    double sampleRate = 44100.0;
    std::atomic<float> midiActivityForUi { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HomeChordsAudioProcessor)
};
