#include "MidiExport.h"
#include "NoteLedger.h"
#include "../Timeline/SequencePlayer.h"
#include <cmath>

namespace midiexport
{
    juce::MidiFile buildMidiFile (const progression::ProgressionModel& model, const ExportSettings& settings)
    {
        const double bpm = juce::jlimit (20.0, 300.0, settings.bpm);
        const int channel = juce::jlimit (1, 16, settings.midiChannel);
        const int beatsPerBar = juce::jlimit (1, 16, settings.beatsPerBar);

        juce::MidiMessageSequence tempoTrack;
        tempoTrack.addEvent (juce::MidiMessage::textMetaEvent (3, "Home-Chords tempo"), 0.0);
        tempoTrack.addEvent (juce::MidiMessage::tempoMetaEvent (static_cast<int> (std::lround (60'000'000.0 / bpm))), 0.0);
        tempoTrack.addEvent (juce::MidiMessage::timeSignatureMetaEvent (beatsPerBar, 4), 0.0);

        juce::MidiMessageSequence noteTrack;
        noteTrack.addEvent (juce::MidiMessage::textMetaEvent (3, "Home-Chords"), 0.0);

        if (! model.isEmpty())
        {
            constexpr double sampleRate = 48000.0;
            constexpr int blockSize = 512;

            // Local, throw-away engine state: nothing here touches the live
            // plugin's player, ledger or model lock (the model is only read).
            timeline::SequencePlayer player;
            midiengine::NoteLedger ledger;

            player.prepare (sampleRate);
            player.setTempoBpm (bpm);
            player.setLooping (false);
            player.setHumanize (settings.humanizeTimingMs, settings.humanizeVelocity);
            player.setVoicingStyle (settings.voicing);
            player.setAutoInversion (settings.autoInversion);
            player.setPlaying (true);

            const double ticksPerSample = (bpm / 60.0) / sampleRate * static_cast<double> (ticksPerQuarterNote);

            // Long enough for the last event to end and for humanize/strum
            // delays to flush, plus a safety second.
            const double totalSeconds = (model.getTotalLengthBeats() + 1.0) * 60.0 / bpm + 1.0;
            const long long totalSamples = static_cast<long long> (std::ceil (totalSeconds * sampleRate));

            long long rendered = 0;

            while (rendered < totalSamples)
            {
                juce::MidiBuffer block;
                player.advanceAndRender (model, block, ledger, channel, blockSize, settings.rootOctaveOffset, nullptr);

                for (const auto metadata : block)
                {
                    auto message = metadata.getMessage();

                    if (! (message.isNoteOn() || message.isNoteOff()))
                        continue;

                    message.setTimeStamp (std::round (static_cast<double> (rendered + metadata.samplePosition) * ticksPerSample));
                    noteTrack.addEvent (message);
                }

                rendered += blockSize;
            }

            // Belt and braces: the last event has ended by now, but never
            // write a file with a note left open.
            juce::MidiBuffer tail;
            player.releaseAllNotes (tail, ledger, channel);

            for (const auto metadata : tail)
            {
                auto message = metadata.getMessage();
                message.setTimeStamp (std::round (static_cast<double> (rendered) * ticksPerSample));
                noteTrack.addEvent (message);
            }
        }

        noteTrack.sort();
        noteTrack.updateMatchedPairs();

        juce::MidiFile file;
        file.setTicksPerQuarterNote (ticksPerQuarterNote);
        file.addTrack (tempoTrack);
        file.addTrack (noteTrack);
        return file;
    }

    bool writeMidiFile (const juce::MidiFile& midiFile, const juce::File& destination)
    {
        if (destination.existsAsFile() && ! destination.deleteFile())
            return false;

        if (const auto parent = destination.getParentDirectory(); ! parent.exists())
            parent.createDirectory();

        juce::FileOutputStream stream (destination);

        if (! stream.openedOk())
            return false;

        return midiFile.writeTo (stream, 1);   // format 1: tempo track + note track
    }
}
