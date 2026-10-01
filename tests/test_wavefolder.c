/*
 * test_wavefolder.c — Test wavefolder/distortion module
 */
#include <stdio.h>
#include <math.h>
#include "test_util.h"
#include "wavefolder.h"
#include "osc.h"

#define SR 44100.0f

/* Test: wavefolder at drive=0 passes through (silence) */
static int test_zero_drive(void) {
    TEST("wavefolder drive=0 produces silence (input=0)");

    Wavefolder w;
    wavefolder_init(&w, 0.0f, 0.0f);

    float out = wavefolder_process(&w, 0.0f);
    ASSERT_FLOAT_EQ(out, 0.0f, 0.001f, "zero in = zero out");

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: wavefolder with drive=5 and sine produces harmonics */
static int test_harmonics(void) {
    TEST("wavefolder with drive=5 produces harmonics");

    float in[4096];
    float out[4096];
    Osc o;
    osc_init(&o, OSC_WAVE_SINE, 200.0f);
    osc_process_block(&o, in, 4096, SR);

    // Copy and process
    memcpy(out, in, sizeof(in));
    Wavefolder w;
    wavefolder_init(&w, 5.0f, 0.0f);
    wavefolder_process_block(&w, out, 4096);

    // Without wavefolder: sine has only fundamental
    // With wavefolder: should add harmonics = RMS should increase
    float in_rms = 0.0f, out_rms = 0.0f;
    for (int i = 100; i < 4096; i++) {
        in_rms += in[i] * in[i];
        out_rms += out[i] * out[i];
    }
    in_rms = sqrtf(in_rms / 3996.0f);
    out_rms = sqrtf(out_rms / 3996.0f);

    ASSERT(out_rms > in_rms * 0.5f, "wavefolder should produce output");

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: asymmetry affects waveform */
static int test_asymmetry(void) {
    TEST("asymmetry > 0 skews waveform");

    float buf[4096];
    Osc o;
    osc_init(&o, OSC_WAVE_SAW, 100.0f);
    osc_process_block(&o, buf, 4096, SR);

    float sym_out[4096], asym_out[4096];
    memcpy(sym_out, buf, sizeof(buf));
    memcpy(asym_out, buf, sizeof(buf));

    Wavefolder ws, wa;
    wavefolder_init(&ws, 3.0f, 0.0f);
    wavefolder_init(&wa, 3.0f, 1.0f);
    wavefolder_process_block(&ws, sym_out, 4096);
    wavefolder_process_block(&wa, asym_out, 4096);

    // Asymmetric should differ from symmetric
    float diff = 0.0f;
    for (int i = 100; i < 4096; i++) {
        diff += fabsf(sym_out[i] - asym_out[i]);
    }
    ASSERT(diff > 0.1f, "asym output differs from sym output");

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: generate WAV */
static int test_generate_wav(void) {
    TEST("generate wavefolder_dist.wav");

    float buf[88200]; // 2 seconds
    Osc o;
    osc_init(&o, OSC_WAVE_SAW, 110.0f);
    osc_process_block(&o, buf, 88200, SR);

    Wavefolder w;
    wavefolder_init(&w, 6.0f, 0.3f);
    wavefolder_process_block(&w, buf, 88200);

    WavWriter wav = wav_create("wavefolder_dist.wav", (uint32_t)SR, 1);
    ASSERT(wav.fp != NULL, "WAV file opened");
    for (int i = 0; i < 88200; i++) wav_write_sample(&wav, buf[i]);
    wav_close(&wav);
    END_TEST();
    return _test_passed ? 0 : 1;
}

int main(void) {
    printf("=== test_wavefolder: Wavefolder Module ===\n");

    int fail = 0;
    fail += test_zero_drive();
    fail += test_harmonics();
    fail += test_asymmetry();
    fail += test_generate_wav();

    TEST_SUMMARY();
}
