# Red / Blue research and prototype scope — 0.3.0

## Evidence

Focusrite's [Red 3/7 brochure](https://fael-downloads-prod.focusrite.com/customer/prod/s3fs-public/downloads/red3and72.pdf)
describes a discrete balanced audio VCA, three sidechain VCAs coordinating separate
compression and limiting, and peak rather than averaged RMS response. It specifies
low distortion. This does not justify adding arbitrary heavy saturation as a
substitute for the circuit.

The [original hardware manual, page 8](https://fael-downloads-prod.focusrite.com/customer/prod/s3fs-public/downloads/redrangemanual4.pdf)
describes Auto release responding more slowly to stronger, longer compression.
The [Focusrite plugin manual](https://fael-downloads-prod.focusrite.com/customer/prod/downloads/red_plugin_suite_v2-pdf-en_1.pdf)
identifies a soft-knee response. That plugin is supporting behavioral evidence,
not a service schematic for the original hardware.

The indexed [dbx 160SL manufacturer page](https://dbxpro.com/pt/products/160sl)
describes paired proprietary V8 VCA modules, based on the Blackmer decilinear VCA.
The full dbx page returned HTTP 403 during this research. Neither a verified
160SL service schematic nor a Red 3 service schematic was obtained. A component
comparison, detector topology reconstruction, transformer model, or exact
hardware transfer/time-constant claim would therefore be premature.

| Question | Red 3 evidence | Blue plugin / 160SL distinction |
|---|---|---|
| Gain-control element | Proprietary discrete balanced VCA | 160SL uses proprietary V8 modules; our Blue DSP does not model those components |
| Detector | Hardware brochure specifies peak response | Blue uses power detection: 10 ms in ORIG, fast power averaging with manual attack |
| Recovery | Hardware Auto depends on signal level and duration | Blue Auto is our existing two-component behavioral recovery |
| Circuit accuracy | Schematics and measurements still needed | Existing Blue is not a calibrated 160SL emulation either |

## Implemented Red prototype

The new RED switch selects a separate linked peak-envelope compressor. The
rectified detector amplitude is smoothed before the static compression curve.
Blue smooths gain reduction in dB after its power detector. These paths produce
different transient and recovery behavior even with the same knob values.

Red soft knee is 9 dB (an explicit tuning choice, not a measured Red 3 value).
Hard knee remains available. Auto release varies from 0.1 to 2 seconds according
to a smoothed reduction history: charge 0.3 s, decay 1.2 s, full extension at
20 dB history. These are provisional coefficients. ORIG attack selects 10 ms
in Red, and ORIG release selects Red Auto; their literal legacy meaning remains
exclusive to Blue. Manual timings use the existing stepped controls, not a
reproduction of the hardware's continuous pots. All timings are envelope
coefficients, not a guaranteed time to reach a particular dB of reduction.

No invented transformer saturation, noise, or dedicated hardware limiter is
included. Zero latency, stereo linking, detector-only HPF, Mix and Bypass remain.
Both engines run continuously; the model switch crossfades gains over 40 ms.
The RED state is saved and automated. Missing model parameters explicitly load
BLUE, including when loading an older project after a Red preset.

The panel changes to brushed red metal with a circular cream needle meter and
Lindell branding. This is an original drawing inspired by the requested visual
style, not a copied Focusrite panel or logo.

## Validation and next calibration step

Portable tests check Blue sample-exact compatibility with 0.2, Red audible-output
difference, stereo/antiphase, bypass/dry/unity, history-dependent Auto and the
continuous-engine crossfade at four rates. Existing timing and legacy tests remain.
The Mac workflow builds both architectures and loads the actual VST3, exercises
Red audio/state and old-state migration, and renders both panel variants.

For genuine Red 3 matching, obtain its verified schematic (including VCA and
sidechain sheets), and measured static curves, tone-burst responses and harmonic
spectra from an identified hardware revision. Compare at matched gain reduction
and loudness; equal knob positions do not imply equal compression in these modes.
