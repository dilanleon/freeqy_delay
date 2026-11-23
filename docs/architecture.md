# DSP architecture

Authoritative signal-flow, STFT, bank, delay, modulation, and real-time requirements.

## Identity and signal flow

C++20 JUCE audio effect, macOS VST3, native CLAP and Standalone development targets.
The host-facing product name and build target are FREEQY DELAY / FreeqyDelay; the manufacturer
name is Station Sound. Keep the existing bundle and four-character identities stable.
Host state uses the `FreeqyDelay` APVTS root. Payloads saved under the previous root are rejected.
Independent L/R curves set delay, feedback and level across frequency.
Input trim precedes the dry/wet split; output trim follows linear mixing.

## STFT, latency and real-time engine

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

## Curve banks and morph

One to four active banks each store all six stereo curves; default four.
Bank selection edits storage without changing the audible morph position.
curveMorph (0–100%) blends adjacent active banks in sequence.
Four-bank anchors: 0, 33⅓, 66⅔, 100 percent; one bank ignores morph.
Morph smooths over 20 ms; evaluate only the active FFT bins each hop.
Solid markers show the edit bank; dashed caps preview the target morph.
No y-axis legend is shown; keep grid lines and the 20 Hz–20 kHz x-axis.
Morph modulation uses Depth (0–100%, default 0%) and Rate (0–10 Hz, default .3).
The stereoMorph direction toggle selects same or opposite L/R offsets; Depth zero turns it off.
Wrap both offsets modulo 100; retain the manual 100% endpoint at zero depth; zero rate holds
+Depth left and either +Depth or -Depth right according to direction.
Smooth direction/depth/rate over 20 ms; run modulation with the editor closed.
Publish live positions through atomics for the GUI; never read GUI state in DSP.
Undo/redo snapshots curves, all banks and active count.
Inactive banks retain data when the count is reduced.

## Delay, curve shift and clearing

Delay curve is a fraction of Max Delay; feedback and wet level are 0–1.
At exactly 100% feedback loops do not decay; report infinite tail if a stored
active bank can reach unity feedback. Clear Buffer still removes the tail.
Feedback reads precede writes with a minimum one-hop delay.
Phase-align complex frames before interpolating fractional read positions.
Glide delay over 50 ms; gains, feedback and morph smooth over 20 ms.
Stereo Curve Shift moves all three maps along the frequency axis, never audio bins.
Amount is 0–1000 Hz; rate is 0–10 Hz (default .3).
The Amount knob uses a 20 Hz offset logarithmic scale with an exact zero; its host parameter
normalization remains unchanged.
Modulation follows a symmetric asinh-shaped logarithmic mapping of the sinusoidal LFO, with a
continuous center slope and +Amount at its positive peak; right offset is the negative of left.
Positive offset moves shapes up: read the saved map at bin frequency minus offset.
Pin the map at the first and last visible buckets (20 Hz and 20 kHz), interpolating curve
values to moving joins based on 75 Hz and 19 kHz, including across shifted-out regions. Fade
the interpolation in near zero shift to preserve the original curve continuously. Frequencies
outside the visible range retain their original curve values. Interpolate reference points and banks;
hold lookups outside DC/Nyquist.
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
