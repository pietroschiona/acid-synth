# BUILD.md — Acid Synth Plugin

## Cos'è

Sintetizzatore acid in **C puro** (kernel DSP) + **C++ minimo** (wrapper VST3).
Zero dipendenze esterne per il kernel. Zero attribuzioni obbligatorie.
Basato su: Pirkle (Designing Audio Effect/Synth Plugins), Boulanger (The Audio Programming Book).

## Struttura

```
acid_synth_plugin/
├── src/
│   ├── osc.c/h              ← VCO (saw, square, sine, tri, noise)
│   ├── lfo.c/h              ← LFO (sin, saw, square, tri, S&H)
│   ├── filter.c/h           ← Biquad risonante LP/HP/BP
│   ├── delay.c/h            ← Delay line + feedback
│   ├── wavefolder.c/h       ← Distorsione multistadio
│   ├── reverb.c/h           ← Schroeder reverb (4 comb + 2 allpass)
│   ├── sequencer.c/h        ← Step sequencer 16 note
│   ├── synth.c/h            ← Integrazione ACID SYNTH
│   ├── vst3_wrapper.cpp     ← Wrapper VST3 (richiede SDK)
│   └── standalone_portaudio.c ← Standalone con PortAudio
├── tests/
│   ├── test_util.h           ← Framework test + WAV writer + FFT
│   ├── test_osc.c            ← 8 test
│   ├── test_filter.c         ← 6 test
│   ├── test_delay.c          ← 4 test
│   ├── test_wavefolder.c     ← 4 test
│   ├── test_reverb.c         ← 2 test
│   └── test_synth.c          ← 2 test (full integration)
├── vst3sdk/                  ← (vuoto — aggiungere VST3 SDK)
├── docker/Dockerfile.gcc     ← gcc su Ubuntu 22.04 + PortAudio
├── CMakeLists.txt            ← Build modulare
└── BUILD.md                  ← Questo file
```

## Test (Docker)

```bash
# Build immagine
docker build -t acid-synth-toolchain -f docker/Dockerfile.gcc .

# Compila e testa l'oscillatore
docker run --rm -v "$(pwd):/workspace" acid-synth-toolchain \
    bash -c 'cd /workspace && gcc -O2 tests/test_osc.c src/osc.c -Isrc -Itests -lm -o test_osc && ./test_osc'
```

## Eseguire tutti i test

```bash
# Tutti i moduli
for test in osc filter delay wavefolder reverb synth; do
    SRCS="src/osc.c"
    [ "$test" = "filter" ] && SRCS="$SRCS src/filter.c"
    [ "$test" = "delay" ] && SRCS="$SRCS src/delay.c"
    [ "$test" = "wavefolder" ] && SRCS="$SRCS src/wavefolder.c"
    [ "$test" = "reverb" ] && SRCS="$SRCS src/reverb.c"
    [ "$test" = "synth" ] && SRCS="src/osc.c src/filter.c src/delay.c src/wavefolder.c src/reverb.c src/sequencer.c src/lfo.c src/synth.c"
    docker run --rm -v "$(pwd):/workspace" acid-synth-toolchain \
        bash -c "cd /workspace && gcc -O2 tests/test_$test.c $SRCS -Isrc -Itests -lm -o test_$test && ./test_$test"
done
```

## Standalone con UI (Windows)

Con MinGW/MSYS2 (git-bash):

```bash
gcc -O2 src/ui_win32.c src/osc.c src/filter.c src/delay.c src/wavefolder.c \
    src/reverb.c src/sequencer.c src/lfo.c src/synth.c \
    -Isrc -lm -lportaudio -lgdi32 -mwindows -o acid_synth.exe
```

Avvia `acid_synth.exe`. Finestra con 9 slider:
- **Filter Cutoff/Reso** — controllo del filtro
- **Delay Mix/Fdbk** — echo
- **Reverb Mix/Decay** — riverbero
- **Volume/Squel Rate/Drive** — master, sequencer, distorsione
- **Sweep checkbox** — sweep automatico del filtro
- **+ / - keys** — regolazione fine dello slider selezionato
- **Quit** — esci

## Solo terminale (cross-platform)

```bash
gcc -O2 src/standalone_portaudio.c src/osc.c src/filter.c src/delay.c \
    src/wavefolder.c src/reverb.c src/sequencer.c src/lfo.c src/synth.c \
    -Isrc -lm -lportaudio -o acid_synth
```

Controlli: `q`=quit, `f`=filter sweep, `r`=reverb toggle, `d`=delay toggle.

## VST3 (Windows con MSVC + VST3 SDK)

1. Scarica VST3 SDK da https://github.com/steinbergmedia/vst3sdk
2. Metti i header in `vst3sdk/pluginterfaces/`
3. Compila:

```bash
cl /nologo /O2 /LD vst3_wrapper.cpp src/osc.c src/filter.c src/delay.c \
    src/wavefolder.c src/reverb.c src/sequencer.c src/lfo.c src/synth.c \
    -Isrc -Ivst3sdk /Feacid_synth.vst3
```

## WAV demo generati

| File | Contenuto |
|------|-----------|
| osc_440.wav | Saw 440Hz, 1s |
| filter_sweep.wav | Filter sweep 100Hz→8kHz |
| delay_echo.wav | Delay 300ms + feedback |
| wavefolder_dist.wav | Distortion drive=6 |
| reverb_tail.wav | Reverb tail |
| acid_synth_demo.wav | Demo completa 6s stereo |
