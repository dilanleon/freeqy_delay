# Spectral Delay

A C++20 JUCE spectral delay for macOS, built as VST3 and Standalone.
Draw independent left/right delay, feedback and level curves over the input
spectrum. Select the curve and channel above the graph.
The visible frequency axis is logarithmic from 20 Hz to 21 kHz; processing
and saved curves still cover the full spectrum. Drawing affects visible bins.
Link mirrors edits; copy buttons copy all curves to the other channel. Reset affects the selected
curve, and Undo/Redo groups a mouse gesture into one edit.

Max Delay scales the delay curve up to ten seconds. Extra Copies adds up to
16 normalized taps. Spread sets their maximum offset, Depth moves the taps
toward zero, and Rate controls their movement. Post placement adds conventional
wet-output taps; Spectral placement also includes the taps in bin feedback.

## Controls and clearing tails

The editor groups **Curves & Banks** above the graph, with **Delay & Mix**,
**Chorus / Copies**, and **Input / Output** below it. Copy placement lives with
the chorus controls. The frequency axis remains logarithmic, 20 Hz–21 kHz.

**Clear Buffer** in the upper-right removes all stored delay, feedback and
chorus audio. It fades the wet signal down over 5 ms, clears the history, and
fades back in over 5 ms. New input continues entering the effect. Dry audio,
curve banks and control values remain intact. Clearing is a momentary action,
not an automatable or saved parameter.

## Curve banks and morph automation

Choose one to four active banks, then select **Edit bank 1–4** to draw each
bank's left/right delay, feedback and level curves. **Copy to bank …** copies
all six curves into the indicated next bank. Undo/Redo also covers bank copies
and count changes. Reducing the count retains the hidden banks' curves.

Automate **Curve Morph (%)** in the DAW or move **Morph** in the editor.
The full range traverses the active banks in order. With four banks, the
anchors are 0%, 33⅓%, 66⅔% and 100%; with two, they are 0% and 100%.
Between anchors, all six curves blend linearly. With one bank, morph has no
effect. Position changes are smoothed before updating the DSP at FFT hops.
This continues with the editor closed.

Solid curves show the bank being edited; dashed curves preview the morphed
result. Selecting a bank does not move Morph. All banks and the morph position
are saved in project/plugin state. Old single-bank states load with identical
curves in all four banks and Morph at 0%.

## Build

Requires Apple developer command line tools and CMake >=3.22. JUCE 9.0.5 is
pinned by commit and fetched automatically unless `JUCE_DIR` is supplied.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

In this workspace CMake and JUCE are already available locally:

```sh
.deps/venv/bin/cmake -S . -B build -DJUCE_DIR="$PWD/.deps/JUCE" -DCMAKE_BUILD_TYPE=Debug
.deps/venv/bin/cmake --build build -j 4
.deps/venv/bin/ctest --test-dir build --output-on-failure
```

Use `build-release` and `-DCMAKE_BUILD_TYPE=Release` for optimized builds.
Outputs are under `build-release/SpectralDelay_artefacts/Release/`:
`VST3/Spectral Delay.vst3` and `Standalone/Spectral Delay.app`.
No plugins are copied into system folders automatically.

## FFT / sine checkpoint

Build the minimal development interface separately:

```sh
.deps/venv/bin/cmake -S . -B build-diagnostic -DJUCE_DIR="$PWD/.deps/JUCE" -DCMAKE_BUILD_TYPE=Debug -DSPECTRAL_DIAGNOSTIC=ON
.deps/venv/bin/cmake --build build-diagnostic -j 4
.deps/venv/bin/ctest --test-dir build-diagnostic --output-on-failure
```

This variant displays incoming FFT magnitude and replaces the output with a
440 Hz, -18 dBFS sine. It has zero reported latency. The normal build disables
this route and reports 2048 samples of latency. Standalone input starts muted
by JUCE; use its device settings and appropriate routing for live-input tests.

## Verification and limits

`DspTests` checks reconstruction, integer-hop delay timing, fractional spectral
reads, feedback decay, level mute, bypass latency, offset bounds, finite output,
and ordinary C++ heap allocation during maximum-voice processing. `HostCheck`
scans and loads the built VST3, processes audio, recalls nondefault parameter
state, and creates its editor. Diagnostic checks also measure sine output.
StateTests covers bank recall, morph automation, legacy migration and invalid data.
These are automated JUCE-host checks, not certification in a commercial DAW.

Spectral interpolation is approximate between hops and intentionally has
windowed coloration. Curve positions are relative to Nyquist. Delay contents
are not saved; reopening a project starts a new tail. Current state format is
version 2, with version 1 migration. Unsigned local builds are ad-hoc signed by JUCE's build tooling.

See AGENTS.md for DSP invariants, milestone gates and validation status.
