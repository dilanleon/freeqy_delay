# Editor design

Authoritative layout, interaction, drawing, point-mode, and preset requirements.

## Editor, layout and interaction

Match the user sketch: four full-width bank tabs above the enlarged graph, Banks +/- at right.
Below the graph: Delay / Level / Feedback, Scroll +/-, Resolution +/-, link icon,
L/R, Reset, Clear Buffer. No visible copy or undo/redo controls.
The link toggle displays two disconnected circles when off and overlapping connected circles when on.
The top-bar Presets menu separates Plug-in and Selected Bank categories. Each submenu has
Default, Save..., and Load... actions; the latter two use a native file dialog.
Plug-in Default resets curve values (Delay 25%, Feedback 35%, Level 100%), enables one active
bank, and sets Max Delay 1 s, Resolution 1024 samples, Wet 50%, and other parameters to zero.
Selected Bank Default sets only that bank's curves to those curve values; it preserves global
parameters, active bank count, selection, and all other banks.
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
Point mode stores per-bank/channel/parameter points and segment curvature as optional v4 editor
state, renders them onto the same saved grid, and includes them in undo/link/reset. Point
coordinates follow the logarithmic visible frequency axis; drag solid segment midpoint handles
vertically to change curvature, while hollow points move the anchors. Flat segments ignore handle
drags. Existing drawn curves are adaptively sampled when first converted to points; preserve
narrow peaks within the 64-point editor-state capacity. Right-click removes interior points.
Cancel an active drawing gesture if the automated STFT size changes: restore the pre-gesture
curves and points, and do not add an undo entry.
Use the sketch’s continuous bordered layout and restrained dark palette.
