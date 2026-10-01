/*
 * analyze_click.c — Generate 10s demo WAV + analyze discontinuities
 * Mathematical analysis of the "click + step" artefact
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "synth.h"
#include "scale.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SR 44100.0f
#define DURATION 10.0f
#define NSAMP (int)(SR * DURATION)

int main() {
    AcidSynth s;
    synth_init(&s, SR);

    /* Use default dreamscape settings */
    s.seq_mode = 1;
    s.master_volume = 0.8f;

    float *outL = (float*)calloc(NSAMP, sizeof(float));
    float *outR = (float*)calloc(NSAMP, sizeof(float));

    /* Allocate sample-by-sample state tracking */
    float *env_track = (float*)calloc(NSAMP, sizeof(float));
    float *gate_track = (int*)calloc(NSAMP, sizeof(int));
    float *diff_track = (float*)calloc(NSAMP, sizeof(float));

    /* Process sample-by-sample so we can track envelope at every point */
    synth_gate(&s, 1);
    for (int i = 0; i < NSAMP; i++) {
        synth_process(&s, &outL[i], &outR[i]);
        env_track[i] = s.envelope;
        gate_track[i] = s.gate_on;
    }
    synth_gate(&s, 0);

    /* Add release tail (1.5s) */
    int tail = (int)(SR * 1.5f);
    outL = (float*)realloc(outL, (NSAMP + tail) * sizeof(float));
    outR = (float*)realloc(outR, (NSAMP + tail) * sizeof(float));
    env_track = (float*)realloc(env_track, (NSAMP + tail) * sizeof(float));
    gate_track = (int*)realloc(gate_track, (NSAMP + tail) * sizeof(int));
    diff_track = (float*)realloc(diff_track, (NSAMP + tail) * sizeof(float));

    for (int i = 0; i < tail; i++) {
        synth_process(&s, &outL[NSAMP + i], &outR[NSAMP + i]);
        env_track[NSAMP + i] = s.envelope;
        gate_track[NSAMP + i] = s.gate_on;
    }

    int total = NSAMP + tail;

    /* Compute sample-to-sample differences */
    float max_diff = 0.0f;
    int max_diff_idx = 0;
    for (int i = 1; i < total; i++) {
        float diff = fabsf(outL[i] - outL[i-1]);
        diff_track[i] = diff;
        if (diff > max_diff) {
            max_diff = diff;
            max_diff_idx = i;
        }
    }

    /* Find all discontinuities > 0.01 (audible click threshold) */
    printf("=== ANALISI DISCONTINUITA' ===\n");
    printf("Sample rate: %.0f Hz\n", SR);
    printf("Duration: %.1f s (%d samples)\n", DURATION, total);
    printf("\n");

    printf("CAMBI ENVELOPPE > 0.05/sample:\n");
    int env_jumps = 0;
    for (int i = 1; i < total; i++) {
        float env_diff = fabsf(env_track[i] - env_track[i-1]);
        if (env_diff > 0.05f) {
            float t = (float)i / SR;
            printf("  t=%.3fs sample=%d env: %.4f → %.4f (Δ=%.4f)\n",
                   t, i, env_track[i-1], env_track[i], env_diff);
            env_jumps++;
        }
    }
    if (env_jumps == 0) printf("  (nessun jump > 0.05)\n");
    printf("\n");

    printf("DISCONTINUITA' AUDIO > 0.01 (possibili click):\n");
    int click_count = 0;
    for (int i = 2; i < total; i++) {
        if (diff_track[i] > 0.01f) {
            float t = (float)i / SR;
            /* Report context: 5 samples before and after */
            int start = (i - 3 > 0) ? i - 3 : 0;
            int end = (i + 3 < total) ? i + 3 : total;
            printf("  t=%.3fs sample=%d Δ=%.6f\n", t, i, diff_track[i]);
            printf("    context [%d..%d]: ", start, end);
            for (int j = start; j < end; j++)
                printf("%.4f ", outL[j]);
            printf("\n");
            printf("    env@hit: %.4f gate=%d\n", env_track[i], gate_track[i]);
            click_count++;
            if (click_count > 20) {
                printf("  ... (%d total clicks, showing first 20)\n", click_count);
                break;
            }
        }
    }
    if (click_count == 0) printf("  (nessun click > 0.01)\n");
    printf("\n");

    printf("STATISTICHE:\n");
    printf("  Peak: %.4f\n", outL[0]);
    float max_abs = 0;
    for (int i = 0; i < total; i++) {
        float a = fabsf(outL[i]);
        if (a > max_abs) max_abs = a;
    }
    printf("  Max abs: %.4f\n", max_abs);
    printf("  Max discontinuity: %.6f @ sample %d (t=%.3fs)\n",
           max_diff, max_diff_idx, (float)max_diff_idx / SR);

    /* RMS */
    float rms_sum = 0;
    for (int i = 5000; i < total - 5000; i++)
        rms_sum += outL[i] * outL[i];
    float rms = sqrtf(rms_sum / (total - 10000));
    printf("  RMS (excl edges): %.4f\n", rms);

    /* Envelope stats */
    float env_max = 0, env_min = 1, env_avg = 0;
    for (int i = 0; i < total; i++) {
        if (env_track[i] > env_max) env_max = env_track[i];
        if (env_track[i] < env_min) env_min = env_track[i];
        env_avg += env_track[i];
    }
    env_avg /= total;
    printf("  Envelope: min=%.4f max=%.4f avg=%.4f\n", env_min, env_max, env_avg);

    /* Write WAV */
    {
        FILE *fp = fopen("demo_click.wav", "wb");
        if (fp) {
            short fmt = 1; int sr_int = 44100;
            fwrite("RIFF", 1, 4, fp);
            int data_sz = total * 4;
            int riff_sz = 36 + data_sz;
            fwrite(&riff_sz, 4, 1, fp);
            fwrite("WAVE", 1, 4, fp);
            fwrite("fmt ", 1, 4, fp);
            int fmt_sz = 16;
            fwrite(&fmt_sz, 4, 1, fp);
            fwrite(&fmt, 2, 1, fp);
            short ch = 2;
            fwrite(&ch, 2, 1, fp);
            fwrite(&sr_int, 4, 1, fp);
            int bps = sr_int * 4;
            fwrite(&bps, 4, 1, fp);
            short ba = 4;
            fwrite(&ba, 2, 1, fp);
            short bits = 16;
            fwrite(&bits, 2, 1, fp);
            fwrite("data", 1, 4, fp);
            fwrite(&data_sz, 4, 1, fp);
            for (int i = 0; i < total; i++) {
                short sl = (short)(outL[i] * 32767.0f);
                short sr_ = (short)(outR[i] * 32767.0f);
                fwrite(&sl, 2, 1, fp);
                fwrite(&sr_, 2, 1, fp);
            }
            fclose(fp);
            printf("\nWAV: demo_click.wav (%d samples, %.1fs)\n", total, (float)total/SR);
        }
    }

    free(outL); free(outR);
    free(env_track); free(gate_track); free(diff_track);
    synth_free(&s);
    return 0;
}
