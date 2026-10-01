#include "lfo.h"
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define TWOPI (2.0 * M_PI)

void lfo_init(Lfo *l, LfoWaveform wave, float rate_hz, float sr) {
    l->wave = wave;
    l->phase = 0.0;
    l->rate = rate_hz;
    l->sr = sr;
    l->amplitude = 1.0f;
    l->random_val = 0.0f;
}

void lfo_set_rate(Lfo *l, float rate_hz) {
    l->rate = rate_hz;
}

void lfo_set_amp(Lfo *l, float amp) {
    if (amp < 0.0f) amp = 0.0f;
    l->amplitude = amp;
}

void lfo_set_wave(Lfo *l, LfoWaveform wave) {
    l->wave = wave;
}

float lfo_process(Lfo *l) {
    if (l->sr <= 0.0f || l->rate <= 0.0f) return 0.0f;

    double inc = (double)l->rate / (double)l->sr * TWOPI;
    double t = l->phase / TWOPI; // [0..1)
    float out = 0.0f;

    switch (l->wave) {
        case LFO_WAVE_SINE:
            out = (float)sin(l->phase);
            break;
        case LFO_WAVE_SAW:
            out = (float)(2.0 * t - 1.0);
            break;
        case LFO_WAVE_SQUARE:
            out = (t < 0.5f) ? 1.0f : -1.0f;
            break;
        case LFO_WAVE_TRI:
            out = (float)(4.0 * fabs(t - 0.5) - 1.0);
            break;
        case LFO_WAVE_RANDOM:
            // Sample & hold: update on phase wrap
            if (l->phase + inc >= TWOPI) {
                l->random_val = (float)(2.0 * (double)rand() / RAND_MAX - 1.0);
            }
            out = l->random_val;
            break;
    }

    l->phase += inc;
    if (l->phase >= TWOPI) l->phase -= TWOPI;

    return out * l->amplitude;
}
