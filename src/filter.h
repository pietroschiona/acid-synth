#ifndef FILTER_H
#define FILTER_H

#ifdef __cplusplus
extern "C" {
#endif


/* ----------------------------------------------------------------- */
/*  Filter module — biquad IIR filter with resonant LP/HP/BP         */
/*  Pure C, no dependencies. Based on Pirkle cap 11 + Zavalishin VAF */
/* ----------------------------------------------------------------- */

typedef enum {
    FILTER_LP,      // Low-pass  (2-pole resonant)
    FILTER_HP,      // High-pass
    FILTER_BP,      // Band-pass
    FILTER_NOTCH    // Notch
} FilterType;

typedef struct {
    FilterType  type;       // filter type
    float       freq;       // cutoff frequency (Hz)
    float       resonance;  // resonance / Q (0.0 to 1.0, where 1.0 = self-oscillating)
    float       sr;         // sample rate

    // Biquad coefficients (direct form 1)
    double b0, b1, b2;
    double a1, a2;

    // State (DF1)
    double x1, x2;  // input history
    double y1, y2;  // output history
} Filter;

/* Initialize filter with type, cutoff freq, resonance */
void filter_init(Filter *f, FilterType type, float freq, float resonance, float sr);

/* Set cutoff frequency (Hz) */
void filter_set_freq(Filter *f, float freq_hz);

/* Set resonance (0.0 to 1.0, 1.0 = self-oscillation) */
void filter_set_resonance(Filter *f, float res);

/* Recalculate biquad coefficients after parameter change */
void filter_calc_coeffs(Filter *f);

/* Process one sample through the filter */
float filter_process(Filter *f, float input);

/* Process a buffer of samples */
void filter_process_block(Filter *f, float *buf, int n);

/* Reset filter state (no click) */
void filter_reset(Filter *f);

#ifdef __cplusplus
}
#endif

#endif
