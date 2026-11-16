# Visualization pipeline

Authoritative display-cell, analyzer, queue, history, and palette requirements.

## Visualization pipeline

The editor combines all three controls with input and delay-return activity in STFT bucket bars.
Display the selected STFT's full positive-bin count as logical cells across 20 Hz–20 kHz.
Keep those cell bounds and count sample-rate independent; resample analyzer and saved-curve
values at physical frequencies so 44.1–192 kHz all maximize the visible bucket count.
All controls use bright horizontal top markers with volume-driven shading below.
The selected curve also shows a dim opposite-channel line and shading when the channels differ.
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
On resume, discard queued frames older than 150 ms relative to the latest audio timestamp.
Store both channels in a GUI-only ring; channel switches/resizes retain history.
Cache raster rows and update only dirty rows; all history allocation is off audio.
Hold a missing analyzer frame for at most 150 ms, then scroll blank rows.
Clear Buffer clears visual history and queued pre-clear frames; history is not serialized and
starts empty on open. Speed changes also discard frames captured for the previous time scale.
Tag queued spectra with a display generation captured at audio-block start; visual resets invalidate
older generations without resetting the SPSC queue from the producer thread.
Do not enqueue analyzer frames during the wet clear fade; resume after the fade completes.
Meter the delay return after level/feedback/clear fade, before dry/wet and output trim. Thus stored tails remain visible with dry mix selected.
Both sources use stereo 2048-point analysis; parameter markers overlay the history.
Never apply level/trim again in the GUI; the return already includes wet gain.
Analyzer measures L/R independently; smooth live input but retain raw wet frames in history.
The development FFT/sine diagnostic intentionally retains input metering.
The frequency axis is logarithmic, 20 Hz–20 kHz; DSP covers the full spectrum.
