#include "osc.h"
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TWOPI (2.0 * M_PI)

void osc_init(Osc *o, OscWaveform wave, float freq_hz) {
    o->phase       = 0.0;
    o->waveform    = wave;
    o->freq        = freq_hz;
    o->target_freq = freq_hz;
    o->freq_step   = 0.0f;
    o->glide_samples = 0;
    o->pulse_width = 0.5f;
    o->amp         = 1.0f;
}

void osc_set_freq(Osc *o, float freq_hz) {
    o->freq = freq_hz;
    o->target_freq = freq_hz;
    o->glide_samples = 0;
}

void osc_set_target_freq(Osc *o, float freq_hz, float sr) {
    if (o->freq == freq_hz) return;
    o->target_freq = freq_hz;
    /* 2ms glide = 88 samples at 44.1kHz — fast enough to be
     * imperceptible as portamento, slow enough to eliminate
     * the click from instant frequency change through resonant
     * filters and reverb/delay buffers. (Pirkle, cap.8) */
    int n = (int)(sr * 0.002f);
    if (n < 1) n = 1;
    o->freq_step = (freq_hz - o->freq) / (float)n;
    o->glide_samples = n;
}

void osc_set_amp(Osc *o, float amp) {
    o->amp = amp;
}

void osc_set_pulse_width(Osc *o, float pw) {
    if (pw < 0.05f) pw = 0.05f;
    if (pw > 0.95f) pw = 0.95f;
    o->pulse_width = pw;
}

void osc_reset_phase(Osc *o) {
    o->phase = 0.0;
}

void osc_silence(Osc *o) {
    o->phase = 0.0;
}

float osc_process(Osc *o, float sr) {
    if (sr <= 0.0f || o->freq <= 0.0f) return 0.0f;

    /* Apply frequency glide (smooths note transitions through filter/effects) */
    if (o->glide_samples > 0) {
        o->freq += o->freq_step;
        o->glide_samples--;
        if (o->glide_samples == 0) o->freq = o->target_freq;
    }

    double phase = o->phase;
    double inc = (double)o->freq / (double)sr * TWOPI;

    float sample = 0.0f;

    switch (o->waveform) {
        case OSC_WAVE_SAW: {
            // Unipolar [0..1) → bipolar [-1..1]
            double t = phase / TWOPI;  // [0..1)
            sample = (float)(2.0 * t - 1.0);
            break;
        }
        case OSC_WAVE_SQUARE: {
            double t = phase / TWOPI;
            sample = (t < o->pulse_width) ? 1.0f : -1.0f;
            break;
        }
        case OSC_WAVE_SINE: {
            sample = (float)sin(phase);
            break;
        }
        case OSC_WAVE_TRI: {
            // Bipolar triangle: up then down
            double t = phase / TWOPI;
            sample = (float)(4.0 * fabs(t - 0.5) - 1.0);
            break;
        }
        case OSC_WAVE_NOISE: {
            // White noise — phase accumulator still advances
            sample = (float)(2.0 * (double)rand() / RAND_MAX - 1.0);
            break;
        }
        default:
            sample = 0.0f;
            break;
    }

    // Advance phase
    phase += inc;
    if (phase >= TWOPI) phase -= TWOPI;
    o->phase = phase;

    return sample * o->amp;
}

void osc_process_block(Osc *o, float *out, int n, float sr) {
    for (int i = 0; i < n; i++) {
        out[i] = osc_process(o, sr);
    }
}
