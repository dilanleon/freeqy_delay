# FREEQY DELAY

FREEQY DELAY is a C++20 JUCE spectral delay for macOS. It builds as VST3 and Standalone,
with native CLAP and AAX Native as opt-in formats. Station Sound is the manufacturer.

Each stereo channel has independent **Delay**, **Feedback**, and **Level** curves across
a logarithmic 20 Hz–20 kHz display. Up to four banks hold complete sets of curves. Morph
blends adjacent active banks without changing the bank selected for editing.

## Build and test

Requirements: macOS, Apple command line developer tools, and CMake 3.22 or newer.
`CMakeLists.txt` pins JUCE 9.0.5 by commit. Set `JUCE_DIR` to a local JUCE checkout to
avoid a fetch.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

For Release, use `build-release` and `-DCMAKE_BUILD_TYPE=Release`. This workspace may
have `.deps/venv/bin/cmake` and `.deps/JUCE`; substitute that CMake executable and pass
`-DJUCE_DIR="$PWD/.deps/JUCE"` when available. Ordinary CMake builds leave plugin
bundles in the build tree and do not install them.

The root Makefile uses a separate `build-system` tree. `make` builds Release VST3 only.
`make clap` builds native CLAP; `make aax AAX=1` builds AAX Native. See [Formats and
installation](#formats-and-installation) before installing a bundle.

## Use the editor

The editor opens at 1120 × 800 and can shrink to 960 × 700. Its main graph combines the
selected curve with a dark live input FFT and a scrolling wet-return spectrogram. Bright
horizontal caps mark curve values; dashed caps show the current shifted and morphed
target. A dim line shows the other channel when its selected curve differs.

| Area | Controls | What they do |
| --- | --- | --- |
| Top bar | Presets; Draw / Points | Load the Default preset or choose how to edit a curve. |
| Above graph | Bank 1–4; Banks − / + | Select an edit bank or change the active count. Disabled banks retain their shapes. |
| Below graph | Delay / Level / Feedback; Scroll − / +; Resolution − / +; Link; L/R; Reset; Clear Buffer | Select a curve and channel, adjust the display and STFT, link edits, reset the selected curve, or clear the delay tail. |
| Bottom strip | Morph / Depth / Rate and direction; Max Delay; Curve Shift Depth / Rate; Wet; IN / OUT | Set bank morph, delay scale, stereo curve movement, mix, and trims. |

**Draw** paints entire display buckets on the saved curve grid. **Points** lets you move
hollow anchors, drag solid segment midpoint handles vertically to bend a segment, and
right-click an interior anchor to remove it. The first switch to Points samples the
existing drawn curve into point anchors. Point coordinates follow the logarithmic
display axis; adaptive sampling prioritizes narrow peaks within the 64-point limit.
**Link** mirrors edits to the other channel; enabling it copies the
selected channel's curves. Command-Z undoes one drawing gesture, reset, or bank-count
change; Shift-Command-Z redoes it. There are no visible undo/redo buttons.

**Presets → Default** sets all curves in all banks to Delay 25%, Feedback 35%, and Level
100%, enables one bank, and selects Bank 1. It sets Max Delay to 1 s, Resolution to 1024
samples, Wet to 50%, and all other parameters to zero. The initial state without loading
this preset has four active banks and 0.3 Hz defaults for the two modulation rates.

Max Delay displays milliseconds below 1000 ms and seconds from 1 s upward. A bare typed
number uses the displayed unit; an explicit `ms` or `s` suffix overrides it. The Morph,
Depth, and Rate readouts support vertical dragging even though they show only numbers.

## Sound and display behavior

### Curves, banks, and modulation

The delay curve is a fraction of **Max Delay** (1–10,000 ms). Feedback and wet level
range from 0–100%. At exactly 100% feedback, stored repeats can persist until **Clear
Buffer**. Input trim precedes the dry/wet split; output trim follows mixing. The
wet-return meter is taken after the curve's level and the clear fade, before Wet and
output trim, so a stored tail remains visible at 0% Wet.

**Morph** moves through the active banks in order. With four banks, their anchors are
0%, 33⅓%, 66⅔%, and 100%; one bank ignores Morph. Morph **Depth** (0–100%) and **Rate**
(0–10 Hz) modulate the left and right positions. The arrow toggle selects same or
opposite direction. Positions wrap around the 0–100% range; zero rate holds an offset.
Depth zero disables modulation. These controls run with the editor closed and are
automatable.

**Curve Shift Depth** (0–1000 Hz) moves all three curve maps along the frequency axis,
with opposite offsets for left and right. **Rate** (0–10 Hz) sets the motion; zero rate
holds left up and right down. Mono uses the left offset. This changes which curve value
treats a bin, not the bin's audio frequency. For example, a level peak at 1 kHz with a
held 100 Hz shift appears at 1.1 kHz on the left and 900 Hz on the right. The 20 Hz and
20 kHz curve endpoints stay pinned, and stored bank shapes do not change.

### Resolution, latency, and clearing

**Resolution** selects an STFT size of 16, 32, 64, 128, 256, 512, 1024, or 2048 samples;
the default is 1024. Each uses an N/8 hop. The display always shows the selected size's
full positive-bin count as logical cells from 20 Hz to 20 kHz, independent of sample
rate. The 1025-point saved curves and separate 2048-point analyzers are resampled by
physical frequency for display. DSP covers the full spectrum.

Host latency stays at **2048 samples** at every STFT size. A size change fades the wet
signal, discards its tail, and retains dry alignment and bank data. **Clear Buffer**
fades wet down and up over 5 ms each while clearing stored delay audio. It preserves dry
audio, controls, curves, and modulation phase. Clear Buffer is a momentary command, not
an automatable or saved parameter.

### Spectrogram

The graph places the newest wet-return activity at the top. **Scroll** chooses 0.5×, 1×,
2×, or 4×, showing 12, 6, 3, or 1.5 seconds across 180 rows. The default is 1×. Faster
settings give taps more vertical separation. The 2048-point stereo analyzers emit frames
every 256 samples; the editor drains timestamped frames at 30 Hz and holds a missing
frame for at most 150 ms. Ink, indigo, blue, and cyan show increasing return level.
Switching channels or resizing retains the editor's visual history. Changing Scroll
speed restarts that history without changing the sound. History starts empty when the
editor opens and is never saved. After a GUI stall, frames older than 150 ms relative
to the latest audio timestamp are dropped. Clear Buffer also invalidates queued frames
captured before the clear request.

### Saved state and compatibility

The current state schema is version 4. It saves four stereo curve banks, the active bank
count, edit selection, point-mode data, and display speed. Delay history and visual
history are not serialized. Versions 1–3 remain readable: version 1 single curves are
copied to all banks and morph resets to zero; versions 2 and 3 retain their banks. Older
states default new controls, and retired copy/chorus parameters are not reused. The
internal APVTS root remains `SpectralDelay` for saved-state compatibility.

## Formats and installation

| Format | Enable/build | Local output in a Release CMake tree |
| --- | --- | --- |
| VST3 | Built by default | `FreeqyDelay_artefacts/Release/VST3/FREEQY DELAY.vst3` |
| Standalone | Built by default with CMake | `FreeqyDelay_artefacts/Release/Standalone/FREEQY DELAY.app` |
| Native CLAP | `make clap` or `-DSPECTRAL_BUILD_CLAP=ON` | `FreeqyDelay_artefacts/Release/CLAP/FREEQY DELAY.clap` |
| AAX Native | `make aax AAX=1` or `-DSPECTRAL_BUILD_AAX=ON` | `FreeqyDelay_artefacts/Release/AAX/FREEQY DELAY.aaxplugin` |

Native CLAP uses the `clap-juce-extensions` commit pinned in `CMakeLists.txt`. Supply
`CLAP_JUCE_EXTENSIONS_DIR` for a local checkout or allow CMake to fetch it. `make clap`
clears quarantine from its local bundle after packaging. The CLAP ID is
`com.stationsound.freeqydelay`; its parameter IDs and versioned state payload are shared
with VST3.

The Makefile installs only when explicitly requested:

```sh
make install-vst3
make install-clap
make install AAX=1
make install CLAP=1 VST3_DIR="$PWD/build-stage/VST3" CLAP_DIR="$PWD/build-stage/CLAP"
```

Default destinations are `/Library/Audio/Plug-Ins/VST3`, `/Library/Audio/Plug-Ins/CLAP`,
and `/Library/Application Support/Avid/Audio/Plug-Ins`. Override `VST3_DIR`, `CLAP_DIR`,
or `AAX_DIR` for staging. `make install` builds and installs enabled formats; `CLAP=1`
and `AAX=1` opt those formats in. The VST3 and CLAP install components copy first, then
clear quarantine from the installed bundle and fail if that step fails. Close hosts
before replacing a loaded plugin.

If the system destination requires administrator access, build as your normal user, then
run only the install component with elevated privileges, for example:

```sh
sudo cmake --install build-system --config Release --component VST3
```

The AAX target uses JUCE's bundled SDK unless `AAX_SDK_PATH` selects an external SDK;
the Makefile also detects a single local `aax-sdk*/Interfaces/AAX.h` folder. AAX
compilation is separate from PACE signing and Pro Tools acceptance. Retail Pro Tools
requires PACE signing. The current native macOS CLAP bundle is not WebCLAP; a WASM
adapter and browser UI would be separate targets. No `.wclap` artifact is built.

## Development map

Read [AGENTS.md](AGENTS.md) for the project's detailed behavior, compatibility, and
real-time constraints. Use this map to find the implementation and relevant checks:

| Area | Implementation | Tests and checks |
| --- | --- | --- |
| Build, formats, install | `CMakeLists.txt`, `Makefile` | `Tests/HostCheck.cpp`, `Tests/ClapCheck.cpp` |
| STFT, delay history, feedback, morph, curve shift, latency | `Source/Engine.h`, `Source/Engine.cpp` | `Tests/Main.cpp` |
| Host parameters, state versions, analyzer queues | `Source/PluginProcessor.h`, `Source/PluginProcessor.cpp` | `Tests/StateTests.cpp`, format checks |
| Editor layout, drawing, points, undo, keyboard commands | `Source/PluginEditor.h`, `Source/PluginEditor.cpp`, `Source/PointCurves.h` | `Tests/StateTests.cpp` |
| Native controls and palette | `Source/EditorStyle.h`, `Source/FreeqyPalette.h` | editor rendering cases in `Tests/StateTests.cpp` |
| Display cells, analyzer, scrolling history | `Source/BucketDisplay.h`, `Source/SpectrumAnalyzer.h`, `Source/SpectrogramHistory.h` | `Tests/StateTests.cpp` |

A parameter change usually spans `Processor::layout`, `Processor::readSettings`, editor
attachments, state migration, and host/state checks. Curve changes span `CurveBanks`,
processor snapshot transfer, editor drawing, and DSP/state checks. Spectrogram changes
span the analyzers, bounded processor queue, GUI history, and rasterization. Keep
allocation and locks off the audio path. Search a parameter ID or symbol across `Source`
and `Tests` before changing a cross-file behavior.

## Validation status and diagnostic

`ctest` runs DSP, VST3 host, and state/UI checks in a normal CMake tree. A CLAP-enabled
tree adds `clap_host`. The DSP suite covers all eight STFT sizes, 44.1–192 kHz,
reconstruction, mono/stereo, bank morph, curve shifting, feedback, clearing, size
transitions, and ordinary C++ audio-thread heap allocation. State/UI checks cover
migrations, display mapping, analyzer queues, editor controls, and rendering. These
automated checks do not establish commercial DAW listening acceptance. AAX Pro Tools
loading remains untested.

For the development FFT/sine route, configure a separate tree:

```sh
cmake -S . -B build-diagnostic -DCMAKE_BUILD_TYPE=Debug -DSPECTRAL_DIAGNOSTIC=ON
cmake --build build-diagnostic -j 4
ctest --test-dir build-diagnostic --output-on-failure
```

That route retains input FFT metering, replaces normal output with a 440 Hz, −18 dBFS
sine, and reports zero latency. Normal builds have no sine route. Local bundles are
ad-hoc signed; distribution signing, notarization, and installers remain separate work.
