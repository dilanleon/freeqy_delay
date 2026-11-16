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
Only version 4 is accepted. Earlier and unknown versions are rejected without changing the current state.
Retire copies, spreadMs, depth, rateHz, copyMode; never reuse their IDs.
Clamp invalid values in version 4 states.
State curve bytes are native little-endian floats on supported macOS targets.
The old multi-copy/chorus engine has been removed.
