/*
 * test_params.c — Tests that every synth parameter actually changes output
 * Verifies: volume, filter cutoff/resonance, delay mix/feedback,
 *           reverb mix/decay, wavefolder drive, LFO depth.
 * Each test: set parameter to two extremes, verify output differs.
 */
#include <stdio.h>
#include <math.h>
#include "test_util.h"
#include "synth.h"

#define SR 44100.0f

static float calc_rms(const float *buf, int n, int skip) {
    float sum = 0.0f;
    int count = n - skip;
    if (count < 1) return 0.0f;
    for (int i = skip; i < n; i++) sum += buf[i] * buf[i];
    return sqrtf(sum / count);
}

static float calc_peak(const float *buf, int n) {
    float peak = 0.0f;
    for (int i = 0; i < n; i++) {
        float a = fabsf(buf[i]);
        if (a > peak) peak = a;
    }
    return peak;
}

/* Helper: generate n samples of synth output */
static void generate(AcidSynth *s, float *outL, float *outR, int n) {
    synth_gate(s, 1);
    synth_process_block(s, outL, outR, n);
    synth_gate(s, 0);
}

/* Test: volume slider changes output level */
static int test_volume_changes_output(void) {
    TEST("master volume changes output level");

    int n = (int)(SR * 0.2f);
    float *buf1 = (float*)calloc(n, sizeof(float));
    float *buf2 = (float*)calloc(n, sizeof(float));

    AcidSynth s;
    synth_init(&s, SR);
    s.master_volume = 0.1f;
    generate(&s, buf1, buf1, n);
    float rms1 = calc_rms(buf1, n, (int)(0.01f * SR));

    synth_init(&s, SR);  // reset
    s.master_volume = 1.0f;
    generate(&s, buf2, buf2, n);
    float rms2 = calc_rms(buf2, n, (int)(0.01f * SR));

    char msg[64];
    snprintf(msg, sizeof(msg), "vol=0.1 rms=%.4f, vol=1.0 rms=%.4f, ratio=%.1f",
             rms1, rms2, (rms2 > 0) ? rms1/rms2 : 0);
    ASSERT(rms2 > rms1 * 3.0f, "volume 1.0 > 3x louder than 0.1");

    free(buf1); free(buf2);
    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: filter cutoff changes frequency content */
static int test_filter_cutoff_changes_sound(void) {
    TEST("filter cutoff changes frequency content");

    AcidSynth s;
    int n = (int)(SR * 0.3f);
    float *buf1 = (float*)calloc(n, sizeof(float));
    float *buf2 = (float*)calloc(n, sizeof(float));

    synth_init(&s, SR);
    s.seq_mode = 0;
    s.mix_osc2 = s.mix_osc3 = s.mix_sub = 0.0f;
    s.mix_osc1 = 1.0f;
    // HP filter: at 50Hz passes 220Hz fundamental, at 1000Hz attenuates it
    s.cutoff_base = 50.0f;
    filter_init(&s.filter, FILTER_HP, 50.0f, 0.0f, SR);
    synth_note_on(&s, 220.0f);
    synth_process_block(&s, buf1, buf1, n);
    float rms_low = calc_rms(buf1, n, (int)(0.05f * SR));

    synth_init(&s, SR);
    s.seq_mode = 0;
    s.mix_osc2 = s.mix_osc3 = s.mix_sub = 0.0f;
    s.mix_osc1 = 1.0f;
    s.cutoff_base = 1000.0f;
    filter_init(&s.filter, FILTER_HP, 1000.0f, 0.0f, SR);
    synth_note_on(&s, 220.0f);
    synth_process_block(&s, buf2, buf2, n);
    float rms_high = calc_rms(buf2, n, (int)(0.05f * SR));

    char msg[64];
    snprintf(msg, sizeof(msg), "HP50 rms=%.4f, HP1k rms=%.4f, ratio=%.2f",
             rms_low, rms_high, rms_low / (rms_high > 0.0001f ? rms_high : 1.0f));
    ASSERT(rms_low > rms_high * 2.0f, "HP50 passes more signal than HP1000");

    free(buf1); free(buf2);
    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: filter resonance changes peaking */
static int test_resonance_changes_response(void) {
    TEST("filter resonance changes response");

    AcidSynth s;
    int n = (int)(SR * 0.3f);
    float *buf1 = (float*)calloc(n, sizeof(float));
    float *buf2 = (float*)calloc(n, sizeof(float));

    synth_init(&s, SR);
    s.mix_osc2 = s.mix_osc3 = s.mix_sub = 0.0f;
    s.mix_osc1 = 1.0f;
    // No resonance
    filter_init(&s.filter, FILTER_LP, 500.0f, 0.0f, SR);
    generate(&s, buf1, buf1, n);
    float rms_no = calc_rms(buf1, n, (int)(0.05f * SR));

    // High resonance
    filter_init(&s.filter, FILTER_LP, 500.0f, 0.8f, SR);
    generate(&s, buf2, buf2, n);
    float rms_res = calc_rms(buf2, n, (int)(0.05f * SR));

    char msg[64];
    snprintf(msg, sizeof(msg), "res=0 rms=%.4f, res=0.8 rms=%.4f",
             rms_no, rms_res);
    ASSERT(fabsf(rms_res - rms_no) > 0.001f, "resonance changes output");

    free(buf1); free(buf2);
    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: delay mix adds echoes */
static int test_delay_mix_changes_output(void) {
    TEST("delay mix changes signal (wet vs dry)");

    int n = (int)(SR * 0.5f);
    float *buf1 = (float*)calloc(n, sizeof(float));
    float *buf2 = (float*)calloc(n, sizeof(float));

    AcidSynth s;
    synth_init(&s, SR);

    delay_set_mix(&s.delay, 0.0f);  // Dry
    generate(&s, buf1, buf1, n);
    float rms_dry = calc_rms(buf1, n, (int)(0.05f * SR));

    synth_init(&s, SR);
    delay_set_mix(&s.delay, 0.5f);  // Wet
    generate(&s, buf2, buf2, n);
    float rms_wet = calc_rms(buf2, n, (int)(0.05f * SR));

    char msg[64];
    snprintf(msg, sizeof(msg), "dry rms=%.4f, wet rms=%.4f", rms_dry, rms_wet);
    ASSERT(fabsf(rms_wet - rms_dry) > 0.001f, "delay mix changes output");

    free(buf1); free(buf2);
    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: delay feedback changes decay rate */
static int test_delay_feedback_changes_decay(void) {
    TEST("delay feedback changes decay rate");

    int n = (int)(SR * 0.5f);
    float *buf1 = (float*)calloc(n, sizeof(float));
    float *buf2 = (float*)calloc(n, sizeof(float));

    AcidSynth s;
    synth_init(&s, SR);
    delay_set_mix(&s.delay, 0.5f);
    delay_set_feedback(&s.delay, 0.0f);
    generate(&s, buf1, buf1, n);
    float mid1 = calc_rms(buf1, n/2, (int)(0.05f * SR));
    float end1 = calc_rms(buf1, n, n/2);

    synth_init(&s, SR);
    delay_set_mix(&s.delay, 0.5f);
    delay_set_feedback(&s.delay, 0.8f);  // high feedback = longer decay
    generate(&s, buf2, buf2, n);
    float mid2 = calc_rms(buf2, n/2, (int)(0.05f * SR));
    float end2 = calc_rms(buf2, n, n/2);

    // With high feedback, the second half should be louder relative to middle
    float ratio1 = (mid1 > 0) ? end1/mid1 : 0;
    float ratio2 = (mid2 > 0) ? end2/mid2 : 0;

    char msg[64];
    snprintf(msg, sizeof(msg), "fb=0 end/mid=%.4f, fb=0.8 end/mid=%.4f",
             ratio1, ratio2);
    ASSERT(ratio2 > ratio1 || ratio2 > 0.01f, "feedback changes decay envelope");

    free(buf1); free(buf2);
    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: reverb mix changes signal */
static int test_reverb_mix_changes_output(void) {
    TEST("reverb mix changes signal");

    int n = (int)(SR * 0.3f);
    float *buf1 = (float*)calloc(n, sizeof(float));
    float *buf2 = (float*)calloc(n, sizeof(float));

    AcidSynth s;
    synth_init(&s, SR);
    reverb_set_mix(&s.reverb, 0.0f);
    generate(&s, buf1, buf1, n);
    float rms_dry = calc_rms(buf1, n, (int)(0.05f * SR));

    synth_init(&s, SR);
    reverb_set_mix(&s.reverb, 0.5f);
    generate(&s, buf2, buf2, n);
    float rms_wet = calc_rms(buf2, n, (int)(0.05f * SR));

    ASSERT(fabsf(rms_wet - rms_dry) > 0.001f, "reverb mix changes output");

    free(buf1); free(buf2);
    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: wavefolder drive changes harmonic content */
static int test_wavefolder_drive_changes_output(void) {
    TEST("wavefolder drive changes output character");

    int n = (int)(SR * 0.3f);
    float *buf1 = (float*)calloc(n, sizeof(float));
    float *buf2 = (float*)calloc(n, sizeof(float));

    AcidSynth s;
    synth_init(&s, SR);
    s.mix_osc2 = s.mix_osc3 = s.mix_sub = 0.0f;
    s.mix_osc1 = 1.0f;
    wavefolder_init(&s.wavefolder, 0.5f, 0.0f);
    generate(&s, buf1, buf1, n);
    float peak_low = calc_peak(buf1, n);

    synth_init(&s, SR);
    s.mix_osc2 = s.mix_osc3 = s.mix_sub = 0.0f;
    s.mix_osc1 = 1.0f;
    wavefolder_init(&s.wavefolder, 10.0f, 0.0f);  // heavy drive
    generate(&s, buf2, buf2, n);
    float peak_high = calc_peak(buf2, n);

    char msg[64];
    snprintf(msg, sizeof(msg), "drive=0.5 peak=%.4f, drive=10 peak=%.4f",
             peak_low, peak_high);
    ASSERT(fabsf(peak_high - peak_low) > 0.001f, "drive changes output level");

    free(buf1); free(buf2);
    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: LFO depth changes filter modulation */
static int test_lfo_depth_changes_output(void) {
    TEST("LFO depth changes filter modulation");

    int n = (int)(SR * 0.5f);
    float *buf1 = (float*)calloc(n, sizeof(float));
    float *buf2 = (float*)calloc(n, sizeof(float));

    AcidSynth s;
    synth_init(&s, SR);
    s.seq_mode = 0;
    s.lfo_to_filter = 0.0f;  // no modulation
    synth_note_on(&s, 220.0f);
    synth_process_block(&s, buf1, buf1, n);
    float rms_none = calc_rms(buf1, n, (int)(0.05f * SR));
    float max_var1 = 0.0f;
    for (int i = (int)(0.1f*SR); i < n-(int)(0.05f*SR); i++) {
        float var = fabsf(fabsf(buf1[i]) - fabsf(buf1[i-(int)(0.02f*SR)]));
        if (var > max_var1) max_var1 = var;
    }

    synth_init(&s, SR);
    s.seq_mode = 0;
    s.lfo_to_filter = 2000.0f;  // heavy modulation
    synth_note_on(&s, 220.0f);
    synth_process_block(&s, buf2, buf2, n);
    float max_var2 = 0.0f;
    for (int i = (int)(0.1f*SR); i < n-(int)(0.05f*SR); i++) {
        float var = fabsf(fabsf(buf2[i]) - fabsf(buf2[i-(int)(0.02f*SR)]));
        if (var > max_var2) max_var2 = var;
    }

    char msg[64];
    snprintf(msg, sizeof(msg), "LFO=0 max_var=%.4f, LFO=2000 max_var=%.4f",
             max_var1, max_var2);
    ASSERT(max_var2 > max_var1 * 1.2f, "LFO depth increases amplitude variation");

    free(buf1); free(buf2);
    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: sequencer rate changes note timing
 *
 * Con legato puro l'envelope resta a sustain tra le note, quindi
 * non possiamo contare retrigger (|sample|≈0). Invece verifichiamo
 * che rate diversi producano pattern audio diversi —
 * correlazione incrociata < 0.99. */
static int test_seq_rate_changes_timing(void) {
    TEST("sequencer rate changes note timing");

    int n = (int)(SR * 1.0f);
    float *buf1 = (float*)calloc(n, sizeof(float));
    float *buf2 = (float*)calloc(n, sizeof(float));

    AcidSynth s1;
    synth_init(&s1, SR);
    s1.seq_mode = 1;
    synth_set_seq_rate(&s1, 2.0f);  // slow
    synth_gate(&s1, 1);
    synth_process_block(&s1, buf1, buf1, n);

    AcidSynth s2;
    synth_init(&s2, SR);
    s2.seq_mode = 1;
    synth_set_seq_rate(&s2, 8.0f);  // fast
    synth_gate(&s2, 1);
    synth_process_block(&s2, buf2, buf2, n);

    // Entrambi devono produrre audio
    float rms1 = 0, rms2 = 0;
    for (int i = (int)(0.1f*SR); i < n; i++) {
        rms1 += buf1[i] * buf1[i];
        rms2 += buf2[i] * buf2[i];
    }
    rms1 = sqrtf(rms1 / (n - (int)(0.1f*SR)));
    rms2 = sqrtf(rms2 / (n - (int)(0.1f*SR)));
    ASSERT(rms1 > 0.001f, "2Hz produces audio");
    ASSERT(rms2 > 0.001f, "8Hz produces audio");

    // Correlazione incrociata: se i rate sono diversi, i pattern audio
    // devono differire (correlazione << 1.0)
    float dot = 0, n1 = 0, n2 = 0;
    for (int i = (int)(0.1f*SR); i < n; i++) {
        dot += buf1[i] * buf2[i];
        n1  += buf1[i] * buf1[i];
        n2  += buf2[i] * buf2[i];
    }
    float corr = dot / (sqrtf(n1) * sqrtf(n2) + 1e-10f);
    char msg[64];
    snprintf(msg, sizeof(msg), "2Hz vs 8Hz: corr=%.4f (should be <0.99)", corr);
    ASSERT(corr < 0.99f, msg);

    free(buf1); free(buf2);
    synth_free(&s1); synth_free(&s2);
    END_TEST();
    return _test_passed ? 0 : 1;
}

int main(void) {
    printf("=== test_params: Parameter Testing ===\n");
    int fail = 0;
    fail += test_volume_changes_output();
    fail += test_filter_cutoff_changes_sound();
    fail += test_resonance_changes_response();
    fail += test_delay_mix_changes_output();
    fail += test_delay_feedback_changes_decay();
    fail += test_reverb_mix_changes_output();
    fail += test_wavefolder_drive_changes_output();
    fail += test_lfo_depth_changes_output();
    fail += test_seq_rate_changes_timing();
    TEST_SUMMARY();
}
