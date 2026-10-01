#include "filter.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* RBJ biquad cookbook — resonant low/high/band-pass */
void filter_calc_coeffs(Filter *f) {
    if (f->sr <= 0.0f) return;

    double w0 = 2.0 * M_PI * f->freq / f->sr;
    if (w0 >= M_PI) w0 = M_PI * 0.99; // clamp below Nyquist

    // Q = 0.5 (critically damped) at resonance=0, Q = ~15 at resonance=1
    // Max Q=15: ancora molto risonante (self-oscillation audibile), ma evita
    // il fischio acuto da intermodulazione con wavefolder (Q=50 prima del fix).
    double q = 0.5 + 14.5 * f->resonance;
    double alpha = sin(w0) / (2.0 * q);
    double cos_w0 = cos(w0);

    double a0_inv = 1.0 / (1.0 + alpha);

    switch (f->type) {
        case FILTER_LP: {
            double b = (1.0 - cos_w0) * 0.5;
            f->b0 = b * a0_inv;
            f->b1 = 2.0 * b * a0_inv;
            f->b2 = b * a0_inv;
            f->a1 = (-2.0 * cos_w0) * a0_inv;
            f->a2 = (1.0 - alpha) * a0_inv;
            break;
        }
        case FILTER_HP: {
            double b = (1.0 + cos_w0) * 0.5;
            f->b0 = b * a0_inv;
            f->b1 = (-2.0 * b) * a0_inv;
            f->b2 = b * a0_inv;
            f->a1 = (-2.0 * cos_w0) * a0_inv;
            f->a2 = (1.0 - alpha) * a0_inv;
            break;
        }
        case FILTER_BP: {
            f->b0 = alpha * a0_inv;
            f->b1 = 0.0;
            f->b2 = (-alpha) * a0_inv;
            f->a1 = (-2.0 * cos_w0) * a0_inv;
            f->a2 = (1.0 - alpha) * a0_inv;
            break;
        }
        case FILTER_NOTCH: {
            f->b0 = 1.0 * a0_inv;
            f->b1 = (-2.0 * cos_w0) * a0_inv;
            f->b2 = 1.0 * a0_inv;
            f->a1 = (-2.0 * cos_w0) * a0_inv;
            f->a2 = (1.0 - alpha) * a0_inv;
            break;
        }
    }
}

void filter_init(Filter *f, FilterType type, float freq, float resonance, float sr) {
    f->type      = type;
    f->freq      = freq;
    f->resonance = resonance;
    f->sr        = sr;
    f->x1 = f->x2 = 0.0;
    f->y1 = f->y2 = 0.0;
    f->b0 = f->b1 = f->b2 = 0.0;
    f->a1 = f->a2 = 0.0;
    filter_calc_coeffs(f);
}

void filter_set_freq(Filter *f, float freq_hz) {
    f->freq = freq_hz;
    filter_calc_coeffs(f);
}

void filter_set_resonance(Filter *f, float res) {
    if (res < 0.0f) res = 0.0f;
    if (res > 1.0f) res = 1.0f;
    f->resonance = res;
    filter_calc_coeffs(f);
}

void filter_reset(Filter *f) {
    f->x1 = f->x2 = 0.0;
    f->y1 = f->y2 = 0.0;
}

float filter_process(Filter *f, float input) {
    // Direct Form 1: y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2
    double y = f->b0 * input + f->b1 * f->x1 + f->b2 * f->x2
               - f->a1 * f->y1 - f->a2 * f->y2;

    // Shift state
    f->x2 = f->x1;
    f->x1 = input;
    f->y2 = f->y1;
    f->y1 = y;

    return (float)y;
}

void filter_process_block(Filter *f, float *buf, int n) {
    for (int i = 0; i < n; i++) {
        buf[i] = filter_process(f, buf[i]);
    }
}
