#ifndef SEQUENCER_H
#define SEQUENCER_H

#ifdef __cplusplus
extern "C" {
#endif


/* ----------------------------------------------------------------- */
/*  Step sequencer module — 16-step pattern with note table          */
/*  Pure C, no dependencies.                                          */
/* ----------------------------------------------------------------- */

typedef struct {
    int     steps;          // number of steps (2..64)
    float  *notes;          // note values per step (MIDI or Hz)
    float   rate;           // tick rate in Hz
    float   sample_count;   // internal sample counter
    float   sr;
    int     current_step;
    float   current_note;
} Sequencer;

/* Initialize sequencer with N steps */
void sequencer_init(Sequencer *s, int steps, float rate_hz, float sr);

/* Free */
void sequencer_free(Sequencer *s);

/* Set tick rate in Hz */
void sequencer_set_rate(Sequencer *s, float rate_hz);

/* Set note value at step */
void sequencer_set_note(Sequencer *s, int step, float note);

/* Get current note value */
float sequencer_get_note(Sequencer *s);

/* Get current step index */
int sequencer_get_step(Sequencer *s);

/* Advance sequencer by one sample. Returns 1 if stepped, 0 otherwise. */
int sequencer_tick(Sequencer *s);

/* Reset to step 0 */
void sequencer_reset(Sequencer *s);

#ifdef __cplusplus
}
#endif

#endif
