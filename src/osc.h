#ifndef OSC_H
#define OSC_H

#ifdef __cplusplus
extern "C" {
#endif


/* ----------------------------------------------------------------- */
/*  Oscillator module — saw, square, sine, sub                       */
/*  Pure C, no dependencies.                                          */
/* ----------------------------------------------------------------- */

typedef enum {
    OSC_WAVE_SAW,
    OSC_WAVE_SQUARE,
    OSC_WAVE_SINE,
    OSC_WAVE_TRI,
    OSC_WAVE_NOISE,
    OSC_WAVE_COUNT
} OscWaveform;

typedef struct {
    double      phase;          // phase accumulator [0..2pi)
    OscWaveform waveform;       // waveform type
    float       freq;           // current frequency (may be gliding to target)
    float       target_freq;    // final frequency to reach
    float       freq_step;      // per-sample step during glide
    int         glide_samples;  // remaining samples in glide
    float       pulse_width;    // for square: duty cycle [0..1]
    float       amp;            // amplitude [0..1]
} Osc;

/* Initialize oscillator */
void osc_init(Osc *o, OscWaveform wave, float freq_hz);

/* Set frequency */
void osc_set_freq(Osc *o, float freq_hz);

/* Set target frequency with smooth glide (anti-click).
 * Actual frequency ramps to target over 2ms at 44.1kHz,
 * eliminating the transient from instant frequency change
 * through resonant filters and effects. */
void osc_set_target_freq(Osc *o, float freq_hz, float sr);

/* Set amplitude */
void osc_set_amp(Osc *o, float amp);

/* Set pulse width (square wave only, 0.1-0.9) */
void osc_set_pulse_width(Osc *o, float pw);

/* Reset phase to 0 */
void osc_reset_phase(Osc *o);

/* Generate one sample. Call sequentially — phase advances internally. */
float osc_process(Osc *o, float sr);

/* Fill a buffer of n samples */
void osc_process_block(Osc *o, float *out, int n, float sr);

/* Generate silence / reset accumulator without click */
void osc_silence(Osc *o);

#ifdef __cplusplus
}
#endif

#endif
