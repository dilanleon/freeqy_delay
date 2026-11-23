# Build and validation

Authoritative dependency, format, install, and validation requirements.

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

## Internal installers

Run **Installer builds and releases** from the repository's Actions tab. Each native
job builds and tests Release and uploads its installer as a 14-day workflow artifact.
The platform input defaults to all three and also supports rerunning a single platform.
Manual runs produce artifacts only. A push to `main`, including a merge, publishes a
numbered GitHub release with all three installers after every platform passes. Releases
follow the repository's visibility. For validation results, check the workflow run.

| CI build | Release asset | Install destinations |
| --- | --- | --- |
| macOS 13+ target, universal arm64/x86_64 | `FREEQY-DELAY-macOS.pkg` | System `/Library/Audio/Plug-Ins/{VST3,CLAP,Components}` |
| Windows 11 x64 target, Visual Studio 2026 and Inno Setup 6 | `FREEQY-DELAY-Windows-x64-Setup.exe` | System `%CommonProgramFiles%\VST3` and `%CommonProgramFiles%\CLAP` |
| Ubuntu 24.04 baseline, x86-64, JUCE Linux dependencies | `FREEQY-DELAY-Linux-x86_64.tar.gz` | Per-user `~/.vst3` and `~/.clap` |

Each installer lets the user select formats. Release builds keep the complete VST3
bundle. Audio Units v2 is opt-in with `SPECTRAL_BUILD_AU=ON` and macOS-only. Linux
users extract the archive and run `./install.sh` in a terminal as their own user.
Use `--vst3`, `--clap`, or both for unattended Linux installations.

For a local build, use the matching configure options and dependencies in
`.github/workflows/internal-installers.yml`. Build `InstallerPayload` after the
normal build and pass `build/installer-payload/Release` and an output directory to the
platform's `package.sh` or `package.ps1`. The installer scripts in `packaging/` test
one-format and default installs and verify that deselecting a format during an upgrade
leaves its existing files in place. Run system installation tests only on disposable
test systems. Linux tests use a temporary home. The suites run VST3 and CLAP host
scans from installed paths and macOS also runs `auval -v aufx Frqy Dleo`.

The macOS package is unsigned. The release notes and Station Website disclose this.
Before opening the repository to the public, confirm the JUCE and project source
licensing and clear private test artifacts. Check downloads signed out before merging
the Station Website download links. The website links directly to GitHub release
assets, which serve the installer bytes.

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
Adaptive point conversion, sample-rate-aware point recall, host-recall undo invalidation,
STFT-change gesture cancellation, stalled-queue filtering and
pre-clear frame rejection tests pass.
Bucket boundary/hit-test, sample-rate-independent visible bounds, analyzer resampling,
dual-source level/mute/tail metering and stereo anti-phase checks pass.
Stereo morph modulation, disabled/single-bank cases and v4 recall pass.
All eight sizes, smoothed zero-rate holds, curve-axis shift direction and pitch preservation,
display-aware Max Delay parsing, queue resets, and legacy version rejection pass.
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
Public macOS signing and notarization are deferred. Revalidate an installed plugin
rather than assuming the build artifact is used.
Update this file when DSP assumptions, interfaces or commands change.
