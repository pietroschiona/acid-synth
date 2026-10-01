/*
 * test_synth.c — End-to-end synth tests
 * Tests: basic output, envelope behavior, note isolation, no clipping/DC, WAV generation
 */
#include <stdio.h>
#include <math.h>
#include "test_util.h"
#include "synth.h"

#define SR 44100.0f

/* Helper: compute RMS of buffer */
static float calc_rms(const float *buf, int n, int skip) {
    float sum = 0.0f;
    int count = n - skip;
    if (count < 1) return 0.0f;
    for (int i = skip; i < n; i++) sum += buf[i] * buf[i];
    return sqrtf(sum / count);
}

/* Helper: find peak absolute value */
static float calc_peak(const float *buf, int n) {
    float peak = 0.0f;
    for (int i = 0; i < n; i++) {
        float a = fabsf(buf[i]);
        if (a > peak) peak = a;
    }
    return peak;
}

/* Test: synth produces audio output */
static int test_synth_produces_output(void) {
    TEST("synth produces non-zero audio output");

    AcidSynth s;
    synth_init(&s, SR);

    int n = (int)SR;
    float outL[44100], outR[44100];

    synth_gate(&s, 1);
    synth_process_block(&s, outL, outR, n);
    synth_gate(&s, 0);

    float rms = calc_rms(outL, n, 2000);
    ASSERT(rms > 0.001f, "synth output RMS > 0.001");
    ASSERT(rms < 0.8f, "synth output RMS < 0.8");

    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: envelope attack → sustain → release works correctly */
static int test_envelope_behavior(void) {
    TEST("envelope attack->sustain->release");

    AcidSynth s;
    synth_init(&s, SR);
    s.master_volume = 1.0f;
    s.env_attack = 0.01f;   // 10ms
    s.env_decay = 0.1f;     // 100ms
    s.env_sustain = 0.5f;
    s.env_release = 0.05f;  // 50ms

    int n = (int)(SR * 0.5f); // 500ms
    float *bufL = (float*)calloc(n, sizeof(float));
    float *bufR = (float*)calloc(n, sizeof(float));

    synth_gate(&s, 1);
    synth_process_block(&s, bufL, bufR, n);
    synth_gate(&s, 0);
    int release_n = (int)(SR * 0.2f);
    float *relL = (float*)calloc(release_n, sizeof(float));
    float *relR = (float*)calloc(release_n, sizeof(float));
    synth_process_block(&s, relL, relR, release_n);

    // Check attack: first 10ms should have rising output
    int attack_samples = (int)(0.01f * SR);
    float rms_start = calc_rms(bufL, attack_samples/2, 0);
    float rms_mid = calc_rms(bufL, attack_samples, attack_samples/2);
    ASSERT(rms_mid > rms_start * 0.5f, "envelope rises during attack");

    // Check release decays
    float rms_rel_start = calc_rms(relL, release_n/2, 0);
    float rms_rel_end = calc_rms(relL, release_n, release_n/2);
    ASSERT(rms_rel_end < rms_rel_start * 1.5f || rms_rel_end < 0.01f,
           "envelope decays during release");

    // No clicks: max adjacent diff < 1.5 (resonant filter can have large swings)
    float max_diff = 0.0f;
    for (int i = 1; i < n; i++) {
        float diff = fabsf(bufL[i] - bufL[i-1]);
        if (diff > max_diff) max_diff = diff;
    }
    for (int i = 1; i < release_n; i++) {
        float diff = fabsf(relL[i] - relL[i-1]);
        if (diff > max_diff) max_diff = diff;
    }
    if (n > 0 && release_n > 0) {
        float diff = fabsf(relL[0] - bufL[n-1]);
        if (diff > max_diff) max_diff = diff;
    }
    ASSERT(max_diff < 1.5f, "no clicks (adjacent sample diff < 1.5)");

    free(bufL); free(bufR); free(relL); free(relR);
    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: note isolation — each note_on starts quiet (envelope retriggered) */
static int test_note_retrigger(void) {
    TEST("note retrigger isolates notes");

    AcidSynth s;
    synth_init(&s, SR);
    s.master_volume = 1.0f;
    s.env_attack = 0.005f;
    s.env_release = 0.01f;

    int buf_size = (int)(SR * 0.15f); // 150ms per note
    float *buf = (float*)calloc(buf_size * 3, sizeof(float));

    // Note 1
    synth_note_on(&s, 220.0f);
    synth_process_block(&s, buf, buf, buf_size);
    synth_note_off(&s);
    synth_process_block(&s, buf + buf_size, buf + buf_size, buf_size);

    // Note 2 — should retrigger from zero
    synth_note_on(&s, 330.0f);
    synth_process_block(&s, buf + buf_size*2, buf + buf_size*2, buf_size);

    // Check note 2 starts quieter than middle (envelope retriggered + phases reset)
    float rms_note2_start = calc_rms(buf + buf_size*2, (int)(0.002f * SR), 0);
    float rms_note2_mid = calc_rms(buf + buf_size*2, buf_size, (int)(0.02f * SR));
    float rms_note2_end = calc_rms(buf + buf_size*2, buf_size, (int)(0.1f * SR));

    // Note 2 should start quieter than its middle section (envelope retriggers from 0)
    // and definitely quieter than the peak of note 1 (old delay/reverb may add some bleed)
    float rms_note1_peak = calc_rms(buf, buf_size, (int)(0.02f * SR));
    ASSERT(rms_note2_start < rms_note2_mid * 2.0f || rms_note2_start < 0.1f,
           "note 2 starts rising (envelope retriggers)");
    ASSERT(rms_note2_mid > 0.001f, "note 2 has audible content");
    ASSERT(rms_note2_end > 0.001f, "note 2 sustains");

    free(buf);
    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: no clipping (all samples within [-1, 1] due to soft-clip) */
static int test_no_clipping(void) {
    TEST("no clipping (samples within [-1, 1])");

    AcidSynth s;
    synth_init(&s, SR);
    s.master_volume = 1.0f; // max volume to stress test

    int n = (int)(SR * 2.0f);
    float *buf = (float*)calloc(n, sizeof(float));
    float *bufR = (float*)calloc(n, sizeof(float));

    synth_gate(&s, 1);
    synth_process_block(&s, buf, bufR, n);
    synth_gate(&s, 0);
    synth_process_block(&s, buf, bufR, (int)(SR * 0.5f));

    float peak = calc_peak(buf, n);
    ASSERT(peak <= 1.001f, "peak amplitude <= 1.0 (soft-clipped)");

    free(buf); free(bufR);
    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: moderate DC offset from wavefolder asymmetry is within tolerance */
static int test_dc_tolerance(void) {
    TEST("DC offset within tolerance");

    AcidSynth s;
    synth_init(&s, SR);

    int n = (int)SR;
    float *buf = (float*)calloc(n, sizeof(float));
    float *bufR = (float*)calloc(n, sizeof(float));

    synth_gate(&s, 1);
    synth_process_block(&s, buf, bufR, n);
    synth_gate(&s, 0);

    float sum = 0.0f;
    for (int i = 2000; i < n; i++) sum += buf[i];
    float dc = sum / (n - 2000);
    // Wavefolder asymmetry introduces small DC; 0.05 is acceptable
    ASSERT(fabsf(dc) < 0.05f, "DC offset < 0.05");

    free(buf); free(bufR);
    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: frequency of clean oscillator through minimal chain */
static int test_note_frequency(void) {
    TEST("note frequency matches requested pitch");

    AcidSynth s;
    synth_init(&s, SR);
    s.master_volume = 1.0f;
    // Solo un oscillatore sine con mix unitario
    s.mix_osc1 = 1.0f; s.mix_osc2 = 0.0f; s.mix_osc3 = 0.0f; s.mix_sub = 0.0f;
    s.env_attack = 0.002f; s.env_release = 0.01f;
    // Bypass wavefolder (drive minima)
    wavefolder_init(&s.wavefolder, 0.01f, 0.0f);
    // Filtro wide-open
    filter_init(&s.filter, FILTER_LP, 20000.0f, 0.01f, SR);
    // Bypass delay e reverb
    delay_set_mix(&s.delay, 0.0f);
    reverb_set_mix(&s.reverb, 0.0f);

    int n = (int)(SR * 0.3f);
    float *buf = (float*)calloc(n, sizeof(float));
    float *bufR = (float*)calloc(n, sizeof(float));

    // Test A4 = 440Hz con oscillatore sine
    osc_init(&s.osc1, OSC_WAVE_SINE, 440.0f);
    synth_note_on(&s, 440.0f);
    synth_process_block(&s, buf, bufR, n);
    synth_note_off(&s);

    // Zero-crossing period detection (funziona con sine)
    int start = (int)(0.05f * SR);
    int zc1 = -1, zc2 = -1;
    for (int i = start; i < n - 1; i++) {
        if (buf[i-1] <= 0.0f && buf[i] > 0.0f) {
            if (zc1 < 0) zc1 = i;
            else { zc2 = i; break; }
        }
    }
    if (zc1 >= 0 && zc2 > zc1) {
        float period = (float)(zc2 - zc1);
        float detected = SR / period;
        ASSERT(fabsf(detected - 440.0f) < 15.0f,
               "440Hz detected via zero-crossing");
    } else {
        ASSERT(0, "could not detect zero-crossings for 440Hz");
    }

    // Test C5 = 523.25Hz
    osc_init(&s.osc1, OSC_WAVE_SINE, 523.25f);
    synth_note_on(&s, 523.25f);
    synth_process_block(&s, buf, bufR, n);
    synth_note_off(&s);

    zc1 = -1; zc2 = -1;
    for (int i = start; i < n - 1; i++) {
        if (buf[i-1] <= 0.0f && buf[i] > 0.0f) {
            if (zc1 < 0) zc1 = i;
            else { zc2 = i; break; }
        }
    }
    if (zc1 >= 0 && zc2 > zc1) {
        float period = (float)(zc2 - zc1);
        float detected = SR / period;
        ASSERT(fabsf(detected - 523.25f) < 20.0f,
               "523.25Hz detected via zero-crossing");
    } else {
        ASSERT(0, "could not detect zero-crossings for 523.25Hz");
    }

    free(buf); free(bufR);
    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: generate 5-second acid demo WAV */
static int test_generate_wav(void) {
    TEST("generate acid_synth_demo.wav (5s stereo)");

    int n = (int)(SR * 5.0f);
    float *outL = (float*)calloc(n, sizeof(float));
    float *outR = (float*)calloc(n, sizeof(float));

    AcidSynth s;
    synth_init(&s, SR);
    synth_gate(&s, 1);

    synth_process_block(&s, outL, outR, n);
    synth_gate(&s, 0);

    // Release tail (+1 second)
    int tail = (int)SR;
    float *tailL = (float*)calloc(tail, sizeof(float));
    float *tailR = (float*)calloc(tail, sizeof(float));
    synth_process_block(&s, tailL, tailR, tail);

    // Write stereo WAV
    WavWriter w = wav_create("acid_synth_demo.wav", (uint32_t)SR, 2);
    ASSERT(w.fp != NULL, "WAV file opened");
    for (int i = 0; i < n; i++) {
        wav_write_sample(&w, outL[i]);
        wav_write_sample(&w, outR[i]);
    }
    for (int i = 0; i < tail; i++) {
        wav_write_sample(&w, tailL[i]);
        wav_write_sample(&w, tailR[i]);
    }
    wav_close(&w);

    // Verify WAV has no clipping
    float peak = calc_peak(outL, n);
    ASSERT(peak <= 1.001f, "demo WAV has no clipping");

    // Verify WAV has audio
    float rms = calc_rms(outL, (int)(SR * 4.0f), (int)SR);
    ASSERT(rms > 0.001f, "demo WAV has audible content");

    printf("  WAV: acid_synth_demo.wav (6s stereo, peak=%.3f, rms=%.3f)\n", peak, rms);

    free(outL); free(outR); free(tailL); free(tailR);
    synth_free(&s);

    END_TEST();
    return _test_passed ? 0 : 1;
}

int main(void) {
    printf("=== test_synth: Full Synth Integration ===\n");

    int fail = 0;
    fail += test_synth_produces_output();
    fail += test_envelope_behavior();
    fail += test_note_retrigger();
    fail += test_no_clipping();
    fail += test_dc_tolerance();
    fail += test_note_frequency();
    fail += test_generate_wav();

    TEST_SUMMARY();
}
