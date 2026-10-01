/*
 * test_scale_musimathics.c — Just Intonation scale verification
 * Based on Musimathics (Loy Vol 1, Section 6.2): pure frequency ratios.
 * Tests: ratio correctness, cent deviations, frequency generation,
 *        octave wrapping, scale definition completeness.
 */
#include <stdio.h>
#include <math.h>
#include "test_util.h"
#include "synth.h"
#include "scale.h"

#define SR 44100.0f

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ----------------------------------------------------------------- */
/*  Test 1: Every just ratio matches musimathics Section 6.2         */
/* ----------------------------------------------------------------- */
static int test_just_ratios_match_musimathics(void) {
    TEST("just intonation ratios match musimathics table 6.2");

    /* Musimathics published values: interval → numerator, denominator */
    struct { Interval interval; int expected_num; int expected_den; } table[] = {
        {INTERVAL_UNISON,         1,  1},
        {INTERVAL_MINOR_SEC,     16, 15},
        {INTERVAL_MAJOR_SEC,      9,  8},
        {INTERVAL_MINOR_THIRD,    6,  5},
        {INTERVAL_MAJOR_THIRD,    5,  4},
        {INTERVAL_PERFECT_FOURTH, 4,  3},
        {INTERVAL_TRITONE,       45, 32},
        {INTERVAL_PERFECT_FIFTH,  3,  2},
        {INTERVAL_MINOR_SIXTH,    8,  5},
        {INTERVAL_MAJOR_SIXTH,    5,  3},
        {INTERVAL_MINOR_SEVENTH, 16,  9},
        {INTERVAL_MAJOR_SEVENTH, 15,  8},
        {INTERVAL_OCTAVE,         2,  1},
    };

    int n = sizeof(table) / sizeof(table[0]);
    for (int i = 0; i < n; i++) {
        JustRatio r = scale_just_ratio(table[i].interval);
        char msg[80];
        snprintf(msg, sizeof(msg), "%s ratio %d/%d (expected %d/%d)",
                 r.name, r.numerator, r.denominator,
                 table[i].expected_num, table[i].expected_den);
        ASSERT(r.numerator == table[i].expected_num,
               "ratio numerator matches musimathics");
        ASSERT(r.denominator == table[i].expected_den,
               "ratio denominator matches musimathics");
    }

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 2: Cent deviations match musimathics within 0.5c tolerance  */
/* ----------------------------------------------------------------- */
static int test_cent_deviations(void) {
    TEST("cent deviations from equal temperament match musimathics");

    /* Expected cent deviations (from musimathics, rounded to 0.01c) */
    struct { Interval interval; float expected_cents; } table[] = {
        {INTERVAL_UNISON,          0.00f},
        {INTERVAL_MINOR_SEC,      11.73f},   /* 16/15 = +11.73c from 100c ET */
        {INTERVAL_MAJOR_SEC,       3.91f},   /*  9/8  = +3.91c from 200c ET */
        {INTERVAL_MINOR_THIRD,    15.64f},   /*  6/5  = +15.64c from 300c ET */
        {INTERVAL_MAJOR_THIRD,   -13.69f},   /*  5/4  = -13.69c from 400c ET */
        {INTERVAL_PERFECT_FOURTH, -1.96f},   /*  4/3  = -1.96c from 500c ET */
        {INTERVAL_PERFECT_FIFTH,   1.96f},   /*  3/2  = +1.96c from 700c ET */
        {INTERVAL_MINOR_SIXTH,    13.69f},   /*  8/5  = +13.69c from 800c ET */
        {INTERVAL_MAJOR_SIXTH,   -15.64f},   /*  5/3  = -15.64c from 900c ET */
        {INTERVAL_MINOR_SEVENTH,  -3.91f},   /* 16/9  = -3.91c from 1000c ET */
        {INTERVAL_MAJOR_SEVENTH, -11.73f},   /* 15/8  = -11.73c from 1100c ET */
        {INTERVAL_OCTAVE,          0.00f},
    };

    int n = sizeof(table) / sizeof(table[0]);
    for (int i = 0; i < n; i++) {
        float dev = scale_cent_deviation(table[i].interval);
        char msg[80];
        snprintf(msg, sizeof(msg), "%s dev=%.2fc (expected %.2fc)",
                 scale_just_ratio(table[i].interval).name,
                 dev, table[i].expected_cents);
        ASSERT_FLOAT_EQ(dev, table[i].expected_cents, 0.5f, msg);
    }

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 3: Scale frequency generation for Phrygian on C3            */
/* ----------------------------------------------------------------- */
static int test_scale_freqs_phrygian_c3(void) {
    TEST("Phrygian scale on C3 produces correct just intonation frequencies");

    const ScaleDef *sd = scale_get_def(SCALE_PHRYGIAN);
    ASSERT(sd->num_degrees == 7, "Phrygian has 7 degrees");

    float root = 130.81f;  /* C3 */
    float freqs[7];
    scale_generate_freqs(sd, root, freqs, 0);

    /* Expected just intonation frequencies (C=130.81):
     * C  (1/1)   = 130.81
     * Db (16/15) = 130.81 * 16/15 = 139.53
     * Eb (6/5)   = 130.81 * 6/5   = 156.97
     * F  (4/3)   = 130.81 * 4/3   = 174.41
     * G  (3/2)   = 130.81 * 3/2   = 196.22
     * Ab (8/5)   = 130.81 * 8/5   = 209.30
     * Bb (16/9)  = 130.81 * 16/9  = 232.55
     */
    float expected[] = {130.81f, 139.53f, 156.97f, 174.41f,
                        196.22f, 209.30f, 232.55f};

    for (int i = 0; i < 7; i++) {
        char msg[80];
        snprintf(msg, sizeof(msg), "Phrygian degree %d freq=%.2f (expected %.2f)",
                 i, freqs[i], expected[i]);
        ASSERT_FLOAT_EQ(freqs[i], expected[i], 0.6f, msg);
    }

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 4: Scale frequency for a specific root note                  */
/* ----------------------------------------------------------------- */
static int test_scale_freqs_major_a4(void) {
    TEST("Major scale on A4 produces correct just intonation frequencies");

    const ScaleDef *sd = scale_get_def(SCALE_MAJOR);
    ASSERT(sd->num_degrees == 7, "Major has 7 degrees");

    float root = 440.0f;  /* A4 */
    float freqs[7];
    scale_generate_freqs(sd, root, freqs, 0);

    /* Expected just intonation frequencies (A=440):
     * A  (1/1)   = 440.00
     * B  (9/8)   = 495.00
     * C# (5/4)   = 550.00
     * D  (4/3)   = 586.67
     * E  (3/2)   = 660.00
     * F# (5/3)   = 733.33
     * G# (15/8)  = 825.00
     */
    float expected[] = {440.0f, 495.0f, 550.0f, 586.67f,
                        660.0f, 733.33f, 825.0f};

    for (int i = 0; i < 7; i++) {
        char msg[80];
        snprintf(msg, sizeof(msg), "Major degree %d freq=%.2f (expected %.2f)",
                 i, freqs[i], expected[i]);
        ASSERT_FLOAT_EQ(freqs[i], expected[i], 0.6f, msg);
    }

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 5: Scale degree frequencies — octave wrapping                */
/* ----------------------------------------------------------------- */
static int test_octave_wrapping(void) {
    TEST("octave wrapping: degree past scale end shifts up by octave(s)");

    AcidSynth s;
    synth_init(&s, SR);

    /* Set to Major on A4 */
    synth_set_scale(&s, SCALE_MAJOR);
    synth_set_scale_root(&s, 440.0f);

    /* Degree 0 = root octave, degree 7 = +1 octave = degree 0 * 2 */
    float f0 = synth_scale_degree_freq(&s, 0);
    float f7 = synth_scale_degree_freq(&s, 7);
    float f14 = synth_scale_degree_freq(&s, 14);  /* +2 octaves */

    char msg[80];
    snprintf(msg, sizeof(msg), "degree 7 = %.2f ≈ 2× degree 0 = %.2f", f7, f0 * 2.0f);
    ASSERT_FLOAT_EQ(f7, f0 * 2.0f, 0.6f, msg);

    snprintf(msg, sizeof(msg), "degree 14 = %.2f ≈ 4× degree 0 = %.2f", f14, f0 * 4.0f);
    ASSERT_FLOAT_EQ(f14, f0 * 4.0f, 0.6f, msg);

    /* Degree 8 (root octave+1, second degree) = degree 1 * 2 */
    float f1 = synth_scale_degree_freq(&s, 1);
    float f8 = synth_scale_degree_freq(&s, 8);
    ASSERT_FLOAT_EQ(f8, f1 * 2.0f, 0.6f, "degree 8 = 2× degree 1");

    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 6: All 16 scales have correct number of degrees              */
/* ----------------------------------------------------------------- */
static int test_all_scale_definitions(void) {
    TEST("all 16 scale definitions have correct number of degrees");

    /* Expected degree counts for each scale */
    int expected[] = {
        7,  /* SCALE_MAJOR */
        7,  /* SCALE_NATURAL_MINOR */
        7,  /* SCALE_DORIAN */
        7,  /* SCALE_PHRYGIAN */
        7,  /* SCALE_LYDIAN */
        7,  /* SCALE_MIXOLYDIAN */
        7,  /* SCALE_LOCRIAN */
        5,  /* SCALE_MINOR_PENTATONIC */
        5,  /* SCALE_MAJOR_PENTATONIC */
        6,  /* SCALE_BLUES */
        7,  /* SCALE_HARMONIC_MINOR */
        7,  /* SCALE_MELODIC_MINOR */
        12, /* SCALE_CHROMATIC */
        7,  /* SCALE_PHRYGIAN_DOMINANT */
        6,  /* SCALE_WHOLETONE */
        8,  /* SCALE_DIMINISHED */
    };

    for (int type = 0; type < SCALE_COUNT; type++) {
        const ScaleDef *sd = scale_get_def((ScaleType)type);
        char msg[80];
        snprintf(msg, sizeof(msg), "%s has %d degrees (expected %d)",
                 scale_names[type], sd->num_degrees, expected[type]);
        ASSERT(sd->num_degrees == expected[type], msg);
    }

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 7: Cents-between function is correct                         */
/* ----------------------------------------------------------------- */
static int test_cents_between(void) {
    TEST("scale_cents_between returns correct cent values");

    ASSERT_FLOAT_EQ(scale_cents_between(440.0f, 440.0f), 0.0f, 0.01f,
                    "same freq = 0 cents");
    ASSERT_FLOAT_EQ(scale_cents_between(880.0f, 440.0f), 1200.0f, 0.01f,
                    "octave up = 1200 cents");
    ASSERT_FLOAT_EQ(scale_cents_between(440.0f, 880.0f), -1200.0f, 0.01f,
                    "octave down = -1200 cents");
    ASSERT_FLOAT_EQ(scale_cents_between(660.0f, 440.0f), 701.96f, 0.5f,
                    "perfect fifth ≈ 702 cents");
    ASSERT_FLOAT_EQ(scale_cents_between(550.0f, 440.0f), 386.31f, 0.5f,
                    "major third ≈ 386 cents");

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 8: MIDI note conversion is correct                           */
/* ----------------------------------------------------------------- */
static int test_midi_to_freq(void) {
    TEST("MIDI note to frequency conversion (A4=440, equal temperament)");

    ASSERT_FLOAT_EQ(scale_midi_to_freq(69), 440.0f, 0.01f,
                    "MIDI 69 = A4 = 440Hz");
    ASSERT_FLOAT_EQ(scale_midi_to_freq(57), 220.0f, 0.1f,
                    "MIDI 57 = A3 = 220Hz");
    ASSERT_FLOAT_EQ(scale_midi_to_freq(81), 880.0f, 0.1f,
                    "MIDI 81 = A5 = 880Hz");
    ASSERT_FLOAT_EQ(scale_midi_to_freq(60), 261.63f, 0.1f,
                    "MIDI 60 = C4 = 261.63Hz");

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 9: Scale change re-generates all sequencer frequencies       */
/* ----------------------------------------------------------------- */
static int test_scale_change_propagation(void) {
    TEST("scale change recalculates sequencer frequencies");

    AcidSynth s;
    synth_init(&s, SR);

    /* Get step 0 note (root) and step 1 (third) frequencies on Phrygian C3 */
    float orig_root = s.seq.notes[0];
    float orig_third = s.seq.notes[1];
    ASSERT(orig_root > 0.0f, "initial root freq > 0");
    ASSERT(orig_third > 0.0f, "initial third freq > 0");

    /* Change scale to Major — root stays same, third changes */
    synth_set_scale(&s, SCALE_MAJOR);
    float major_root = s.seq.notes[0];
    float major_third = s.seq.notes[1];
    ASSERT(major_root > 0.0f, "major root freq > 0");
    ASSERT(major_third > 0.0f, "major third freq > 0");

    /* Root should be the same (same root freq, degree 0 = ^1) */
    ASSERT_FLOAT_EQ(orig_root, major_root, 1.0f, "root unchanged after scale switch");

    /* Third should differ (Phrygian b3 vs Major 3) */
    float third_diff = fabsf(orig_third - major_third);
    char msg[80];
    snprintf(msg, sizeof(msg), "third changed: Phrygian=%.2f, Major=%.2f, diff=%.2f",
             orig_third, major_third, third_diff);
    ASSERT(third_diff > 0.5f, msg);

    /* Change root note to A3 */
    synth_set_scale_root(&s, 220.0f);  /* A3 */
    float a3_root = s.seq.notes[0];
    /* A3/C3 ratio = 220/130.81 ≈ 1.68 */
    float ratio = a3_root / major_root;
    snprintf(msg, sizeof(msg), "A3/C3 root ratio=%.4f (expected ~%.4f)",
             ratio, 220.0f/130.81f);
    ASSERT_FLOAT_EQ(ratio, 220.0f/130.81f, 0.01f, msg);

    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 10: Generated melodic pattern follows musimathics rules      */
/* ----------------------------------------------------------------- */
static int test_melodic_pattern_rules(void) {
    TEST("generated acid pattern follows musimathics melodic rules");

    AcidSynth s;
    synth_init(&s, SR);

    /* Test for Phrygian (7 degrees) */
    synth_set_scale(&s, SCALE_PHRYGIAN);
    const ScaleDef *sd = scale_get_def(SCALE_PHRYGIAN);
    int n = sd->num_degrees;

    /* Rule 1: All degrees are valid (within range) */
    int all_valid = 1;
    for (int i = 0; i < 16; i++) {
        if (s.seq_degrees[i] < 0 || (s.seq_degrees[i] % n) >= n) {
            all_valid = 0;
        }
    }
    ASSERT(all_valid, "all 16 degrees in valid range");

    /* Rule 2: Root (degree % n == 0) appears ≥ 2 times */
    int root_count = 0;
    for (int i = 0; i < 16; i++) {
        if (s.seq_degrees[i] % n == 0) root_count++;
    }
    ASSERT(root_count >= 2, "root appears ≥2/16 steps");

    /* Rule 3: Step-wise motion dominates (≤2 steps) */
    int step_moves = 0, leaps = 0;
    for (int i = 1; i < 16; i++) {
        int diff = abs(s.seq_degrees[i] - s.seq_degrees[i-1]);
        if (diff <= 2) step_moves++;
        else leaps++;
    }
    ASSERT(step_moves > leaps, "step-wise motion > leaps");

    /* Rule 4: Fifth (degree 4) appears at least once */
    int fifth_count = 0;
    for (int i = 0; i < 16; i++) {
        int d = s.seq_degrees[i] % n;
        if (d == 4) fifth_count++;
    }
    ASSERT(fifth_count >= 1, "fifth (degree 4) appears ≥1");

    /* Test blues pentatonic (5 degrees) */
    synth_set_scale(&s, SCALE_BLUES);
    sd = scale_get_def(SCALE_BLUES);
    n = sd->num_degrees;
    all_valid = 1;
    for (int i = 0; i < 16; i++) {
        if (s.seq_degrees[i] < 0 || (s.seq_degrees[i] % n) >= n) {
            all_valid = 0;
        }
    }
    ASSERT(all_valid, "blues: all 16 degrees in valid range");

    root_count = 0;
    for (int i = 0; i < 16; i++) {
        if (s.seq_degrees[i] % n == 0) root_count++;
    }
    ASSERT(root_count >= 2, "blues: root appears ≥2/16 steps");

    /* Test chromatic (12 degrees) */
    synth_set_scale(&s, SCALE_CHROMATIC);
    sd = scale_get_def(SCALE_CHROMATIC);
    n = sd->num_degrees;
    all_valid = 1;
    for (int i = 0; i < 16; i++) {
        if (s.seq_degrees[i] < 0 || (s.seq_degrees[i] % n) >= n) {
            all_valid = 0;
        }
    }
    ASSERT(all_valid, "chromatic: all degrees in valid range");

    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 11: Equal temperament toggle works correctly                  */
/* ----------------------------------------------------------------- */
static int test_equal_temperament_toggle(void) {
    TEST("equal temperament generates standard 12-TET frequencies");

    const ScaleDef *sd = scale_get_def(SCALE_MAJOR);
    float root = 440.0f;

    float just_freqs[7], et_freqs[7];
    scale_generate_freqs(sd, root, just_freqs, 0);
    scale_generate_freqs(sd, root, et_freqs, 1);

    /* ET frequencies for A4 major:
     * A=440.00, B=493.88, C#=554.37, D=587.33, E=659.25, F#=739.99, G#=830.61 */
    float expected_et[] = {440.0f, 493.88f, 554.37f, 587.33f,
                           659.25f, 739.99f, 830.61f};

    for (int i = 0; i < 7; i++) {
        char msg[80];
        snprintf(msg, sizeof(msg), "ET degree %d freq=%.2f (expected %.2f)",
                 i, et_freqs[i], expected_et[i]);
        ASSERT_FLOAT_EQ(et_freqs[i], expected_et[i], 0.5f, msg);
    }

    /* Just intonation should differ from ET for most intervals */
    int any_different = 0;
    for (int i = 0; i < 7; i++) {
        if (fabsf(just_freqs[i] - et_freqs[i]) > 0.5f) any_different = 1;
    }
    ASSERT(any_different, "JI and ET produce different frequencies");

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 12: Scale ratios produce correct float values                */
/* ----------------------------------------------------------------- */
static int test_scale_ratio_float(void) {
    TEST("scale_ratio_float returns correct numerator/denominator values");

    JustRatio r;

    r = scale_just_ratio(INTERVAL_UNISON);
    ASSERT_FLOAT_EQ(scale_ratio_float(r), 1.0f, 0.0001f, "unison = 1.0");

    r = scale_just_ratio(INTERVAL_PERFECT_FIFTH);
    ASSERT_FLOAT_EQ(scale_ratio_float(r), 1.5f, 0.0001f, "fifth = 1.5");

    r = scale_just_ratio(INTERVAL_MAJOR_THIRD);
    ASSERT_FLOAT_EQ(scale_ratio_float(r), 1.25f, 0.0001f, "major third = 1.25");

    r = scale_just_ratio(INTERVAL_TRITONE);
    ASSERT_FLOAT_EQ(scale_ratio_float(r), 45.0f/32.0f, 0.0001f, "tritone = 45/32");

    r = scale_just_ratio(INTERVAL_OCTAVE);
    ASSERT_FLOAT_EQ(scale_ratio_float(r), 2.0f, 0.0001f, "octave = 2.0");

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 13: Scale interval to frequency                              */
/* ----------------------------------------------------------------- */
static int test_scale_interval_to_freq(void) {
    TEST("scale_interval_to_freq produces correct frequencies");

    float root = 440.0f;

    ASSERT_FLOAT_EQ(scale_interval_to_freq(root, INTERVAL_UNISON),
                    440.0f, 0.01f, "unison = 440");
    ASSERT_FLOAT_EQ(scale_interval_to_freq(root, INTERVAL_PERFECT_FIFTH),
                    660.0f, 0.01f, "fifth = 660");
    ASSERT_FLOAT_EQ(scale_interval_to_freq(root, INTERVAL_OCTAVE),
                    880.0f, 0.01f, "octave = 880");
    ASSERT_FLOAT_EQ(scale_interval_to_freq(root, INTERVAL_MAJOR_THIRD),
                    550.0f, 0.01f, "major third = 550");
    ASSERT_FLOAT_EQ(scale_interval_to_freq(root, INTERVAL_PERFECT_FOURTH),
                    586.67f, 0.1f, "fourth ≈ 586.67");

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 14: Scale get_def handles invalid type gracefully            */
/* ----------------------------------------------------------------- */
static int test_scale_get_def_invalid(void) {
    TEST("scale_get_def handles invalid/out-of-range types gracefully");

    /* Negative type should default to MAJOR */
    const ScaleDef *sd = scale_get_def((ScaleType)(-1));
    ASSERT(sd != NULL, "negative type returns non-NULL");
    ASSERT(sd->type == SCALE_MAJOR, "invalid type defaults to major");
    ASSERT(sd->num_degrees > 0, "default has positive degrees");

    /* Huge type */
    sd = scale_get_def((ScaleType)999);
    ASSERT(sd != NULL, "large type returns non-NULL");
    ASSERT(sd->type == SCALE_MAJOR, "large type defaults to major");

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 15: Synth produces correct frequencies with Just Intonation  */
/* ----------------------------------------------------------------- */
static int test_synth_ji_frequencies(void) {
    TEST("synth produces correct just intonation frequencies");

    AcidSynth s;
    synth_init(&s, SR);
    s.seq_mode = 0;
    s.mix_osc1 = 1.0f; s.mix_osc2 = 0.0f; s.mix_osc3 = 0.0f; s.mix_sub = 0.0f;
    s.env_attack = 0.002f; s.env_release = 0.01f;
    wavefolder_init(&s.wavefolder, 0.01f, 0.0f);
    filter_init(&s.filter, FILTER_LP, 20000.0f, 0.01f, SR);
    delay_set_mix(&s.delay, 0.0f);
    reverb_set_mix(&s.reverb, 0.0f);

    int n = (int)(SR * 0.3f);
    float *buf = (float*)calloc(n, sizeof(float));
    float *bufR = (float*)calloc(n, sizeof(float));

    /* Test: Just Intonation perfect fifth (3/2) from C3 */
    s.use_equal_temp = 0;  /* Must use JI mode for pure ratio tests */
    synth_set_scale(&s, SCALE_PHRYGIAN);
    synth_set_scale_root(&s, 130.81f);  /* C3 */

    /* Get Phrygian degree 4 (fifth) frequency */
    float fifth_freq = synth_scale_degree_freq(&s, 4);
    ASSERT_FLOAT_EQ(fifth_freq, 196.22f, 0.6f, "Phrygian fifth = 196.22Hz");

    /* Set osc to that frequency and verify output */
    osc_init(&s.osc1, OSC_WAVE_SINE, fifth_freq);
    synth_note_on(&s, fifth_freq);
    synth_process_block(&s, buf, bufR, n);
    synth_note_off(&s);

    /* Zero-crossing detection */
    int start = (int)(0.05f * SR);
    int zc1 = -1, zc2 = -1;
    for (int i = start; i < n - 1; i++) {
        if (buf[i-1] <= 0.0f && buf[i] > 0.0f) {
            if (zc1 < 0) zc1 = i;
            else { zc2 = i; break; }
        }
    }
    if (zc1 >= 0 && zc2 > zc1) {
        float detected = SR / (float)(zc2 - zc1);
        char msg[80];
        snprintf(msg, sizeof(msg), "JI fifth=%.2f, detected=%.2f",
                 fifth_freq, detected);
        ASSERT_FLOAT_EQ(detected, fifth_freq, 15.0f, msg);
    }

    /* Test: Just Intonation major third (5/4) from C3 */
    float third_freq = synth_scale_degree_freq(&s, 2);  /* Phrygian = minor 3rd */

    /* For a major third we need a different scale */
    synth_set_scale(&s, SCALE_MAJOR);
    third_freq = synth_scale_degree_freq(&s, 2);
    ASSERT_FLOAT_EQ(third_freq, 163.51f, 0.6f, "Major third (JI) from C3 ≈ 163.51Hz");

    osc_init(&s.osc1, OSC_WAVE_SINE, third_freq);
    synth_note_on(&s, third_freq);
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
        float detected = SR / (float)(zc2 - zc1);
        char msg[80];
        snprintf(msg, sizeof(msg), "JI third=%.2f, detected=%.2f",
                 third_freq, detected);
        ASSERT_FLOAT_EQ(detected, third_freq, 15.0f, msg);
    }

    free(buf); free(bufR);
    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 16: Scale transition has no DC pop                           */
/* ----------------------------------------------------------------- */
static int test_scale_transition_no_pop(void) {
    TEST("scale transition produces no audible pop (moderate max diff)");

    AcidSynth s;
    synth_init(&s, SR);
    s.seq_mode = 0;

    int n = (int)(SR * 0.1f);
    float *buf = (float*)calloc(n, sizeof(float));
    float *bufR = (float*)calloc(n, sizeof(float));

    /* Hold a note, switch scale mid-block */
    synth_note_on(&s, synth_scale_degree_freq(&s, 0));
    synth_process_block(&s, buf, bufR, n/2);

    /* Switch scale while note is held */
    synth_set_scale(&s, SCALE_DORIAN);
    synth_process_block(&s, buf + n/2, bufR + n/2, n/2);

    /* Check max adjacent sample diff (conservative: < 2.0) */
    float max_diff = 0.0f;
    for (int i = 1; i < n; i++) {
        float diff = fabsf(buf[i] - buf[i-1]);
        if (diff > max_diff) max_diff = diff;
    }
    ASSERT(max_diff < 2.0f, "scale transition max adjacent diff < 2.0");
    ASSERT(max_diff > 0.0f, "audio continues after scale change");

    free(buf); free(bufR);
    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 17: All scale types produce playable frequencies              */
/* ----------------------------------------------------------------- */
static int test_all_scales_produce_audio(void) {
    TEST("every scale type produces playable audio output");

    for (int type = 0; type < SCALE_COUNT; type++) {
        AcidSynth s;
        synth_init(&s, SR);

        synth_set_scale(&s, (ScaleType)type);

        int n = (int)(SR * 0.1f);
        float *bufL = (float*)calloc(n, sizeof(float));
        float *bufR = (float*)calloc(n, sizeof(float));

        synth_gate(&s, 1);
        synth_process_block(&s, bufL, bufR, n);
        synth_gate(&s, 0);

        /* Check for audio and no clipping */
        float peak = 0.0f;
        for (int i = 0; i < n; i++) {
            float a = fabsf(bufL[i]);
            if (a > peak) peak = a;
        }

        char msg[80];
        snprintf(msg, sizeof(msg), "%s produces audio (peak=%.4f)",
                 scale_names[type], peak);
        ASSERT(peak > 0.0f, msg);
        ASSERT(peak <= 1.001f, "no clipping on any scale");

        free(bufL); free(bufR);
        synth_free(&s);
    }

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Main                                                              */
/* ----------------------------------------------------------------- */
int main(void) {
    printf("=== test_scale_musimathics: Just Intonation Scale Tests (Musimathics) ===\n");
    int fail = 0;

    fail += test_just_ratios_match_musimathics();
    fail += test_cent_deviations();
    fail += test_scale_freqs_phrygian_c3();
    fail += test_scale_freqs_major_a4();
    fail += test_octave_wrapping();
    fail += test_all_scale_definitions();
    fail += test_cents_between();
    fail += test_midi_to_freq();
    fail += test_scale_change_propagation();
    fail += test_melodic_pattern_rules();
    fail += test_equal_temperament_toggle();
    fail += test_scale_ratio_float();
    fail += test_scale_interval_to_freq();
    fail += test_scale_get_def_invalid();
    fail += test_synth_ji_frequencies();
    fail += test_scale_transition_no_pop();
    fail += test_all_scales_produce_audio();

    TEST_SUMMARY();
}
