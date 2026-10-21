#include "ColorChords.h"

namespace musictheory
{
    namespace
    {
        struct ColorTemplate
        {
            const char* label;
            int semitones;                 // root, as semitones above the tonic
            ChordQuality quality;
            Extension extension;
            const char* nameSuffixOverride;   // nullptr: derive from quality + extension
            const char* description;
        };

        constexpr ColorTemplate majorSide[] = {
            { "bIII",  3, ChordQuality::Major, Extension::None,         nullptr, "Borrowed from the parallel minor: bright but with a shadow" },
            { "iv",    5, ChordQuality::Minor, Extension::None,         nullptr, "Minor subdominant -- the classic bittersweet lift" },
            { "bVI",   8, ChordQuality::Major, Extension::None,         nullptr, "Borrowed bVI: wide, cinematic, resolves up to bVII or down to V" },
            { "bVII", 10, ChordQuality::Major, Extension::None,         nullptr, "Mixolydian lift; the rock 'back door' to the tonic" },
            { "bII",   1, ChordQuality::Major, Extension::None,         nullptr, "Neapolitan: a dramatic half-step-above approach to V or I" },
            { "V/V",   2, ChordQuality::Major, Extension::MinorSeventh, nullptr, "Secondary dominant -- pulls strongly to V" },
            { "V/vi",  4, ChordQuality::Major, Extension::MinorSeventh, nullptr, "Secondary dominant -- pulls strongly to vi" },
            { "V/ii",  9, ChordQuality::Major, Extension::MinorSeventh, nullptr, "Secondary dominant -- pulls strongly to ii" },
            { "I7",    0, ChordQuality::Major, Extension::MinorSeventh, nullptr, "Dominant-flavoured tonic -- pulls toward IV (blues)" },
        };

        constexpr ColorTemplate minorSide[] = {
            { "I",      0, ChordQuality::Major, Extension::None,         nullptr, "Parallel-major tonic (Picardy ending): sudden light" },
            { "IV",     5, ChordQuality::Major, Extension::None,         nullptr, "Major IV (Dorian colour) -- lifts a minor key" },
            { "V",      7, ChordQuality::Major, Extension::None,         nullptr, "Major dominant -- the strongest pull back to i" },
            { "V7",     7, ChordQuality::Major, Extension::MinorSeventh, nullptr, "Dominant 7th -- the full cadence in a minor key" },
            { "bII",    1, ChordQuality::Major, Extension::None,         nullptr, "Neapolitan: dark, dramatic approach to V or i" },
            { "bVII",  10, ChordQuality::Major, Extension::None,         nullptr, "Subtonic -- pairs with bVI and i in the 'Andalusian' feel" },
            { "viio7", 11, ChordQuality::Diminished, Extension::Sixth,   "dim7", "Leading-tone diminished 7th -- tense, resolves up to i" },
        };

        template <size_t N>
        void append (std::vector<ColorChord>& out, const ColorTemplate (&templates)[N], int tonic, bool keyUsesFlats, bool sharpsForced)
        {
            for (const auto& t : templates)
            {
                ColorChord chord;
                chord.label = t.label;
                chord.description = t.description;
                chord.rootPitchClass = ((tonic + t.semitones) % 12 + 12) % 12;
                chord.quality = t.quality;
                chord.extension = t.extension;
                chord.shape = buildOverriddenShape (chord.rootPitchClass, t.quality, t.extension);

                const juce::String suffix = t.nameSuffixOverride != nullptr
                                                ? juce::String (t.nameSuffixOverride)
                                                : chordQualitySuffix (t.quality) + extensionSuffix (t.extension);

                // A flat-labelled chord (bVII, bVI, ...) is named for the flat
                // degree it is, so C major's bVII is Bb, never A#, whatever
                // the key's own spelling -- unless the user forced sharps.
                const bool flatLabelled = chord.label.startsWithChar ('b');
                const bool useFlats = ! sharpsForced && (keyUsesFlats || flatLabelled);

                chord.chordName = noteName (chord.rootPitchClass, useFlats) + suffix;
                out.push_back (std::move (chord));
            }
        }
    }

    bool isMinorSounding (ScaleType scale) noexcept
    {
        std::array<int, 8> semitones {};
        const int count = getScaleIntervals (scale, semitones);

        // Third scale degree (index 2): 3 semitones = minor, 4 = major.
        // Scales too short to have one (never the case today) count as major.
        return count > 2 && semitones[2] == 3;
    }

    std::vector<ColorChord> buildColorChords (int tonicPitchClass, ScaleType scale, NoteSpelling spelling)
    {
        bool useFlats = false;
        switch (spelling)
        {
            case NoteSpelling::Sharps: useFlats = false; break;
            case NoteSpelling::Flats:  useFlats = true;  break;
            default:                   useFlats = prefersFlats (tonicPitchClass, scale); break;
        }

        std::vector<ColorChord> result;
        result.reserve (10);

        const bool sharpsForced = (spelling == NoteSpelling::Sharps);

        if (isMinorSounding (scale))
            append (result, minorSide, tonicPitchClass, useFlats, sharpsForced);
        else
            append (result, majorSide, tonicPitchClass, useFlats, sharpsForced);

        return result;
    }
}
