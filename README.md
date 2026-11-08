# J60 Chorus (working name)

A JUCE plugin modelling a Juno-60-style chorus pedal (the GEM-61 V2 design in
`assets/`), in the spirit of the TC Electronic June-60 V2, itself a recreation
of the Roland Juno-60 chorus.

## Controls

| Control | What it does |
| --- | --- |
| Chorus I / Chorus II buttons | The pedal's two mode buttons. Both on = I+II. Neither = dry. Each has an LED that lights while it is on. |
| Mono / Stereo slider | Click either side of the slider, or its MONO / STEREO label. |
| Footswitch | Bypass. A "false bypass": the signal keeps running through the unit (still summed to mono) and only the chorus fades out. The LEDs go dark while bypassed. |
| Speed Set | Juno-60 (the V2's slow DIP setting) or June-60 V1 (its fast DIP setting). No on-screen control yet: use your host's parameter list. |

The pedal's internal Guitar/Keys input switch is just an input pad, so it is
not modelled. Use your host's gain for that.

## The GUI

The editor is a 1920x1080 render of the pedal (`assets/bg.png`) with small
sprites drawn on top for the slider, the two LEDs and the two buttons. It is
resizable at a fixed 16:9 shape (default 960x540). The sprites have widened,
feathered alpha edges so they blend into the background without seams.

To change the artwork, re-render the scene from a fixed camera with the same
noise seed in five states (slider on MONO with nothing pressed, slider on
STEREO with nothing pressed, chorus I, chorus II, both), then run:

    python tools/make_sprites.py --mono-off a.png --stereo-off b.png \
        --chorus1 c.png --chorus2 d.png --both e.png

It needs Python with numpy, scipy and Pillow. It rewrites `assets/*.png` and
`EditorLayout.h` (sprite positions and click areas), writes
`tools/mask_preview.png` so you can check the masks by eye, and reports how
closely the sprites rebuild each render. The footswitch click circle and the
slider label area are set by hand at the top of the script (`FOOT`,
`SLIDER_HIT`). Keep the buttons' LEDs and the slider where they are, or update
the search boxes in `ZONES` too.

The background render is what users see, so check any props in it for real
brand logos before you release.

## Build

Requires CMake 3.22+, a C++17 compiler, and git (CMake downloads JUCE).

    cmake -B build
    cmake --build build --config Release

Outputs land under `build/J60Chorus_artefacts/Release/` (VST3, Standalone, and
AU on macOS). `COPY_PLUGIN_AFTER_BUILD` also installs the VST3/AU into your
system plugin folder. If the JUCE tag (8.0.6) does not exist for you, change
`GIT_TAG` in `CMakeLists.txt` to any recent 8.x release.

Before releasing, change `COMPANY_NAME`, the two plugin codes and the product
name in `CMakeLists.txt`. "June-60" and "Juno-60" belong to TC Electronic and
Roland, so pick your own name for anything public.

## Tuning (the important part)

Everything that defines the sound is at the top of `JuneChorus.h`:

- `kJunoSpeeds`: rate, mean delay and depth for I, II and I+II. Values are
  from memory of commonly quoted Juno-60 figures, not measured. Starting points.
- `kPedalV1Speeds`: **placeholder numbers**. Replace after measuring.
- `kFilterHz`, `kDryGain`, `kWetGain`, `kHiss`: band-limiting, levels and noise floor.

To measure the pedal: record it with a steady sine (for example 1 kHz), then
track the phase of the wet signal over time. The phase swing gives delay depth,
and the repeat time of the sweep gives the LFO rate. Do it for each button and
for both speed sets.

## Not done yet

- BBD clock noise, aliasing and the pedal's own input/output stage coloration.
  The model has band-limiting and hiss only.
- A pressed look for the footswitch, and an on-screen control for Speed Set.
- Not yet built or tested inside a host. The editor and plugin wrapper were
  written without compiling JUCE, so expect to fix small build errors. The DSP
  core (`JuneChorus.h`) has no JUCE dependency and has its own test in
  `test_dsp.cpp`:

      g++ -std=c++17 -O2 test_dsp.cpp -o test_dsp && ./test_dsp
