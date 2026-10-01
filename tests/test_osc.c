/*
 * test_osc.c — Test oscillator module
 * RED phase: oscillatore non esiste o produce frequenza/ampiezza sbagliate
 * GREEN phase: tutte le verifiche passano
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "test_util.h"
#include "osc.h"

#define SR 44100.0f
#define DURATION 1.0f
#define N_SAMPLES ((int)(SR * DURATION))

/* Zero-crossing frequency detection (efficient for large buffers) */
static float detect_freq_zc(const float *buf, int n, float sr) {
    int crossings = 0;
    int zero_idx = -1;
    for (int i = 1; i < n; i++) {
        if (buf[i-1] <= 0.0f && buf[i] > 0.0f) {
            crossings++;
            if (crossings == 1) zero_idx = i;
        }
    }
    if (crossings < 4) return 0.0f;
    // Use middle section for accuracy
    int start = zero_idx;
    int end = n - 1;
    // Find last crossing
    for (int i = n-1; i > n/2; i--) {
        if (buf[i-1] <= 0.0f && buf[i] > 0.0f) {
            end = i;
            break;
        }
    }
    int cycles = 0;
    for (int i = start; i < end; i++) {
        if (buf[i-1] <= 0.0f && buf[i] > 0.0f) cycles++;
    }
    if (cycles < 2) return 0.0f;
    float duration = (float)(end - start) / sr;
    return (float)cycles / duration;
}

/* Test: frequency accuracy */
static int test_freq_accuracy(void) {
    TEST("saw 440Hz frequency accuracy");

    float buf[N_SAMPLES / 4]; // 11025 samples, ~250ms — plenty
    int n = sizeof(buf) / sizeof(buf[0]);
    Osc o;
    osc_init(&o, OSC_WAVE_SAW, 440.0f);
    osc_process_block(&o, buf, n, SR);

    float detected = detect_freq_zc(buf, n, SR);
    ASSERT_FLOAT_EQ(detected, 440.0f, 5.0f, "saw 440Hz should be ~440Hz");

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: amplitude accuracy */
static int test_amp_accuracy(void) {
    TEST("saw amplitude = 1.0");

    float buf[4096]; // 4096 samples, about 93ms
    Osc o;
    osc_init(&o, OSC_WAVE_SAW, 100.0f);
    osc_set_amp(&o, 1.0f);
    osc_process_block(&o, buf, 4096, SR);

    // Peak should be ~1.0 (saw is bipolar)
    float peak = 0.0f;
    for (int i = 0; i < 4096; i++) {
        float absv = fabsf(buf[i]);
        if (absv > peak) peak = absv;
    }
    ASSERT_FLOAT_EQ(peak, 1.0f, 0.01f, "peak amplitude should be 1.0");
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: set amplitude works */
static int test_set_amp(void) {
    TEST("set amplitude = 0.5");

    float buf[4096];
    Osc o;
    osc_init(&o, OSC_WAVE_SAW, 100.0f);
    osc_set_amp(&o, 0.5f);
    osc_process_block(&o, buf, 4096, SR);

    float peak = 0.0f;
    for (int i = 0; i < 4096; i++) {
        float absv = fabsf(buf[i]);
        if (absv > peak) peak = absv;
    }
    ASSERT_FLOAT_EQ(peak, 0.5f, 0.01f, "peak amplitude should be 0.5");
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: sine wave produces clean sinusoid */
static int test_sine(void) {
    TEST("sine 440Hz approximation");

    float buf[4096];
    Osc o;
    osc_init(&o, OSC_WAVE_SINE, 440.0f);
    osc_process_block(&o, buf, 4096, SR);

    float detected = detect_freq_zc(buf, 4096, SR);
    ASSERT_FLOAT_EQ(detected, 440.0f, 5.0f, "sine 440Hz frequency");

    // Check DC offset (should be near 0 for sine)
    float sum = 0.0f;
    for (int i = 0; i < 4096; i++) sum += buf[i];
    float dc = sum / 4096.0f;
    ASSERT_FLOAT_EQ(dc, 0.0f, 0.01f, "sine should have near-zero DC");

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: square wave duty cycle */
static int test_square_pw(void) {
    TEST("square wave 25% duty cycle");

    float buf[4096];
    Osc o;
    osc_init(&o, OSC_WAVE_SQUARE, 100.0f);
    osc_set_pulse_width(&o, 0.25f);
    osc_process_block(&o, buf, 4096, SR);

    // Count positive samples in steady state
    int pos = 0, total = 0;
    int half = 4096 / 2;
    for (int i = half; i < 4096; i++) {
        if (buf[i] > 0) pos++;
        total++;
    }
    float ratio = (float)pos / total;
    ASSERT_FLOAT_EQ(ratio, 0.25f, 0.03f, "25% duty cycle should have ~25% positive samples");
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: waveform types produce output */
static int test_all_waveforms(void) {
    TEST("all waveform types produce non-zero output");

    for (int w = 0; w < OSC_WAVE_COUNT; w++) {
        float buf[4096];
        Osc o;
        osc_init(&o, (OscWaveform)w, 200.0f);
        osc_process_block(&o, buf, 4096, SR);

        float rms = 0.0f;
        for (int i = 0; i < 4096; i++) rms += buf[i] * buf[i];
        rms = sqrtf(rms / 4096.0f);

        char msg[64];
        snprintf(msg, sizeof(msg), "waveform %d RMS > 0", w);
        ASSERT(rms > 0.001f, msg);
    }

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: generates WAV for manual verification */
static int test_generate_wav(void) {
    TEST("generate osc_440.wav for manual check");

    float buf[N_SAMPLES];
    Osc o;
    osc_init(&o, OSC_WAVE_SAW, 440.0f);
    osc_process_block(&o, buf, N_SAMPLES, SR);

    WavWriter w = wav_create("osc_440.wav", (uint32_t)SR, 1);
    ASSERT(w.fp != NULL, "WAV file opened");
    for (int i = 0; i < N_SAMPLES; i++) {
        wav_write_sample(&w, buf[i]);
    }
    wav_close(&w);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: multi-oscillator detune pattern (for 3-VCO acid) */
static int test_detune(void) {
    TEST("3 oscillators detuned produce audible beating");

    float mix[4096];
    memset(mix, 0, sizeof(mix));

    for (int i = 0; i < 3; i++) {
        float buf[4096];
        Osc o;
        float base = 220.0f;
        float detune[3] = {0.0f, 0.3f, -0.3f}; // Hz offset
        osc_init(&o, OSC_WAVE_SAW, base + detune[i]);
        osc_set_amp(&o, 0.33f);
        osc_process_block(&o, buf, 4096, SR);
        for (int s = 0; s < 4096; s++) mix[s] += buf[s];
    }

    // Should have energy at multiple frequencies near 220Hz
    float detected = detect_freq_zc(mix, 4096, SR);
    ASSERT_FLOAT_EQ(detected, 220.0f, 10.0f, "detuned saws should be near 220Hz");
    END_TEST();
    return _test_passed ? 0 : 1;
}

int main(void) {
    printf("=== test_osc: Oscillator Module ===\n");

    int fail = 0;
    fail += test_freq_accuracy();
    fail += test_amp_accuracy();
    fail += test_set_amp();
    fail += test_sine();
    fail += test_square_pw();
    fail += test_all_waveforms();
    fail += test_generate_wav();
    fail += test_detune();

    TEST_SUMMARY();
}
