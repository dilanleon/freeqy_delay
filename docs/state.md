# Parameters and state

Authoritative parameter IDs, defaults, versioning, and recall requirements.

## Parameters and saved-state compatibility

Stable current IDs: mix, maxDelayMs, inputTrimDb, outputTrimDb, curveMorph,
shiftHz, shiftRateHz, stftSize, stereoMorph, morphDepth, morphRateHz.
Defaults: mix .5, maxDelayMs 1000, trims 0 dB, morph 0%, shift 0 Hz,
shift rate .3 Hz, morph depth 0%, STFT choice index 6 (1024 samples).
Curve defaults: delay .25, feedback .35, level 1, both channels/all banks.
Bank count and edit selection are saved, not automated; new controls automate.
Schema version 4 saves four banks of six 1025-value float arrays and editor state.
Point metadata carries its sample rate. On a rate mismatch, retain the saved grid and
regenerate points on demand; untagged older metadata is assumed to use 48 kHz.
Only the `FreeqyDelay` root with version 4 is accepted. Other roots and versions are rejected without changing the current state.
Retire copies, spreadMs, depth, rateHz, copyMode; never reuse their IDs.
Clamp invalid values in version 4 states.
State curve bytes are native little-endian floats on supported macOS targets.
The old multi-copy/chorus engine has been removed.

## Preset files

Plug-in `.freeqy` files contain the complete version 4 `FreeqyDelay` host state, including
parameters, all four stored banks, point editor data, bank count and editor selection. A
plug-in load replaces the current state. Selected Bank `.freeqybank` files have a separate
`FreeqyBankPreset` root with version 2 and contain one bank's six curves and point editor data.
Loading one applies it to the currently selected bank, preserving global parameters, bank
count, selection and other banks. Preset files have distinct types; malformed, unsupported or
truncated files, invalid curve or point values, and incomplete or invalid parameter sets
are rejected before state is changed. File I/O runs from the editor, never
the audio thread. Preset files use the project's native little-endian float layout on the
supported macOS target.
