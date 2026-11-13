# FREEQY DELAY

A C++20 JUCE spectral delay for macOS, built as VST3 and Standalone.
Edit independent left/right delay, feedback and level values in a live bucket display.
The frequency axis is logarithmic from 20 Hz–20 kHz. Select a curve and channel;
Link mirrors edits, Reset restores the selected curve, and Command-Z / Shift-Command-Z
undo or redo one gesture.

## Spectral view

The dark live **input FFT** is overlaid on a scrolling **delay-return spectrogram**.
Frequency stays horizontal on the logarithmic 20 Hz–20 kHz axis. Newest activity
appears at the top and scrolls downward. **Scroll** selects 0.5×, 1×, 2× or 4×
speed, showing 12, 6 (default), 3 or 1.5 seconds of history in the same height.
Faster speeds give closely spaced taps more vertical separation and finer time
resolution. Analyzer frames arrive every 256 samples, retaining events between
GUI refreshes. The 2048-sample analysis window still sets the minimum transient
smearing. Speed is saved with the project; changing it restarts visual history
without changing the sound or clearing delay tails.

Brighter indigo/blue/cyan pixels indicate louder buffer activity, using FREEQY DELAY's violet–magenta–cyan palette. It measures the wet
return after feedback/level, before dry/wet and output trim.

Delay, Feedback and Level retain bright horizontal bucket markers and dashed
morph targets above the history. Selecting a control still edits whole buckets.
History is captured while the editor is open; L/R switching and resizing retain it.
Opening the editor, changing Scroll speed, or using Clear Buffer discards older queued
visual frames so they cannot reappear as current activity.

Use the **Delay / Feedback / Level** buttons inside the view to choose what to
edit. **L / R** switches the editing channel; the chain icon links channel edits.
Four wide **Bank** tabs above the graph select stored shapes. **Banks + / −**
changes the active count; inactive tabs gray out. Selecting a bank does not move
Morph. Scroll and Resolution steppers, Reset and Clear Buffer share the toolbar
below the graph. Drawing edits whole STFT bucket cells.

## Delay and STFT size

Max Delay scales the delay curve up to ten seconds. Feedback reaches 100%,
which permits non-decaying repeats; Clear Buffer removes the stored tail. Select an STFT window of
**16, 32, 64, 128, 256, 512, 1024, or 2048 samples**, including through DAW automation.
Small windows produce broad frequency bands; large windows separate frequencies more
finely. Every sample rate shows the selected size's full positive-bin count across the
20 Hz–20 kHz view. The display resamples its independent 2048-point analyzer and saved
curve grid by physical frequency; DSP bins themselves remain native to the host rate.

Latency stays at **2048 samples** for every setting, so DAW compensation and
mix/bypass alignment remain stable. Changing size briefly fades the wet signal
and clears its existing tail. Bank shapes and dry audio remain intact.

## Stereo curve shift

**Amount Hz** (0–1000 Hz) moves the delay, feedback and level shapes along the
frequency axis. **Rate Hz** (0–10 Hz) makes the left and right shapes move in
opposite directions, then exchange directions sinusoidally. Rate 0 holds a
fixed left-up/right-down offset; Amount 0 disables movement. Both controls
are exposed to DAW automation; mono uses the left shift.

For example, Amount 100 Hz and Rate 0 moves a level peak drawn at 1 kHz to
1.1 kHz on the left and 900 Hz on the right. Audio stays in its original bins:
a 1 kHz input tone remains 1 kHz, with its treatment determined by the moving
maps. Changing delays can still produce the usual delay modulation effects.
Lookups interpolate between stored curve points and hold endpoint values
outside DC/Nyquist. Saved banks stay unchanged; dashed caps preview the shifted,
morphed targets. Existing `shiftHz` and `shiftRateHz` automation IDs are retained.

## Clearing tails

**Clear Buffer** removes stored delay and feedback audio. It fades wet
out over 5 ms, clears history, then fades back in over 5 ms. New input continues
entering the effect. Dry audio, curve banks and control values remain intact.
It is a momentary action, not an automatable or saved parameter.

## Curve banks and morph automation

Choose one to four banks with **Banks + / −**, then select **Bank 1–4** to edit.
Inactive banks retain their data. Undo/Redo includes bank-count changes.

The bottom strip groups **Bank Morph**, a large **Max Delay** knob, **Curve Shift**,
**Wet**, and stacked **IN / OUT** trims. The paired-arrow button toggles Stereo Morph.
Numerical values use a seven-segment display with a glowing unit label.
Max Delay reads in milliseconds below one second and seconds at or above one second.
Bare typed values use the displayed unit; append `ms` or `s` to override it. Automate **Curve Morph (%)** or move
**Morph** to blend adjacent banks. Four-bank anchors are 0%, 33⅓%, 66⅔% and
100%; two-bank anchors are 0% and 100%. One bank ignores morph.

Enable **Stereo Morph** with the arrow button to move left/right bank positions in opposite directions
around the main Morph position. **Depth %** sets the offset (0–100%, default
25%), and its own **Rate Hz** controls motion (0–10 Hz, default .3). Positions
wrap around 0/100%; rate zero holds a fixed positive left/negative right offset.
Dashed caps preview the live positions. This works independently of curve shifting,
continues with the editor closed, and supports DAW
automation. Turning it off smoothly returns both channels to the main Morph.

State format is version 4. Versions 1–3 remain readable: old single-bank states
populate all four banks, old copy controls are retired, and Stereo Morph starts
off for all older states. Existing version 3 shift amount/rate settings survive with the corrected curve-shift behavior.

## Build

Requires Apple developer command line tools and CMake >=3.22. JUCE 9.0.5 is
pinned by commit. Pass JUCE_DIR for an existing checkout, or let CMake fetch it.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

This workspace already has project-local tools and JUCE:

```sh
.deps/venv/bin/cmake -S . -B build -DJUCE_DIR="$PWD/.deps/JUCE" -DCMAKE_BUILD_TYPE=Debug
.deps/venv/bin/cmake --build build -j 4
.deps/venv/bin/ctest --test-dir build --output-on-failure
```

Use build-release and -DCMAKE_BUILD_TYPE=Release for optimized builds. Output:
`build-release/FreeqyDelay_artefacts/Release/VST3/FREEQY DELAY.vst3` and
`build-release/FreeqyDelay_artefacts/Release/Standalone/FREEQY DELAY.app`.
Nothing is automatically installed into system plugin folders.

## Makefile, system installation, and AAX

The root Makefile builds into its own `build-system` tree, leaving other build
trees independent. Release VST3 is the default; AAX compilation is disabled.
AAX requires explicit `AAX=1`, even when an SDK is present. With AAX enabled,
a single `aax-sdk*/Interfaces/AAX.h` folder supplies the SDK path automatically.
Explicit `AAX_SDK_PATH` overrides discovery.

```sh
make                          # Build VST3 only
make install                  # Build, then install VST3 system-wide
make install-vst3 AAX=0        # Build/install only VST3
make aax AAX=1                 # Build AAX with JUCE's bundled SDK if none is detected
make install AAX=1 AAX_SDK_PATH="/absolute/path/to/AAX_SDK"
```

VST3 installs to `/Library/Audio/Plug-Ins/VST3`; AAX installs to
`/Library/Application Support/Avid/Audio/Plug-Ins`. Close hosts before replacing
loaded bundles. Installation requires write access to those folders. If denied,
run the install steps with administrator privileges **after** the ordinary build:

```sh
sudo .deps/venv/bin/cmake --install build-system --config Release --component VST3
sudo .deps/venv/bin/cmake --install build-system --config Release --component AAX
```

The AAX component exists only when AAX was enabled during configuration. Do not
run the build itself with sudo. Override `BUILD_DIR`, `CONFIG`, `JOBS`, `VST3_DIR`
and `AAX_DIR` on the make command line as needed. Example local staging:

```sh
make install VST3_DIR="$PWD/build-stage/VST3" AAX_DIR="$PWD/build-stage/AAX"
```

CMake-only users can enable `-DSPECTRAL_BUILD_AAX=ON` and optionally supply
`-DAAX_SDK_PATH=/path/to/sdk`. JUCE 9.0.5 includes a bundled SDK, so an external
SDK is optional. This produces AAX Native, not AAX DSP for HDX hardware.

To test an unsigned AAX build, register as an AAX developer with Avid, obtain a
Pro Tools Developer build and its iLok authorization. Normal retail Pro Tools
requires PACE digital signing; Apple ad-hoc/Developer ID signing alone is not a
substitute. Request the signing tools through Avid when ready. The build/install
targets do not perform PACE signing or notarization. See
[Avid's AAX setup guide](https://github.com/audiosdk/aax/blob/main/README.md) and
[JUCE's AAX instructions](https://github.com/juce-framework/JUCE/blob/master/README.md#aax-plug-ins).
No external AAX SDK is downloaded separately; CMake may obtain JUCE's bundled copy when it
fetches JUCE.

## FFT / sine diagnostic

```sh
.deps/venv/bin/cmake -S . -B build-diagnostic -DJUCE_DIR="$PWD/.deps/JUCE" -DCMAKE_BUILD_TYPE=Debug -DSPECTRAL_DIAGNOSTIC=ON
.deps/venv/bin/cmake --build build-diagnostic -j 4
.deps/venv/bin/ctest --test-dir build-diagnostic --output-on-failure
```

This displays input FFT magnitude and replaces output with a continuous 440 Hz,
-18 dBFS sine, with zero reported latency. Normal builds disable this route.
JUCE Standalone starts with input muted to prevent a hardware feedback loop.

## Validation

DspTests exercises all eight windows, mono/stereo, 44.1–192 kHz, reconstruction,
delay, feedback, curve remapping, bank morph, stereo curve shifting,
size transitions, clearing, dry continuity and ordinary C++ audio-thread heap
allocation. HostCheck scans/loads the VST3, checks exposed automation/latency,
processes audio, recalls state and creates the editor. StateTests covers bank and
new-control recall, old-state migration and malformed data. These are automated
JUCE-host checks, not commercial-DAW certification or listening acceptance.
Local bundles are ad-hoc signed; distribution notarization is deferred.
See AGENTS.md for DSP invariants and maintenance instructions.
