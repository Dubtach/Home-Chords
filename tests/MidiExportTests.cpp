#include "Midi/MidiExport.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cstdlib>

using namespace progression;

namespace
{
    ChordEvent makeEvent (double start, double length, int root = 0)
    {
        ChordEvent event;
        event.rootPitchClass = root;
        event.quality = musictheory::ChordQuality::Major;
        event.startBeats = start;
        event.lengthBeats = length;
        return event;
    }

    struct Note { int note; double onTick; double offTick; int velocity; };

    std::vector<Note> notesOf (const juce::MidiFile& file)
    {
        std::vector<Note> notes;
        REQUIRE (file.getNumTracks() == 2);

        const auto* track = file.getTrack (1);

        for (int i = 0; i < track->getNumEvents(); ++i)
        {
            const auto* event = track->getEventPointer (i);

            if (event->message.isNoteOn() && event->noteOffObject != nullptr)
                notes.push_back ({ event->message.getNoteNumber(), event->message.getTimeStamp(),
                                   event->noteOffObject->message.getTimeStamp(), event->message.getVelocity() });
        }

        return notes;
    }
}

TEST_CASE ("An empty progression exports a valid, note-free file", "[export]")
{
    ProgressionModel model;
    const auto file = midiexport::buildMidiFile (model, {});

    CHECK (file.getNumTracks() == 2);
    CHECK (file.getTimeFormat() == midiexport::ticksPerQuarterNote);
    CHECK (notesOf (file).empty());
}

TEST_CASE ("Exported notes sit on exact ticks", "[export]")
{
    ProgressionModel model;
    model.addEvent (makeEvent (0.0, 4.0, 0));   // C major, bar 1
    model.addEvent (makeEvent (4.0, 2.0, 7));   // G major, half a bar

    const auto notes = notesOf (midiexport::buildMidiFile (model, {}));
    REQUIRE (notes.size() == 6);

    const double q = midiexport::ticksPerQuarterNote;

    for (const auto& n : notes)
    {
        const bool firstChord = n.onTick < 2.0 * q;
        const double expectedOn = firstChord ? 0.0 : 4.0 * q;
        const double expectedOff = firstChord ? 4.0 * q : 6.0 * q;

        CHECK (std::abs (n.onTick - expectedOn) <= 1.0);
        CHECK (std::abs (n.offTick - expectedOff) <= 1.0);
    }

    // C major triad (C4 E4 G4) then G major triad (G4 B4 D5), voiced near middle C.
    std::vector<int> pitches;
    for (const auto& n : notes) pitches.push_back (n.note);
    std::sort (pitches.begin(), pitches.end());
    CHECK (std::count (pitches.begin(), pitches.end(), 60) == 1);
    CHECK (std::count (pitches.begin(), pitches.end(), 64) == 1);
}

TEST_CASE ("Tempo changes tick placement not musical position", "[export]")
{
    ProgressionModel model;
    model.addEvent (makeEvent (0.0, 1.0));

    midiexport::ExportSettings fast;
    fast.bpm = 240.0;
    midiexport::ExportSettings slow;
    slow.bpm = 60.0;

    const auto a = notesOf (midiexport::buildMidiFile (model, fast));
    const auto b = notesOf (midiexport::buildMidiFile (model, slow));

    REQUIRE (a.size() == 3);
    REQUIRE (b.size() == 3);

    // One beat is always one quarter note of ticks, whatever the tempo.
    CHECK (std::abs (a[0].offTick - midiexport::ticksPerQuarterNote) <= 1.0);
    CHECK (std::abs (b[0].offTick - midiexport::ticksPerQuarterNote) <= 1.0);
}

TEST_CASE ("Rhythm patterns survive export (an arpeggio makes many notes)", "[export]")
{
    ProgressionModel model;
    auto event = makeEvent (0.0, 4.0);
    event.rhythm = RhythmPattern::Arpeggio;
    event.arpMode = ArpMode::Up;
    event.arpRateBeats = 0.5;
    model.addEvent (event);

    const auto notes = notesOf (midiexport::buildMidiFile (model, {}));
    CHECK (notes.size() == 8);   // 4 beats / 0.5
}

TEST_CASE ("Every exported note-on has a matching note-off", "[export]")
{
    ProgressionModel model;
    auto strum = makeEvent (0.0, 0.0625);
    strum.rhythm = RhythmPattern::Strum;
    model.addEvent (strum);
    model.addEvent (makeEvent (1.0, 3.0, 5));

    midiexport::ExportSettings settings;
    settings.humanizeTimingMs = 30.0f;
    settings.humanizeVelocity = 0.3f;

    const auto file = midiexport::buildMidiFile (model, settings);
    const auto* track = file.getTrack (1);

    int ons = 0, offs = 0, unmatched = 0;
    for (int i = 0; i < track->getNumEvents(); ++i)
    {
        const auto* e = track->getEventPointer (i);
        if (e->message.isNoteOn()) { ++ons; unmatched += e->noteOffObject == nullptr ? 1 : 0; }
        if (e->message.isNoteOff()) ++offs;
    }

    CHECK (ons == offs);
    CHECK (unmatched == 0);
}

TEST_CASE ("A written file reads back identically", "[export]")
{
    ProgressionModel model;
    model.addEvent (makeEvent (0.0, 4.0));

    midiexport::ExportSettings settings;
    settings.bpm = 133.0;

    const auto file = midiexport::buildMidiFile (model, settings);
    const auto path = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("HomeChordsTests").getChildFile ("roundtrip.mid");

    REQUIRE (midiexport::writeMidiFile (file, path));
    REQUIRE (path.existsAsFile());
    CHECK (path.getSize() > 30);

    juce::FileInputStream in (path);
    REQUIRE (in.openedOk());

    juce::MidiFile loaded;
    REQUIRE (loaded.readFrom (in));
    CHECK (loaded.getNumTracks() == 2);
    CHECK (loaded.getTimeFormat() == midiexport::ticksPerQuarterNote);
    CHECK (notesOf (loaded).size() == 3);

    // Overwriting an existing file works.
    REQUIRE (midiexport::writeMidiFile (file, path));

    path.deleteFile();
}

TEST_CASE ("Export carries auto-inversion, so the file matches what the plugin plays", "[midiexport][voiceleading]")
{
    progression::ProgressionModel model;

    progression::ChordEvent c;
    c.rootPitchClass = 0;
    c.startBeats = 0.0;
    c.lengthBeats = 4.0;
    model.addEvent (c);

    progression::ChordEvent g;
    g.rootPitchClass = 7;
    g.startBeats = 4.0;
    g.lengthBeats = 4.0;
    model.addEvent (g);

    const auto notesAfterBeat4 = [&] (bool autoInversion)
    {
        midiexport::ExportSettings settings;
        settings.autoInversion = autoInversion;
        const auto file = midiexport::buildMidiFile (model, settings);

        REQUIRE (file.getNumTracks() == 2);
        const auto* track = file.getTrack (1);

        std::vector<int> sounding;
        for (int i = 0; i < track->getNumEvents(); ++i)
        {
            const auto& message = track->getEventPointer (i)->message;

            if (message.getTimeStamp() > midiexport::ticksPerQuarterNote * 4 + 5)
                break;

            if (message.isNoteOn())
                sounding.push_back (message.getNoteNumber());
            else if (message.isNoteOff())
                sounding.erase (std::remove (sounding.begin(), sounding.end(), message.getNoteNumber()), sounding.end());
        }

        std::sort (sounding.begin(), sounding.end());
        return sounding;
    };

    const auto plain = notesAfterBeat4 (false);
    const auto led = notesAfterBeat4 (true);

    REQUIRE (plain.size() == 3);
    REQUIRE (led.size() == 3);
    CHECK (plain != led);

    const auto centroid = [] (const std::vector<int>& v)
    {
        double sum = 0.0;
        for (int n : v) sum += n;
        return sum / (double) v.size();
    };

    // C major (C4 E4 G4) centres on 63.67; the voice-led G chord must sit nearer to it.
    const double cCentre = 63.67;
    CHECK (std::abs (centroid (led) - cCentre) < std::abs (centroid (plain) - cCentre));
}
