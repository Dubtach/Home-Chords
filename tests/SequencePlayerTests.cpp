#include "Timeline/SequencePlayer.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cstdlib>
#include <vector>

using namespace timeline;
using namespace progression;

// =============================================================================
// SequencePlayer is the riskiest piece of the project (audio-thread, sample-
// accurate scheduling), so these tests drive it exactly the way processBlock
// does -- fixed-size blocks, absolute sample positions reconstructed from the
// MIDI buffer -- and check timing and note balance, not just "something came
// out".
// =============================================================================

namespace
{
    constexpr double sampleRate = 48000.0;

    struct LoggedMidi
    {
        bool isOn = false;
        int note = 0;
        long long sample = 0;
    };

    struct Harness
    {
        SequencePlayer player;
        ProgressionModel model;
        midiengine::NoteLedger ledger;
        std::vector<LoggedMidi> log;
        long long samplesRendered = 0;

        explicit Harness (double tempoBpm = 120.0)
        {
            player.prepare (sampleRate);
            player.setTempoBpm (tempoBpm);
            player.setLooping (false);
            player.setLoopRange (0.0, 16.0);
            player.setPlaying (true);
        }

        void run (long long totalSamples, int blockSize)
        {
            long long done = 0;

            while (done < totalSamples)
            {
                const int n = static_cast<int> (std::min<long long> (blockSize, totalSamples - done));

                juce::MidiBuffer midi;
                player.advanceAndRender (model, midi, ledger, 1, n, 0, nullptr);

                for (const auto metadata : midi)
                {
                    const auto message = metadata.getMessage();

                    if (message.isNoteOn() || message.isNoteOff())
                        log.push_back ({ message.isNoteOn(), message.getNoteNumber(), samplesRendered + metadata.samplePosition });
                }

                samplesRendered += n;
                done += n;
            }
        }

        std::vector<LoggedMidi> filtered (bool on, int note) const
        {
            std::vector<LoggedMidi> result;
            for (const auto& entry : log)
                if (entry.isOn == on && entry.note == note)
                    result.push_back (entry);
            return result;
        }

        int count (bool on) const
        {
            return static_cast<int> (std::count_if (log.begin(), log.end(), [on] (const LoggedMidi& e) { return e.isOn == on; }));
        }
    };

    ChordEvent makeEvent (double start, double length, int rootPitchClass = 0)
    {
        ChordEvent event;
        event.rootPitchClass = rootPitchClass;
        event.quality = musictheory::ChordQuality::Major;
        event.startBeats = start;
        event.lengthBeats = length;
        return event;
    }

    // 120 BPM at 48 kHz -> 1 beat = 24000 samples.
    constexpr long long samplesPerBeat = 24000;
}

TEST_CASE ("A sustained chord starts and ends at its exact sample", "[sequencer]")
{
    Harness h;
    h.model.addEvent (makeEvent (1.0, 2.0));   // beats 1..3

    h.run (samplesPerBeat * 4, 512);

    REQUIRE (h.count (true) == 3);    // C major triad
    REQUIRE (h.count (false) == 3);

    for (const auto& entry : h.log)
    {
        const long long expected = entry.isOn ? samplesPerBeat * 1 : samplesPerBeat * 3;
        CHECK (std::llabs (entry.sample - expected) <= 1);
    }

    CHECK_FALSE (h.ledger.isAnythingSounding());
}

TEST_CASE ("Sample-accurate timing does not depend on block size", "[sequencer]")
{
    for (const int blockSize : { 32, 64, 441, 512, 1000, 2048 })
    {
        Harness h;
        h.model.addEvent (makeEvent (0.5, 1.0));

        h.run (samplesPerBeat * 3, blockSize);

        REQUIRE (h.count (true) == 3);
        for (const auto& entry : h.log)
        {
            const long long expected = entry.isOn ? samplesPerBeat / 2 : samplesPerBeat * 3 / 2;
            INFO ("block size " << blockSize);
            CHECK (std::llabs (entry.sample - expected) <= 1);
        }
    }
}

TEST_CASE ("Looping repeats with exact period regardless of block size", "[sequencer][loop]")
{
    // Loop is 0..4 beats = 96000 samples. Every restart of the event at
    // beat 0 must land exactly one loop-length after the previous one --
    // the remainder of the block that crosses the loop end has to carry
    // over into the next pass rather than being thrown away.
    constexpr long long loopSamples = samplesPerBeat * 4;

    for (const int blockSize : { 64, 441, 512, 1000, 2048 })
    {
        Harness h;
        h.player.setLooping (true);
        h.player.setLoopRange (0.0, 4.0);
        h.model.addEvent (makeEvent (0.0, 2.0));

        h.run (loopSamples * 4 + 100, blockSize);

        const auto onsets = h.filtered (true, 60);   // C4 is the root of a C major triad in root position
        REQUIRE (onsets.size() >= 4);

        INFO ("block size " << blockSize);
        for (size_t i = 0; i < onsets.size(); ++i)
            CHECK (std::llabs (onsets[i].sample - static_cast<long long> (i) * loopSamples) <= 1);
    }
}

TEST_CASE ("An event running past the loop end is cut at the wrap without stuck notes", "[sequencer][loop]")
{
    Harness h;
    h.player.setLooping (true);
    h.player.setLoopRange (0.0, 4.0);
    h.model.addEvent (makeEvent (2.0, 6.0));   // 2..8, longer than the loop

    h.run (samplesPerBeat * 4 * 3, 512);

    CHECK (h.count (true) == h.count (false) + (h.ledger.isAnythingSounding() ? 3 : 0));

    // Ends of passes must be released at the loop boundary, not carried over.
    const auto offs = h.filtered (false, 60);
    REQUIRE (offs.size() >= 2);
    CHECK (std::llabs (offs[0].sample - samplesPerBeat * 4) <= 1);
}

TEST_CASE ("Deleting a sounding event releases its notes", "[sequencer]")
{
    Harness h;
    const auto id = h.model.addEvent (makeEvent (0.0, 8.0));

    h.run (samplesPerBeat, 512);
    REQUIRE (h.ledger.isAnythingSounding());

    h.model.removeEvent (id);
    h.run (1024, 512);

    CHECK_FALSE (h.ledger.isAnythingSounding());
    CHECK (h.count (true) == h.count (false));
}

TEST_CASE ("releaseAllNotes silences everything the player started and nothing else", "[sequencer]")
{
    Harness h;
    h.model.addEvent (makeEvent (0.0, 8.0));
    h.run (2048, 512);
    REQUIRE (h.ledger.isAnythingSounding());

    // A note that belongs to someone else (e.g. live keyboard play).
    juce::MidiBuffer midi;
    h.ledger.noteOn (midi, 1, 100, 90, 0);

    h.player.releaseAllNotes (midi, h.ledger, 1);

    CHECK (h.ledger.getCount (100) == 1);
    CHECK (h.ledger.getCount (60) == 0);
    CHECK (h.ledger.getCount (64) == 0);
    CHECK (h.ledger.getCount (67) == 0);
}

TEST_CASE ("Disabled events never sound", "[sequencer]")
{
    Harness h;
    auto event = makeEvent (0.0, 2.0);
    event.enabled = false;
    h.model.addEvent (event);

    h.run (samplesPerBeat * 3, 512);

    CHECK (h.log.empty());
}

TEST_CASE ("Shrinking the loop below the playhead wraps instead of running away", "[sequencer][loop]")
{
    Harness h;
    h.player.setLooping (true);
    h.player.setLoopRange (0.0, 16.0);
    h.model.addEvent (makeEvent (0.0, 1.0));

    h.run (samplesPerBeat * 6, 512);
    h.player.setLoopRange (0.0, 2.0);   // playhead is at ~6 beats, now past the end
    h.run (samplesPerBeat * 2, 512);

    CHECK (h.player.getPositionBeats() < 2.0);
    CHECK (h.count (true) == h.count (false) + (h.ledger.isAnythingSounding() ? 3 : 0));
}

TEST_CASE ("Arpeggio plays one note per step, ascending for Up", "[sequencer][arp]")
{
    Harness h;
    auto event = makeEvent (0.0, 4.0);
    event.rhythm = RhythmPattern::Arpeggio;
    event.arpMode = ArpMode::Up;
    event.arpRateBeats = 0.5;   // 12000 samples
    h.model.addEvent (event);

    h.run (samplesPerBeat * 4 + 1024, 512);   // a little past the event's end so the final block releases it

    std::vector<int> notesInOrder;
    for (const auto& entry : h.log)
        if (entry.isOn)
            notesInOrder.push_back (entry.note);

    REQUIRE (notesInOrder.size() >= 6);
    CHECK (notesInOrder[0] == 60);
    CHECK (notesInOrder[1] == 64);
    CHECK (notesInOrder[2] == 67);
    CHECK (notesInOrder[3] == 60);   // wraps around

    CHECK_FALSE (h.ledger.isAnythingSounding());
}
