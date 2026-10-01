/*
 * test_filter.c — Test biquad filter module
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "test_util.h"
#include "filter.h"
#include "osc.h"

#define SR 44100.0f

/* Test: DC gain of LP filter at freq=0 should be 1.0 (all frequencies pass) */
static int test_lp_dc_gain(void) {
    TEST("LP filter DC gain = 1.0");

    Filter f;
    filter_init(&f, FILTER_LP, 20000.0f, 0.0f, SR); // Nyquist

    // Feed DC (value 1.0) for 100 samples, check output stabilizes
    float out[200];
    for (int i = 0; i < 200; i++) out[i] = filter_process(&f, 1.0f);

    ASSERT_FLOAT_EQ(out[199], 1.0f, 0.01f, "DC should pass through LP");
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: LP filter attenuates high frequencies */
static int test_lp_attenuation(void) {
    TEST("LP filter attenuates 10kHz at cutoff=500Hz");

    // Generate 10kHz saw wave, run through 500Hz LP filter
    float buf[4410]; // 100ms
    Osc o;
    osc_init(&o, OSC_WAVE_SAW, 10000.0f);
    osc_process_block(&o, buf, 4410, SR);

    float orig_rms = 0.0f;
    for (int i = 0; i < 4410; i++) orig_rms += buf[i] * buf[i];
    orig_rms = sqrtf(orig_rms / 4410.0f);

    Filter f;
    filter_init(&f, FILTER_LP, 500.0f, 0.0f, SR);
    filter_process_block(&f, buf, 4410);

    float filtered_rms = 0.0f;
    for (int i = 0; i < 4410; i++) filtered_rms += buf[i] * buf[i];
    filtered_rms = sqrtf(filtered_rms / 4410.0f);

    // Filtered should be significantly quieter
    float ratio = filtered_rms / orig_rms;
    ASSERT(ratio < 0.5f, "filtered RMS should be < 50% of original at 10kHz/500Hz cutoff");
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: resonance creates peak at cutoff */
static int test_lp_resonance(void) {
    TEST("LP filter resonance peaks near cutoff");

    // White noise → resonant LP filter → dominant frequency should be near cutoff
    float in[4096];
    Osc noise;
    osc_init(&noise, OSC_WAVE_NOISE, 1.0f);
    osc_process_block(&noise, in, 4096, SR);

    Filter f;
    filter_init(&f, FILTER_LP, 1000.0f, 0.85f, SR);
    filter_process_block(&f, in, 4096);

    float detected = detect_frequency(in, 4096, (int)SR);
    ASSERT(detected > 500.0f && detected < 2000.0f,
           "resonant peak should be near cutoff (1000Hz)");
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: HP filter passes high frequencies */
static int test_hp_passes_high(void) {
    TEST("HP filter passes 5kHz with 1kHz cutoff");

    float buf[2048];
    Osc o;
    osc_init(&o, OSC_WAVE_SAW, 5000.0f);
    osc_process_block(&o, buf, 2048, SR);

    float orig_rms = 0.0f;
    for (int i = 0; i < 2048; i++) orig_rms += buf[i] * buf[i];
    orig_rms = sqrtf(orig_rms / 2048.0f);

    Filter f;
    filter_init(&f, FILTER_HP, 1000.0f, 0.0f, SR);
    filter_process_block(&f, buf, 2048);

    float rms = 0.0f;
    for (int i = 0; i < 2048; i++) rms += buf[i] * buf[i];
    rms = sqrtf(rms / 2048.0f);

    ASSERT(rms > orig_rms * 0.5f, "HP should pass most of 5kHz with 1kHz cutoff");
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: resonance self-oscillation at max resonance */
static int test_self_oscillation(void) {
    TEST("LP filter self-oscillates at resonance=1.0");

    float buf[4410];
    memset(buf, 0, sizeof(buf));

    // Give one impulse
    buf[0] = 1.0f;

    Filter f;
    filter_init(&f, FILTER_LP, 440.0f, 1.0f, SR);
    filter_process_block(&f, buf, 4410);

    // Should oscillate at ~440Hz after impulse
    // Q max = 15: l'oscillazione decade gradualmente, ma nei primi
    // 200 campioni (≈9 cicli a 440Hz) è ancora forte.
    float early_peak = 0.0f;
    for (int i = 10; i < 200; i++) {
        float absv = fabsf(buf[i]);
        if (absv > early_peak) early_peak = absv;
    }
    ASSERT(early_peak > 0.05f, "self-oscillation peaks within first cycles");

    float detected = detect_frequency(buf + 10, 200, (int)SR);
    ASSERT_FLOAT_EQ(detected, 440.0f, 50.0f, "self-oscillation near cutoff freq");

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: generate WAV for manual listening */
static int test_generate_wav(void) {
    TEST("generate filter sweep WAV");

    // Saw through resonant LP with sweep
    float buf[88200]; // 2 seconds
    Osc o;
    osc_init(&o, OSC_WAVE_SAW, 220.0f);
    osc_process_block(&o, buf, 88200, SR);

    Filter f;
    filter_init(&f, FILTER_LP, 100.0f, 0.7f, SR);

    // Sweep cutoff from 100Hz to 8kHz over 2 seconds
    for (int i = 0; i < 88200; i++) {
        float t = (float)i / SR;
        float freq = 100.0f + (8000.0f - 100.0f) * (t / 2.0f);
        filter_set_freq(&f, freq);
        buf[i] = filter_process(&f, buf[i]);
    }

    WavWriter w = wav_create("filter_sweep.wav", (uint32_t)SR, 1);
    ASSERT(w.fp != NULL, "WAV file opened");
    for (int i = 0; i < 88200; i++) wav_write_sample(&w, buf[i]);
    wav_close(&w);
    END_TEST();
    return _test_passed ? 0 : 1;
}

int main(void) {
    printf("=== test_filter: Biquad Filter Module ===\n");

    int fail = 0;
    fail += test_lp_dc_gain();
    fail += test_lp_attenuation();
    fail += test_lp_resonance();
    fail += test_hp_passes_high();
    fail += test_self_oscillation();
    fail += test_generate_wav();

    TEST_SUMMARY();
}
