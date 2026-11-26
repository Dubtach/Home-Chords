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
- **ADD mode** *(removed in v3 — chords are now dragged from the cards
  onto the timeline; see the v3 section)*.
- **Rhythm engine** (spec section 21), all 8 patterns: Sustained, Whole,
  Half, Quarter, Eighth, Sixteenth, Strum, Arpeggio. Stored per event but
  set globally since v3 (the "All chords" row).
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

## Known scope limits (deliberate, not oversights)

*(Older limits that the v2 pass below removed — host sync, sequenced
voice-leading, timeline spelling, editable quality/extension, snap grid,
MIDI export, colour chords, suggestions, humanize, voicing styles — have
been deleted from this list. What remains:)*

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
- **External MIDI keyboard velocity is ignored** in favour of the
  Velocity knob, for consistency with computer-keyboard input (which has
  no real velocity) and to avoid threading a second, per-source velocity
  through `ChordKeyboardEngine`.
- **Sections** (spec section 30) are not implemented.
- **No "Simple" voicing mode** — voicing styles are Close / Open / Wide /
  Piano.
- **MIDI export is rendered, not copied**: the file is produced by running
  the real SequencePlayer offline (48 kHz, 512-sample blocks, 960 PPQ), so
  it matches playback, including humanize and voice-leading, but humanize
  jitter in the file is one deterministic take, not "live" randomness.

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

## v2 pass — features added

Added on top of the passing CI build:

- **Host-transport sync** (`Host sync` checkbox, `SYNC_HOST`): when on,
  the sequencer follows the host's play state and PPQ position
  (`SequencePlayer::syncToHost`); the internal Play/Restart/Loop/Tempo are
  disabled. Turning it off stops the sequencer and releases notes.
- **MIDI export and drag-out**: `Midi/MidiExport.*` renders the timeline to
  a format-1 `.mid`. *Export* opens an async file chooser; the *DRAG MIDI*
  tile writes a temp file and starts an external drag.
- **Borrowed / colour chords** (`MusicTheory/ColorChords.*`, `Colour`
  button): 9 chords for major-sounding keys (bIII, iv, bVI, bVII, bII, V/V,
  V/vi, V/ii, I7) and 7 for minor-sounding ones; flat-labelled chords are
  spelled with flats. Adds to the timeline as their own events.
- **Chord suggestions** (`MusicTheory/Suggestions.*`, `Hints` toggle):
  after the last chord, up to 3 likely next chords get a dashed outline,
  the top one a "NEXT" tag, and a tooltip with the reason.
- **Humanize**: timing (0–30 ms) and velocity (0–30 %) knobs; deterministic
  RNG, applied in the sequencer and in exports.
- **Voicing styles** (Close/Open/Wide/Piano) and **sequenced voice-leading**
  (Bass=Inversion events auto-invert between consecutive timeline chords).
- **Snap grid** (1/16, 1/8, 1/4, 1/2, Bar), **timeline spelling** that
  follows the sharps/flats preference, **editable quality/extension** on
  placed events, and a per-event **arp gate**.
- Fixes made along the way: see the first pass notes (TimelineComponent
  compile errors, model locking, undo transactions, etc.).

### v2 testing

- 76 Catch2 cases / 20,389 assertions pass (built locally with g++ against
  JUCE modules); new suites cover voice-leading, colour chords,
  suggestions and export (including auto-inversion).
- All sources compile with `clang -fsyntax-only` against JUCE develop with
  no warnings, and a link-level symbol check found no undefined project
  symbols.
- **Not run here**: the GUI itself. The editor wiring, drag-out, file
  chooser, host sync against a real host, and tooltip/hint visuals are
  compile-checked only. Click through them and check the CI run.

## v3 pass — redesign and new interaction model

Requested after trying v2; all of it is in this build.

- **Add mode removed.** Chords reach the timeline by **dragging a chord
  card onto it** (drop position snapped, never overlapping another chord;
  a ghost block previews the landing spot) or by double-clicking a card
  or choosing "Add to timeline" from its menu.
- **Right-click a chord card** now holds everything per-chord: Quality,
  Extension, **Borrowed chord** (replaces the old Colour button — the card
  itself becomes the borrowed chord, root and all) and Reset. A borrowed
  chord persists per slot (`SLOT<n>_COLOUR` parameters), is played from the
  audio thread through a real-time-safe lookup
  (`colorChordShape`), and carries the card's roman numeral (bVII, V/V...).
  The timeline's Quality/Extension toolbar is gone.
- **Global Rhythm / Bass / Arp mode / Arp rate / Gate** (new APVTS params
  `RHYTHM`, `BASS_MODE`, `ARP_MODE`, `ARP_RATE`, `GATE`). Changing one
  stamps it on every chord of the timeline as one undo step; new chords
  pick up the current values. Per-event fields still exist underneath, so
  the sequencer and MIDI export are unchanged. Sessions saved before v3
  adopt the first chord's style as the global one. "Custom" bass needs a
  pitch, so the global Bass list is Root / Third / Fifth / Inversion.
- **Timeline** rewritten and made short (about 85 px at the default
  size): multi-select (shift/ctrl/cmd-click, rubber band, Ctrl/Cmd+A),
  move/resize all selected, **drag out of the strip to delete**, copy /
  cut / paste / duplicate / nudge, right-click menu, bar ruler following
  the time signature, pixel scale frozen during drags, and a loop end that
  grows to cover new chords.
- **27 presets** in Pop, Jazz, Chill, Minor and Basics (jazz/chill ones
  carry 7th / maj7 / add9 extensions; the 12-bar blues uses dominant 7ths),
  shown only when they suit the current scale. A longer preset stretches the
  loop so nothing is cut off.
- **Compact UI**: 820 × 346 default (min 780 × 322, resizable), one-line
  brand, smaller combo/button text, tools row, global playback row,
  five-knob footer. Press **?** for the shortcut sheet.
- **Input**: numpad 1-7 now plays chords (in the Numbers modes); Ctrl/Cmd/
  Alt combos never sound chords; shortcuts for undo/redo, play/stop,
  restart, loop, select all, copy/cut/paste, duplicate, delete, nudge,
  octave. The **MIDI keyboard white keys C D E F G A B = chords 1-7** (any
  octave) mapping was already in v2 and is unchanged.

### v3 testing

- 83 Catch2 cases / 35,033 assertions pass. New: preset library (valid in
  every key/scale where offered, grouped, side filtering, jazz extensions),
  real-time colour-chord lookup equals the display palette, global
  playback settings, `findFreeStart`.
- **A scripted GUI test** (built here against JUCE develop, run under
  Xvfb, screenshots reviewed) drives the real editor: card drops (incl.
  onto an occupied spot), borrowed chords and overrides, global style +
  undo, rubber-band select, Ctrl+A, nudge, duplicate, drag-out delete,
  Ctrl+Z / Delete via the keyboard, preset stretching the loop, and a
  resize.
- **Still not exercised here**: real OS drag-and-drop (the drop is invoked
  directly), popup menus on screen, the DRAG MIDI external drag, file
  chooser, host sync, and running inside an actual DAW. Check these in the
  Standalone build and in your host.

## v4 pass — smarter timeline and controls

- **Magnetic timeline.** Chords always sit back to back from beat 0:
  no overlap, no gaps. Dropping a card inserts it (later chords make room);
  dragging sideways reorders; the right edge resizes and pushes later chords;
  deleting (key, menu or dragging out) closes the gap. The loop always spans
  exactly the chords (0 .. end of last chord) and follows every edit, undo
  and preset, so the manual loop markers were removed. All of it lives in
  `Progression/TimelineLayout.h` (pure functions, unit-tested); every gesture
  is one `SetEventsAction` undo step; drags are previewed without touching
  the model. Old sessions are packed on load.
- **Playback controls scoped to the selection.** Rhythm / Bass / Arp mode /
  Rate / Gate change the selected chords, or all chords when none is
  selected. The boxes show the scope's common value ("Mixed" otherwise); the
  Arp mode / rate / gate boxes are shown only when the scope contains an
  Arpeggio rhythm. These boxes are no longer parameter attachments, so a
  selection-only change doesn't touch the parameters.
- **UI:** Undo/Redo buttons removed (shortcuts remain); footer knobs
  redrawn as compact dials with caption and value beside them; arrow keys
  move the selection one place.
- 91 Catch2 cases pass (new: layout, undo action, playback fields). The
  scripted Xvfb GUI test was rewritten for the new behaviour (drops, mid
  insert, delete + loop refit, reorder, resize, drag-out delete,
  selection-scoped style, hide/show of arp boxes, shortcuts, presets).
- Windows CI: switched to MSVC `cl` (the runner's LLVM 20 is too old for
  VS 18's STL) and fixed lambda-capture / shadowing issues MSVC reported.
- **Still not exercised here:** real OS drag-and-drop, on-screen popup
  menus, DRAG MIDI external drag, host sync, a real DAW, MSVC build.

## v5 pass — layout tidy

- Time signature moved into a metronome chip in the timeline's top-right
  corner (the ruler leaves that corner clear). Grid snap moved to the
  timeline's right-click menu. The hidden combo boxes still own the
  TIME_SIG / SNAP parameters through their attachments.
- Ruler: bold bar numbers, beat ticks with beat numbers when there is room,
  faint snap-grid lines, bar lines through the grid.
- Default window height raised to 402 (min 362) so the timeline blocks are
  tall enough; footer is 74 px with Spread / Bass / Smooth voicing grouped.
- Hints toggle moved to the settings panel.

## v6 pass — steppers, click-free audio, drag previews

- **Steppers.** Every selector (Key, Scale, Spread, Bass, Rhythm, Arp mode /
  rate, Gate) is a `homeUI::StepperBox`: a real ComboBox drawn as `< value >`.
  Arrows step (wrapping); clicking the value opens the list; mouse wheel still
  works. Attachments and onChange are unchanged.
- **Audio clicks (PreviewSynth).** Causes found: (1) a note retriggered while
  still sounding/releasing restarted its phase at zero (waveform jump) --
  restarts on restart, fast chords and fast arps; (2) when all voices were busy
  a voice was cut off instantly; (3) note events were applied at the start of
  the block, not at their sample position; (4) the mix could hard-clip and the
  preview volume stepped. Fixes: same-note retrigger keeps the voice and phase
  (envelope attacks from its current level, velocity glides); stolen voices
  fade out in a "ghost" over 128 samples; events are applied sample-accurately
  (render up to each event); tanh soft clip; smoothed volume; 32 voices;
  release 0.45 s. New `[clicks]` Catch2 tests measure the largest
  sample-to-sample step (they fail against the old phase-reset behaviour).
- **Timeline previews.** Dragging blocks: the others close up around an open
  dashed slot (the landing place) and the dragged blocks float under the
  pointer (red + faded when outside = will delete). Card drops show the real
  chord (not a default C) in a dashed ghost slot.
- **Removed:** the "..." settings popup and the "?" shortcut popup (and
  their files). KEYBOARD_MODE, NOTE_SPELLING and Next-Chord Hints remain as
  host-automatable parameters with their defaults (letters+numbers, auto,
  hints on) -- there is no longer a UI for them.
- **Moved:** Export and DRAG MIDI to the header beside Key/Scale; Presets
  to the right end of the tools row.

## v7 pass — drag and drop

- **Reorder by left edge.** The dragged block lands in the gap its left edge is
  nearest (`slotForLeftEdge`), not where the pointer is, so a long chord needs
  no extra travel to reach the start; the floating block keeps the grab point
  under the pointer.
- **Card drops: replace or insert.** `dropTargetAt`: the middle half of a chord
  replaces it (keeps id, position, length and playback style; takes the new
  chord's degree/root/quality/extension); the outer quarters insert before /
  after it; past the last chord appends; Shift forces insert. Previews: REPLACE
  tag on the target, or a dashed INSERT ghost with the later chords moved.
- Bug found by the GUI test while building this: `commit()` skipped edits that
  left the layout unchanged, which silently ignored a replace; it now also
  compares chord identity.
- Arp rate stepper was too narrow (58 px: the value was clipped to nothing
  between the arrows); widened to 104 px with "Rate 1/8" style labels.
- 100 Catch2 cases pass; scripted GUI test covers replace, insert, append,
  left-edge reorder and the arp rate.

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
5. After that: sections (spec 30), per-source MIDI keyboard velocity, a
   Simple voicing mode, and sample-accurate internal pulses are the
   natural next pieces.

## v8 pass — drag feedback, transport icons, footer
- Card drops now use the same preview as reordering: the card floats under the pointer, the others slide aside around a soft slot. No dashed ghost, no INSERT/REPLACE tags. When a drop would replace a chord, that chord is tinted red.
- Blocks ease smoothly to their new positions (card drops, reorders, deletes, undo).
- Shift+click on a chord card adds it to the timeline (replaces double-click). On the timeline: Shift+click toggles selection, Shift+drag duplicates the selected chords (drag outside to cancel), double-click a chord removes it. Ctrl/Cmd+click still toggles.
- Play/Stop, Back to start, Loop and Host sync are icon buttons centred under the timeline. The chord-style row (scope label, Rhythm, arp controls, Presets) moved up under the chord cards.
- Footer: dark panel, light captions, separate VOICING section (Spread, Bass, Smooth voicing), Octave on the left, five knobs on the right.
- Not exercised in real environments: OS-level drag and drop, DAW host sync, MSVC build, listening tests.
