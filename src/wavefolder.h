#ifndef WAVEFOLDER_H
#define WAVEFOLDER_H

#ifdef __cplusplus
extern "C" {
#endif


/* ----------------------------------------------------------------- */
/*  Wavefolder module — multi-stage distortion (Pirkle cap 19)       */
/*  Pure C, no dependencies.                                          */
/* ----------------------------------------------------------------- */

typedef struct {
    float drive;       // pre-gain [0..10]
    float asym;        // asymmetry [0..1] (blend toward asymmetrical)
    float output_gain; // post-gain [0..1]
} Wavefolder;

void wavefolder_init(Wavefolder *w, float drive, float asym);
void wavefolder_set_drive(Wavefolder *w, float drive);
float wavefolder_process(Wavefolder *w, float input);
void wavefolder_process_block(Wavefolder *w, float *buf, int n);

#ifdef __cplusplus
}
#endif

#endif
