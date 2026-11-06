# COHESION – EQ/COMP

An original JUCE/VST3 plugin by **Pepperwood**: a high-quality parametric EQ,
flexible multiband dynamics, an output brick-wall limiter, and an **ASSIST**
engine that listens to the signal and proposes EQ + compression settings per
instrument and style.

Part of the **COHESION** plugin suite.

## Features

- **EQ** – 6-band parametric EQ (TPT/ZDF state-variable filters) plus HPF/LPF,
  with an interactive curve, live before/after spectrum and draggable nodes.
- **Multiband** – 2–5 bands with Linkwitz-Riley crossovers; per-band Comp / Exp /
  Gate, threshold, ratio, attack, release, width, makeup gain, solo/mute/bypass.
- **Limiter** – look-ahead brick-wall limiter on the output.
- **ASSIST** – analyses the input and proposes EQ moves, a compressor preset and
  a limiter ceiling; APPLY writes them to the EQ and MULTIBAND screens.
- Switchable **EQ ⇄ COMP** processing order, resizable GUI, user presets.

## Build

Requires CMake and a C++17 compiler. JUCE 7.0.12 is fetched automatically.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The built VST3 lands in `build/CohesionEQComp_artefacts/Release/VST3/`.

## License

© Pepperwood. All rights reserved.
