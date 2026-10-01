#include "wavefolder.h"
#include <math.h>

void wavefolder_init(Wavefolder *w, float drive, float asym) {
    w->asym = asym;
    w->output_gain = 1.5f;  // boost fisso per la componente wet
    wavefolder_set_drive(w, drive);
}

void wavefolder_set_drive(Wavefolder *w, float drive) {
    if (drive < 0.0f) drive = 0.0f;
    w->drive = drive;
}

/* Soft clipping sigmoid (Pirkle cap 19, eq 19.1) */
static float soft_clip(float x) {
    // Tanh soft clipping
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    return tanhf(x);
}

/* Asymmetric waveshaping */
static float asym_shape(float x, float amount) {
    // Blend symmetric and asymmetric
    float sym = soft_clip(x);
    float asym = soft_clip(x * 1.5f + 0.5f) * 0.8f - 0.1f;
    return sym * (1.0f - amount) + asym * amount;
}

/* Multi-stage wavefolder (from Pd patch acid_synth_hvcc) */
static float wavefold(float x, float drive) {
    float s = x * drive;

    // Stage 1: clip at ±0.8
    if (s > 0.8f) s = 0.8f;
    if (s < -0.8f) s = -0.8f;

    // Stage 2: amplify 1.25x, clip at ±0.6
    s *= 1.25f;
    if (s > 0.6f) s = 0.6f;
    if (s < -0.6f) s = -0.6f;

    // Stage 3: amplify 1.67x, clip at ±0.9
    s *= 1.67f;
    if (s > 0.9f) s = 0.9f;
    if (s < -0.9f) s = -0.9f;

    // Output normalize
    return s * 0.9f;
}

float wavefolder_process(Wavefolder *w, float input) {
    /* drive ≈ 0 = true bypass (segnale pulito, volume pieno) */
    if (w->drive < 0.01f) return input;

    float s = input;

    // Distortion processing
    if (w->drive < 1.0f) {
        // Low drive: soft clip only
        s = soft_clip(s * w->drive * 0.5f);
    } else {
        // High drive: wavefolder
        s = wavefold(s, w->drive);
    }

    // Asymmetry
    s = asym_shape(s, w->asym);

    /* Dry/wet mix: drive 0→0% wet, drive≥2→100% wet.
     *
     * A drive basso il segnale pulito (dry) passa quasi intatto,
     * quindi il volume resta normale ANCHE senza effetto percepibile.
     * A drive alto la componente wet domina e l'effetto è pieno. */
    float wet = fminf(w->drive * 0.5f, 1.0f);
    return input * (1.0f - wet) + s * w->output_gain * wet;
}

void wavefolder_process_block(Wavefolder *w, float *buf, int n) {
    for (int i = 0; i < n; i++) {
        buf[i] = wavefolder_process(w, buf[i]);
    }
}
