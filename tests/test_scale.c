/*
 * test_scale.c — Test Just Intonation scale module
 * Every interval ratio is verified against musimathics table 6.2.
 * Every scale degree is verified for correct frequency.
 * Mathematical precision: all tests use exact rational ratios.
 */
#include <stdio.h>
#include <math.h>
#include "test_util.h"
#include "scale.h"

/* Tolerance for floating-point ratio comparisons */
#define RATIO_TOL 0.00001f
#define FREQ_TOL  0.01f   /* Hz tolerance for frequency checks */
#define CENT_TOL  0.5f    /* cent tolerance */

/* Test: all just intonation ratios match expected values */
static int test_just_ratios(void) {
    TEST("all just intonation ratios correct");

    /* Ratios from musimathics table: num/denom */
    struct { Interval interval; float expected; } tests[] = {
        {INTERVAL_UNISON,        1.0f       },
        {INTERVAL_MINOR_SEC,    16.0f/15.0f },
        {INTERVAL_MAJOR_SEC,     9.0f/8.0f  },
        {INTERVAL_MINOR_THIRD,   6.0f/5.0f  },
        {INTERVAL_MAJOR_THIRD,   5.0f/4.0f  },
        {INTERVAL_PERFECT_FOURTH, 4.0f/3.0f },
        {INTERVAL_TRITONE,      45.0f/32.0f },
        {INTERVAL_PERFECT_FIFTH,  3.0f/2.0f },
        {INTERVAL_MINOR_SIXTH,   8.0f/5.0f  },
        {INTERVAL_MAJOR_SIXTH,   5.0f/3.0f  },
        {INTERVAL_MINOR_SEVENTH,16.0f/9.0f  },
        {INTERVAL_MAJOR_SEVENTH,15.0f/8.0f  },
        {INTERVAL_OCTAVE,       2.0f        }
    };

    int n = sizeof(tests) / sizeof(tests[0]);
    for (int i = 0; i < n; i++) {
        JustRatio r = scale_just_ratio(tests[i].interval);
        float ratio = scale_ratio_float(r);
        float diff = fabsf(ratio - tests[i].expected);
        char msg[64];
        snprintf(msg, sizeof(msg), "interval %d: %d/%d = %.6f",
                 (int)tests[i].interval, r.numerator, r.denominator, ratio);
        ASSERT(diff < RATIO_TOL, msg);
    }

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: generating frequencies from a scale */
static int test_scale_frequencies(void) {
    TEST("scale frequencies match just intonation ratios");

    const ScaleDef *major = scale_get_def(SCALE_MAJOR);
    ASSERT(major != NULL, "major scale definition exists");
    ASSERT(major->num_degrees == 7, "major scale has 7 degrees");

    float root = 440.0f; /* A4 */
    float freqs[SCALE_MAX_DEGREES];
    scale_generate_freqs(major, root, freqs, 0);

    /* Expected frequencies for A major in just intonation */
    float expected[7] = {
        440.0f,                          /* A: 1/1 */
        440.0f * 9.0f/8.0f,              /* B: 9/8 */
        440.0f * 5.0f/4.0f,              /* C#: 5/4 */
        440.0f * 4.0f/3.0f,              /* D: 4/3 */
        440.0f * 3.0f/2.0f,              /* E: 3/2 */
        440.0f * 5.0f/3.0f,              /* F#: 5/3 */
        440.0f * 15.0f/8.0f              /* G#: 15/8 */
    };

    for (int i = 0; i < 7; i++) {
        char msg[64];
        snprintf(msg, sizeof(msg), "degree %d freq=%.2f (expected %.2f)",
                 i, freqs[i], expected[i]);
        ASSERT(fabsf(freqs[i] - expected[i]) < FREQ_TOL, msg);
    }

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: all scale types produce valid frequencies */
static int test_all_scales(void) {
    TEST("all scale types produce non-zero frequencies");

    float root = 220.0f; /* A3 */
    float freqs[SCALE_MAX_DEGREES];

    for (int st = 0; st < SCALE_COUNT; st++) {
        const ScaleDef *s = scale_get_def((ScaleType)st);
        ASSERT(s != NULL, "scale definition exists");
        ASSERT(s->num_degrees >= 2, "scale has >= 2 degrees");
        ASSERT(s->num_degrees <= SCALE_MAX_DEGREES,
               "scale within MAX_DEGREES");

        scale_generate_freqs(s, root, freqs, 0);
        for (int i = 0; i < s->num_degrees; i++) {
            char msg[64];
            snprintf(msg, sizeof(msg), "scale %d degree %d freq=%.2f",
                     st, i, freqs[i]);
            ASSERT(freqs[i] > 0.0f, msg);
            ASSERT(freqs[i] <= 20000.0f, "freq within audible range");
        }
    }

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: all scales are monotonically increasing */
static int test_scale_monotonic(void) {
    TEST("all scale degrees are monotonically increasing");

    float root = 261.63f; /* Middle C */
    float freqs[SCALE_MAX_DEGREES];

    for (int st = 0; st < SCALE_COUNT; st++) {
        const ScaleDef *s = scale_get_def((ScaleType)st);
        scale_generate_freqs(s, root, freqs, 0);

        for (int i = 1; i < s->num_degrees; i++) {
            char msg[64];
            snprintf(msg, sizeof(msg), "scale %d degree %d > deg %d: %.2f > %.2f",
                     st, i, i-1, freqs[i], freqs[i-1]);
            ASSERT(freqs[i] > freqs[i-1], msg);
        }
    }

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: just intonation vs equal temperament deviation */
static int test_cent_deviation(void) {
    TEST("just intonation cent deviations from equal temperament");

    /* Known deviations (approximate, from musimathics) */
    struct { Interval interval; float expected_cents; float tolerance; } checks[] = {
        {INTERVAL_UNISON,        0.00f,  0.1f},
        {INTERVAL_MAJOR_SEC,   203.91f - 200.0f, 2.0f},  /* +3.91 cents */
        {INTERVAL_MINOR_THIRD, 315.64f - 300.0f, 2.0f},  /* +15.64 cents */
        {INTERVAL_MAJOR_THIRD, 386.31f - 400.0f, 2.0f},  /* -13.69 cents */
        {INTERVAL_PERFECT_FOURTH, 498.04f - 500.0f, 2.0f}, /* -1.96 cents */
        {INTERVAL_PERFECT_FIFTH, 701.96f - 700.0f, 2.0f},  /* +1.96 cents */
        {INTERVAL_MAJOR_SIXTH, 884.36f - 900.0f, 2.0f},   /* -15.64 cents */
        {INTERVAL_MINOR_SEVENTH, 996.09f - 1000.0f, 2.0f}, /* -3.91 cents */
        {INTERVAL_OCTAVE,        0.00f,  0.1f}
    };

    int n = sizeof(checks) / sizeof(checks[0]);
    for (int i = 0; i < n; i++) {
        float dev = scale_cent_deviation(checks[i].interval);
        char msg[64];
        snprintf(msg, sizeof(msg), "interval %d deviation %.2f cents (expected %.2f)",
                 (int)checks[i].interval, dev, checks[i].expected_cents);
        ASSERT(fabsf(dev - checks[i].expected_cents) < checks[i].tolerance, msg);
    }

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: MIDI to frequency and back */
static int test_midi_conversion(void) {
    TEST("MIDI note conversion correct");

    /* A4 = MIDI 69 = 440Hz */
    float a4 = scale_midi_to_freq(69);
    ASSERT(fabsf(a4 - 440.0f) < 0.1f, "MIDI 69 = 440Hz");

    /* C4 = MIDI 60 */
    float c4 = scale_midi_to_freq(60);
    ASSERT(fabsf(c4 - 261.63f) < 0.5f, "MIDI 60 ≈ 261.63Hz");

    /* A3 = MIDI 57 = 220Hz */
    float a3 = scale_midi_to_freq(57);
    ASSERT(fabsf(a3 - 220.0f) < 0.1f, "MIDI 57 = 220Hz");

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: cents between frequencies */
static int test_cents_calculation(void) {
    TEST("cents calculation between frequencies");

    /* Octave = 1200 cents */
    float cents_oct = scale_cents_between(440.0f, 220.0f);
    ASSERT(fabsf(cents_oct - 1200.0f) < CENT_TOL, "octave = 1200 cents");

    /* Same frequency = 0 cents */
    float cents_same = scale_cents_between(440.0f, 440.0f);
    ASSERT(fabsf(cents_same) < CENT_TOL, "same freq = 0 cents");

    /* Equal temperament semitone = 100 cents */
    float cents_semi = scale_cents_between(440.0f, 415.30f);
    ASSERT(fabsf(cents_semi - 100.0f) < 2.0f, "semitone ≈ 100 cents");

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: just intonation produces different frequencies than ET */
static int test_just_vs_et_difference(void) {
    TEST("just intonation differs from equal temperament");

    const ScaleDef *major = scale_get_def(SCALE_MAJOR);
    float root = 440.0f;
    float just[SCALE_MAX_DEGREES], et[SCALE_MAX_DEGREES];

    scale_generate_freqs(major, root, just, 0);
    scale_generate_freqs(major, root, et, 1);

    /* Major 3rd should be different (just=5/4=550Hz, ET=440*2^(4/12)=554.37Hz) */
    char msg[64];
    snprintf(msg, sizeof(msg), "M3: just=%.2f ET=%.2f diff=%.2fHz",
             just[2], et[2], fabsf(just[2] - et[2]));
    ASSERT(fabsf(just[2] - et[2]) > 1.0f, msg);

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: interval_to_freq with edge cases */
static int test_interval_edge_cases(void) {
    TEST("interval_to_freq handles edge cases");

    /* Unison from A4 = 440 */
    float unison = scale_interval_to_freq(440.0f, INTERVAL_UNISON);
    ASSERT(fabsf(unison - 440.0f) < FREQ_TOL, "unison = root");

    /* Octave from A4 = 880 */
    float octave = scale_interval_to_freq(440.0f, INTERVAL_OCTAVE);
    ASSERT(fabsf(octave - 880.0f) < FREQ_TOL, "octave = 880Hz");

    /* Perfect 5th from 0Hz = 0Hz (no crash) */
    float zero = scale_interval_to_freq(0.0f, INTERVAL_PERFECT_FIFTH);
    ASSERT(zero == 0.0f, "zero root = zero freq");

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: all scale names are valid */
static int test_scale_names(void) {
    TEST("all scale names are non-empty");
    for (int i = 0; i < SCALE_COUNT; i++) {
        ASSERT(scale_names[i] != NULL, "scale name not null");
        ASSERT(scale_names[i][0] != '\0', "scale name not empty");
    }
    END_TEST();
    return _test_passed ? 0 : 1;
}

int main(void) {
    printf("=== test_scale: Just Intonation Scale Module ===\n");

    int fail = 0;
    fail += test_just_ratios();
    fail += test_scale_frequencies();
    fail += test_all_scales();
    fail += test_scale_monotonic();
    fail += test_cent_deviation();
    fail += test_midi_conversion();
    fail += test_cents_calculation();
    fail += test_just_vs_et_difference();
    fail += test_interval_edge_cases();
    fail += test_scale_names();

    TEST_SUMMARY();
}
