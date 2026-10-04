# Home-Chords

A MIDI chord generator / songwriting plugin — part of the Dubtach "Home"
series alongside Home-Disto and Home-Sidechain.

Pick a Key and Scale, and the 7 diatonic chords are laid out on your
computer keyboard (**A S D F G H J** and/or **1-7**), your mouse, or an
external MIDI keyboard's white keys. Play live, or flip on Add Mode to
build a progression on the timeline, complete with rhythm patterns, an
arpeggiator, a bass system, and presets.

> **This is still a work in progress**, built incrementally and tested
> through real CI feedback rather than compiled locally. See
> [`STATUS.md`](STATUS.md) for exactly what's built, what's a deliberate
> scope simplification, what's untouched, and — importantly — which parts
> of the latest pass carry more risk than others and deserve extra
> listening before you trust them.

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
.github/workflows/       CI: build_and_test.yml (macOS + Windows,
                          ctest, pluginval), adapted from Home-Disto's.
source/
  PluginProcessor.*      Audio processor: parameters, the keyboard
                          engine, the sequencer, the preview synth.
  PluginEditor.*          The UI: chord row, transport, timeline,
                          selected-event toolbar, footer.
  MusicTheory/
    Scale.h/Chord.h        Zero-JUCE-dependency real-time-safe core:
                          scale tables, chord stacking, naming.
    MusicTheoryEngine.h     The juce::String-bearing display layer.
    Voicing.h               Shared note-placement/voice-leading math,
                          used by both live play and the sequencer.
    Presets.h               The 5 named scale-aware progression presets.
  Midi/
    ChordKeyboardEngine.h   Live play: keyboard/mouse/MIDI -> real,
                          stuck-note-safe MIDI, with Auto Inversion.
    NoteLedger.h            Shared per-note reference counting, so live
                          play and the sequencer can't step on each
                          other's notes.
  Progression/
    ChordEvent.h / ProgressionModel.h   The timeline's data model
                          (position, length, quality, rhythm, arp, bass).
    ProgressionActions.h    juce::UndoableAction wrappers for undo/redo.
  Timeline/
    SequencePlayer.h        The audio-thread sequencer: Play/Stop/Loop,
                          rhythm/arpeggio scheduling.
    TimelineComponent.h      The timeline UI: drag, resize, select.
  Shared/
    HomeSeriesUI.h           The shared Home-series look (colours,
                          card/well painters, controls, ChordCard).
    ChordEditPanel.h         Right-click quality/extension override popup.
    SettingsPanel.h          Keyboard-mode / note-spelling popup.
tests/                    Catch2 unit tests.
```

## Controls

- **Key / Scale** — the two combo boxes in the header.
- **A S D F G H J and/or 1-7** — play the 7 diatonic chords (which keys
  are active is set in Settings). Also clickable, and playable from an
  external MIDI keyboard's white keys (mapped to degrees 1–7 by ordinal
  position, not absolute pitch).
- **Right-click a chord card** — opens a popup to override that slot's
  quality and/or extension, keeping the same root note. An overridden
  card shows a small dot.
- **Settings (the "..." button)** — keyboard input mode (Letters/
  Numbers/Both) and note spelling (Auto/Sharps/Flats).
- **Add Mode** — pressing a keyboard/MIDI key (not a mouse click) appends
  that chord to the end of the timeline instead of just sounding it live.
- **Timeline** — drag a block to move it, its right edge to resize it,
  click to select it (shows a toolbar for Rhythm/Bass/Arp/Delete/
  Duplicate). Drag the small markers above the ruler to set the loop
  range.
- **Play / Stop / Restart / Loop** — the timeline's own transport, on its
  own internal clock (Tempo knob), independent of host playback.
- **Presets** — apply one of 5 scale-aware progression templates.
- **Undo / Redo** — for every timeline edit.
- **Auto Inversion** — on by default; smooths voice movement between
  chords you play live.
- **Octave / Velocity / Tempo / Preview** — the footer knobs/stepper.

Full detail, architectural reasoning, and an honest list of what isn't
built yet (or is a deliberate simplification): [`STATUS.md`](STATUS.md).
