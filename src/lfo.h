#ifndef LFO_H
#define LFO_H

#ifdef __cplusplus
extern "C" {
#endif


/* ----------------------------------------------------------------- */
/*  LFO module — low-frequency oscillator for modulation             */
/*  Pure C, no dependencies.                                           */
/* ----------------------------------------------------------------- */

typedef enum {
    LFO_WAVE_SINE,
    LFO_WAVE_SAW,
    LFO_WAVE_SQUARE,
    LFO_WAVE_TRI,
    LFO_WAVE_RANDOM,  // Sample & hold
    LFO_WAVE_COUNT
} LfoWaveform;

typedef struct {
    LfoWaveform wave;
    double      phase;
    float       rate;       // Hz
    float       sr;
    float       amplitude;  // [0..1]
    float       random_val; // current S&H value
} Lfo;

void lfo_init(Lfo *l, LfoWaveform wave, float rate_hz, float sr);
void lfo_set_rate(Lfo *l, float rate_hz);
void lfo_set_amp(Lfo *l, float amp);
void lfo_set_wave(Lfo *l, LfoWaveform wave);
float lfo_process(Lfo *l);

#ifdef __cplusplus
}
#endif

#endif
