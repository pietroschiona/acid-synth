#include "delay.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

void delay_init(Delay *d, float max_delay_ms, float sr) {
    d->sr = sr;
    d->delay_ms = max_delay_ms < 1.0f ? 1.0f : max_delay_ms;
    d->feedback = 0.0f;
    d->mix = 0.5f;
    d->write_pos = 0;

    // Allocate buffer for max delay
    int max_samples = (int)(sr * max_delay_ms / 1000.0f) + 1;
    d->buffer_size = max_samples;
    d->buffer = (float*)calloc(max_samples, sizeof(float));
}

void delay_free(Delay *d) {
    free(d->buffer);
    d->buffer = NULL;
    d->buffer_size = 0;
}

void delay_set_time(Delay *d, float delay_ms) {
    if (delay_ms < 1.0f) delay_ms = 1.0f;
    d->delay_ms = delay_ms;
}

void delay_set_feedback(Delay *d, float feedback) {
    if (feedback < 0.0f) feedback = 0.0f;
    if (feedback > 1.0f) feedback = 1.0f;
    d->feedback = feedback;
}

void delay_set_mix(Delay *d, float mix) {
    if (mix < 0.0f) mix = 0.0f;
    if (mix > 1.0f) mix = 1.0f;
    d->mix = mix;
}

void delay_reset(Delay *d) {
    memset(d->buffer, 0, d->buffer_size * sizeof(float));
    d->write_pos = 0;
}

float delay_process(Delay *d, float input) {
    // Calculate read position: write_pos - delay_samples (circular)
    int delay_samples = (int)(d->sr * d->delay_ms / 1000.0f);
    if (delay_samples >= d->buffer_size) delay_samples = d->buffer_size - 1;
    if (delay_samples < 1) delay_samples = 1;

    int read_pos = d->write_pos - delay_samples;
    if (read_pos < 0) read_pos += d->buffer_size;

    // Read delayed sample
    float delayed = d->buffer[read_pos];

    // Write input + feedback * delayed to buffer
    d->buffer[d->write_pos] = input + d->feedback * delayed;

    // Advance write position
    d->write_pos++;
    if (d->write_pos >= d->buffer_size) d->write_pos = 0;

    // Wet/dry mix
    return input * (1.0f - d->mix) + delayed * d->mix;
}

void delay_process_block(Delay *d, float *buf, int n) {
    for (int i = 0; i < n; i++) {
        buf[i] = delay_process(d, buf[i]);
    }
}
