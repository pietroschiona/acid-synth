/*
 * deep_analyze.c — Per-sample analysis of ALL intermediate signals
 * Traces every stage: osc_mix, env, wavefolder, filter, delay, reverb, soft_clip
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "synth.h"
#include "scale.h"
#include "osc.h"
#include "filter.h"
#include "wavefolder.h"
#include "delay.h"
#include "reverb.h"
#include "lfo.h"

#define SR 44100.0f
#define DURATION 10.0f

static float sc(float x) {
    if (x > 1.0f) return 1.0f;
    if (x < -1.0f) return -1.0f;
    return x * (1.5f - 0.5f * x * x);
}

static float upd_env(AcidSynth *s, float dt) {
    if (s->gate_on) {
        if (s->envelope < s->env_sustain) {
            s->envelope += dt / s->env_attack;
            if (s->envelope > s->env_sustain) s->envelope = s->env_sustain;
        } else if (s->envelope > s->env_sustain) {
            s->envelope -= dt / s->env_decay;
            if (s->envelope < s->env_sustain) s->envelope = s->env_sustain;
        }
    } else {
        if (s->envelope > 0.0f) {
            s->envelope -= dt / s->env_release;
            if (s->envelope < 0.0f) s->envelope = 0.0f;
        }
    }
    return s->envelope;
}

int main() {
    AcidSynth s;
    synth_init(&s, SR);
    s.seq_mode = 1;
    s.master_volume = 0.8f;

    /* Default dreamscape rate: ~2Hz */
    float seq_rate = s.seq.rate;

    int ns = (int)(SR * DURATION) + (int)SR;
    float *out = (float*)calloc(ns, sizeof(float));
    float *env_t = (float*)calloc(ns, sizeof(float));
    float *osc_mix = (float*)calloc(ns, sizeof(float));
    float *folded = (float*)calloc(ns, sizeof(float));
    float *flt = (float*)calloc(ns, sizeof(float));
    float *dly = (float*)calloc(ns, sizeof(float));
    float *rvb = (float*)calloc(ns, sizeof(float));
    float *notef = (float*)calloc(ns, sizeof(float));
    int *tick = (int*)calloc(ns, sizeof(int));

    synth_gate(&s, 1);
    for (int i = 0; i < ns; i++) {
        float dt = 1.0f / SR;
        int seq_ticked = 0;

        if (sequencer_tick(&s.seq)) {
            seq_ticked = 1;
            if (s.seq_mode) {
                int step = sequencer_get_step(&s.seq);
                int degree = s.seq_degrees[step];
                float note_freq = synth_scale_degree_freq(&s, degree);
                synth_set_note(&s, note_freq);
                s.gate_on = 1;
            }
        }

        float env = upd_env(&s, dt);
        if (env <= 0.0f && !s.gate_on) {
            s.gate_on = 0;
            filter_reset(&s.filter);
            delay_reset(&s.delay);
            reverb_reset(&s.reverb);
        }

        float lfo_val = lfo_process(&s.lfo);
        float osc1 = osc_process(&s.osc1, SR);
        float osc2 = osc_process(&s.osc2, SR);
        float osc3 = osc_process(&s.osc3, SR);
        float sub  = osc_process(&s.osc_sub, SR);

        float mix = osc1 * s.mix_osc1 + osc2 * s.mix_osc2 + osc3 * s.mix_osc3 + sub * s.mix_sub;
        mix *= env;

        float fld = wavefolder_process(&s.wavefolder, mix);

        float cutoff = s.cutoff_base + lfo_val * s.lfo_to_filter;
        if (cutoff < 50.0f) cutoff = 50.0f;
        if (cutoff > SR * 0.45f) cutoff = SR * 0.45f;
        filter_set_freq(&s.filter, cutoff);
        filter_set_resonance(&s.filter, s.reso_base);
        float filt = filter_process(&s.filter, fld);

        float dly_sig = delay_process(&s.delay, filt);
        float rvb_sig = reverb_process(&s.reverb, dly_sig);

        float wet = rvb_sig * s.master_volume;
        wet = sc(wet);

        out[i] = wet;
        env_t[i] = env;
        osc_mix[i] = mix;
        folded[i] = fld;
        flt[i] = filt;
        dly[i] = dly_sig;
        rvb[i] = rvb_sig;
        notef[i] = s.osc1.freq;
        tick[i] = seq_ticked;
    }
    synth_gate(&s, 0);
    synth_free(&s);

    /* === ANALYSIS === */

    printf("=== ANALISI MULTI-STADIO (seq_rate=%.2fHz) ===\n\n", seq_rate);

    const char *stages[] = {"FINAL_OUT", "ENVELOPE", "OSC_MIX", "WAVEFOLDER", "FILTER", "DELAY", "REVERB"};
    float *data[] = {out, env_t, osc_mix, folded, flt, dly, rvb};
    int n_stages = 7;

    /* Max diffs at tick vs non-tick */
    printf("--- MAX DISCONTINUITA' PER STADIO ---\n");
    for (int st = 0; st < n_stages; st++) {
        float max_t = 0, max_nt = 0;
        int idx_t = 0, idx_nt = 0;
        for (int i = 5; i < ns - 5; i++) {
            float d = fabsf(data[st][i] - data[st][i-1]);
            int near_tick = 0;
            for (int j = i-5; j <= i+5; j++)
                if (j >= 0 && j < ns && tick[j]) { near_tick = 1; break; }
            if (near_tick) {
                if (d > max_t) { max_t = d; idx_t = i; }
            } else {
                if (d > max_nt) { max_nt = d; idx_nt = i; }
            }
        }
        printf("  %s:  tick_max=%.6f @%d(%.3fs)  non_tick_max=%.6f @%d(%.3fs)\n",
               stages[st], max_t, idx_t, (float)idx_t/SR,
               max_nt, idx_nt, (float)idx_nt/SR);
    }

    /* Find max tick-correlated diff in final output */
    printf("\n--- TOP 10 TICK DISCONTINUITA' (FINAL_OUT) ---\n");
    int found_tick = 0;
    for (int i = 10; i < ns - 10; i++) {
        if (tick[i]) {
            float d = fabsf(out[i] - out[i-1]);
            if (d > 0.01f && found_tick < 10) {
                found_tick++;
                printf("  #%d t=%.3fs samp=%d diff=%.6f env=%.4f freq=%.1f\n",
                       found_tick, (float)i/SR, i, d, env_t[i], notef[i]);
                printf("    out[%d..%d]: ", i-3, i+3);
                for (int j = i-3; j <= i+3; j++)
                    printf("%.4f ", out[j]);
                printf("\n");
                printf("    filter[%d]:%.4f delay[%d]:%.4f reverb[%d]:%.4f\n",
                       i, flt[i], i, dly[i], i, rvb[i]);
            }
        }
    }
    if (found_tick == 0) printf("  (nessuna diff > 0.01 ai tick)\n");

    /* Find tick-correlated DIFFERENCES BETWEEN last non-tick sample and tick sample */
    printf("\n--- ANALISI: CAMBIO FASE OSCILLATORE AL TICK ---\n");
    found_tick = 0;
    for (int i = 15; i < ns - 5; i++) {
        if (tick[i]) {
            /* Compare: envelope-in value at tick vs 1 sample before */
            /* If envelope hasn't changed, any click is from oscillator freq change */
            float env_before = env_t[i-1];
            float env_after = env_t[i];
            float env_delta = fabsf(env_after - env_before);

            /* Check if filter caused the jump */
            float filt_before = flt[i-1];
            float filt_after = flt[i];
            float filt_delta = fabsf(filt_after - filt_before);

            /* Check if reverb caused the jump */
            float rvb_before = rvb[i-1];
            float rvb_after = rvb[i];
            float rvb_delta = fabsf(rvb_after - rvb_before);

            /* Check if delay caused the jump */
            float dly_before = dly[i-1];
            float dly_after = dly[i];
            float dly_delta = fabsf(dly_after - dly_before);

            if (env_delta > 0.001f && found_tick < 5) {
                found_tick++;
                printf("  #%d t=%.3fs env_delta=%.6f (ENVELOPE CAUSED THIS!)\n",
                       found_tick, (float)i/SR, env_delta);
            }
            if (filt_delta > 0.01f && found_tick < 10) {
                found_tick++;
                printf("  #%d t=%.3fs freq=%.1f->%.1f env=%.4f filt=%.4f->%.4f dly=%.4f->%.4f rvb=%.4f->%.4f | out=%.4f->%.4f\n",
                       found_tick, (float)i/SR,
                       notef[i-1], notef[i],
                       env_t[i],
                       flt[i-1], flt[i],
                       dly[i-1], dly[i],
                       rvb[i-1], rvb[i],
                       out[i-1], out[i]);
            }
        }
    }
    if (found_tick == 0) printf("  (tutti i tick sono smooth)\n");

    printf("\n");
    printf("--- TUTTI I TICK: diff del segnale ---\n");
    found_tick = 0;
    for (int i = 10; i < ns - 10; i++) {
        if (tick[i]) {
            found_tick++;
            float out_d = fabsf(out[i] - out[i-1]);
            float filt_d = fabsf(flt[i] - flt[i-1]);
            float rvb_d = fabsf(rvb[i] - rvb[i-1]);
            printf("  tick#%d t=%.3fs outΔ=%.6f filtΔ=%.6f rvbΔ=%.6f env=%.4f freq=%.1f\n",
                   found_tick, (float)i/SR, out_d, filt_d, rvb_d, env_t[i], notef[i]);
        }
    }

    /* Write WAV */
    {
        FILE *fp = fopen("deep_analyze.wav", "wb");
        if (fp) {
            short fmt = 1; int sr_int = 44100;
            int data_sz = ns * 4;
            int riff_sz = 36 + data_sz;
            fwrite("RIFF", 1, 4, fp);
            fwrite(&riff_sz, 4, 1, fp);
            fwrite("WAVE", 1, 4, fp);
            fwrite("fmt ", 1, 4, fp);
            int fsz = 16;
            fwrite(&fsz, 4, 1, fp);
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
            for (int i = 0; i < ns; i++) {
                short sl = (short)(out[i] * 32767.0f);
                fwrite(&sl, 2, 1, fp);
                fwrite(&sl, 2, 1, fp);
            }
            fclose(fp);
        }
    }

    free(out); free(env_t); free(osc_mix); free(folded);
    free(flt); free(dly); free(rvb); free(notef); free(tick);

    printf("\nWAV: deep_analyze.wav\n");
    return 0;
}
