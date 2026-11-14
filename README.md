# FREEQY DELAY

A C++20 JUCE spectral delay for macOS, built as VST3, native CLAP, and Standalone.
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
frequency axis. Its knob uses logarithmic spacing with an exact zero, providing finer control
at low frequencies while retaining the existing host automation mapping. **Rate Hz** (0–10 Hz) makes the left and right shapes move in
opposite directions. A symmetric logarithmic response shapes the sinusoidal modulation, with
a smooth center crossing and the selected Amount reached at each peak. Rate 0 holds a
fixed left-up/right-down offset; Amount 0 disables movement. Both controls
are exposed to DAW automation; mono uses the left shift.

For example, Amount 100 Hz and Rate 0 moves a level peak drawn at 1 kHz to
1.1 kHz on the left and 900 Hz on the right. Audio stays in its original bins:
a 1 kHz input tone remains 1 kHz, with its treatment determined by the moving
maps. Changing delays can still produce the usual delay modulation effects.
Lookups interpolate between stored curve points and hold endpoint values
outside DC/Nyquist. Curve Shift pins the visible endpoints and interpolates them into the moving curve, including
where a shift would otherwise leave a flat shelf. The original curve is retained at zero shift. Saved banks stay unchanged; dashed caps preview the shifted,
morphed targets. Existing `shiftHz` and `shiftRateHz` automation IDs are retained.

## Clearing tails

**Clear Buffer** removes stored delay and feedback audio. It fades wet
out over 5 ms, clears history, then fades back in over 5 ms. New input continues
entering the effect. Dry audio, curve banks and control values remain intact.
It is a momentary action, not an automatable or saved parameter.

## Curve banks and morph automation

Use the top-bar **Presets** menu to load **Default**, which sets the curves to their Reset values (Delay 25%, Feedback 35%, Level 100%) and enables one
bank. Max Delay is 1 s, Resolution is 1024 samples, Wet is 50%, and the other parameters are zero.

Choose one to four banks with **Banks + / −**, then select **Bank 1–4** to edit.
Inactive banks retain their data. Undo/Redo includes bank-count changes.

The bottom strip groups **Bank Morph**, a large **Max Delay** knob, **Curve Shift**,
**Wet**, and stacked **IN / OUT** trims. The paired-arrow button selects the Morph modulation direction.
Numerical values use a seven-segment display with a glowing unit label.
Max Delay reads in milliseconds below one second and seconds at or above one second.
Bare typed values use the displayed unit; append `ms` or `s` to override it. Automate **Curve Morph (%)** or move
**Morph** to blend adjacent banks. Four-bank anchors are 0%, 33⅓%, 66⅔% and
100%; two-bank anchors are 0% and 100%. One bank ignores morph.

The paired-arrow button selects same-direction or opposite-direction movement of the left and
right bank positions around Morph. **Depth %** sets the offset (0–100%, default 0%);
set it to zero to turn modulation off. **Rate Hz** controls motion (0–10 Hz, default .3).
Positions wrap around 0/100%; rate zero holds a positive offset on the left and either a
positive or negative offset on the right, according to direction. Dashed caps preview live
positions. Modulation runs with the editor closed and supports DAW automation.

State format is version 4. Versions 1–3 remain readable: old single-bank states
populate all four banks and old copy controls are retired. Existing version 3 shift amount/rate settings survive with the corrected curve-shift behavior.

## Codebase map for contributors and agents

Start with `AGENTS.md` for the non-negotiable DSP, real-time, UI, state, and
validation invariants. Then use this map instead of reading the repository in
filename order:

| Area | Authoritative files | Go here when… |
|---|---|---|
| Build/formats/install | `CMakeLists.txt`, `Makefile` | adding a format, dependency, test target, signing, or explicit installation step |
| Spectral DSP | `Source/Engine.h`, `Source/Engine.cpp` | changing STFT, delay history, feedback, morph, curve shift, latency, or buffer clearing |
| Host and state | `Source/PluginProcessor.h`, `Source/PluginProcessor.cpp` | changing parameters, automation, analyzer transfer, APVTS state, or migrations |
| Editor behavior | `Source/PluginEditor.h`, `Source/PluginEditor.cpp`, `Source/PointCurves.h` | changing layout, curve drawing, bank tools, keyboard undo, or visualization updates |
| Look and feel | `Source/EditorStyle.h`, `Source/FreeqyPalette.h` | changing controls, seven-segment values, bevels, bloom, or canonical colors |
| Frequency mapping | `Source/BucketDisplay.h` | changing visible bucket bounds, hit testing, or analyzer-to-bucket resampling |
| Visualization data | `Source/SpectrumAnalyzer.h`, `Source/SpectrogramHistory.h` | changing FFT capture, scrolling history, raster rows, or spectrogram colors |
| DSP regression | `Tests/Main.cpp` | proving audio behavior, latency, reconstruction, modulation, and real-time allocation rules |
| State/UI regression | `Tests/StateTests.cpp` | proving migrations, editor actions, display mapping, screenshots, and analyzer queues |
| Format smoke tests | `Tests/HostCheck.cpp`, `Tests/ClapCheck.cpp` | checking VST3 or native CLAP discovery, processing, state, ports, and metadata |

Useful shortcuts:

- A parameter change normally touches `createParameterLayout`/`readSettings` in
  `PluginProcessor.cpp`, its editor attachment, host checks, and state tests.
  Never recycle a retired parameter ID.
- Curve or bank changes cross `CurveBanks` in `Engine.h`, processor
  serialization/messaging, editor drawing, and both DSP/state regressions.
- Spectrogram changes flow from the engine/processor analyzer queues through
  `SpectrogramHistory` to editor rasterization; audio-thread code must only use
  bounded preallocated transfer.
- Run `rg -n "parameterId|symbol" Source Tests CMakeLists.txt` before editing a
  cross-cutting feature. Run the focused test first, then the full CTest suite.
- For layout work, use the screenshot cases in `StateTests`; for DSP work, keep
  the engine testable without constructing the editor.

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

## Native CLAP

Native CLAP is opt-in and uses the pinned
`clap-juce-extensions` revision recorded in `CMakeLists.txt`. A local checkout
may be supplied with `CLAP_JUCE_EXTENSIONS_DIR`; otherwise CMake fetches that
exact revision and its submodules.

```sh
make clap                         # Build and locally unquarantine the CLAP bundle
make install-clap                 # Copy to the CLAP folder, then clear quarantine

# Equivalent CMake configuration
.deps/venv/bin/cmake -S . -B build-clap \
  -DJUCE_DIR="$PWD/.deps/JUCE" \
  -DSPECTRAL_BUILD_CLAP=ON \
  -DCLAP_JUCE_EXTENSIONS_DIR="$PWD/.deps/clap-juce-extensions" \
  -DCMAKE_BUILD_TYPE=Release
.deps/venv/bin/cmake --build build-clap --target FreeqyDelay_CLAP_LocalReady -j 4
```

The output is
`build-clap/FreeqyDelay_artefacts/Release/CLAP/FREEQY DELAY.clap`.
The CLAP ID is `com.stationsound.freeqydelay`; parameter IDs and the versioned
JUCE state payload are shared with VST3.

WebCLAP is not the macOS CLAP bundle renamed as `.wclap`: it requires a WASM
CLAP module and a browser-compatible editor. The current JUCE/Cocoa processor
target cannot be compiled for WASI as-is, so a WebCLAP adapter and portable DSP
boundary remain a separate implementation task. No WebCLAP artifact is claimed
by the current build.

## Makefile, system installation, and AAX

The root Makefile builds into its own `build-system` tree, leaving other build
trees independent. Release VST3 is the default; native CLAP and AAX are opt-in.
AAX requires explicit `AAX=1`, even when an SDK is present. With AAX enabled,
a single `aax-sdk*/Interfaces/AAX.h` folder supplies the SDK path automatically.
Explicit `AAX_SDK_PATH` overrides discovery.

```sh
make                          # Build VST3 only
make install                  # Build, then install VST3 system-wide
make install-vst3             # Build/install only VST3
make install CLAP=1           # Build/install VST3 and native CLAP
make aax AAX=1                 # Build AAX with JUCE's bundled SDK if none is detected
make install AAX=1 AAX_SDK_PATH="/absolute/path/to/AAX_SDK"
```

VST3 installs to `/Library/Audio/Plug-Ins/VST3`, CLAP to
`/Library/Audio/Plug-Ins/CLAP`, and AAX to
`/Library/Application Support/Avid/Audio/Plug-Ins`. Close hosts before replacing
loaded bundles. After copying VST3 or CLAP, its install component removes
`com.apple.quarantine` so local hosts can load it. Installation requires
write access to those folders. If denied,
run the install steps with administrator privileges **after** the ordinary build:

```sh
sudo .deps/venv/bin/cmake --install build-system --config Release --component VST3
sudo .deps/venv/bin/cmake --install build-system --config Release --component CLAP
sudo .deps/venv/bin/cmake --install build-system --config Release --component AAX
```

CLAP and AAX components exist only when their formats were enabled during
configuration. The direct VST3 and CLAP install commands also perform post-copy
quarantine removal. Do not run the build itself with sudo. Override `BUILD_DIR`,
`CONFIG`, `JOBS`, `VST3_DIR`, `CLAP_DIR`, and `AAX_DIR` as needed. Example local staging:

```sh
make install CLAP=1 VST3_DIR="$PWD/build-stage/VST3" \
  CLAP_DIR="$PWD/build-stage/CLAP" AAX_DIR="$PWD/build-stage/AAX"
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
new-control recall, old-state migration and malformed data. ClapCheck stages and
loads the native CLAP, verifies Station Sound metadata/stereo ports, processes
audio, and round-trips state. These are automated format checks, not commercial-DAW
certification or listening acceptance.
Local bundles are ad-hoc signed; distribution notarization is deferred.
See AGENTS.md for DSP invariants and maintenance instructions.
