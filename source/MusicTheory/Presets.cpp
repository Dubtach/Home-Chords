#include "Presets.h"

namespace musictheory
{
    const std::vector<PresetDefinition>& getAllPresets()
    {
        static const std::vector<PresetDefinition> presets = {
            { "I - V - vi - IV",   { 0, 4, 5, 3 } },
            { "vi - IV - I - V",   { 5, 3, 0, 4 } },
            { "I - IV - V - I",    { 0, 3, 4, 0 } },
            { "I - iii - vi - IV", { 0, 2, 5, 3 } },
            { "vi - V - IV - V",   { 5, 4, 3, 4 } },
        };

        return presets;
    }

    progression::ProgressionModel buildProgressionFromPreset (const PresetDefinition& preset, int tonicPitchClass,
                                                               ScaleType scale, double beatsPerChord)
    {
        progression::ProgressionModel model;
        const auto chords = buildDiatonicChords (tonicPitchClass, scale);

        double position = 0.0;

        for (const int degree : preset.scaleDegrees)
        {
            if (degree < 0 || degree >= static_cast<int> (chords.size()))
                continue;   // e.g. a preset degree beyond a short scale's range

            const auto& chord = chords[static_cast<size_t> (degree)];

            progression::ChordEvent event;
            event.scaleDegree = chord.scaleDegree;
            event.rootPitchClass = chord.rootPitchClass;
            event.quality = chord.quality;
            event.startBeats = position;
            event.lengthBeats = beatsPerChord;

            model.addEvent (event);
            position += beatsPerChord;
        }

        return model;
    }
}
