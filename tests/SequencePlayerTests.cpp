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
        int velocity = 0;
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
                    {
                        // A host may drop or mis-time events positioned
                        // outside the block, so this is a hard requirement.
                        REQUIRE (metadata.samplePosition >= 0);
                        REQUIRE (metadata.samplePosition < n);

                        log.push_back ({ message.isNoteOn(), message.getNoteNumber(), samplesRendered + metadata.samplePosition,
                                         static_cast<int> (message.getVelocity()) });
                    }
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

// ---- scheduler: strum, humanize, gate ---------------------------------------

TEST_CASE ("Strum spreads notes over 18 ms steps even when blocks are far shorter", "[sequencer][strum]")
{
    // 18 ms = 864 samples at 48 kHz; a 64-sample block is 1.3 ms. The later
    // notes are due many blocks after the event starts, so they must wait in
    // the scheduler instead of being written past the end of a block.
    for (const int blockSize : { 32, 64, 441, 2048 })
    {
        Harness h;
        auto event = makeEvent (0.0, 4.0);
        event.rhythm = RhythmPattern::Strum;
        h.model.addEvent (event);

        h.run (samplesPerBeat * 5, blockSize);

        INFO ("block size " << blockSize);
        const auto on60 = h.filtered (true, 60);
        const auto on64 = h.filtered (true, 64);
        const auto on67 = h.filtered (true, 67);
        REQUIRE (on60.size() == 1);
        REQUIRE (on64.size() == 1);
        REQUIRE (on67.size() == 1);

        CHECK (std::llabs (on60[0].sample - 0) <= 1);
        CHECK (std::llabs (on64[0].sample - 864) <= 1);
        CHECK (std::llabs (on67[0].sample - 1728) <= 1);

        CHECK (h.count (true) == h.count (false));
        CHECK_FALSE (h.ledger.isAnythingSounding());
    }
}

TEST_CASE ("A strum notes still waiting when its event ends are dropped, never stuck", "[sequencer][strum]")
{
    Harness h;
    auto event = makeEvent (0.0, 0.0625);   // 1500 samples: ends before the 3rd note (1728)
    event.rhythm = RhythmPattern::Strum;
    h.model.addEvent (event);

    h.run (samplesPerBeat * 2, 64);

    CHECK (h.count (true) == h.count (false));
    CHECK (h.filtered (true, 67).empty());
    CHECK_FALSE (h.ledger.isAnythingSounding());
}

TEST_CASE ("Humanize delays onsets (never early) and varies velocity, deterministically", "[sequencer][humanize]")
{
    const auto runOnce = [] (float timingMs, float velAmount)
    {
        Harness h;
        h.player.setHumanize (timingMs, velAmount);
        h.model.addEvent (makeEvent (0.0, 1.0));
        h.model.addEvent (makeEvent (1.0, 1.0, 5));
        h.model.addEvent (makeEvent (2.0, 1.0, 7));
        h.run (samplesPerBeat * 4, 128);
        return h.log;
    };

    SECTION ("timing: 0..20 ms late, not all identical")
    {
        const auto log = runOnce (20.0f, 0.0f);
        int lateCount = 0;

        for (const auto& entry : log)
        {
            if (! entry.isOn)
                continue;

            const long long nominal = (entry.sample / samplesPerBeat) * samplesPerBeat;
            const long long late = entry.sample - nominal;
            CHECK (late >= 0);
            CHECK (late <= 960 + 1);   // 20 ms at 48 kHz
            lateCount += late > 1 ? 1 : 0;
        }

        CHECK (lateCount > 0);
    }

    SECTION ("velocity: stays in range, actually varies")
    {
        const auto log = runOnce (0.0f, 0.3f);
        int lowest = 127, highest = 0;

        for (const auto& entry : log)
            if (entry.isOn)
            {
                CHECK (entry.velocity >= 1);
                CHECK (entry.velocity <= 127);
                lowest = std::min (lowest, entry.velocity);
                highest = std::max (highest, entry.velocity);
            }

        CHECK (highest > lowest);
    }

    SECTION ("the same progression humanizes identically every time")
    {
        const auto a = runOnce (15.0f, 0.2f);
        const auto b = runOnce (15.0f, 0.2f);
        REQUIRE (a.size() == b.size());

        for (size_t i = 0; i < a.size(); ++i)
        {
            CHECK (a[i].sample == b[i].sample);
            CHECK (a[i].velocity == b[i].velocity);
            CHECK (a[i].note == b[i].note);
        }
    }

    SECTION ("nothing is left sounding")
    {
        Harness h;
        h.player.setHumanize (50.0f, 0.5f);
        auto event = makeEvent (0.0, 0.0625);   // shorter than the maximum delay
        h.model.addEvent (event);
        h.run (samplesPerBeat * 2, 64);
        CHECK (h.count (true) == h.count (false));
        CHECK_FALSE (h.ledger.isAnythingSounding());
    }
}

TEST_CASE ("Arp gate shortens each step's note", "[sequencer][arp]")
{
    Harness h;
    auto event = makeEvent (0.0, 4.0);
    event.rhythm = RhythmPattern::Arpeggio;
    event.arpMode = ArpMode::Up;
    event.arpRateBeats = 0.5;   // 12000 samples per step
    event.arpGate = 0.5;        // held for 6000
    h.model.addEvent (event);

    h.run (samplesPerBeat * 4 + 1024, 512);

    const auto ons = h.filtered (true, 60);
    const auto offs = h.filtered (false, 60);
    REQUIRE (! ons.empty());
    REQUIRE (! offs.empty());

    CHECK (std::llabs (ons[0].sample - 0) <= 1);
    CHECK (std::llabs (offs[0].sample - 6000) <= 1);
    CHECK (h.count (true) == h.count (false));
    CHECK_FALSE (h.ledger.isAnythingSounding());
}

TEST_CASE ("The sequencer honours the voicing style", "[sequencer][voicing]")
{
    Harness h;
    h.player.setVoicingStyle (musictheory::VoicingStyle::Open);
    h.model.addEvent (makeEvent (0.0, 1.0));
    h.run (samplesPerBeat * 2, 512);

    CHECK (h.filtered (true, 52).size() == 1);   // E3 (dropped an octave)
    CHECK (h.filtered (true, 60).size() == 1);
    CHECK (h.filtered (true, 67).size() == 1);
    CHECK (h.filtered (true, 64).empty());
}

TEST_CASE ("A custom slash bass sounds below a seventh chord too", "[sequencer][bass]")
{
    Harness h;
    auto event = makeEvent (0.0, 1.0);
    event.extension = musictheory::Extension::MajorSeventh;   // 4 tones
    event.bassMode = BassMode::Custom;
    event.customBassPitchClass = 7;   // G
    h.model.addEvent (event);
    h.run (samplesPerBeat * 2, 512);

    CHECK (h.count (true) == 5);
    CHECK (h.filtered (true, 55).size() == 1);   // G3, the G just below the chord's C4
}

// ---- host sync --------------------------------------------------------------

TEST_CASE ("Host sync starts, follows, relocates and stops with the host", "[sequencer][host]")
{
    Harness h;
    h.player.setPlaying (false);
    h.model.addEvent (makeEvent (0.0, 4.0));
    h.model.addEvent (makeEvent (8.0, 4.0, 7));

    SequencePlayer::HostTransport host;
    host.valid = true;
    host.playing = true;
    host.ppq = 0.0;
    host.bpm = 90.0;

    juce::MidiBuffer scratch;
    h.player.syncToHost (host, scratch, h.ledger, 1);
    CHECK (h.player.isPlaying());
    CHECK (h.player.getTempoBpm() == 90.0);
    CHECK (h.player.getPositionBeats() == 0.0);

    SECTION ("it plays the progression from the host position")
    {
        h.run (24000, 512);   // 0.5 s at 90 bpm -> 0.75 beats
        CHECK (h.count (true) == 3);
        CHECK (h.player.getPositionBeats() > 0.7);
        CHECK (h.player.getPositionBeats() < 0.8);

        // Host keeps going exactly where we are: no re-seek, no note cut.
        host.ppq = h.player.getPositionBeats();
        juce::MidiBuffer m;
        h.player.syncToHost (host, m, h.ledger, 1);
        CHECK (m.isEmpty());
        CHECK (h.ledger.isAnythingSounding());
    }

    SECTION ("a host locate releases the sounding chord and jumps")
    {
        h.run (4096, 512);
        REQUIRE (h.ledger.isAnythingSounding());

        host.ppq = 8.0;
        juce::MidiBuffer m;
        h.player.syncToHost (host, m, h.ledger, 1);

        CHECK_FALSE (h.ledger.isAnythingSounding());
        CHECK (h.player.getPositionBeats() == 8.0);

        h.run (2048, 512);
        CHECK (h.filtered (true, 67).size() == 1);   // the G chord at beat 8
    }

    SECTION ("host stop silences and stops the player")
    {
        h.run (4096, 512);
        REQUIRE (h.ledger.isAnythingSounding());

        host.playing = false;
        juce::MidiBuffer m;
        h.player.syncToHost (host, m, h.ledger, 1);

        CHECK_FALSE (h.player.isPlaying());
        CHECK_FALSE (h.ledger.isAnythingSounding());
    }

    SECTION ("a host with no transport info counts as stopped")
    {
        host.valid = false;
        juce::MidiBuffer m;
        h.player.syncToHost (host, m, h.ledger, 1);
        CHECK_FALSE (h.player.isPlaying());
    }
}

// =============================================================================
// Voice-leading between sequenced chords
// =============================================================================

namespace
{
    std::vector<int> onsetNotes (const Harness& h, long long sample)
    {
        std::vector<int> notes;
        for (const auto& e : h.log)
            if (e.isOn && std::llabs (e.sample - sample) <= 1)
                notes.push_back (e.note);
        std::sort (notes.begin(), notes.end());
        return notes;
    }
}

TEST_CASE ("Sequenced chords voice-lead when auto-inversion is on", "[sequencer][voiceleading]")
{
    // C major then G major: root position G sits well above C, so the
    // nearest G-major inversion should be lower than root position.
    const auto build = [] (bool autoInvert)
    {
        auto h = std::make_unique<Harness>();
        h->player.setAutoInversion (autoInvert);
        h->model.addEvent (makeEvent (0.0, 4.0, 0));   // C
        h->model.addEvent (makeEvent (4.0, 4.0, 7));   // G
        h->run (samplesPerBeat * 9, 512);
        return h;
    };

    const auto off = build (false);
    const auto on = build (true);

    // The notes actually sounding just after the second chord starts (a
    // common tone with the first chord stays held and is not re-triggered,
    // so counting Note Ons would undercount).
    const auto soundingAfter = [] (const Harness& h, long long sample)
    {
        std::vector<int> notes;
        for (const auto& e : h.log)
        {
            if (e.sample > sample)
                break;

            if (e.isOn)
                notes.push_back (e.note);
            else
                notes.erase (std::remove (notes.begin(), notes.end(), e.note), notes.end());
        }
        return notes;
    };

    const auto mean = [] (const std::vector<int>& v)
    {
        double sum = 0.0;
        for (int n : v) sum += n;
        return v.empty() ? -1.0 : sum / static_cast<double> (v.size());
    };

    const auto offG = soundingAfter (*off, samplesPerBeat * 4 + 10);
    const auto onG = soundingAfter (*on, samplesPerBeat * 4 + 10);

    REQUIRE (offG.size() == 3);
    REQUIRE (onG.size() == 3);

    // Off: plain root position. On: closer to the first chord's centre.
    const double cCentre = mean (soundingAfter (*on, 10));
    CHECK (std::abs (mean (onG) - cCentre) < std::abs (mean (offG) - cCentre));

    // The first chord has nothing to lead from, so it is identical either way.
    CHECK (onsetNotes (*on, 0) == onsetNotes (*off, 0));

    // And nothing hangs.
    CHECK_FALSE (on->ledger.isAnythingSounding());
}

TEST_CASE ("Voice-leading restarts on every loop pass, so passes are identical", "[sequencer][voiceleading][loop]")
{
    Harness h;
    h.player.setAutoInversion (true);
    h.player.setLooping (true);
    h.player.setLoopRange (0.0, 8.0);
    h.model.addEvent (makeEvent (0.0, 4.0, 0));
    h.model.addEvent (makeEvent (4.0, 4.0, 7));

    const long long pass = samplesPerBeat * 8;
    h.run (pass * 3 + 100, 512);

    const auto first = onsetNotes (h, 0);
    CHECK (onsetNotes (h, pass) == first);
    CHECK (onsetNotes (h, pass * 2) == first);
    CHECK (onsetNotes (h, samplesPerBeat * 4 + pass) == onsetNotes (h, samplesPerBeat * 4));
}

TEST_CASE ("Bass = Root forces root position even with auto-inversion on", "[sequencer][voiceleading]")
{
    Harness h;
    h.player.setAutoInversion (true);
    h.model.addEvent (makeEvent (0.0, 4.0, 0));

    auto g = makeEvent (4.0, 4.0, 7);
    g.bassMode = BassMode::Root;
    h.model.addEvent (g);

    h.run (samplesPerBeat * 9, 512);

    const auto notes = onsetNotes (h, samplesPerBeat * 4);
    REQUIRE (notes.size() == 3);
    CHECK (notes[0] % 12 == 7);   // G is the lowest note
}
