#include "Scale.h"

namespace musictheory
{
    namespace
    {
        // Semitone offsets from the tonic, ascending. All heptatonic scales
        // share one table shape; short (5/6-note) scales get their own.
        constexpr std::array<int, 7> majorIntervals          { 0, 2, 4, 5, 7, 9, 11 };
        constexpr std::array<int, 7> naturalMinorIntervals   { 0, 2, 3, 5, 7, 8, 10 };
        constexpr std::array<int, 7> harmonicMinorIntervals  { 0, 2, 3, 5, 7, 8, 11 };
        constexpr std::array<int, 7> melodicMinorIntervals   { 0, 2, 3, 5, 7, 9, 11 };   // ascending form
        constexpr std::array<int, 7> dorianIntervals         { 0, 2, 3, 5, 7, 9, 10 };
        constexpr std::array<int, 7> phrygianIntervals       { 0, 1, 3, 5, 7, 8, 10 };
        constexpr std::array<int, 7> lydianIntervals         { 0, 2, 4, 6, 7, 9, 11 };
        constexpr std::array<int, 7> mixolydianIntervals     { 0, 2, 4, 5, 7, 9, 10 };
        constexpr std::array<int, 7> locrianIntervals        { 0, 1, 3, 5, 6, 8, 10 };

        constexpr std::array<int, 6> majorPentatonicIntervals { 0, 2, 4, 7, 9, -1 };   // last slot unused
        constexpr std::array<int, 6> minorPentatonicIntervals { 0, 3, 5, 7, 10, -1 };  // last slot unused
        constexpr std::array<int, 6> bluesIntervals            { 0, 3, 5, 6, 7, 10 };

        bool isShortScale (ScaleType scale) noexcept
        {
            return scale == ScaleType::MajorPentatonic
                || scale == ScaleType::MinorPentatonic
                || scale == ScaleType::Blues;
        }

        // Semitones from this scale's own "parent major" tonic up to this
        // scale's tonic -- i.e. the modal relationship. Used only to decide
        // sharp-vs-flat spelling, so harmonic/melodic minor and the two
        // pentatonics are mapped onto whichever heptatonic parent gives the
        // conventional key-signature bias (same as natural minor, in both
        // minor-pentatonic's and harmonic/melodic minor's case).
        int parentMajorOffset (ScaleType scale) noexcept
        {
            switch (scale)
            {
                case ScaleType::Major:            return 0;
                case ScaleType::Dorian:           return 2;
                case ScaleType::Phrygian:         return 4;
                case ScaleType::Lydian:           return 5;
                case ScaleType::Mixolydian:       return 7;
                case ScaleType::NaturalMinor:     return 9;
                case ScaleType::HarmonicMinor:    return 9;
                case ScaleType::MelodicMinor:     return 9;
                case ScaleType::Locrian:          return 11;
                case ScaleType::MajorPentatonic:  return 0;
                case ScaleType::MinorPentatonic:  return 9;
                case ScaleType::Blues:            return 9;
                default:                          return 0;
            }
        }
    }

    const std::array<int, 7>& getHeptatonicIntervals (ScaleType scale) noexcept
    {
        switch (scale)
        {
            case ScaleType::Major:         return majorIntervals;
            case ScaleType::NaturalMinor:  return naturalMinorIntervals;
            case ScaleType::HarmonicMinor: return harmonicMinorIntervals;
            case ScaleType::MelodicMinor:  return melodicMinorIntervals;
            case ScaleType::Dorian:        return dorianIntervals;
            case ScaleType::Phrygian:      return phrygianIntervals;
            case ScaleType::Lydian:        return lydianIntervals;
            case ScaleType::Mixolydian:    return mixolydianIntervals;
            case ScaleType::Locrian:       return locrianIntervals;
            default:                       return majorIntervals;   // short scales don't use this table
        }
    }

    const std::array<int, 6>& getShortScaleIntervals (ScaleType scale) noexcept
    {
        switch (scale)
        {
            case ScaleType::MajorPentatonic: return majorPentatonicIntervals;
            case ScaleType::MinorPentatonic: return minorPentatonicIntervals;
            case ScaleType::Blues:           return bluesIntervals;
            default:                         return bluesIntervals;   // never hit for heptatonic scales
        }
    }

    int getDegreeCount (ScaleType scale) noexcept
    {
        if (scale == ScaleType::MajorPentatonic || scale == ScaleType::MinorPentatonic)
            return 5;
        if (scale == ScaleType::Blues)
            return 6;
        return 7;
    }

    int getScaleIntervals (ScaleType scale, std::array<int, 8>& outSemitones) noexcept
    {
        const int degreeCount = getDegreeCount (scale);

        if (isShortScale (scale))
        {
            const auto& table = getShortScaleIntervals (scale);
            for (int i = 0; i < degreeCount; ++i)
                outSemitones[static_cast<size_t> (i)] = table[static_cast<size_t> (i)];
        }
        else
        {
            const auto& table = getHeptatonicIntervals (scale);
            for (int i = 0; i < degreeCount; ++i)
                outSemitones[static_cast<size_t> (i)] = table[static_cast<size_t> (i)];
        }

        // The octave repeat of the tonic, one past the last real degree --
        // chord-stacking wraps into this when it needs to reach "up an
        // octave from degree 0", so every table gets it for free here.
        outSemitones[static_cast<size_t> (degreeCount)] = 12;

        return degreeCount;
    }

    bool prefersFlats (int tonicPitchClass, ScaleType scale) noexcept
    {
        const int offset = parentMajorOffset (scale);
        const int parentMajorTonic = ((tonicPitchClass - offset) % 12 + 12) % 12;

        switch (parentMajorTonic)
        {
            case 1: case 3: case 5: case 8: case 10:   // Db, Eb, F, Ab, Bb
                return true;
            default:
                return false;
        }
    }

    juce::String noteName (int pitchClass, bool useFlats)
    {
        static const juce::StringArray sharpNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        static const juce::StringArray flatNames  { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

        const int pc = ((pitchClass % 12) + 12) % 12;
        return useFlats ? flatNames[pc] : sharpNames[pc];
    }

    juce::String getScaleName (ScaleType scale)
    {
        switch (scale)
        {
            case ScaleType::Major:           return "Major";
            case ScaleType::NaturalMinor:    return "Natural Minor";
            case ScaleType::HarmonicMinor:   return "Harmonic Minor";
            case ScaleType::MelodicMinor:    return "Melodic Minor";
            case ScaleType::Dorian:          return "Dorian";
            case ScaleType::Phrygian:        return "Phrygian";
            case ScaleType::Lydian:          return "Lydian";
            case ScaleType::Mixolydian:      return "Mixolydian";
            case ScaleType::Locrian:         return "Locrian";
            case ScaleType::MajorPentatonic: return "Major Pentatonic";
            case ScaleType::MinorPentatonic: return "Minor Pentatonic";
            case ScaleType::Blues:           return "Blues";
            default:                         return "Major";
        }
    }

    juce::StringArray getAllScaleNames()
    {
        juce::StringArray names;
        for (int i = 0; i < numScaleTypes; ++i)
            names.add (getScaleName (static_cast<ScaleType> (i)));
        return names;
    }

    juce::StringArray getAllKeyNames()
    {
        return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    }

    juce::String chordQualitySuffix (ChordQuality quality)
    {
        switch (quality)
        {
            case ChordQuality::Major:      return "";
            case ChordQuality::Minor:      return "m";
            case ChordQuality::Diminished: return "dim";
            case ChordQuality::Augmented:  return "aug";
            case ChordQuality::Sus2:       return "sus2";
            case ChordQuality::Sus4:       return "sus4";
            default:                       return "";
        }
    }

    juce::String chordQualityLabel (ChordQuality quality)
    {
        switch (quality)
        {
            case ChordQuality::Major:      return "Major";
            case ChordQuality::Minor:      return "Minor";
            case ChordQuality::Diminished: return "Diminished";
            case ChordQuality::Augmented:  return "Augmented";
            case ChordQuality::Sus2:       return "Sus2";
            case ChordQuality::Sus4:       return "Sus4";
            default:                       return "Chord";
        }
    }
}
