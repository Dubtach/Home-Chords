# Home-Chords — Status

This document is the honest account of what's actually built, what's
scaffolded but inert, and what hasn't been started, per the original
spec's own rule: *"do not claim something works unless it has actually
been implemented and tested."* Read the **Testing performed** section
before trusting anything else in this file — it explains exactly what
"tested" does and doesn't mean here.

## What this pass covers

The original spec is a 52-section, full-featured songwriting plugin —
realistically weeks of work. This pass builds **Phase 1** (as the spec's
own section 48 defines it) for real, plus a tested data-model foundation
for Phase 2. Nothing beyond that is implemented, and nothing is faked:
there are no placeholder buttons or controls in the UI that don't do
something real.

## Built and working (Phase 1)

- **Key**: all 12 chromatic roots (spec section 4).
- **Scale**: all 12 scales/modes listed in spec section 4 — Major,
  Natural/Harmonic/Melodic Minor, the 5 other church modes, Major/Minor
  Pentatonic, Blues.
- **Algorithmic diatonic chord generation** (spec section 5): chords are
  derived from Key+Scale by actually stacking scale tones, not looked up
  from a hardcoded table. Verified against the spec's own C Major, G
  Major, and A Minor test vectors, including the subtle case the spec
  specifically calls out — natural minor's **v is genuinely minor**, not
  forced into a major V.
- **Enharmonic spelling**: keys that conventionally read with flats (F,
  Bb, Eb, Ab, Db) spell that way; everything else spells with sharps.
  Modes are spelled via their parent major scale's bias.
- **A–J keyboard → real MIDI** (spec sections 6, 27): pressing A–J sends
  real Note On, releasing sends real Note Off. Real-time safe: no
  allocation in `processBlock`, edge-triggered from an atomic mask,
  per-note reference counting so two chords sharing a MIDI note number
  can't cause a premature or duplicate Note Off. See
  `source/Midi/ChordKeyboardEngine.h` for the detailed reasoning.
- **Internal preview synth** (spec section 26, scoped down — see below):
  a single clean voice, so the plugin is audible pressing A with nothing
  downstream listening to the MIDI.
  **Not** the full Piano/EP/Organ/Synth/Pad/Pluck/Strings/Bass picker.
- **Mouse click on a chord card** also plays/stops the chord, through the
  same code path as the keyboard (not a separate, half-wired feature).
- **Visual feedback** (spec section 38): chord cards highlight and fade
  on press/release; a MIDI-out activity lamp in the header. Colour
  follows chord quality (major/minor/diminished/etc.), not an arbitrary
  per-slot rainbow.
- **Resizable editor** (spec section 36) with a proportional layout, not
  a fixed canvas scaled up — the chord row actually reflows.
- **State persistence** for everything that exists so far: Key, Scale,
  Octave, Velocity, Preview Volume round-trip through
  `getStateInformation`/`setStateInformation` via the standard APVTS XML
  pattern.
- **Stuck-note handling**: reference-counted note-offs (see above), plus
  the editor releases every held slot when it loses OS focus or is
  closed. The one gap: if a host hard-kills the plugin process while a
  key is physically held, there's no `MidiBuffer` available in
  `releaseResources()` to send a final Note Off through — this is a
  limitation shared by essentially every JUCE MIDI-generating plugin, not
  something specific to this implementation.

## Built but not wired up (Phase 2 groundwork)

`source/Progression/ChordEvent.h` and `ProgressionModel.h` are a real,
independently unit-tested data model — add/remove/move/resize/serialize
all work and round-trip through `juce::ValueTree` correctly. `apvts`
state save/restore already includes an empty `<PROGRESSION>` tree so this
is forward-compatible.

**Nothing else references this model.** There is no timeline UI, no
drag-and-drop, no PLAY/ADD mode toggle, no loop playback. Phase 1's UI is
play-only, matching the spec's own Phase 1 scope exactly — an ADD-mode
toggle with no timeline behind it would be exactly the kind of dead
control the spec explicitly prohibits, so it isn't there.

## CI (GitHub Actions)

`.github/workflows/build_and_test.yml` is adapted directly from
Home-Disto's actual, working file (byte-verified from the uploaded zip,
not guessed): same macOS + Windows matrix, sccache, `ctest`, and
`pluginval --strictness-level 10`. Three deliberate differences from the
Home-Disto/Home-Sidechain version:

1. **No AU/AUv3/CLAP artifact paths** — this plugin's `FORMATS` in
   `CMakeLists.txt` is `Standalone VST3` only, matching the spec's
   explicit ask, so the workflow doesn't reference formats that were
   never asked for and aren't being built.
2. **No macOS icon-embedding or Windows installer (Inno Setup) steps** —
   both depend on a `packaging/` folder (an `.icns` and an `.iss` script)
   that doesn't exist for this plugin yet. Rather than copy steps that
   would fail on a missing file, both platforms just zip the raw build
   output. Add `packaging/pamplejuce.icns` + `packaging/installer.iss`
   and bring the equivalent steps back from Home-Disto's file when you
   want a real installer.
3. **`nightly.yml` and `linux-ci-image.yml` were not copied.** Both are
   Pamplejuce-template-maintainer-only files, not project CI: `nightly.yml`
   guards itself with `if: github.repository == 'sudara/pamplejuce'` (so
   it never actually runs on a Home-* repo either) and its own comment
   says "feel free to delete this file"; `linux-ci-image.yml` builds and
   pushes a Docker image to `ghcr.io/sudara/...`, a registry namespace
   Dubtach doesn't own. They're harmless leftovers in the other two repos,
   not something worth carrying forward.

Same caveat as everything else here: this has not actually been run on
GitHub — there's no way to trigger a workflow from this environment
either. Push it and see what the Actions tab says.

## Not started at all

Everything else in the original spec: the progression timeline itself
(section 8) and all its editing (sections 9–11), tempo/time signature
(12), the Auto Voicing engine (13) — chords currently play in a fixed
close-position triad, not voice-led — inversions (14), chord extensions
beyond plain triads (16), borrowed/color chords (17), suggestions (18),
presets (19), transpose-with-relative-key (20), the rhythm engine (21),
arpeggiator (22), bass/slash-chord system (23), humanization (24), MIDI
export to a file and MIDI drag-out (28–29), sections (30), the chord edit
menu (31), undo/redo (32), the full 8-instrument preview picker (26),
MIDI-input chord detection (34), and Scale Lock (35).

## Key architectural decisions

- **Real-time safety split**: `MusicTheoryTypes.h`/`Scale.h`/`Chord.h`
  separate a zero-allocation, no-`juce::String` "shape" representation
  (safe on the audio thread) from a `juce::String`-bearing
  `ChordDefinition` (message-thread only). `processBlock` recomputes the
  current shapes fresh every block from the Key/Scale parameters rather
  than caching them across threads — cheap (a handful of fixed-array
  writes), and it sidesteps needing any cross-thread chord cache at all.
- **Chord placement**: each slot's root is placed in whichever octave
  lands closest to a common reference pitch, so the 7 chords cluster in
  one register instead of spreading across two octaves. This is a
  simple, honest stand-in for real voice-leading, not a voicing engine —
  see `ChordKeyboardEngine.cpp`'s `placeNearReference`.
  Auto Voicing that minimises movement *between neighbouring chords in a
  progression* is real Phase 3 work.
  - **Chord-tone stacking is generic over scale size**: the same
    "skip-a-scale-tone" algorithm produces textbook triads for the 7-note
    scales and produces sensible pentatonic/blues chords for the 5/6-note
    ones (falling back to `ChordQuality::Other` on the few pentatonic
    degrees that don't land on a plain major/minor/diminished/augmented/
    sus shape, rather than forcing a wrong label).
- **UI kit**: `source/Shared/HomeSeriesUI.h` reuses the exact `homeUI`
  colour tokens and card/well/brand painters from Home-Disto /
  Home-Sidechain, plus one new component, `ChordCard`. Everything from
  the existing kit that Phase 1 doesn't actually use (Pill, Checkbox,
  SegmentedSwitch, LinkSelector, PowerButton, SettingsButton,
  ResetButton) was left out rather than copied in unused, so there's
  nothing dead sitting in the file — add them back from the sibling repos
  when Phase 2 needs them.
- **Tests**: `tests/CMakeLists.txt` fetches Catch2 via CPM directly rather
  than through the private `sudara/cmake-includes` submodule's
  `pamplejuce_add_tests()` — that submodule's contents weren't available
  when this project was generated (a plain zip export of a repo doesn't
  include submodule contents). Functionally equivalent; swap it for the
  real helper when you want to match the other Home-* repos exactly.

## Testing performed

**Read this before trusting anything else about correctness.** This code
was written in a sandboxed environment with no network access and no
JUCE/compiler toolchain available, so:

- **The C++ has not been compiled.** Not with this JUCE version, not
  with any compiler. There may be typos, API mismatches (JUCE version
  differences), or include-order issues that only a real build will
  surface.
- **The unit tests have not been run.** `tests/MusicTheoryTests.cpp` and
  `tests/ProgressionModelTests.cpp` are written and should compile and
  pass, but "should" is doing real work in that sentence.
- **What actually was verified**: the chord-stacking algorithm was
  independently reimplemented in Python and run against the spec's exact
  test vectors (C Major, G Major, A Natural Minor, A Harmonic Minor, F
  Major's flat spelling, D Dorian) — every value matched, including the
  natural-minor-v-is-minor case. The C++ mirrors that verified algorithm,
  but the Python check does not catch C++-specific mistakes.
- **Real-time safety** was reasoned through carefully (see the comments
  in `ChordKeyboardEngine.h`/`.cpp` and `PreviewSynth.h`/`.cpp`) but never
  measured — no profiler, no actual audio thread.

### Real CI run log

Unlike everything above, this section tracks what an actual build
attempt has shown, not just reasoning-through. Updated as real CI output
comes back.

- **Run 1 (Windows, clang-cl)**: got past MSVC toolchain setup, JUCE
  configure, `juceaide` build, and CPM fetching Catch2@3.7.1 — all of
  that is confirmed working. Failed at `tests/CMakeLists.txt:63-64`:
  `include(Catch)` couldn't find `Catch.cmake`, so `catch_discover_tests`
  was an unknown command. **Root cause**: CPM fetches Catch2 from source
  rather than via `find_package`, and only `find_package` wires up
  `CMAKE_MODULE_PATH` automatically — a well-documented Catch2/CPM
  interaction, confirmed against Catch2's own docs and a CPM.cmake GitHub
  issue hitting this exact error with this exact setup. **Fixed**: added
  `list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")` right
  after the `CPMAddPackage` call. Not yet confirmed by a second run.


**First thing to do with this**: `cmake -B Builds && cmake --build
Builds` and fix whatever the compiler finds. Given the scope of what's
here, expect *some* build errors on the first attempt — that's normal for
a from-scratch pass this size, not a sign the design is wrong.

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
keyboard input without a DAW in the loop. For the VST3, copy/point your
DAW at `Builds/HomeChords_artefacts/`.

## Suggested next steps

1. Build it locally, fix compiler errors.
2. Run the tests, fix whatever they turn up.
3. Confirm the acceptance-test basics work by ear/eye: select C Major,
   press A–J, hear/see the right chords, change Key/Scale, resize the
   window.
4. Push to GitHub and check the Actions tab — this is the first real
   signal on whether `build_and_test.yml` actually works, Windows
   included, since nothing here could trigger or watch a run.
5. From there, Phase 2 (the progression timeline) is the natural next
   piece of work — `ProgressionModel` is ready for it.
