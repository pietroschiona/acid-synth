#ifndef DELAY_H
#define DELAY_H

#ifdef __cplusplus
extern "C" {
#endif


/* ----------------------------------------------------------------- */
/*  Delay module — circular buffer with feedback and wet/dry mix     */
/*  Pure C, no dependencies. Based on Pirkle cap 14.                  */
/* ----------------------------------------------------------------- */

typedef struct {
    float  *buffer;         // circular buffer
    int     buffer_size;    // allocated size
    int     write_pos;      // current write position
    float   delay_ms;       // delay time in milliseconds
    float   feedback;       // feedback amount [0..1]
    float   mix;            // wet/dry mix [0..1]
    float   sr;             // sample rate
} Delay;

/* Initialize delay with max delay time in ms */
void delay_init(Delay *d, float max_delay_ms, float sr);

/* Free delay buffer */
void delay_free(Delay *d);

/* Set delay time in ms */
void delay_set_time(Delay *d, float delay_ms);

/* Set feedback [0..1] */
void delay_set_feedback(Delay *d, float feedback);

/* Set wet/dry mix [0..1] */
void delay_set_mix(Delay *d, float mix);

/* Process one sample (wet/dry mix applied) */
float delay_process(Delay *d, float input);

/* Process buffer */
void delay_process_block(Delay *d, float *buf, int n);

/* Reset delay buffer (clear to zero) */
void delay_reset(Delay *d);

#ifdef __cplusplus
}
#endif

#endif
