# FreeBass

A structured model of Waves Renaissance Bass, built from captures of the real plug-in. Part of **The Ecosystem**.

Version 0.1 (model v0). JUCE 8.0.4, VST3 + AU, universal macOS build (Apple Silicon + Intel) through GitHub Actions. Zero latency. Mono or stereo.

## Controls
| Control | Range | What it does |
|---|---|---|
| Frequency | 32–256 Hz (1 Hz steps), default 80 | Crossover point. Bass below it feeds the harmonic generator. |
| Intensity | −24…+24 dB, default 0 | Level of the processed bass (harmonics + reshaped fundamental). |
| Gain | −30…0 dB, default 0 | Output level, before the final clip. |
| Original Bass | Out / In, default Out | Out removes the original bass below Frequency; In keeps it. |

Meters: HARMONICS shows the processed bass (with the path limiter's gain reduction as an orange strip at the top), OUTPUT shows the plug-in output. The CLIP light latches; click it to reset.

## Signal flow (model v0)
1. Crossover per channel: Butterworth 3rd-order high-pass at Frequency; the matching low-pass is added back with Original Bass In.
2. Generator input: (L+R)/2 through a Butterworth 3rd-order low-pass at 1.212 × Frequency.
3. Upward compressor: peak follower (12.2 ms attack, 12.5 ms release), log-quadratic gain law with a knee.
4. Two branches, all filters scaled by Frequency: a band-shaped fundamental, and a small polynomial shaper for the harmonics.
5. Intensity, then a path limiter (ceiling −3 dBFS, instant attack, 45 ms release).
6. Sum, Gain, hard clip at ±1.0.

All coefficients: `docs/freebass_model_v0.json` and `Source/FreeBassEngine.h`.

## Accuracy (against R Bass captures)
- Complete output: −18 to −28 dB cancellation on the full test signal (−24 to −28 dB at Frequency 90, Intensity 0).
- Processed bass alone: −20 to −26 dB at every Frequency setting from 32 to 256 Hz.
- Same behaviour at 44.1, 48, 88.2 and 96 kHz, like the original.
- Known differences: loud bass notes far below Frequency (R Bass makes weaker even harmonics there), and Intensity near +24 dB.

## Build
Push to GitHub: the **Build FreeBass** workflow builds the universal VST3 and AU and uploads `FreeBass-macOS.zip` as an artifact.

Local: `cmake -B build && cmake --build build --config Release`.

`run_engine` (built with the project) renders a 32-bit float WAV through the engine without JUCE:
`run_engine in.wav out.wav <freq Hz> <intensity dB> <gain dB> <orig 0/1>`.

## Ecosystem
`Ecosystem/` is the shared Ecosystem Bus (layout v1), copied unchanged from Free Rider. v0.1 doesn't use it yet. Next: the kick key (Push/Duck of the processed bass from an Ecosystem channel), then Freq follows the note.
