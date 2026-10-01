# Acid Synth Plugin

Acid synthesizer DSP kernel in **pure C** (zero external dependencies) with a minimal **C++ VST3 wrapper** and Win32/macOS/Linux standalone builds.

Started as a PureData patch compiled via HVCC, then rewritten by hand as a clean C DSP library — same sound, readable code, unit-tested.

## DSP modules (`src/`)

| Module | Description |
|---|---|
| `osc.c` | 3 VCOs (saw, square, sine, tri, noise) + sub |
| `lfo.c` | LFO for filter modulation (sin, saw, square, tri, S&H) |
| `filter.c` | Resonant biquad LP/HP/BP/Notch (RBJ cookbook) |
| `delay.c` | Circular delay line + feedback + mix |
| `wavefolder.c` | Multistage distortion |
| `reverb.c` | Schroeder reverb (4 comb + 2 allpass) |
| `sequencer.c` | 16-step note sequencer |
| `scale.c` | Scale/musical math (frequency generation) |
| `synth.c` | Integration: voice management, MIDI, params |
| `vst3_wrapper.cpp` | VST3 plugin wrapper (VST3 SDK) |
| `standalone_portaudio.c` / `standalone_mac.c` | Standalone builds |
| `ui_win32.c` | Native Win32 GUI |

## Build & test

Requires CMake ≥ 3.16 and a C11/C++17 compiler.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Run the DSP test suite (31 tests: oscillator shape/spectrum, filter response, delay, wavefolder, reverb, full synth):

```bash
./build/Release/test_osc
./build/Release/test_filter
./build/Release/test_delay
./build/Release/test_wavefolder
./build/Release/test_reverb
./build/Release/test_synth
```

Docker builds (Linux gcc / MinGW cross-compile to Windows) in `docker/`. macOS build script: `BUILD_MAC.sh`.

## References

Design informed by Pirkle (*Designing Audio Effect and Synth Plugins*) and Boulanger (*The Audio Programming Book*).

## Author

Pietro Schiona — [github.com/pietroschiona](https://github.com/pietroschiona)
