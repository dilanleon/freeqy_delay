# FREEQY DELAY

## Product
C++20 JUCE audio effect, macOS VST3 and Standalone development targets.
The host-facing product name, bundle identity and build target are FREEQY DELAY / FreeqyDelay.
Keep the internal APVTS root `SpectralDelay` unchanged so existing saved parameter state remains readable.
Independent L/R curves set delay, feedback and level across frequency.
Input trim precedes the dry/wet split; output trim follows linear mixing.
The editor combines all three controls with input and delay-return activity in STFT bucket bars.
Display the selected STFT's full positive-bin count as logical cells across 20 Hz–20 kHz.
Keep those cell bounds and count sample-rate independent; resample analyzer and saved-curve
values at physical frequencies so 44.1–192 kHz all maximize the visible bucket count.
All controls use bright horizontal top markers with volume-driven shading below.
Rasterize to full pixels, merging shared pixels by maxima; never use faint subpixel caps.
Keep a dark live input FFT and a vertically scrolling wet-return spectrogram.
Scroll stepper: 0.5x/1x/2x/4x covers 12/6/3/1.5 s in 180 rows; default 1x.
Faster speeds increase temporal row resolution; newest at top, ink → indigo → blue → cyan.
Use the user-provided palette reference at
`/var/folders/48/q1mwvh057lq0xrm24g5wbny80000gn/T/TemporaryItems/NSIRD_screencaptureui_H4kgbN/Screenshot 2026-11-12 at 17.49.59.png`.
Canonical swatches: #F72585, #B5179E, #7209B7, #560BAD, #480CA8, #3A0CA3,
#3F37C9, #4361EE, #4895EF, #4CC9F0. Source/FreeqyPalette.h is authoritative.
Use violet/purple for surfaces, pink for active emphasis, and cyan for readable values.
Spectrogram anchors are ink, #3A0CA3, #4361EE and #4CC9F0 at 0/85/170/255.
Changing speed restarts visual history only; save display index spectrogramSpeed (not automation).
Both analyzers use a 256-sample hop; cache Hann windows, keep the FFT at 2048.
Transfer all timestamped hops through a bounded 128-slot preallocated SPSC queue.
Drain frames in order on the GUI, preserving sub-paint timing; GUI refresh stays 30 Hz.
Drop frames if the GUI stalls; never block audio. Re-anchor the clock after pauses.
Store both channels in a GUI-only ring; channel switches/resizes retain history.
Cache raster rows and update only dirty rows; all history allocation is off audio.
Hold a missing analyzer frame for at most 150 ms, then scroll blank rows.
Clear Buffer clears visual history and queued pre-clear frames; history is not serialized and
starts empty on open. Speed changes also discard frames captured for the previous time scale.
Meter the delay return after level/feedback/clear fade, before dry/wet and output trim. Thus stored tails remain visible with dry mix selected.
Both sources use stereo 2048-point analysis; parameter markers overlay the history.
Never apply level/trim again in the GUI; the return already includes wet gain.
Match the user sketch: four full-width bank tabs above the enlarged graph, Banks +/- at right.
Below the graph: Delay / Level / Feedback, Scroll +/-, Resolution +/-, link icon,
L/R, Reset, Clear Buffer. No visible copy or undo/redo controls.
The link toggle displays two disconnected circles when off and overlapping connected circles when on.
Bottom strip: numeric Morph/Depth/Rate with stereo arrow toggle, large Max Delay,
Curve Shift depth/rate knobs, Wet, then small stacked IN/OUT trims.
Morph, Depth and Rate use rotary vertical-drag interaction, but render only the seven-segment
value and unit—no knob body, position pointer, track or arc.
Inactive bank tabs are disabled; bank +/- clamps to 1–4 without deleting stored shapes.
Default editor 1120x800; minimum 960x700. Preserve spectrogram and bucket rendering.
Source/EditorStyle.h owns the native JUCE styling and choice steppers; no bitmap assets.
All slider value boxes use the EditorStyle seven-segment display. Units are larger bold pink text.
Unit indicators have no box, fill or outline; use a soft low-alpha bloom around the text.
Max Delay displays milliseconds below 1000 ms and seconds at/above 1 s. Bare typed values use
the currently displayed unit; explicit `ms` or `s` suffixes override it.
Rotary controls use the EditorStyle's layered recess, bevel and shaded cap.
Undo/redo remains available with Command-Z / Shift-Command-Z (one drawing gesture).
Morph targets use dashed caps; parameter values never interpolate visually between buckets.
Drawing updates whole bucket cells on the 1025-point saved grid, with undo/link.
Cancel an active drawing gesture if the automated STFT size changes.
Analyzer measures L/R independently; smooth live input but retain raw wet frames in history.
The development FFT/sine diagnostic intentionally retains input metering.
The frequency axis is logarithmic, 20 Hz–20 kHz; DSP covers the full spectrum.
Use the sketch’s continuous bordered layout and restrained dark palette.
The old multi-copy/chorus engine has been removed.

## Banks and morph
One to four active banks each store all six stereo curves; default four.
Bank selection edits storage without changing the audible morph position.
curveMorph (0–100%) blends adjacent active banks in sequence.
Four-bank anchors: 0, 33⅓, 66⅔, 100 percent; one bank ignores morph.
Morph smooths over 20 ms; evaluate only the active FFT bins each hop.
Solid markers show the edit bank; dashed caps preview the target morph.
No y-axis legend is shown; keep grid lines and the 20 Hz–20 kHz x-axis.
Stereo Morph (default off) uses independent Depth (0–100%, default 25%) and
Rate (0–10 Hz, default .3) to offset L/R morph positions oppositely around Morph.
Wrap both stereo offsets modulo 100; retain the manual 100% endpoint when off; rate zero holds +Depth left and -Depth right.
Smooth toggling/depth/rate over 20 ms; run modulation with the editor closed.
Publish live positions through atomics for the GUI; never read GUI state in DSP.
Undo/redo snapshots curves, all banks and active count.
Inactive banks retain data when the count is reduced.

## STFT and latency
STFT choices are 16, 32, 64, 128, 256, 512, 1024, 2048 samples; default 1024.
Each uses hop size N/8 and periodic Hann analysis/synthesis windows.
Normalize overlap-add by three: the sum of eight squared Hann windows.
Preserve complex coefficients, with real DC and Nyquist.
Positive-frequency bin counts range from 9 to 1025.
Curve storage remains 1025 points (relative to Nyquist), independent of FFT size.
Sample each bank at the active bin frequencies; never destructively resize curves.
Analyzer remains a separate 2048-point FFT for readable frequency display.
Fixed host latency is 2048 samples, the maximum STFT window.
Pad smaller windows to that latency; dry and bypass paths match exactly.
FFT-size changes fade the wet signal, clear its history, and switch at zero gain.
Dry latency and bank data remain intact; changing size deliberately discards tails.
Cache all eight FFT objects and allocate worst-case history during prepare.
History capacity supports ten seconds at every size, including the two-sample hop.
No allocation or host-latency change is allowed when selecting a size.

## Delay, shifting and clearing
Delay curve is a fraction of Max Delay; feedback and wet level are 0–1.
At exactly 100% feedback loops do not decay; report infinite tail if a stored
active bank can reach unity feedback. Clear Buffer still removes the tail.
Feedback reads precede writes with a minimum one-hop delay.
Phase-align complex frames before interpolating fractional read positions.
Glide delay over 50 ms; gains, feedback and morph smooth over 20 ms.
Stereo Curve Shift moves all three maps along the frequency axis, never audio bins.
Amount is 0–1000 Hz; rate is 0–10 Hz (default .3).
Instantaneous offsets: +Amount*cos(2π Rate t) left, negative right.
Positive offset moves shapes up: read the saved map at bin frequency minus offset.
Interpolate reference points and banks; hold endpoints outside DC/Nyquist.
Zero rate holds left-up/right-down; mono uses left. Smooth amount/rate and transitions into or
out of the zero-rate hold over 20 ms.
Flat maps keep their sound at every amount/rate; saved shapes are unchanged.
Publish live offsets atomically; dashed target caps preview shift plus bank morph.
Clear Buffer is a transient, unsaved command passed with an atomic flag.
Fade wet down/up over 5 ms each; invalidate spectral history with epoch tags.
Clear input, overlap-add, alignment buffers on the audio thread.
Preserve dry latency, curves, controls, smoothing and modulation phase.
No allocation, mutex, I/O, serialization or GUI access during audio processing.
Publish complete curve-bank snapshots through bounded preallocated messaging.
Never serialize active delay history.

## Parameters and compatibility
Stable current IDs: mix, maxDelayMs, inputTrimDb, outputTrimDb, curveMorph,
shiftHz, shiftRateHz, stftSize, stereoMorph, morphDepth, morphRateHz.
Defaults: mix .5, maxDelayMs 1000, trims 0 dB, morph 0%, shift 0 Hz,
shift rate .3 Hz, STFT choice index 6 (1024 samples).
Curve defaults: delay .25, feedback .35, level 1, both channels/all banks.
Bank count and edit selection are saved, not automated; new controls automate.
Schema version 4 saves four banks of six 1025-value float arrays and editor state.
Version 1: copy its single curves to all banks, morph/edit selection to zero.
Version 2: retain banks/morph. For both, default to shift off and STFT 1024.
Retire copies, spreadMs, depth, rateHz, copyMode; never reuse their IDs.
Version 3 keeps its shift settings. All older states default Stereo Morph off.
Accept versions 1–4, clamp invalid values, reject unknown versions safely.
State curve bytes are native little-endian floats on supported macOS targets.

## Build and layout
Pinned JUCE 9.0.5, commit d60b7a78030661574a615b586d9325ba7a7fa7dd.
The authoritative dependency revision is in CMakeLists.txt.
Requires CMake >=3.22 and Apple command line developer tools.
Local CMake executable: .deps/venv/bin/cmake.
Configure: cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
Build: cmake --build build -j 4
Test: ctest --test-dir build --output-on-failure
Repeat with build-release and -DCMAKE_BUILD_TYPE=Release.
JUCE_DIR can point to .deps/JUCE; otherwise CMake fetches the pinned dependency.
Ordinary CMake builds keep artifacts local. Root Makefile uses build-system.
make defaults to VST3 only; AAX remains disabled even if an SDK is present.
Explicit AAX=1 enables it; AAX_SDK_PATH selects an external SDK, otherwise
a single local SDK is detected or JUCE's bundled SDK is used.
SPECTRAL_BUILD_AAX enables the CMake format; AAX_SDK_PATH is optional.
make install explicitly copies enabled formats to system VST3/AAX folders.
Use make install-vst3 / install-aax for one format; never compile as root.
VST3_DIR and AAX_DIR override destinations; CMake installation can be staged.
PACE signing/Pro Tools Developer testing are separate from compilation.
Release VST3/AAX compilation and local staged installation were verified with
SDK 2.10.0; staged VST3 signature/host loading pass. AAX Pro Tools loading is untested.
Seal the VST3 bundle after manifest generation; do not alter PACE-signed AAX bundles.
Source/Engine.*: independently testable DSP; PluginProcessor.*: host/state.
Source/PluginEditor.*: native controls/drawing; Tests/: DSP/state/VST3 coverage.
SPECTRAL_DIAGNOSTIC=ON retains input FFT plus 440 Hz/-18 dBFS sine output.
Diagnostic latency is zero; the sine route is absent from normal controls.

## Validation and maintenance
Latest Debug/Release DSP, VST3-host and state suites pass.
FREEQY DELAY Release VST3 has a valid ad-hoc signature and its local quarantine attribute was cleared for host validation.
Spectrogram speed/row spacing, timestamp delivery, bounded overflow, state recall, chronology, ring wrap, stereo switching, resize and clearing tests pass.
Bucket boundary/hit-test, sample-rate-independent visible bounds, analyzer resampling,
dual-source level/mute/tail metering and stereo anti-phase checks pass.
Stereo morph modulation, disabled/single-bank cases, v4 recall and v3 migration pass.
All eight sizes, smoothed zero-rate holds, curve-axis shift direction and pitch preservation,
display-aware Max Delay parsing, queue resets, v1/v2 migration and diagnostic sine pass.
Default/minimum editor renders at coarse/fine FFT sizes were visually inspected. Commercial DAW listening
acceptance remains pending.
Require reconstruction below -90 dB at all eight sizes and 44.1–192 kHz.
Exercise mono/stereo, irregular blocks, bank morph, bin remapping and feedback.
Check shifted delay/feedback/level routing, edge clamping, modulation and unchanged audio frequencies.
Check size changes/clearing without C++ heap allocation and with continuous dry.
Check v1/v2 migration, v3 recall, sanitization and exposed VST3 automation.
Render editor at default/minimum sizes when layout changes; verify toolbar/control bounds and bank count buttons.
Do not claim commercial-DAW acceptance or listening tests passed unless performed.
Signing/notarization and installers are deferred; local Gatekeeper exceptions
may be needed. Revalidate a copied plugin rather than assuming the build is used.
Update this file when DSP assumptions, interfaces or commands change.
