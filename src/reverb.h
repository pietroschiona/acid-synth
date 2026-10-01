#ifndef REVERB_H
#define REVERB_H

#ifdef __cplusplus
extern "C" {
#endif


/* ----------------------------------------------------------------- */
/*  Reverb module — Schroeder/Moorer style with comb + allpass       */
/*  Pure C, no dependencies. Based on Pirkle cap 17.                  */
/* ----------------------------------------------------------------- */

typedef struct {
    float *buf;
    int    size;
    int    pos;
    float  feedback;
} CombFilter;

typedef struct {
    float *buf;
    int    size;
    int    pos;
    float  gain;
} Allpass;

typedef struct {
    CombFilter combs[4];    // Parallel comb filters
    Allpass    allpasses[2]; // Series allpass filters
    float      mix;         // wet/dry [0..1]
    float      decay;       // overall decay [0..1]
    float      sr;
} Reverb;

/* Initialize reverb with room size and decay */
void reverb_init(Reverb *r, float room_size, float decay, float sr);

/* Free reverb buffers */
void reverb_free(Reverb *r);

/* Set mix (wet/dry) */
void reverb_set_mix(Reverb *r, float mix);

/* Set decay */
void reverb_set_decay(Reverb *r, float decay);

/* Process one sample */
float reverb_process(Reverb *r, float input);

/* Process block */
void reverb_process_block(Reverb *r, float *buf, int n);

/* Reset */
void reverb_reset(Reverb *r);

#ifdef __cplusplus
}
#endif

#endif
