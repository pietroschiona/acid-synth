#include "reverb.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Comb filter: delay + feedback + low-pass */
static float comb_process(CombFilter *c, float input) {
    float out = c->buf[c->pos];
    c->buf[c->pos] = input + out * c->feedback;
    c->pos++;
    if (c->pos >= c->size) c->pos = 0;
    return out;
}

/* Allpass filter: delay + feedforward + feedback */
static float allpass_process(Allpass *a, float input) {
    float buf_out = a->buf[a->pos];
    float out = -input * a->gain + buf_out;
    a->buf[a->pos] = input + buf_out * a->gain;
    a->pos++;
    if (a->pos >= a->size) a->pos = 0;
    return out;
}

void reverb_init(Reverb *r, float room_size, float decay, float sr) {
    r->mix = 0.3f;
    r->decay = decay;
    r->sr = sr;

    // Comb filter delays (Schroeder primaries — prime number lengths)
    // Scale by room_size
    float scale = room_size;
    int comb_delays[4] = {
        (int)(0.0297f * sr * scale),
        (int)(0.0371f * sr * scale),
        (int)(0.0411f * sr * scale),
        (int)(0.0437f * sr * scale)
    };

    float comb_fb[4] = {0.75f, 0.72f, 0.70f, 0.68f};
    for (int i = 0; i < 4; i++) {
        if (comb_delays[i] < 100) comb_delays[i] = 100;
        r->combs[i].size = comb_delays[i];
        r->combs[i].buf = (float*)calloc(comb_delays[i], sizeof(float));
        r->combs[i].pos = 0;
        r->combs[i].feedback = comb_fb[i] * decay;
    }

    // Allpass filter delays
    int ap_delays[2] = {
        (int)(0.0050f * sr * scale),
        (int)(0.0017f * sr * scale)
    };
    for (int i = 0; i < 2; i++) {
        if (ap_delays[i] < 10) ap_delays[i] = 10;
        r->allpasses[i].size = ap_delays[i];
        r->allpasses[i].buf = (float*)calloc(ap_delays[i], sizeof(float));
        r->allpasses[i].pos = 0;
        r->allpasses[i].gain = 0.7f;
    }
}

void reverb_free(Reverb *r) {
    for (int i = 0; i < 4; i++) {
        free(r->combs[i].buf);
        r->combs[i].buf = NULL;
    }
    for (int i = 0; i < 2; i++) {
        free(r->allpasses[i].buf);
        r->allpasses[i].buf = NULL;
    }
}

void reverb_set_mix(Reverb *r, float mix) {
    if (mix < 0.0f) mix = 0.0f;
    if (mix > 1.0f) mix = 1.0f;
    r->mix = mix;
}

void reverb_set_decay(Reverb *r, float decay) {
    r->decay = decay;
    for (int i = 0; i < 4; i++) {
        float base_fb = 0.75f - i * 0.025f;
        r->combs[i].feedback = base_fb * decay;
    }
}

void reverb_reset(Reverb *r) {
    for (int i = 0; i < 4; i++) {
        memset(r->combs[i].buf, 0, r->combs[i].size * sizeof(float));
        r->combs[i].pos = 0;
    }
    for (int i = 0; i < 2; i++) {
        memset(r->allpasses[i].buf, 0, r->allpasses[i].size * sizeof(float));
        r->allpasses[i].pos = 0;
    }
}

float reverb_process(Reverb *r, float input) {
    // Sum through parallel comb filters
    float wet = 0.0f;
    for (int i = 0; i < 4; i++) {
        wet += comb_process(&r->combs[i], input);
    }
    wet *= 0.25f; // Normalize

    // Through series allpass filters
    wet = allpass_process(&r->allpasses[0], wet);
    wet = allpass_process(&r->allpasses[1], wet);

    // Wet/dry mix
    return input * (1.0f - r->mix) + wet * r->mix;
}

void reverb_process_block(Reverb *r, float *buf, int n) {
    for (int i = 0; i < n; i++) {
        buf[i] = reverb_process(r, buf[i]);
    }
}
