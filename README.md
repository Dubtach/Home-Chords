# Home-Chords

A MIDI chord generator / songwriting plugin — part of the Dubtach "Home"
series alongside Home-Disto and Home-Sidechain.

Pick a Key and Scale, and the 7 diatonic chords are laid out on your
computer keyboard (**A S D F G H J**, **1-7** or the **numpad**), your
mouse, or an external MIDI keyboard's white keys. Play live, or drag the
chord cards onto the timeline to build a progression, with rhythm,
arpeggiator and bass settings (all chords or just the selected ones), borrowed chords, 27 presets, and MIDI export.

> **This is still a work in progress**, built incrementally and tested
> through real CI feedback plus a local compile-and-test review pass. See
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
    ColorChords.h/.cpp      Borrowed/colour chord sets by key mode.
    Suggestions.h/.cpp      Functional-harmony "what next" suggestions.
  Midi/
    ChordKeyboardEngine.h   Live play: keyboard/mouse/MIDI -> real,
                          stuck-note-safe MIDI, with Auto Inversion.
    MidiExport.h/.cpp       Offline render of the timeline to a .mid file.
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
    TimelineLayout.h         Gap-free layout rules (pack, reorder, insert, resize).
  Shared/
    HomeSeriesUI.h           The shared Home-series look (colours,
                          card/well painters, controls, ChordCard).
    MidiDragTile.h           "DRAG MIDI" tile for dragging out a .mid.
tests/                    Catch2 unit tests.
```

## Controls

- **Key / Scale** — steppers in the header; **Export** and **DRAG MIDI** sit beside them.
  Every selector is a stepper: click `<` / `>` to step, click the name for the list.
- **Play chords** — **A S D F G H J**, **1-7**, the **numpad 1-7** (which
  keys are active is the KEYBOARD_MODE parameter, default both), the mouse, or an external **MIDI
  keyboard**: the white keys C D E F G A B play chords 1-7 in any octave.
- **Right-click a chord card** — Quality, Extension, and **Borrowed chord**
  (bIII, iv, bVI, bVII, V/V ... for the key). The card changes to that
  chord; "Reset to diatonic" undoes it. Overridden cards show a dot.
- **Drag a chord card onto the timeline** to insert it where you drop it;
  the chords after it make room. Double-click a card to append it.
- **Timeline** — a gap-free track: chords sit back to back from the start,
  never overlap, and the loop always spans exactly the chords (so deleting
  or adding never needs a manual loop fix). Click a block to select;
  shift/ctrl/cmd-click to add to the selection; drag on empty space to
  rubber-band select; drag blocks sideways to reorder, the right edge to
  resize (later chords shift along); drag blocks out of the strip and
  release to delete (the gap closes); right-click for Delete / Duplicate /
  Copy / Paste.
- **Rhythm / Bass / Arp / Rate / Gate** (the row under the timeline) — they
  apply to the **selected chords**, or to **every chord when nothing is
  selected** (the label on the left says which; the boxes show "Mixed" when
  the chords differ). Arp direction, rate and gate only appear while the
  scope uses the Arpeggio rhythm. New chords pick up the last global choice.
- **Play / Restart / Loop** — the timeline's own transport (Tempo knob).
  **Host sync** follows the host's transport and position instead.
- **Time signature** — the metronome chip in the timeline's top-right corner.
  **Grid snap** — right-click the timeline > Grid snap (faint lines show the
  current grid on the ruler).
- **Voicing group (footer)** — Spread (Close/Open/Wide/Piano), Bass note and
  Smooth voicing sit together, since all three shape how a chord is voiced.
  (Bass follows the same selection-or-all rule as Rhythm.)
- **Presets** — 27 progressions in Pop, Jazz, Chill, Minor and Basics,
  filtered to the ones that suit the current scale.
- **Export / DRAG MIDI** (header) — save the timeline as a .mid, or drag it into a DAW track.
- **Hints** — dashed outlines mark likely next chords; "NEXT" is the top
  pick, hover for the reason. On by default; the "Next-Chord Hints" parameter
  turns them off.
- **Humanize / Velocity / Tempo / Preview / Octave / Smooth voicing** — footer.
- **Shortcuts**: Space play/stop, Home restart, L loop,
  Ctrl/Cmd+Z undo, +Shift+Z or Y redo (there are no undo buttons), +A select
  all, +C/X/V copy/cut/paste, +D duplicate, Delete remove, Left/Right move
  the selection one place, Up/Down octave, Esc deselect.

Full detail, architectural reasoning, and an honest list of what isn't
built yet (or is a deliberate simplification): [`STATUS.md`](STATUS.md).
