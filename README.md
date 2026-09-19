# Lindell Rack Compressor — test build 0.1.1

A native JUCE VST3 compressor prototype for REAPER on macOS. Original vector-drawn 19-inch rack-style panel branded **Lindell Plugins**. This is a behavioral compressor inspired by the supplied 160XT schematic and 160A front panel, not a component-accurate or measured hardware emulation. No original logo or photograph is embedded.

## Controls

- Threshold: -60 to 0 dBFS; default -18 dBFS.
- Ratio: 1:1 to 20:1; default 4:1. Negative ratios and infinity are not included in this prototype.
- Output gain: -20 to +20 dB, applied to the wet signal before Mix.
- Sidechain HPF: 30–300 Hz, first-order 6 dB/octave, detector only. Default 30 Hz.
- Mix: 0% untouched input to 100% compressed output. Linear, phase-aligned blending; zero reported latency.
- Soft knee: 6 dB knee when on, hard knee when off.
- Bypass: returns to dry input with a brief smooth transition.
- Knobs: vertical drag, Shift for finer adjustment, double-click reset, editable numeric values.
- Blue brushed-aluminium panel, silver knobs and illuminated analog-style needle meter. Select Input, Output or Gain Reduction; input/output display uses smoothed RMS power, with 0 VU referenced to -18 dBFS (not a certified VU instrument). Independent output peak lamp.

Mono and stereo support. Stereo uses linked maximum channel power, so opposite-polarity channels cannot cancel the detector. RMS detector and program-dependent attack/release are initial tuning choices, not measured 160XT time constants. No clipping or analog saturation is added. This is a compressor, not a brickwall peak limiter. All controls are host-automatable and saved in DAW state.

## Mac cloud build

1. Put the contents of this folder at the root of a GitHub repository, including `.github/workflows/build-mac-vst3.yml`. Use a private repository for development.
2. Open **Actions → Build Mac VST3 → Run workflow** on the default branch.
3. A successful run produces **Lindell-Mac-VST3-[run number]**. Download that artifact, unzip it, then unzip `Lindell-Mac-VST3.zip`.
4. Follow `INSTALL-PLUGIN.txt`. The actual plugin is **Lindell Rack Compressor.vst3** in `release/`.

The workflow compiles a universal arm64/x86_64 bundle with macOS 12 minimum, performs portable DSP tests, checks both architectures, ad-hoc signs the bundle, loads the actual VST3 on the runner, tests dry pass-through/compression/state, and instantiates/renders the editor. It packages the editor screenshot with the plugin.

The workflow must complete successfully before a Mac VST3 exists. This source package does not contain a compiled Mac plugin. It has not yet been tested in REAPER or on an Intel Mac. The smoke host runs natively on the selected GitHub runner, not separately on both architectures.

## Developer notes

JUCE 8.0.15 is pinned in the workflow. Ensure your JUCE licensing covers your intended use and distribution. Development manufacturer/product identifiers must be checked against Lindell's allocated production IDs before commercial release. No AAX build or license activation is included.

Portable tests without JUCE:

    g++ -std=c++17 -O2 Tests/CoreTests.cpp -o compressor-tests
    ./compressor-tests

JUCE build:

    cmake -S . -B build -G Xcode -DJUCE_PATH=/path/to/JUCE -DLRC_BUILD_SMOKE_HOST=ON '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
    cmake --build build --config Release --target LindellRackCompressor_VST3 compressor_tests compressor_smoke
    ctest --test-dir build -C Release --output-on-failure

Local validation completed: DSP tests at 44.1, 48, 88.2, 96 and 192 kHz; dry null, blend, bypass, stereo/antiphase, detector HPF, static curve, silence. Full JUCE compilation and Mac host tests await the cloud build.
