#include "Presets.h"

namespace musictheory
{
    namespace
    {
        constexpr auto N  = Extension::None;
        constexpr auto M7 = Extension::MinorSeventh;   // on a minor chord: m7, on a major one: dominant 7
        constexpr auto J7 = Extension::MajorSeventh;
        constexpr auto A9 = Extension::Ninth;

        PresetDefinition preset (const char* name, const char* category, PresetSide side,
                                 std::vector<int> degrees, std::vector<Extension> extensions = {})
        {
            PresetDefinition p;
            p.name = name;
            p.category = category;
            p.side = side;
            p.scaleDegrees = std::move (degrees);
            p.extensions = std::move (extensions);
            return p;
        }
    }

    const std::vector<PresetDefinition>& getAllPresets()
    {
        constexpr auto Maj = PresetSide::Major;
        constexpr auto Min = PresetSide::Minor;
        constexpr auto Any = PresetSide::Any;

        static const std::vector<PresetDefinition> presets = [&]
        {
            std::vector<PresetDefinition> list;

            // ---- Pop (major) ----
            list.push_back (preset ("I - V - vi - IV",             "Pop", Maj, { 0, 4, 5, 3 }));
            list.push_back (preset ("vi - IV - I - V",             "Pop", Maj, { 5, 3, 0, 4 }));
            list.push_back (preset ("I - vi - IV - V  (50s)",      "Pop", Maj, { 0, 5, 3, 4 }));
            list.push_back (preset ("I - IV - vi - V",             "Pop", Maj, { 0, 3, 5, 4 }));
            list.push_back (preset ("IV - I - V - vi",             "Pop", Maj, { 3, 0, 4, 5 }));
            list.push_back (preset ("I - iii - vi - IV",           "Pop", Maj, { 0, 2, 5, 3 }));
            list.push_back (preset ("vi - V - IV - V",             "Pop", Maj, { 5, 4, 3, 4 }));
            list.push_back (preset ("I - IV - V - I",              "Pop", Maj, { 0, 3, 4, 0 }));
            list.push_back (preset ("I - IV - I - V",              "Pop", Maj, { 0, 3, 0, 4 }));
            list.push_back (preset ("Canon  I V vi iii IV I IV V", "Pop", Maj, { 0, 4, 5, 2, 3, 0, 3, 4 }));

            // ---- Jazz & soul (major) ----
            list.push_back (preset ("ii7 - V7 - Imaj7",            "Jazz", Maj, { 1, 4, 0 },          { M7, M7, J7 }));
            list.push_back (preset ("Imaj7 - vi7 - ii7 - V7",      "Jazz", Maj, { 0, 5, 1, 4 },       { J7, M7, M7, M7 }));
            list.push_back (preset ("iii7 - vi7 - ii7 - V7 - Imaj7","Jazz", Maj, { 2, 5, 1, 4, 0 },   { M7, M7, M7, M7, J7 }));
            list.push_back (preset ("Imaj7 - IVmaj7 - iii7 - vi7", "Jazz", Maj, { 0, 3, 2, 5 },       { J7, J7, M7, M7 }));
            list.push_back (preset ("IVmaj7 - V7 - iii7 - vi7  (Royal Road)", "Jazz", Maj, { 3, 4, 2, 5 }, { J7, M7, M7, M7 }));

            // ---- Dreamy / lo-fi (major) ----
            list.push_back (preset ("Imaj7 - vi7 - IVmaj7 - Vadd9", "Chill", Maj, { 0, 5, 3, 4 },     { J7, M7, J7, A9 }));
            list.push_back (preset ("IVmaj7 - iii7 - vi7 - Vadd9", "Chill", Maj, { 3, 2, 5, 4 },      { J7, M7, M7, A9 }));
            list.push_back (preset ("Imaj7 - iii7 - IVmaj7 - V",   "Chill", Maj, { 0, 2, 3, 4 },      { J7, M7, J7, N }));

            // ---- Minor ----
            list.push_back (preset ("i - VII - VI - VII",          "Minor", Min, { 0, 6, 5, 6 }));
            list.push_back (preset ("i - VI - III - VII",          "Minor", Min, { 0, 5, 2, 6 }));
            list.push_back (preset ("i - VI - VII - i",            "Minor", Min, { 0, 5, 6, 0 }));
            list.push_back (preset ("i - iv - VII - III",          "Minor", Min, { 0, 3, 6, 2 }));
            list.push_back (preset ("i - iv - v - i",              "Minor", Min, { 0, 3, 4, 0 }));
            list.push_back (preset ("i - VII - VI - v  (Andalusian)", "Minor", Min, { 0, 6, 5, 4 }));
            list.push_back (preset ("VI - VII - i - i",            "Minor", Min, { 5, 6, 0, 0 }));
            list.push_back (preset ("i7 - iv7 - VII - III",        "Minor", Min, { 0, 3, 6, 2 },      { M7, M7, N, J7 }));

            // ---- Any key ----
            list.push_back (preset ("I - IV  (vamp)",              "Basics", Any, { 0, 3 }));
            list.push_back (preset ("I - V  (vamp)",               "Basics", Any, { 0, 4 }));
            list.push_back (preset ("12-bar blues",                "Basics", Any,
                                    { 0, 0, 0, 0, 3, 3, 0, 0, 4, 3, 0, 4 },
                                    { M7, M7, M7, M7, M7, M7, M7, M7, M7, M7, M7, M7 }));

            return list;
        }();

        return presets;
    }

    PresetSide presetSideForScale (ScaleType scale) noexcept
    {
        switch (scale)
        {
            case ScaleType::NaturalMinor:
            case ScaleType::HarmonicMinor:
            case ScaleType::MelodicMinor:
            case ScaleType::Dorian:
            case ScaleType::Phrygian:
            case ScaleType::Locrian:
            case ScaleType::MinorPentatonic:
            case ScaleType::Blues:
                return PresetSide::Minor;

            default:
                return PresetSide::Major;
        }
    }

    bool presetSuitsScale (const PresetDefinition& preset, ScaleType scale) noexcept
    {
        return preset.side == PresetSide::Any || preset.side == presetSideForScale (scale);
    }

    progression::ProgressionModel buildProgressionFromPreset (const PresetDefinition& preset, int tonicPitchClass,
                                                               ScaleType scale, double beatsPerChord)
    {
        progression::ProgressionModel model;
        const auto chords = buildDiatonicChords (tonicPitchClass, scale);

        // The caller passes one bar; a preset only overrides that if it sets its own length.
        const double length = preset.beatsPerChord > 0.0 ? preset.beatsPerChord : beatsPerChord;

        double position = 0.0;

        for (size_t i = 0; i < preset.scaleDegrees.size(); ++i)
        {
            const int degree = preset.scaleDegrees[i];

            if (degree < 0 || degree >= static_cast<int> (chords.size()))
                continue;   // e.g. a preset degree beyond a short scale's range

            const auto& chord = chords[static_cast<size_t> (degree)];

            progression::ChordEvent event;
            event.scaleDegree = chord.scaleDegree;
            event.rootPitchClass = chord.rootPitchClass;
            event.quality = chord.quality;
            event.extension = i < preset.extensions.size() ? preset.extensions[i] : Extension::None;
            event.startBeats = position;
            event.lengthBeats = length;

            model.addEvent (event);
            position += length;
        }

        return model;
    }
}
