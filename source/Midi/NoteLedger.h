#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

// =============================================================================
// One reference count per MIDI note number, shared by everything that emits
// notes (live play AND the progression sequencer), so they can never step on
// each other: a note only gets a Note On when its count goes 0 -> 1, and only
// a Note Off when it goes back to 0. Two chords sharing a note number, or a
// live-played chord overlapping a sequenced one, therefore can't cut each
// other off early or send a duplicate Note Off.
//
// Real-time safe: fixed-size, no allocation.
// =============================================================================

namespace midiengine
{
    class NoteLedger
    {
    public:
        void reset() noexcept { counts.fill (0); }

        void noteOn (juce::MidiBuffer& out, int channel, int note, juce::uint8 velocity, int samplePosition) noexcept
        {
            if (note < 0 || note > 127)
                return;

            auto& count = counts[static_cast<size_t> (note)];

            if (count == 0)
                out.addEvent (juce::MidiMessage::noteOn (channel, note, velocity), samplePosition);

            ++count;
        }

        void noteOff (juce::MidiBuffer& out, int channel, int note, int samplePosition) noexcept
        {
            if (note < 0 || note > 127)
                return;

            auto& count = counts[static_cast<size_t> (note)];

            if (count <= 0)
                return;

            if (--count == 0)
                out.addEvent (juce::MidiMessage::noteOff (channel, note), samplePosition);
        }

        int getCount (int note) const noexcept
        {
            return (note >= 0 && note <= 127) ? counts[static_cast<size_t> (note)] : 0;
        }

        bool isAnythingSounding() const noexcept
        {
            for (const int c : counts)
                if (c > 0)
                    return true;

            return false;
        }

    private:
        std::array<int, 128> counts {};
    };
}
