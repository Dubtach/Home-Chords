# Home-Chords — Status

This document is the honest account of what's actually built, what's
scaffolded but inert, and what hasn't been started, per the original
spec's own rule: *"do not claim something works unless it has actually
been implemented and tested."* Read **Testing performed** before trusting
anything else here — it explains exactly what "tested" does and doesn't
mean, and which parts of this pass are higher-risk than others.

## What this pass covers

Three passes so far:
1. **Phase 1**: Key/Scale-driven algorithmic diatonic chords, A–J
   keyboard → real MIDI, a preview synth, a resizable UI, stuck-note-safe
   note handling. Confirmed working against a real screenshot (E Major,
   every one of the 7 chords correct).
2. **Voice-leading pass**: Auto Inversion (centroid-based voice-leading),
   per-slot quality/extension override (right-click a card), loudness
   compensation across chord sizes.
3. **This pass**: a reported octave bug found and fixed; the progression
   timeline (drag/resize/move/delete/duplicate, Play/Stop/Loop, its own
   internal transport); a rhythm engine (8 patterns); an arpeggiator (4
   modes); a bass system (4 modes + custom); 5 scale-aware presets;
   undo/redo for every timeline edit; numeric-key chord triggering with a
   Letters/Numbers/Both setting; a plugin-wide sharp/flat spelling
   preference; external MIDI keyboard input (white keys = scale degrees
   1–7).

This third pass is, by a wide margin, the largest and riskiest single
pass of the project. Said plainly: **this is the first time in this
project I'm shipping something I have real, specific reservations
about** — not from carelessness, but because sample-accurate sequencing
is genuinely hard to get right without a compiler, and this pass had to
build it anyway. The reservations are named specifically throughout this
document, in "Known scope limits" and "Testing performed," rather than
hidden behind a blanket disclaimer.

## The octave bug — found, fixed, verified

You reported: chords felt like they dropped an octave partway through
the row ("123 then 4567"). Confirmed exactly: the old placement logic
snapped each chord's root to *its own* nearest octave independently, so
in C Major, G/A/B's nearest-reference octave landed a full step below
C/D/E/F's, even though musically they're the *next* notes up the scale.

**Fixed** by placing every slot's root by its actual semitone distance
*above* a single shared anchor (slot 0's root — the scale tonic, which an
override never changes) rather than each slot snapping independently.
Verified in Python against both the original bug (reproduced exactly: C
D E F at MIDI 60/62/64/65, then G A B dropping to 55/57/59) and the fix
(C through B ascending smoothly, 60 through 71, checked in multiple
keys including enharmonically awkward ones like B major). See
`MusicTheory/Voicing.h`'s `placeRootAscending`, and
`VoicingAndPresetsTests.cpp` for the regression test encoding this exact
case so it can't silently come back.

## Built and working — Phase 1

- **Key**: all 12 chromatic roots. **Scale**: all 12 scales/modes from
  the spec (Major, Natural/Harmonic/Melodic Minor, the 5 other church
  modes, Major/Minor Pentatonic, Blues).
- **Algorithmic diatonic chord generation**: chords are derived from
  Key+Scale by actually stacking scale tones, not a lookup table.
  Verified against the spec's C Major/G Major/A Minor test vectors,
  including natural minor's v being genuinely minor, not forced major.
- **Enharmonic spelling**: flat-preferring keys (F, Bb, Eb, Ab, Db) spell
  with flats; everything else with sharps; modes follow their parent
  major's bias. (This pass adds a user override on top — see below.)
- **A–J keyboard → real MIDI**: real-time safe, edge-triggered from an
  atomic mask, per-note reference counting so two chords sharing a MIDI
  note can't cause a premature or duplicate Note Off.
- **Internal preview synth**: one clean voice (not the full 8-instrument
  picker from spec section 26), audible with nothing downstream.
- **Mouse click on a chord card** plays/stops the chord through the same
  path as the keyboard.
- **Visual feedback**: chord cards highlight/fade on press/release, a
  MIDI-out activity lamp. Colour follows chord quality.
- **Resizable editor** with proportional (not fixed-canvas) layout.
- **State persistence** via the standard APVTS XML pattern.
- **Stuck-note handling**: reference-counted note-offs, plus the editor
  releases every held slot on focus loss or close. The one gap: a host
  hard-killing the process mid-chord has no `MidiBuffer` available in
  `releaseResources()` for a final Note Off — a limitation shared by
  essentially every JUCE MIDI-generating plugin.

## Built and working — voice-leading pass

- **Auto Inversion**, on by default: a newly-pressed chord is re-voiced
  into whichever inversion's centroid lands closest to the last chord
  actually played, bounded so it never shifts a whole chord by an extra
  octave (can't wander into an extreme register over a long sequence).
  Not the full Auto Voicing engine from spec section 13 (no Open/Wide/
  Piano/Simple modes, no register controls).
- **Per-slot quality/extension override**: right-click (ctrl-click on
  macOS) a chord card to open a popup. Pick a quality (Major/Minor/
  Diminished/Augmented/Sus2/Sus4) and/or an extension (6/7/Maj7/Add9);
  the root never changes. An overridden card shows a small dot. Backed
  by 14 real APVTS parameters (`SLOT0_QUALITY` … `SLOT6_EXTENSION`), so
  overrides get host automation and state save/restore for free.
- **Consistent loudness across chord sizes**: velocity scaled by roughly
  `sqrt(3 / toneCount)` so a 4-note chord doesn't ring louder than a
  3-note one. Applies to the real MIDI sent out, not just the preview.

**Architecture carried forward from that pass**: the override system's
14 parameter IDs are built by two static helpers
(`PluginProcessor::qualityParamId`/`extensionParamId`) used both when
declaring the parameters and when caching raw pointers to them at
construction — one source of truth, so the two can't drift apart, and
`processBlock` never has to build a parameter-ID string (which would
allocate) just to read one of these every block. The edit popup is a
`juce::CallOutBox` holding `homeUI::ChordEditPanel`
(`Shared/ChordEditPanel.h`).

**A naming caveat carried forward**: `Extension::Sixth` (+9 semitones) is
used both for a plain 6th chord and, combined with Diminished quality,
for what's really a fully-diminished 7th — but it always displays as
"6" (e.g. "Cdim6"), never "dim7". Every quality+extension combination
follows a simple, consistent `root + quality suffix + extension suffix`
rule rather than full jazz-notation correctness (e.g. Diminished+
MajorSeventh has no single standard symbol and just gets the mechanical
concatenation). The actual notes are always correct; only the label can
look non-idiomatic on a few unusual combinations. See
`extensionSuffix`/`extensionLabel` in `MusicTheory/Scale.h`.

## Built and working — this pass

- **Progression timeline** (spec sections 8–11): drag a block to move
  it, drag its right edge to resize it, click to select, Delete/
  Duplicate buttons, snapped to a quarter-beat grid. Loop region with
  draggable start/end markers. Play/Stop/Restart transport, driven by
  **this plugin's own internal clock** (a Tempo knob, 20–300 BPM) — see
  "Known scope limits" for why this isn't synced to host transport.
  `Timeline/TimelineComponent` (UI) + `Timeline/SequencePlayer` (the
  audio-thread engine that actually triggers notes).
- **ADD mode**: a checkbox; pressing a keyboard or MIDI key (not a mouse
  click — see `ChordKeyboardEngine`'s class comment) appends that slot's
  current chord to the end of the timeline, one bar long, through the
  undo manager.
- **Rhythm engine** (spec section 21), all 8 patterns: Sustained, Whole,
  Half, Quarter, Eighth, Sixteenth, Strum, Arpeggio. A per-event
  property, edited via a toolbar that appears under the timeline when a
  block is selected.
- **Arpeggiator** (spec section 22): Up, Down, Up/Down, Random, with a
  selectable rate (1/4 to 1/32) — one of the rhythm patterns above,
  sharing its scheduling.
- **Bass system** (spec section 23): Root, Third, Fifth (both map onto
  existing chord inversions), Inversion (root position — the sequencer
  doesn't auto-invert between events, see below), Custom (an explicit
  slash-bass pitch class placed below the chord).
- **Presets** (spec section 19), the 5 named there (I-V-vi-IV, vi-IV-I-V,
  I-IV-V-I, I-iii-vi-IV, vi-V-IV-V), scale-aware — verified in C Major
  and D Major that the actual chords differ correctly, not transposed
  note names. Applying a preset clears and rebuilds the timeline as one
  grouped undo step.
- **Undo/redo** (spec section 32) for every timeline edit — add, remove,
  move, resize, rhythm/bass/arp changes, preset application — via
  `juce::UndoManager` and three `juce::UndoableAction` subclasses
  (`Progression/ProgressionActions.h`) wrapping the existing, already-
  tested `ProgressionModel` rather than rewriting its internals. A
  dedicated UndoManager, deliberately not shared with APVTS parameter
  changes — see "Key architectural decisions."
- **Numeric key input**: the number row (1–7, not the numpad) triggers
  the same 7 slots as A–J. A new setting (Letters / Numbers / Both,
  default Both) controls which are active.
- **Note spelling preference**: Auto (the existing per-key convention) /
  Sharps / Flats, plugin-wide. Applies to the chord row and the override
  popup's title; see "Known scope limits" for where it doesn't yet reach.
- **External MIDI keyboard input**: white keys, by pitch class
  (C,D,E,F,G,A,B → slots 1–7 by ordinal position, not absolute pitch),
  trigger the same slots as the computer keyboard — so "C major's white
  keys" always means "degrees 1–7 of whatever Key/Scale is selected,"
  matching how chord-pad controllers conventionally work. Black keys are
  ignored. Reuses the Velocity knob rather than real MIDI velocity — a
  deliberate simplification, see below.
- **Settings popup**: the header's small button opens a popup for the
  two preferences above (keyboard mode, note spelling).

## Known scope limits in this pass (deliberate, not oversights)

- **The sequencer's transport is self-contained, not host-synced.** It
  never reads `AudioPlayHead` — Play/Stop/tempo are entirely this
  plugin's own, independent of the host transport. Genuine host-sync is
  real additional work for a future pass.
- **The sequencer doesn't auto-invert between timeline events.** Auto
  Inversion's voice-leading is a *live-play* feature. Each timeline event
  plays its own root position (or Third/Fifth bass inversion)
  independently; there's no smoothing of movement *between* consecutive
  timeline chords the way there is between live-played ones.
  `SequencePlayer::startEvent` does feed its played notes into the live
  engine's voice-leading memory, so playing live right after the
  sequencer stops leads smoothly from it — but sequenced-to-sequenced
  voice-leading isn't implemented.
- **Rhythm/arpeggio pulses are block-quantized, not sample-accurate** —
  a deliberate trade-off. An event's *start and end* are placed at the
  exact interpolated sample within whichever block they fall in (so
  nothing drifts or gets stuck), but a repeating pulse *within* an event
  (a Quarter-note retrigger, an arp step) is only checked once per block
  and fires at that block's first sample if due. A few milliseconds of
  jitter at most, inaudible at typical block sizes — but a real,
  conscious simplification, made because precise event start/end (where
  a mistake means an actual stuck note) mattered far more than
  microsecond-precise internal subdivision in the single riskiest piece
  of this pass.
- **Loop wrap can drop up to one block's worth of time** (a few ms) right
  at the loop boundary rather than precisely splitting a block across the
  seam. Inaudible at normal block sizes; a named simplification.
- **Timeline blocks always display with sharp spelling**, regardless of
  the Note Spelling preference — `TimelineComponent` isn't threaded
  through to that parameter yet. The chord row and override popup do
  respect it correctly.
- **An existing timeline block's quality/extension isn't editable after
  placement** — it inherits whatever the live slot's quality/extension
  was at the moment it was added (or whatever a preset specifies). Only
  Rhythm/Bass/Arp are editable post-placement, via the selected-event
  toolbar. A reasonable next addition, scoped out here for time.
- **External MIDI keyboard velocity is ignored** in favour of the
  Velocity knob, for consistency with computer-keyboard input (which has
  no real velocity) and to avoid threading a second, per-source velocity
  through `ChordKeyboardEngine`.
- **No MIDI drag-out or file export** (spec sections 28–29), no sections
  (30), no borrowed/color chords (17), no suggestions (18), no
  humanization (24), no full Auto Voicing with Open/Wide/Piano/Simple
  modes (13) — Auto Inversion remains the only voicing mode.
- **Configurable snap grid** (spec section 10) isn't implemented — the
  timeline snaps to a fixed quarter-beat grid.

## Key architectural decisions (this pass)

- **`NoteLedger`** (`Midi/NoteLedger.h`): a single per-MIDI-note reference
  count, shared by `ChordKeyboardEngine` (live play) and `SequencePlayer`
  (the sequencer). This is what lets live and sequenced notes overlap on
  the same pitch without one cutting the other off early or double-
  releasing — the same reference-counting discipline Phase 1 established
  for overlapping live chords, now extended to a second, independent
  note source.
- **Undo is deliberately its own `UndoManager`**, separate from APVTS's.
  Sharing one would have made every parameter change — including every
  tick of a knob drag — a separate undo step, flooding the stack with
  noise unrelated to timeline editing. Key/Scale/Octave/Velocity/etc.
  have no undo in this plugin (the host's own automation covers them,
  same as Phase 1); only timeline edits go through `undoManager`.
- **Drag gestures produce exactly one undo step**, not one per
  `mouseDrag` callback: `TimelineComponent` applies move/resize live
  (bypassing undo, for responsive visual feedback), then on `mouseUp`
  reverts to the pre-drag state and re-applies the final result through
  a single `UpdateChordAction`. See the comment in
  `TimelineComponent::mouseUp` — getting this backwards (reading the
  "before" state *after* the live preview already changed it) would
  silently make undo a no-op, so the order matters.
- **White-key MIDI mapping uses ordinal position, not absolute pitch**:
  `whiteKeySlotForPitchClass` maps C/D/E/F/G/A/B to slots 1–7 by which
  white key it is, not by transposing — so playing "C major shape" on an
  external keyboard always triggers scale degrees I–VII of whatever Key/
  Scale is currently selected, matching the request and how chord-pad
  controllers conventionally work.
- **Three new `std::atomic` control fields on `SequencePlayer`** (playing,
  tempo, loop range, position) exist because this is a real thread-safety
  requirement: set from the message thread (UI), read every block from
  the audio thread inside `advanceAndRender`. Caught and fixed during
  development, before reaching you as a bug.
- **`ChordEvent` grew rhythm/arp/bass fields directly** rather than a
  separate properties object, since every property is 1:1 with a
  specific timeline event and the existing serialization already handles
  adding fields cleanly.
- **API verification this pass**: `juce::UndoManager`/`UndoableAction`
  (`perform`/`undo`/`redo`/`canUndo`/`canRedo`/`beginNewTransaction`),
  `juce::ComboBox`'s `getSelectedItemIndex`/`setSelectedItemIndex` (and
  its default-notification behaviour, which mattered for avoiding a
  spurious update loop when syncing the selected-event toolbar),
  `juce::Button::setButtonText`/`getButtonText` as base-class methods,
  and `juce::CallOutBox::launchAsynchronously`'s exact signature were all
  checked against current docs.juce.com/master before use. This continues
  the practice the voice-leading pass adopted after the
  `Font::getStringWidth` mistake in Phase 1 (see the CI run log below).

## Review & fix pass (Run 4)

A compiler-and-test-driven review of the third pass, done against real JUCE
(both the 8.0.8 tag and current `develop`), with every source file
syntax-checked under clang and the unit tests built and run (also under
ASan + UBSan). What it found and fixed:

**CI break (Run 4 failure)**
- `TimelineComponent.h` used `timeline::SequencePlayer` without including
  `SequencePlayer.h`. The ~20 other errors in the log (`setWantsKeyboardFocus`,
  `repaint`, `getWidth`, ...) were all cascades of that one missing type.
- `paint()` called `removeFromBottom` on a `const` rectangle and called
  `drawCardText` without its `homeUI::` prefix.

**Real bugs found by review / the new tests**
- *Undo*: nothing called `beginNewTransaction()`, so `juce::UndoManager`
  merged every timeline edit into a single undo step. Every user gesture now
  starts its own transaction (a preset is still one step, deliberately).
- *Selection*: "nothing selected" was `juce::Uuid()`, which is a fresh random
  id, not null -- so clicking empty timeline space never hid the toolbar. Now
  `juce::Uuid::null()`. A selection deleted by undo/restore is cleared too.
- *Data race*: the audio thread iterated the progression while the UI edited
  it. `ProgressionModel` now has a spin lock (mutators take it; the sequencer
  *try*-locks and skips a block rather than waiting) and a revision counter.
- *State*: `getStateInformation` appended a new `PROGRESSION` child each save
  without removing the one a previous load left behind, so load -> save ->
  load restored the oldest progression. Undo history is also cleared on load.
- *Stuck/silent notes*: the shared `NoteLedger` was never reset in
  `prepareToPlay`/`releaseResources`, so stale counts could suppress later
  Note Ons. A chord held with the mouse stayed on if the editor closed.
- *Preview synth*: a host block larger than the prepared size produced
  silence; it now renders in chunks.
- *Loop timing*: a block crossing the loop end discarded its remainder, so
  every pass ran up to one block long (measured 256-3000 samples). Blocks now
  split at the loop end and carry on from the loop start; the period is exact
  for any block size. Rhythm/arp pulses now land on their exact sample (they
  used to be block-quantised) and can't fire at/after their event's end.

**Test-only fixes**: the F-major test asked for sharps while expecting flats;
the multi-action undo test relied on the (buggy) lack of transactions.

**New tests (51 total, was 38)**: `SequencePlayerTests.cpp` (exact start/end
samples at six block sizes, exact loop period at five, no stuck notes on
wrap/delete/shrunk loop, arpeggio order, release-only-own-notes), plus
revision/copy/round-trip tests for the model and a transaction-grouping test.

**Still not covered by any automated test**: the editor wiring and the
mouse-driven timeline gestures (they need a real UI), `Arpeggio`'s `arpGate`
field (stored and saved but not yet applied -- each step is held until the
next), and the Windows/macOS-specific CI steps (pluginval, packaging).

## Testing performed

**Read this before trusting anything new in this pass.** (This section is the
original third-pass account, written without a compiler; the "Review & fix
pass" above supersedes its risk claims for `SequencePlayer`, which now has
real tests.) Same sandbox
limits as every prior pass — no compiler, nothing here has been built —
but this pass additionally has a real *risk gradient* worth being
explicit about, because not all of the new code is equally trustworthy:

**Lower risk — independently verified, same discipline as the Phase 1
music-theory engine:**
- The octave-bug fix (`placeRootAscending`) — reproduced the bug and
  verified the fix in Python (see above).
- `Voicing.h`'s functions (`anchorMidiNote`, `placeRootAscending`,
  `voiceShape`, `compensateVelocityForToneCount`, `chooseInversionNearest`)
  — real unit tests (`VoicingAndPresetsTests.cpp`), including specific
  dominant-7th/m7/minor-major-7th voicings and triad/7th-chord
  inversions, cross-checked in Python first.
- Presets — unit tested; scale-awareness confirmed in two different keys.
- `ProgressionModel`/`ChordEvent` extensions and the three undo actions
  — real unit tests (`ProgressionActionsTests.cpp`), including that
  `UpdateChordAction` restores the *exact* pre-change state (quality,
  position, and length together) on undo.
- Every include was traced by hand against each file's own include chain
  (not "compiles somewhere in the project"), the same process that caught
  two real bugs in the voice-leading pass. This pass added explicit
  `<cmath>` to `PluginEditor.cpp` and `<functional>` to
  `ChordEditPanel.h` after finding they relied on one-level transitive
  includes rather than stating their own dependencies directly.

**Higher risk — reasoned through carefully, but genuinely not testable
without a compiler and a running transport:**
- **`SequencePlayer`** — no automated tests at all. Its correctness
  depends on floating-point beat/sample arithmetic, several interacting
  state machines (per-event runtime, loop wrapping, rhythm pulsing, arp
  stepping), and timing behaviour that's awkward to unit-test
  meaningfully without simulating a sequence of real `processBlock`
  calls. This is the single piece of this pass to listen to most
  carefully: specifically test a Strum event, an Arpeggio event, and a
  loop that wraps mid-event, and listen for anything that doesn't stop
  cleanly.
- **The whole `PluginEditor` wiring for this pass** (transport buttons,
  selected-event toolbar, settings popup, preset application) — correct
  by my own code-tracing, never run.
- **`TimelineComponent`'s mouse-drag-to-undo logic** — reasoned through
  carefully (see "Key architectural decisions") but not tested against an
  actual mouse.

Given that risk gradient, if the next build throws errors, I'd bet on
`SequencePlayer.cpp` or the `PluginEditor`/`TimelineComponent` wiring
before the music-theory or undo-action layers.

### Real CI run log

- **Run 1 (Windows, clang-cl)**: got past MSVC toolchain setup, JUCE
  configure, `juceaide` build, and CPM fetching Catch2@3.7.1. Failed at
  `tests/CMakeLists.txt:63-64`: `include(Catch)` couldn't find
  `Catch.cmake`, so `catch_discover_tests` was an unknown command.
  **Root cause**: CPM fetches Catch2 from source rather than via
  `find_package`, and only `find_package` wires up `CMAKE_MODULE_PATH`
  automatically — confirmed against Catch2's own docs and a CPM.cmake
  GitHub issue hitting this exact error with this exact setup. **Fixed**:
  added `list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")`
  right after the `CPMAddPackage` call.
- **Run 2 (Windows, clang-cl)**: configure succeeded; compilation reached
  `PluginProcessor.cpp`/`PluginEditor.cpp` and failed on both with the
  same 4 errors, all in `Shared/HomeSeriesUI.h`: `no member named
  'getStringWidth' in 'juce::Font'`. **Root cause**: this project builds
  against JUCE's `develop` branch, where `Font::getStringWidth()` has
  been removed outright (not deprecated) — confirmed against JUCE's own
  `BREAKING_CHANGES.md` and the current docs.juce.com reference. Worth
  noting: this exact line used to call `GlyphArrangement::getStringWidthInt()`,
  which an earlier pass changed to `Font::getStringWidth()` in the name
  of using a "more certain" API — that change was wrong; the compiler is
  the actual ground truth, not a guess about API stability. **Fixed**:
  switched all 4 call sites to `juce::TextLayout::getStringWidth (font,
  text)`, the replacement JUCE's own breaking-changes doc names
  explicitly, which also returns `float` directly.
- **Run 3 (Windows, clang-cl)**: confirmed the Run 2 fix held —
  `PluginProcessor.cpp`/`PluginEditor.cpp` no longer showed the
  `getStringWidth` errors. New failure: `Chord.cpp:80`, `use of
  undeclared identifier 'noteName'`. **Root cause**: a genuine missing
  include — `chordToneNames()`'s implementation in `Chord.cpp` calls
  `noteName()`, declared in `Scale.h`, but `Chord.cpp` only included
  `Chord.h` (which doesn't itself include `Scale.h`). It worked when
  called from `MusicTheoryEngine.cpp` only because that file happens to
  include both headers. **Fixed**: added `#include "Scale.h"` to
  `Chord.cpp` directly. This run also confirmed `ChordEvent.cpp` and
  `ProgressionModel.cpp` compile cleanly. **Additional hardening** done
  at the same time (requested explicitly: "fix all other bugs"): traced
  every non-trivial symbol in every `.cpp` file against that file's own
  include chain, the same class of check that would have caught both
  bugs above; found and closed two places relying on `std::move` being
  transitively available via `<vector>`/`<algorithm>` rather than
  `<utility>` directly (`MusicTheoryEngine.cpp`, `ProgressionModel.cpp`)
  — both had already compiled fine, so this was hardening, not an active
  fix. Also read JUCE's full `BREAKING_CHANGES.md` (6.1.0 through 8.0.13)
  end to end and checked every other non-trivial JUCE API used in the
  codebase against it — nothing else matched, and two choices already
  made (the `Attributes`-based parameter constructors, calling
  `setResizable(true, true)` before `setResizeLimits`) turned out to
  already match what current JUCE requires.
- **Run 4 (Windows, clang-cl)**: failed compiling `TimelineComponent.cpp` (missing `SequencePlayer.h` include plus the const/qualification errors) -- fixed in the "Review & fix pass" above. Beyond that, everything described in "this
  pass" above — the octave-bug fix, the timeline, rhythm engine,
  arpeggiator, bass system, presets, undo/redo, numeric/MIDI input, note
  spelling — is new since Run 3 and has not been compiled at all.

**First thing to do with this**: build it, and expect real errors — this
pass is roughly the size of the previous two combined, built without a
compiler throughout. That's not a reason to distrust the design; it's the
reason this document names its risk areas specifically instead of
claiming uniform confidence.

## Build instructions

```bash
# If starting from a fresh checkout without JUCE yet:
git submodule add https://github.com/juce-framework/JUCE.git JUCE
git -C JUCE checkout develop

cmake -B Builds -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build Builds

# Tests (needs network at configure time, to fetch CPM.cmake then Catch2):
ctest --test-dir Builds --verbose --output-on-failure
```

The Standalone build is the fastest way to sanity-check sound and
keyboard/timeline behaviour without a DAW in the loop.

## Suggested next steps

1. Build it, fix compiler errors — expect more than usual this time, see
   above.
2. Run the tests.
3. Listen specifically for: the octave bug being gone (a smooth ascending
   row in several different keys); a Strum and an Arpeggio event playing
   and stopping cleanly; a loop that wraps mid-event leaving nothing
   stuck; Undo/Redo actually restoring state after a drag, a delete, and
   a preset application; numeric keys and MIDI keyboard white keys both
   triggering the right chords.
4. Push to GitHub and check the Actions tab.
5. After that: host-transport sync, sample-accurate internal pulses,
   editable quality/extension on placed timeline blocks, and the rest of
   the "not started" list are the natural next pieces.
