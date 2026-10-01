/*
 * test_delay.c — Test delay module
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "test_util.h"
#include "delay.h"
#include "osc.h"

#define SR 44100.0f

/* Test: delay produces echo at correct time */
static int test_delay_time(void) {
    TEST("delay 100ms produces echo at 0.1s intervals");

    float buf[44100]; // 1 second
    memset(buf, 0, sizeof(buf));
    buf[0] = 1.0f; // single impulse at start

    Delay d;
    delay_init(&d, 1000.0f, SR);
    delay_set_time(&d, 100.0f);  // 100ms
    delay_set_feedback(&d, 0.0f);
    delay_set_mix(&d, 1.0f);     // 100% wet

    delay_process_block(&d, buf, 44100);

    // With mix=1.0 (wet), impulse is stored in buffer but output is 0 until
    // the read pointer catches up. First echo at 4410.
    ASSERT(fabsf(buf[0]) < 0.001f, "no output at start with 100% wet");

    float peak_at_4410 = fabsf(buf[4410]);
    ASSERT(peak_at_4410 > 0.5f, "echo should appear at 4410 samples (100ms)");

    // No other significant peaks
    float noise_floor = 0.0f;
    for (int i = 0; i < 44100; i++) {
        if (i != 0 && i != 4410 && fabsf(buf[i]) > noise_floor)
            noise_floor = fabsf(buf[i]);
    }
    ASSERT(noise_floor < 0.01f, "no other peaks with 0 feedback");

    delay_free(&d);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: feedback produces multiple echoes */
static int test_feedback(void) {
    TEST("delay with feedback produces decaying echoes");

    float buf[88200]; // 2 seconds
    memset(buf, 0, sizeof(buf));
    buf[0] = 1.0f;

    Delay d;
    delay_init(&d, 1000.0f, SR);
    delay_set_time(&d, 200.0f);  // 200ms
    delay_set_feedback(&d, 0.5f);
    delay_set_mix(&d, 1.0f);

    delay_process_block(&d, buf, 88200);

    // Check 3 echoes at 200ms intervals
    int delay_samps = (int)(0.2f * SR);
    for (int e = 1; e <= 3; e++) {
        int idx = e * delay_samps;
        float val = fabsf(buf[idx]);
        ASSERT(val > 0.01f, "echo at each delay interval");
    }

    // Second echo should be weaker than first (decay)
    float echo1 = fabsf(buf[delay_samps]);
    float echo2 = fabsf(buf[2 * delay_samps]);
    ASSERT(echo2 < echo1 * 0.8f, "echoes should decay (feedback=0.5)");

    delay_free(&d);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: wet/dry mix */
static int test_mix(void) {
    TEST("delay mix = 0.0 equals dry signal");

    float in[4410];
    Osc o;
    osc_init(&o, OSC_WAVE_SAW, 220.0f);
    osc_process_block(&o, in, 4410, SR);

    // Copy for processing
    float processed[4410];
    memcpy(processed, in, sizeof(processed));

    Delay d;
    delay_init(&d, 1000.0f, SR);
    delay_set_time(&d, 50.0f);
    delay_set_mix(&d, 0.0f);  // 100% dry
    delay_process_block(&d, processed, 4410);

    // With mix=0, output should equal input
    float max_diff = 0.0f;
    for (int i = 0; i < 4410; i++) {
        float diff = fabsf(processed[i] - in[i]);
        if (diff > max_diff) max_diff = diff;
    }
    ASSERT(max_diff < 0.001f, "mix=0 should pass dry through");

    delay_free(&d);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: generate WAV */
static int test_generate_wav(void) {
    TEST("generate delay_echo.wav for manual check");

    float buf[88200]; // 2 seconds
    Osc o;
    osc_init(&o, OSC_WAVE_SAW, 220.0f);
    osc_process_block(&o, buf, 88200, SR);

    Delay d;
    delay_init(&d, 2000.0f, SR);
    delay_set_time(&d, 300.0f);
    delay_set_feedback(&d, 0.4f);
    delay_set_mix(&d, 0.4f);
    delay_process_block(&d, buf, 88200);

    WavWriter w = wav_create("delay_echo.wav", (uint32_t)SR, 1);
    ASSERT(w.fp != NULL, "WAV file opened");
    for (int i = 0; i < 88200; i++) wav_write_sample(&w, buf[i]);
    wav_close(&w);

    delay_free(&d);
    END_TEST();
    return _test_passed ? 0 : 1;
}

int main(void) {
    printf("=== test_delay: Delay Module ===\n");

    int fail = 0;
    fail += test_delay_time();
    fail += test_feedback();
    fail += test_mix();
    fail += test_generate_wav();

    TEST_SUMMARY();
}
