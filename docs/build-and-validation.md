# Build and validation

Authoritative dependency, format, install, diagnostic, and validation requirements.

## Build and formats

Pinned JUCE 9.0.5, commit d60b7a78030661574a615b586d9325ba7a7fa7dd.
Native CLAP uses pinned clap-juce-extensions commit
6bde108329ff9fc24404b79b8b397261a75cf6a8 and is opt-in.
The authoritative dependency revision is in CMakeLists.txt.
Requires CMake >=3.22 and Apple command line developer tools.
Local CMake executable: .deps/venv/bin/cmake.
Configure: cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
Build: cmake --build build -j 4
Test: ctest --test-dir build --output-on-failure
Repeat with build-release and -DCMAKE_BUILD_TYPE=Release.
JUCE_DIR can point to .deps/JUCE; otherwise CMake fetches the pinned dependency.
Ordinary CMake builds keep artifacts local. Root Makefile uses build-system.
make defaults to VST3 only; CLAP and AAX remain opt-in.
Use make clap / install-clap or CLAP=1; CLAP_DIR overrides its install folder.
The CLAP build target clears quarantine from its local bundle after packaging.
The CLAP install component copies first, then clears quarantine from that copy.
Explicit AAX=1 enables it; AAX_SDK_PATH selects an external SDK, otherwise
a single local SDK is detected or JUCE's bundled SDK is used.
SPECTRAL_BUILD_AAX enables the CMake format; AAX_SDK_PATH is optional.
make install explicitly copies enabled formats to system VST3/CLAP/AAX folders.
VST3 and CLAP install components then clear `com.apple.quarantine` from the copied
bundle and fail if that step fails. Use make install-vst3 / install-clap / install-aax;
never compile as root. VST3_DIR, CLAP_DIR and AAX_DIR override destinations.
PACE signing/Pro Tools Developer testing are separate from compilation.
Release VST3/AAX compilation and local staged installation were verified with
SDK 2.10.0; staged VST3 signature/host loading pass. AAX Pro Tools loading is untested.
Seal the VST3 bundle after manifest generation; do not alter PACE-signed AAX bundles.
WebCLAP needs a WASM CLAP adapter and browser UI; do not label the native macOS
bundle as WebCLAP or claim a .wclap artifact until that separate target exists.
SPECTRAL_DIAGNOSTIC=ON retains input FFT plus 440 Hz/-18 dBFS sine output.
Diagnostic latency is zero; the sine route is absent from normal controls.

## Ownership and cross-file workflows

Source/Engine.*: independently testable DSP; PluginProcessor.*: host/state.
Source/PluginEditor.*: native controls/drawing; Tests/: DSP/state/format coverage.
README.md's “Development map” is the navigation index;
update that section when ownership or cross-file workflows change.

## Validation status

Latest Debug/Release DSP, VST3-host and state suites pass.
Native CLAP Release compiles, is ad-hoc signed, and its staged copy/install
quarantine cleanup passes. Its staged smoke test scans, instantiates, processes,
and round-trips state; commercial CLAP-host acceptance remains pending.
FREEQY DELAY Release VST3 has a valid ad-hoc signature and its local quarantine attribute was cleared for host validation.
Spectrogram speed/row spacing, timestamp delivery, bounded overflow, state recall, chronology, ring wrap, stereo switching, resize and clearing tests pass.
Adaptive point conversion, STFT-change gesture cancellation, stalled-queue filtering and
pre-clear frame rejection tests pass.
Bucket boundary/hit-test, sample-rate-independent visible bounds, analyzer resampling,
dual-source level/mute/tail metering and stereo anti-phase checks pass.
Stereo morph modulation, disabled/single-bank cases and v4 recall pass.
All eight sizes, smoothed zero-rate holds, curve-axis shift direction and pitch preservation,
display-aware Max Delay parsing, queue resets, legacy version rejection and diagnostic sine pass.
Default/minimum editor renders at coarse/fine FFT sizes were visually inspected. Commercial DAW listening
acceptance remains pending.

## Required validation and maintenance

Always run the tests and build after making a change. Develop test-first: write a test for the
expected behavior before implementing it. Do not edit a failing test unless the expected behavior
has changed.

Require reconstruction below -90 dB at all eight sizes and 44.1–192 kHz.
Exercise mono/stereo, irregular blocks, bank morph, bin remapping and feedback.
Check shifted delay/feedback/level routing, edge clamping, modulation and unchanged audio frequencies.
Check size changes/clearing without C++ heap allocation and with continuous dry.
Check legacy version rejection, v4 recall, sanitization and exposed VST3 automation.
Render editor at default/minimum sizes when layout changes; verify toolbar/control bounds and bank count buttons.
Do not claim commercial-DAW acceptance or listening tests passed unless performed.
Signing/notarization and installers are deferred; local Gatekeeper exceptions
may be needed. Revalidate a copied plugin rather than assuming the build is used.
Update this file when DSP assumptions, interfaces or commands change.
