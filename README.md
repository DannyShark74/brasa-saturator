# Brasa

An analog saturation effect (VST3 / AU, macOS universal) for personal use: five circuit models,
from barely-there colour to scorched, with the controls arranged so you decide what distorts
before the circuit and clean up what it produced after it.

## Signal flow

```
in ─┬─ Low Cut (+Thump) ─ Tone tilt ─ Drive (+Punish) ─[ 2x / 8x oversampled circuit ]─ High Cut (6 / 30 dB/oct) ─ Output (+Auto) ─┬─ Mix ─ out
    └──────────────────────────────── dry, delayed by the same latency ─────────────────────────────────────────────────────────┘
```

## The circuits

| Style | Model | Character |
|---|---|---|
| Cinta | Tube record amplifier of a 1950s tape machine: input transformer, a tube stage with very soft knees, a little dark on top, low-mid weight | The smoothest: mostly 2nd harmonic, fewest harmonics overall |
| Consola | Late-60s British transistor console channel: light transformer saturation, asymmetric class-A stage, gentle low and air shelves | Full lows, airy top, creamy 2nd + 3rd |
| Germanio | Germanium class-A channel with heavy iron: the core saturates on low frequencies first, the amp stays clean until it is pushed | Lows distort ~3-4x more than mids at moderate levels; weight on bass and drums |
| Triodo | Overdriven triode biased towards cut-off, 3/2-power law, grid conduction, grid-current bias shift, band-limited ~40 Hz-12 kHz | Strong 2nd harmonic, punch and squash; becomes more symmetric when slammed |
| Pentodo | Overdriven pentode: 3/2 law, cut-off and a hard plate-current ceiling, grid-current bias shift, darker top | Most harmonics and the longest harmonic tail; bite and fizz |

The tube styles are dynamic: when the grid swings positive it charges the coupling capacitor and
moves the bias, which then leaks back over ~0.1 s, so the harmonic balance follows the playing.

The models were tuned against measurements of neural captures of real hardware (a Culture
Vulture in triode and pentode modes, two Neve 1073s, a germanium "57" channel and an EMI-style
chain): harmonic level and even/odd balance over input level and frequency, and small-signal
frequency response. The captures are only references; nothing from them is inside the plugin.

## Building

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBRASA_BUILD_TOOLS=ON
cmake --build build --target Brasa_VST3 BrasaRender
build/BrasaRender_artefacts/Release/BrasaRender --test
build/BrasaRender_artefacts/Release/BrasaRender --harm 3 5 1000 -18          # style drive Hz dBFS-RMS
build/BrasaRender_artefacts/Release/BrasaRender --file in.wav out.wav "Drum Smash"
```

`BrasaRender --test` checks: finite output at extreme settings, silence, Mix 0 = delayed input,
2x / HQ alignment, aliasing (HQ at least 10 dB cleaner), Auto keeps the level within 2 dB, the
character of every style, the filters, clicks when switching style / HQ, sample rates 44.1-192 kHz
and CPU.

CI (GitHub Actions, macOS) builds a universal VST3 + AU, runs the checks, builds and tests the
`.pkg` installer, validates with pluginval and auval and publishes the installer to the
`downloads` branch.
