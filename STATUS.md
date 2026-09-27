# Home-Chords — Status

This document is the honest account of what's actually built, what's
scaffolded but inert, and what hasn't been started, per the original
spec's own rule: *"do not claim something works unless it has actually
been implemented and tested."* Read the **Testing performed** section
before trusting anything else in this file — it explains exactly what
"tested" does and doesn't mean here.

## What this pass covers

The original spec is a 52-section, full-featured songwriting plugin —
realistically weeks of work. The first pass built **Phase 1** (as the
spec's own section 48 defines it) plus a tested data-model foundation for
Phase 2. This second pass adds three things asked for directly once Phase
1 was confirmed working: real voice-leading (Auto Inversion), a per-slot
quality/extension override system, and loudness consistency across chords
of different sizes. Nothing beyond that is implemented, and nothing is
faked: there are no placeholder buttons or controls in the UI that don't
do something real.

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

## Built and working (this pass's additions)

- **Auto Inversion** (spec sections 13-14, scoped down — see below): a
  checkbox, on by default. When on, a newly-pressed chord is re-voiced
  into whichever inversion (which of its own tones sit an octave up)
  lands closest to the centroid of the last chord actually played, so
  chord-to-chord movement is smoother than always jumping to root
  position. When off, every chord plays in root position (Phase 1's
  original, only behaviour). Deliberately bounded: it only chooses among
  a chord's own inversions, never shifts the whole chord by an extra
  octave, so a long sequence can't wander into an extreme register. This
  is **not** the full Auto Voicing engine from spec section 13 (no
  Open/Wide/Piano/Simple modes, no register/min-max controls) — see
  `ChordKeyboardEngine::chooseBestInversion`.
- **Per-slot quality/extension override** (spec section 16, plus a scoped
  version of the chord-edit part of section 31): right-click (or
  ctrl-click on macOS) any chord card to open a small popup. Pick a
  quality (Major/Minor/Diminished/Augmented/Sus2/Sus4) and/or an
  extension (6/7/Maj7/Add9) — the root note never changes, only what's
  built on it. A small white dot appears on an overridden card. "Reset to
  Diatonic" clears both back to the plain scale-derived chord. Backed by
  14 real APVTS parameters (`SLOT0_QUALITY` … `SLOT6_EXTENSION`), so
  overrides get host automation and state save/restore for free, same as
  every other parameter. What's *not* covered: sus2/sus4 as an
  *extension* on top of another quality (they're only offered as
  standalone qualities, matching how the music theory itself treats
  them), and full jazz-chord-naming correctness for every combination —
  see the naming caveat below.
- **Consistent loudness across chord sizes**: velocity is scaled by
  roughly `sqrt(3 / toneCount)` before a chord's notes are sent, so a
  4-note 7th (from the override above) doesn't ring louder than a plain
  3-note triad just because it has one more note sounding. Applies to the
  actual MIDI sent to the host, not just the internal preview synth, so a
  real instrument downstream gets the same balancing. Was a no-op for
  Phase 1 (every chord was a triad, so the factor was always 1.0) —
  matters now that extensions exist.

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
(12), the full Auto Voicing engine with Open/Wide/Piano/Simple modes and
register controls (13), manual (non-Auto) inversion selection, borrowed/
color chords (17), suggestions (18), presets (19), transpose-with-
relative-key (20), the rhythm engine (21), arpeggiator (22), bass/
slash-chord system (23), humanization (24), MIDI export to a file and
MIDI drag-out (28–29), sections (30), the rest of the chord edit menu
beyond quality/extension — Length/Voicing/Octave/Velocity/Bass/Rhythm/
Arpeggio per chord (31), undo/redo (32), the full 8-instrument preview
picker (26), MIDI-input chord detection (34), and Scale Lock (35).

**A naming caveat worth knowing about**: `Extension::Sixth` (+9
semitones) is used both for an added 6th on a major/minor triad *and*,
combined with a Diminished quality, for what's really a fully-diminished
7th chord — but the display always shows it as "6" (e.g. "Cdim6"), never
"dim7", even though "dim7" is the standard name for that specific
combination. Every other quality+extension combination follows a simple,
consistent `root + quality suffix + extension suffix` rule rather than
full jazz-notation correctness (e.g. Diminished+MajorSeventh has no
single standard symbol at all, so it just gets the same mechanical
concatenation). The actual *notes* are always correct regardless of
quality — only the display label can look non-idiomatic on a few unusual
combinations. See `extensionSuffix`/`extensionLabel` in `Scale.h`.

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
- **Override storage**: per-slot overrides are real `AudioParameterChoice`
  parameters (14 of them, `SLOT<n>_QUALITY`/`SLOT<n>_EXTENSION`), not raw
  atomics, specifically so they get host automation and APVTS state
  save/restore for free rather than needing bespoke serialization. Their
  string IDs are built by two small static helpers on the processor
  (`qualityParamId`/`extensionParamId`) used both when declaring the
  parameters and when caching raw pointers to them at construction time —
  one source of truth, so the two can never name different parameters by
  accident. The raw pointers are cached once (construction, message
  thread) specifically so `processBlock` never has to concatenate a
  parameter-ID string — and therefore allocate — just to read one of
  these every block.
- **Auto Inversion's voice-leading model is a centroid comparison, not
  full note-to-note matching**: comparing the average pitch of each
  candidate inversion against the average pitch of the last chord played
  is a simplification of real voice-leading (which would try to minimise
  *each individual voice's* movement, not just the chord's overall
  register), chosen because it's cheap, real-time safe, and handles
  chords of different sizes (triad to 7th to triad) without needing a
  notion of which old note "belongs to" which new one.
- **The edit popup is a `juce::CallOutBox`**, JUCE's standard mechanism
  for a small contextual popup anchored to the component that opened it,
  holding a new `homeUI::ChordEditPanel` (`source/Shared/ChordEditPanel.h`).
  Its quality/extension buttons are a small new radio-style `OptionButton`
  local to that file, not a port of the sibling repos' `Pill`/
  `SegmentedSwitch` components (which weren't carried over into this
  project's `HomeSeriesUI.h` — see the UI kit note above).

## Testing performed

**Read this before trusting anything else about correctness.** This code
was written in a sandboxed environment with no network access and no
JUCE/compiler toolchain available, so:

- **The C++ has not been compiled** for anything added in this pass
  (Auto Inversion, the override system, `ChordEditPanel`, `Checkbox`) —
  Runs 1-3 below only cover Phase 1. There may be typos, API mismatches,
  or include-order issues that only a real build will surface, same as
  every previous pass.
- **New unit tests were added** for this pass's music-theory logic:
  `tests/ChordOverrideTests.cpp` covers `buildOverriddenShape` (named
  chord types like dominant 7th, m7, minor-major 7th, on a fixed root),
  `invertShape` (root position through every inversion of both a triad
  and a 4-note 7th chord, including the wrap-around case), and both
  `applyOverride` overloads (quality-only, extension-only, both at once,
  and the inactive/passthrough case) — including that overriding a
  diatonic chord's quality correctly flips its roman-numeral case. Same
  caveat as always: written and hand-verified, not run.
- **What actually was verified**: the override/inversion math (dominant
  7th, m7, minor-major 7th spellings; 1st/2nd inversion of a triad; 3rd
  inversion of a 7th chord putting the 7th in the bass) was independently
  reimplemented in Python and cross-checked before being committed to C++
  — same discipline as the original chord-stacking verification. All
  values matched. The Python check does not catch C++-specific mistakes.
- **API verification this pass**: two JUCE APIs new to this pass
  (`juce::Button::setButtonText`/`getButtonText` as base-class methods,
  and `juce::CallOutBox::launchAsynchronously`'s exact signature) were
  checked against current docs.juce.com/master before use, rather than
  assumed — directly because of what Runs 2-3 below turned up about
  trusting memory over verification for this specific JUCE branch.
- **Real-time safety** was reasoned through carefully (see the comments
  in `ChordKeyboardEngine.h`/`.cpp`) but never measured — no profiler, no
  actual audio thread. The new velocity-compensation math
  (`compensateVelocityForToneCount`) is a handful of float operations, no
  allocation, same real-time-safety class as everything around it.

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
  after the `CPMAddPackage` call.
- **Run 2 (Windows, clang-cl)**: configure succeeded; compilation reached
  `PluginProcessor.cpp` and `PluginEditor.cpp` and failed on both with the
  same 4 errors, all in `Shared/HomeSeriesUI.h`: `no member named
  'getStringWidth' in 'juce::Font'`. **Root cause**: this project builds
  against JUCE's `develop` branch, where `Font::getStringWidth()` has
  been removed outright (not just deprecated) as part of a font/text
  metrics overhaul — confirmed against JUCE's own `BREAKING_CHANGES.md`
  and the current docs.juce.com reference. Worth noting: this exact line
  used to call `GlyphArrangement::getStringWidthInt()`, which an earlier
  pass in this project changed to `Font::getStringWidth()` in the name of
  using a "more certain" API — that change was wrong. **Fixed**: switched
  all 4 call sites to `juce::TextLayout::getStringWidth (font, text)`,
  the replacement JUCE's own breaking-changes doc names explicitly, which
  also returns `float` directly (removing a few now-redundant casts).
  The rest of `HomeSeriesUI.h`'s Font usage (`FontOptions`, `withName`,
  `withStyle`) produced no errors in this same run, so it's left as-is.
- **Run 3 (Windows, clang-cl)**: confirmed the Run 2 fix held —
  `PluginProcessor.cpp` and `PluginEditor.cpp` no longer show the
  `getStringWidth` errors. New, unrelated failure in `Chord.cpp:80`: `use
  of undeclared identifier 'noteName'`. **Root cause**: a genuine missing
  include, not a JUCE API issue this time — `chordToneNames()`'s
  implementation in `Chord.cpp` calls `noteName()`, which is declared in
  `Scale.h`, but `Chord.cpp` only included `Chord.h` (which does not
  itself include `Scale.h`). `MusicTheoryEngine.cpp` includes both
  headers, which is why the same function worked fine when called from
  there. **Fixed**: added `#include "Scale.h"` to `Chord.cpp` directly.
  This run also gave enough visibility to confirm `ChordEvent.cpp` and
  `ProgressionModel.cpp` compile cleanly (steps 12 and 14, no failures
  reported), in addition to the files Run 2 already confirmed.
  **Additional hardening from a full re-audit** (requested explicitly:
  "fix all other bugs"): traced every non-trivial symbol in every `.cpp`
  file against that file's own include chain (not what a sibling file
  happens to include), the same class of check that would have caught
  both bugs above. Found two places relying on `std::move` being
  transitively available via `<vector>`/`<algorithm>` rather than
  including `<utility>` directly (`MusicTheoryEngine.cpp`,
  `ProgressionModel.cpp`) — both had already compiled successfully in
  Run 2/3, so this isn't a fix for an active failure, just closing a
  portability gap before it becomes one. Also read JUCE's full
  `BREAKING_CHANGES.md` (version 6.1.0 through 8.0.13) end to end and
  checked every other non-trivial JUCE API used in this codebase
  (`Slider`/rotary params, `ADSR`, `dsp::IIR`, `AudioProcessorValueTreeState`
  attachments/listeners, `ValueTree`, the `AudioParameter*Attributes`
  constructors, `setResizable`/`setResizeLimits` ordering) against it —
  nothing else on that list matches this codebase, and two choices
  already made (the `Attributes`-based parameter constructors, calling
  `setResizable(true, true)` before `setResizeLimits`) turn out to
  already match what current JUCE requires.
  Not yet confirmed by a further run. The Tests target (Catch2Main.cpp,
  MusicTheoryTests.cpp, ProgressionModelTests.cpp) has not been reached
  by any real build yet — all three runs so far have failed or stopped
  within the main plugin target.
- **Run 4 and beyond**: not yet attempted. Everything from "this pass's
  additions" above (Auto Inversion, the 14 override parameters,
  `ChordEditPanel`, `Checkbox`, `ChordOverrideTests.cpp`) is new since Run
  3 and has not been through a real build at all. Expect this to be where
  the next round of real compiler feedback lands.

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

1. Build it locally, fix compiler errors — this pass's new code
   (Auto Inversion, overrides, `ChordEditPanel`) hasn't been built at all
   yet, so treat it with at least as much suspicion as Phase 1 got.
2. Run the tests, fix whatever they turn up.
3. Confirm by ear/eye: select C Major, press A–J, hear/see the right
   chords; right-click a card, change its quality/extension, confirm the
   root note doesn't move and the card gets its override dot; toggle Auto
   Inversion off and on and listen for the difference; hold a triad then
   an overridden 7th chord back to back and check they're not obviously
   different in loudness.
4. Push to GitHub and check the Actions tab.
5. From there, Phase 2 (the progression timeline) is the natural next
   piece of work — `ProgressionModel` is ready for it.
