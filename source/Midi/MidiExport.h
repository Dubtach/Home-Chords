#pragma once

#include "../Progression/ProgressionModel.h"
#include "../MusicTheory/MusicTheoryTypes.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

// =============================================================================
// Standard MIDI file export of a progression.
//
// Rather than re-implementing rhythm patterns, strums, arpeggios, bass
// handling, voicing and humanization a second time, the progression is
// rendered offline through the real SequencePlayer -- so the file contains
// exactly the notes the plugin plays, to the sample, then converted to ticks.
//
// Message-thread only (it allocates). Format 1: track 0 = tempo/time
// signature, track 1 = the notes.
// =============================================================================

namespace midiexport
{
    struct ExportSettings
    {
        double bpm = 120.0;
        int beatsPerBar = 4;                                   // time signature numerator (quarter-note beats)
        int rootOctaveOffset = 0;                              // the plugin's OCTAVE setting
        musictheory::VoicingStyle voicing = musictheory::VoicingStyle::Close;
        bool autoInversion = false;                            // voice-lead between consecutive chords, as the plugin does
        float humanizeTimingMs = 0.0f;
        float humanizeVelocity = 0.0f;
        int midiChannel = 1;                                   // 1-16
    };

    inline constexpr int ticksPerQuarterNote = 960;

    // An empty progression yields a valid file with only the tempo track.
    juce::MidiFile buildMidiFile (const progression::ProgressionModel& model, const ExportSettings& settings);

    // Writes `midiFile` to `destination`, replacing any existing file.
    bool writeMidiFile (const juce::MidiFile& midiFile, const juce::File& destination);
}
