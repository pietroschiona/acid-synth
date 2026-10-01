/*
 * test_reverb.c — Test reverb module
 */
#include <stdio.h>
#include <math.h>
#include "test_util.h"
#include "reverb.h"
#include "osc.h"

#define SR 44100.0f

/* Test: reverb produces audible tail that decays */
static int test_decay(void) {
    TEST("reverb: impulse response shows decay");

    float buf[44100]; // 1 second
    memset(buf, 0, sizeof(buf));
    buf[0] = 1.0f; // impulse

    Reverb r;
    reverb_init(&r, 1.0f, 0.85f, SR);
    reverb_set_mix(&r, 1.0f);
    reverb_process_block(&r, buf, 44100);

    // Early energy (first 100ms) should be higher than late (300-500ms)
    float early = 0.0f;
    for (int i = 10; i < 4410; i++) early += buf[i] * buf[i]; // 10-100ms

    float late = 0.0f;
    for (int i = 13230; i < 22050; i++) late += buf[i] * buf[i]; // 300-500ms

    ASSERT(early > 0.001f, "reverb produces early reflections");
    ASSERT(early > late, "reverb decays from early to late");

    reverb_free(&r);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: stereo output generates WAV */
static int test_generate_wav(void) {
    TEST("generate reverb_tail.wav");

    float buf[88200];
    Osc o;
    osc_init(&o, OSC_WAVE_SQUARE, 110.0f);
    osc_process_block(&o, buf, 88200, SR);

    Reverb r;
    reverb_init(&r, 1.0f, 0.6f, SR);
    reverb_set_mix(&r, 0.4f);
    reverb_process_block(&r, buf, 88200);

    WavWriter w = wav_create("reverb_tail.wav", (uint32_t)SR, 1);
    ASSERT(w.fp != NULL, "WAV opened");
    for (int i = 0; i < 88200; i++) wav_write_sample(&w, buf[i]);
    wav_close(&w);

    reverb_free(&r);
    END_TEST();
    return _test_passed ? 0 : 1;
}

int main(void) {
    printf("=== test_reverb: Reverb Module ===\n");

    int fail = 0;
    fail += test_decay();
    fail += test_generate_wav();

    TEST_SUMMARY();
}
