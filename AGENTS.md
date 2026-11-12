# Spectral Delay

## Product
C++20 JUCE audio effect, macOS VST3 and Standalone development targets.
Independent L/R curves set delay, feedback, and level for 1025 FFT bins.
Input trim precedes the dry/wet split; output trim follows linear mixing.
The editor overlays three colored curves on a live input spectrum.
The visible frequency axis is logarithmic from 20 Hz to 21 kHz; processing
and saved curves still cover the full spectrum. Drawing affects visible bins.
Select a curve and channel to draw; linking copies the selected channel.
Curves are saved with projects, not exposed as thousands of parameters.
UI groups Curves & Banks, Delay & Mix, Chorus / Copies, and Input / Output.
Clear Buffer is a transient command: fade wet down/up over 5 ms each, invalidate
spectral history with epoch tags, and clear input/OLA/post taps on the audio thread.
Preserve dry latency, curves, banks, parameter smoothing and modulation phase.
One to four active banks each store all six stereo curves; default four.
Bank selection edits storage and never changes the audible morph position.
curveMorph (0–100%) linearly blends adjacent active banks in sequence.
Bank anchors are evenly spaced; with four: 0, 33⅓, 66⅔, 100 percent.
Morph is smoothed over 20 ms and blended at STFT hops without allocations.
Solid lines show the edit bank; dashed lines show the target morph preview.
Copy-to-next copies all six curves; undo/redo snapshots all banks and count.
Inactive banks retain data and return when the active count is increased.

## DSP contract
2048-point real STFT, hop 256, periodic Hann analysis and synthesis.
Normalize overlap-add by the sum of squared windows (3 at this overlap).
Preserve complex phase; DC and Nyquist must remain real.
Fixed algorithmic latency: 2048 samples, including dry and bypass paths.
Allocate history for 10.1 seconds plus interpolation padding in prepare.
Delay curve is a fraction of Max Delay; feedback is 0–0.99; level is 0–1.
Feedback reads precede writes and use a minimum one-hop delay.
Use bin-center phase alignment when interpolating spectral frames.
Glide delay targets over 50 ms; smooth gains and feedback over 20 ms.
Copies include the original plus 0–16 extra voices, normalized by weight.
Post copies operate on reconstructed wet audio outside feedback.
Spectral copies operate per bin and participate in feedback.
Spread is 0–100 ms; depth sweeps each fixed offset toward zero.
Voice LFO phases are staggered; right channel adds a quarter cycle.
No allocation, mutex, I/O, serialization, or GUI calls during processing.
Publish complete curve snapshots through bounded preallocated messaging.
Never serialize active delay history.

## Parameters
Stable IDs: mix, maxDelayMs, copies, spreadMs, depth, rateHz,
inputTrimDb, outputTrimDb, copyMode, curveMorph.
Defaults: mix .5; maxDelayMs 1000; copies 0; spreadMs 30;
depth 0; rateHz .3; both trims 0 dB; copyMode Post.
Curve defaults: delay .25; feedback .35; level 1, both channels.
curveMorph defaults to 0%; bank count and edit selection are saved, not automated.
Persist four banks of six 1025-value arrays plus editor settings.
State schema version 2; accept version 1 by copying its curves to all banks
and resetting morph/edit selection to zero. Clamp malformed data safely.
State bytes currently store native little-endian float curves on macOS.

## Build
Pinned JUCE 9.0.5, commit d60b7a78030661574a615b586d9325ba7a7fa7dd.
Requires CMake >=3.22 and Apple command line developer tools.
Local CMake: .deps/venv/bin/cmake (add .deps/venv/bin to PATH).
Configure: cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
Build: cmake --build build -j 4
Test: ctest --test-dir build --output-on-failure
Repeat with build-release and -DCMAKE_BUILD_TYPE=Release.
JUCE_DIR may point to a local JUCE checkout; otherwise FetchContent is used.
Artifacts remain under the build tree; no automatic plugin installation.

## Layout
Source/Engine.h and Engine.cpp: independently testable DSP.
Source/PluginProcessor.*: host buses, parameters, analyzer, state.
Source/PluginEditor.*: native controls and drawing.
Tests/: DSP, VST3 host, bank state and v1 migration regression checks.
Keep behavior, documentation, and tests aligned when editing.

## Incremental gates
1. Scaffold: Debug/Release build, standalone launch, VST3 instantiation.
2. Diagnostic: input FFT display and phase-continuous 440 Hz sine at -18 dBFS.
3. Reconstruction: exact 2048-sample latency, error below -90 dB.
4. Delay: timing, fractional phase, per-band routing, history wraparound.
5. Feedback/level: decay, causality, channel separation, finite tails.
6. Editor: curves, link/copy/reset/undo, controls and automation.
7. Copies: both placements, normalization, offset bounds, transitions.
8. State/host: recall, bypass, offline render, sample rates 44.1–192 kHz.
Build and run focused regressions before progressing when tools permit.
Retain diagnostic mode behind a build option, disabled for normal builds.
Do not claim unavailable host or listening tests passed.

## Status
Feature implementation covers milestones 1–8; remaining manual gates are below.
Debug and Release VST3/Standalone builds succeed.
Clear-tail, repeated-clear, dry-continuity and no-allocation checks pass
in both placements at 48/192 kHz; grouped editor renders were inspected.
Bank interpolation/audio automation, v2 state recall, v1 migration and
sanitization pass in Debug/Release; bank editor render visually inspected.
Editor now shows 20 Hz–21 kHz on a logarithmic axis; Debug checks and Release DSP checks pass.
After the user removed quarantine, Release VST3 host loading passes again.
The quarantine attribute is absent and the bundle signature verifies.
The diagnostic variant builds and its sine output passes a VST3-host check.
Standalone final and diagnostic interfaces launch; final editor visually inspected.
Automated VST3 scan, instantiation, audio, state recall and editor creation pass.
Reconstruction error: approximately -140 dB at 44.1, 48, 96 and 192 kHz.
Tests cover integer delay, echo decay, mute, bypass and fractional tone reads.
Fractional tone relative error is -31.9 dB; this is windowed interpolation.
Maximum-voice Release profile: approximately .043 CPU seconds/audio second
for one 48 kHz stereo run on this machine; not a worst-case deadline guarantee.
Ordinary C++ new/new[] allocations are checked during that processing case.
Pending: commercial-DAW project reopen/offline-render/automation acceptance,
live external-tone analyzer measurement, listening, exhaustive thread analysis,
and exhaustive long-tail stress coverage. Do not claim these passed.
A focused maximum-delay history-wrap regression is included.
Dependencies are available locally under .deps; network access may need escalation.

## Deferred
Tempo sync, file playback, variable FFT sizes, signing and installers.
No distribution or automatic installation is authorized by this workflow.

## Maintenance
Update this file when commands, interfaces, DSP assumptions or status change.
Keep tests separate from editor behavior and exercise irregular host blocks.
Measure maximum-voice CPU cost rather than assuming real-time suitability.
Treat spectral modulation as windowed processing, not exact waveform delay.
Preserve parameter IDs and version state before incompatible changes.
