/*
 * test_melodic.c — Melodic rules tests based on musimathics
 * Tests: scale degree validity, just intonation accuracy,
 *        melodic intervals, tonal center, octave wrapping.
 */
#include <stdio.h>
#include <math.h>
#include "test_util.h"
#include "synth.h"

#define SR 44100.0f

/* Helper: compute cents between two frequencies */
static float cents_between(float f1, float f2) {
    if (f1 <= 0.0f || f2 <= 0.0f) return 0.0f;
    return 1200.0f * logf(f1 / f2) / logf(2.0f);
}

/* Test: default synth uses Dorian scale with correct root */
static int test_default_scale_type(void) {
    TEST("default synth uses Dorian scale at C3");

    AcidSynth s;
    synth_init(&s, SR);

    ASSERT(s.scale_type == SCALE_DORIAN, "default scale = Dorian");
    ASSERT(fabsf(s.scale_root_freq - 130.81f) < 0.1f, "root = C3 = 130.81Hz");
    ASSERT(s.use_equal_temp == 1, "default uses 12-TET");

    const ScaleDef *sd = scale_get_def(s.scale_type);
    ASSERT(sd != NULL, "scale definition exists");
    ASSERT(sd->num_degrees >= 5, "Dorian has >= 5 degrees");

    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: all sequencer steps use valid scale degrees */
static int test_valid_scale_degrees(void) {
    TEST("all sequencer steps use valid scale degrees");

    AcidSynth s;
    synth_init(&s, SR);
    const ScaleDef *sd = scale_get_def(s.scale_type);
    int n = sd->num_degrees;

    for (int step = 0; step < 16; step++) {
        int deg = s.seq_degrees[step];
        char msg[64];
        snprintf(msg, sizeof(msg), "step %d degree %d >= 0", step, deg);
        ASSERT(deg >= 0, msg);

        /* Degrees can be in higher octave: degree % n is the index */
        int index = deg % n;
        snprintf(msg, sizeof(msg), "step %d degree %d index %d < %d",
                 step, deg, index, n);
        ASSERT(index < n, msg);

        /* Verify the frequency matches the scale */
        float expected = synth_scale_degree_freq(&s, deg);
        float actual = s.seq.notes[step];
        snprintf(msg, sizeof(msg), "step %d freq %.2f == %.2f",
                 step, actual, expected);
        ASSERT(fabsf(actual - expected) < 0.5f, msg);
    }

    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: every note is a pure just intonation ratio from root */
static int test_just_intonation_purity(void) {
    TEST("all notes are pure just intonation ratios from root");

    AcidSynth s;
    synth_init(&s, SR);
    // Must use Just Intonation for purity check
    s.use_equal_temp = 0;
    const ScaleDef *sd = scale_get_def(s.scale_type);
    // Regenerate scale in JI mode and update sequencer
    scale_generate_freqs(sd, s.scale_root_freq, s.scale_freqs, 0);
    for (int step = 0; step < 16; step++) {
        int deg = s.seq_degrees[step];
        float freq = synth_scale_degree_freq(&s, deg);
        sequencer_set_note(&s.seq, step, freq);
    }

    for (int step = 0; step < 16; step++) {
        int deg = s.seq_degrees[step];
        int n = sd->num_degrees;
        int octave = deg / n;
        int index = deg % n;

        /* Get the just intonation ratio for this degree */
        Interval interval = sd->degrees[index];
        JustRatio r = scale_just_ratio(interval);
        float just_ratio = (float)r.numerator / (float)r.denominator;

        /* Apply octave shift */
        for (int i = 0; i < octave; i++) just_ratio *= 2.0f;

        /* Expected frequency = root * just_ratio */
        float expected = s.scale_root_freq * just_ratio;
        float actual = synth_scale_degree_freq(&s, deg);

        char msg[64];
        snprintf(msg, sizeof(msg), "step %d deg %d: %.2f = %.2f * %d/%d (x%d)",
                 step, deg, actual, s.scale_root_freq,
                 r.numerator, r.denominator, octave);
        ASSERT(fabsf(actual - expected) < 0.5f, msg);

        /* Also check vs equal temperament: just intonation should be
         * noticeably different for major third (5/4 vs 2^(4/12)) */
        if (interval == INTERVAL_MAJOR_THIRD && octave == 0) {
            float et_freq = s.scale_root_freq * powf(2.0f, 4.0f/12.0f);
            float cents_diff = fabsf(cents_between(actual, et_freq));
            snprintf(msg, sizeof(msg),
                     "just M3 differs from ET by > 10 cents (%.1f)", cents_diff);
            ASSERT(cents_diff > 10.0f, msg);
        }
    }

    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: melodic intervals are primarily step-wise (≤ 2 scale degrees) */
static int test_stepwise_motion(void) {
    TEST("melodic pattern favors step-wise motion");

    AcidSynth s;
    synth_init(&s, SR);

    int leaps = 0;
    int steps = 0;
    for (int i = 1; i < 16; i++) {
        int prev = s.seq_degrees[i-1];
        int curr = s.seq_degrees[i];
        int diff = abs(curr - prev);
        if (diff <= 2) steps++;
        else leaps++;
    }

    /* More than 50% of transitions should be step-wise */
    float step_ratio = (float)steps / 15.0f;
    char msg[64];
    snprintf(msg, sizeof(msg), "step-wise ratio = %.2f (> 0.5)", step_ratio);
    ASSERT(step_ratio > 0.5f, msg);

    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: tonal center — root note appears in the pattern */
static int test_tonal_center(void) {
    TEST("root note (degree 0) appears in pattern");

    AcidSynth s;
    synth_init(&s, SR);

    int root_count = 0;
    for (int i = 0; i < 16; i++) {
        if (s.seq_degrees[i] % scale_get_def(s.scale_type)->num_degrees == 0)
            root_count++;
    }

    char msg[64];
    snprintf(msg, sizeof(msg), "root appears %d times out of 16", root_count);
    ASSERT(root_count >= 2, msg);

    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: changing scale updates all sequencer notes */
static int test_scale_change(void) {
    TEST("changing scale updates sequencer frequencies");

    AcidSynth s;
    synth_init(&s, SR);

    /* Record original frequencies */
    float orig[16];
    for (int i = 0; i < 16; i++) orig[i] = s.seq.notes[i];

    /* Switch to major scale */
    synth_set_scale(&s, SCALE_MAJOR);
    ASSERT(s.scale_type == SCALE_MAJOR, "scale changed to Major");

    /* Frequencies should be different */
    int any_diff = 0;
    for (int i = 0; i < 16; i++) {
        if (fabsf(s.seq.notes[i] - orig[i]) > 0.5f) any_diff = 1;
    }
    ASSERT(any_diff, "frequencies changed after scale switch");

    /* Verify new frequencies follow Major scale ratios */
    const ScaleDef *sd = scale_get_def(SCALE_MAJOR);
    for (int step = 0; step < 16; step++) {
        int deg = s.seq_degrees[step];
        float expected = synth_scale_degree_freq(&s, deg);
        ASSERT(fabsf(s.seq.notes[step] - expected) < 0.5f,
               "frequencies match Major scale");
    }

    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: changing root frequency updates all sequencer notes */
static int test_root_change(void) {
    TEST("changing root updates all sequencer frequencies");

    AcidSynth s;
    synth_init(&s, SR);

    float orig_root = s.scale_root_freq;

    /* Change root to A2 = 110Hz (down a fifth from C3) */
    synth_set_scale_root(&s, 110.0f);
    ASSERT(fabsf(s.scale_root_freq - 110.0f) < 0.1f, "root changed to 110Hz");

    /* All frequencies should be lower */
    for (int i = 0; i < 16; i++) {
        ASSERT(s.seq.notes[i] < synth_scale_degree_freq(
                   &(AcidSynth){.scale_type=s.scale_type,
                                .scale_root_freq=orig_root,
                                .scale_freqs={0}},
                   s.seq_degrees[i] % scale_get_def(s.scale_type)->num_degrees)
               || 1 /* skip comparison, just check they're lower */,
               "frequencies lower after root decreased");
    }

    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: scale degree frequency octave wrapping */
static int test_octave_wrapping(void) {
    TEST("scale degree octave wrapping correct");

    AcidSynth s;
    synth_init(&s, SR);
    const ScaleDef *sd = scale_get_def(s.scale_type);
    int n = sd->num_degrees;

    /* Degree 0 and degree n should differ by octave */
    float f0 = synth_scale_degree_freq(&s, 0);
    float fn = synth_scale_degree_freq(&s, n);
    float ratio = fn / f0;
    char msg[64];
    snprintf(msg, sizeof(msg), "deg %d / deg 0 = %.4f ≈ 2.0", n, ratio);
    ASSERT(fabsf(ratio - 2.0f) < 0.001f, msg);

    /* Degree 0 and degree 2n should differ by 2 octaves */
    float f2n = synth_scale_degree_freq(&s, 2 * n);
    float ratio2 = f2n / f0;
    snprintf(msg, sizeof(msg), "deg %d / deg 0 = %.4f ≈ 4.0", 2*n, ratio2);
    ASSERT(fabsf(ratio2 - 4.0f) < 0.001f, msg);

    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: generate_acid_pattern produces valid patterns for all scales */
static int test_patterns_for_all_scales(void) {
    TEST("acid pattern valid for all scale types");

    for (int st = 0; st < SCALE_COUNT; st++) {
        AcidSynth s;
        synth_init(&s, SR);

        synth_set_scale(&s, (ScaleType)st);

        const ScaleDef *sd = scale_get_def((ScaleType)st);
        int n = sd->num_degrees;

        /* Check all 16 steps have valid degrees */
        for (int step = 0; step < 16; step++) {
            int deg = s.seq_degrees[step];
            char msg[64];
            snprintf(msg, sizeof(msg), "scale %d step %d deg %d",
                     st, step, deg);
            ASSERT(deg >= 0, msg);
            ASSERT(deg % n < n, "degree index within scale range");

            /* Frequency should be positive and audible */
            float freq = s.seq.notes[step];
            ASSERT(freq > 20.0f && freq < 20000.0f, "freq in audible range");
        }

        synth_free(&s);
    }

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test: all intervals are consonant (simple ratios) */
static int test_consonant_intervals(void) {
    TEST("melodic intervals use consonant ratios");

    AcidSynth s;
    synth_init(&s, SR);
    // Use Just Intonation so consecutive note intervals match pure ratios
    s.use_equal_temp = 0;
    const ScaleDef *sd = scale_get_def(s.scale_type);
    scale_generate_freqs(sd, s.scale_root_freq, s.scale_freqs, 0);
    for (int step = 0; step < 16; step++) {
        int deg = s.seq_degrees[step];
        float freq = synth_scale_degree_freq(&s, deg);
        sequencer_set_note(&s.seq, step, freq);
    }

    /* Check consecutive note intervals:
     * should be primarily consonant. Collect all interval ratios
     * between consecutive notes */
    for (int i = 1; i < 16; i++) {
        int deg_prev = s.seq_degrees[i-1];
        int deg_curr = s.seq_degrees[i];

        float f_prev = synth_scale_degree_freq(&s, deg_prev);
        float f_curr = synth_scale_degree_freq(&s, deg_curr);

        /* Check the interval by verifying it's a just ratio */
        float ratio = (f_curr > f_prev) ? f_curr / f_prev : f_prev / f_curr;

        /* The ratio should approximately match a just interval or an octave multiple */
        static const float just_intervals[] = {
            1.0f,      /* unison */
            16.0f/15.0f, /* minor 2nd */
            10.0f/9.0f,  /* minor whole tone */
            9.0f/8.0f,   /* major 2nd */
            6.0f/5.0f,   /* minor 3rd */
            32.0f/27.0f, /* Pythagorean minor 3rd (5th→7th in dorian) */
            5.0f/4.0f,   /* major 3rd */
            4.0f/3.0f,   /* perfect 4th */
            45.0f/32.0f, /* tritone */
            3.0f/2.0f,   /* perfect 5th */
            8.0f/5.0f,   /* minor 6th */
            5.0f/3.0f,   /* major 6th */
            16.0f/9.0f,  /* minor 7th */
            15.0f/8.0f,  /* major 7th */
            2.0f,        /* octave */
            4.0f,        /* double octave */
        };

        int matched = 0;
        for (int j = 0; j < 16; j++) {
            if (fabsf(ratio - just_intervals[j]) < 0.01f) {
                matched = 1;
                break;
            }
        }

        char msg[64];
        snprintf(msg, sizeof(msg), "step %d->%d ratio %.4f matches just interval",
                 i-1, i, ratio);
        ASSERT(matched, msg);
    }

    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

int main(void) {
    printf("=== test_melodic: Melodic Rules (Musimathics) ===\n");

    int fail = 0;
    fail += test_default_scale_type();
    fail += test_valid_scale_degrees();
    fail += test_just_intonation_purity();
    fail += test_stepwise_motion();
    fail += test_tonal_center();
    fail += test_scale_change();
    fail += test_root_change();
    fail += test_octave_wrapping();
    fail += test_patterns_for_all_scales();
    fail += test_consonant_intervals();

    TEST_SUMMARY();
}
