#include "sequencer.h"
#include <stdlib.h>
#include <string.h>

void sequencer_init(Sequencer *s, int steps, float rate_hz, float sr) {
    if (steps < 2) steps = 2;
    if (steps > 64) steps = 64;
    s->steps = steps;
    s->rate = rate_hz;
    s->sr = sr;
    s->sample_count = 0.0f;
    s->current_step = 0;
    s->current_note = 0.0f;

    s->notes = (float*)calloc(steps, sizeof(float));
}

void sequencer_free(Sequencer *s) {
    free(s->notes);
    s->notes = NULL;
}

void sequencer_set_rate(Sequencer *s, float rate_hz) {
    if (rate_hz < 0.01f) rate_hz = 0.01f;
    s->rate = rate_hz;
}

void sequencer_set_note(Sequencer *s, int step, float note) {
    if (step >= 0 && step < s->steps) {
        s->notes[step] = note;
    }
}

float sequencer_get_note(Sequencer *s) {
    return s->current_note;
}

int sequencer_get_step(Sequencer *s) {
    return s->current_step;
}

void sequencer_reset(Sequencer *s) {
    s->current_step = 0;
    s->sample_count = 0.0f;
    s->current_note = s->notes[0];
}

int sequencer_tick(Sequencer *s) {
    float samples_per_tick = s->sr / s->rate;
    s->sample_count += 1.0f;

    if (s->sample_count >= samples_per_tick) {
        s->sample_count -= samples_per_tick;

        // Advance to next step
        s->current_step++;
        if (s->current_step >= s->steps) {
            s->current_step = 0;
        }

        s->current_note = s->notes[s->current_step];
        return 1; // stepped
    }
    return 0; // no step
}
