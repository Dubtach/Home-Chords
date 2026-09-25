# Home-Chords

A MIDI chord generator / songwriting plugin — part of the Dubtach "Home"
series alongside Home-Disto and Home-Sidechain.

Pick a Key and Scale, and the 7 diatonic chords are laid out on your
computer keyboard: **A S D F G H J**. Press one to hear it and send real
MIDI to your DAW.

> **This is a Phase 1 build.** It's the live-playable keyboard-to-MIDI
> instrument, not yet the full songwriting tool (progression timeline,
> MIDI export, voicing engine, etc. from the original spec). See
> [`STATUS.md`](STATUS.md) for exactly what's built, what's scaffolded,
> what's untouched, and — importantly — what testing has and hasn't been
> done on this code.

## Quick start

```bash
git submodule add https://github.com/juce-framework/JUCE.git JUCE   # if not already present
git -C JUCE checkout develop

cmake -B Builds -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build Builds
```

Run the Standalone build first — it's the fastest way to hear it without
a DAW. Select **C Major**, click into the plugin window, and press `A`.

## Layout

```
source/
  PluginProcessor.*      Audio processor: owns the parameters, the
                          keyboard engine, and the preview synth.
  PluginEditor.*          The UI.
  MusicTheory/            Key + Scale -> diatonic chords. Zero-JUCE-
                          dependency real-time-safe core (Scale.h,
                          Chord.h), plus the juce::String-bearing
                          display layer (MusicTheoryEngine.h).
  Midi/                   ChordKeyboardEngine -- turns held keyboard
                          slots into real, stuck-note-safe MIDI.
  Preview/                PreviewSynth -- the internal audible voice.
  Progression/            ChordEvent / ProgressionModel -- Phase 2
                          groundwork, tested but not yet wired in.
  Shared/                 HomeSeriesUI.h -- the shared Home-series look
                          (colours, card/well painters, controls),
                          extended with ChordCard for this plugin.
tests/                    Catch2 unit tests for MusicTheory and
                          ProgressionModel.
```

## Controls (Phase 1)

- **Key / Scale** — the two combo boxes in the header.
- **A S D F G H J** — play the 7 diatonic chords. Also clickable.
- **Octave** — shifts the whole keyboard up/down.
- **Velocity** — MIDI velocity for keyboard-triggered notes.
- **Preview** — internal preview synth volume.

Full detail, architectural reasoning, and an honest list of what isn't
built yet: [`STATUS.md`](STATUS.md).
