#include "scale.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ----------------------------------------------------------------- */
/*  Just intonation ratios (musimathics Section 6.2)                  */
/* ----------------------------------------------------------------- */
static const JustRatio just_ratios[INTERVAL_COUNT] = {
    {  1,  1, "Unison"       },   /* 0 */
    { 16, 15, "Minor 2nd"    },   /* 1 */
    {  9,  8, "Major 2nd"    },   /* 2 */
    {  6,  5, "Minor 3rd"    },   /* 3 */
    {  5,  4, "Major 3rd"    },   /* 4 */
    {  4,  3, "Perfect 4th"  },   /* 5 */
    { 45, 32, "Tritone"      },   /* 6 */
    {  3,  2, "Perfect 5th"  },   /* 7 */
    {  8,  5, "Minor 6th"    },   /* 8 */
    {  5,  3, "Major 6th"    },   /* 9 */
    { 16,  9, "Minor 7th"    },   /* 10 */
    { 15,  8, "Major 7th"    },   /* 11 */
    {  2,  1, "Octave"       }    /* 12 */
};

const char *scale_names[SCALE_COUNT] = {
    "Major (Ionian)",
    "Natural Minor (Aeolian)",
    "Dorian",
    "Phrygian",
    "Lydian",
    "Mixolydian",
    "Locrian",
    "Minor Pentatonic",
    "Major Pentatonic",
    "Blues",
    "Harmonic Minor",
    "Melodic Minor",
    "Chromatic",
    "Phrygian Dominant",
    "Wholetone",
    "Diminished"
};

JustRatio scale_just_ratio(Interval interval) {
    if (interval < 0 || interval >= INTERVAL_COUNT)
        interval = INTERVAL_UNISON;
    return just_ratios[interval];
}

float scale_ratio_float(JustRatio r) {
    return (float)r.numerator / (float)r.denominator;
}

/* ----------------------------------------------------------------- */
/*  Scale definitions — each scale maps degrees to intervals          */
/* ----------------------------------------------------------------- */

#define DEFINE_SCALE(name, degs, ...) \
    static const ScaleDef scale_##name = { name, degs, { __VA_ARGS__ } }

DEFINE_SCALE(SCALE_MAJOR, 7,
    INTERVAL_UNISON, INTERVAL_MAJOR_SEC, INTERVAL_MAJOR_THIRD,
    INTERVAL_PERFECT_FOURTH, INTERVAL_PERFECT_FIFTH, INTERVAL_MAJOR_SIXTH,
    INTERVAL_MAJOR_SEVENTH);

DEFINE_SCALE(SCALE_NATURAL_MINOR, 7,
    INTERVAL_UNISON, INTERVAL_MAJOR_SEC, INTERVAL_MINOR_THIRD,
    INTERVAL_PERFECT_FOURTH, INTERVAL_PERFECT_FIFTH, INTERVAL_MINOR_SIXTH,
    INTERVAL_MINOR_SEVENTH);

DEFINE_SCALE(SCALE_DORIAN, 7,
    INTERVAL_UNISON, INTERVAL_MAJOR_SEC, INTERVAL_MINOR_THIRD,
    INTERVAL_PERFECT_FOURTH, INTERVAL_PERFECT_FIFTH, INTERVAL_MAJOR_SIXTH,
    INTERVAL_MINOR_SEVENTH);

DEFINE_SCALE(SCALE_PHRYGIAN, 7,
    INTERVAL_UNISON, INTERVAL_MINOR_SEC, INTERVAL_MINOR_THIRD,
    INTERVAL_PERFECT_FOURTH, INTERVAL_PERFECT_FIFTH, INTERVAL_MINOR_SIXTH,
    INTERVAL_MINOR_SEVENTH);

DEFINE_SCALE(SCALE_LYDIAN, 7,
    INTERVAL_UNISON, INTERVAL_MAJOR_SEC, INTERVAL_MAJOR_THIRD,
    INTERVAL_TRITONE, INTERVAL_PERFECT_FIFTH, INTERVAL_MAJOR_SIXTH,
    INTERVAL_MAJOR_SEVENTH);

DEFINE_SCALE(SCALE_MIXOLYDIAN, 7,
    INTERVAL_UNISON, INTERVAL_MAJOR_SEC, INTERVAL_MAJOR_THIRD,
    INTERVAL_PERFECT_FOURTH, INTERVAL_PERFECT_FIFTH, INTERVAL_MAJOR_SIXTH,
    INTERVAL_MINOR_SEVENTH);

DEFINE_SCALE(SCALE_LOCRIAN, 7,
    INTERVAL_UNISON, INTERVAL_MINOR_SEC, INTERVAL_MINOR_THIRD,
    INTERVAL_PERFECT_FOURTH, INTERVAL_TRITONE, INTERVAL_MINOR_SIXTH,
    INTERVAL_MINOR_SEVENTH);

/* Pentatonics */
DEFINE_SCALE(SCALE_MINOR_PENTATONIC, 5,
    INTERVAL_UNISON, INTERVAL_MINOR_THIRD, INTERVAL_PERFECT_FOURTH,
    INTERVAL_PERFECT_FIFTH, INTERVAL_MINOR_SEVENTH);

DEFINE_SCALE(SCALE_MAJOR_PENTATONIC, 5,
    INTERVAL_UNISON, INTERVAL_MAJOR_SEC, INTERVAL_MAJOR_THIRD,
    INTERVAL_PERFECT_FIFTH, INTERVAL_MAJOR_SIXTH);

/* Blues = minor pentatonic + flat 5 (tritone) */
DEFINE_SCALE(SCALE_BLUES, 6,
    INTERVAL_UNISON, INTERVAL_MINOR_THIRD, INTERVAL_PERFECT_FOURTH,
    INTERVAL_TRITONE, INTERVAL_PERFECT_FIFTH, INTERVAL_MINOR_SEVENTH);

/* Harmonic minor = natural minor + major 7th */
DEFINE_SCALE(SCALE_HARMONIC_MINOR, 7,
    INTERVAL_UNISON, INTERVAL_MAJOR_SEC, INTERVAL_MINOR_THIRD,
    INTERVAL_PERFECT_FOURTH, INTERVAL_PERFECT_FIFTH, INTERVAL_MINOR_SIXTH,
    INTERVAL_MAJOR_SEVENTH);

/* Melodic minor (ascending) = natural minor with 6th and 7th raised */
DEFINE_SCALE(SCALE_MELODIC_MINOR, 7,
    INTERVAL_UNISON, INTERVAL_MAJOR_SEC, INTERVAL_MINOR_THIRD,
    INTERVAL_PERFECT_FOURTH, INTERVAL_PERFECT_FIFTH, INTERVAL_MAJOR_SIXTH,
    INTERVAL_MAJOR_SEVENTH);

/* Chromatic = all 12 semitones */
DEFINE_SCALE(SCALE_CHROMATIC, 12,
    INTERVAL_UNISON, INTERVAL_MINOR_SEC, INTERVAL_MAJOR_SEC,
    INTERVAL_MINOR_THIRD, INTERVAL_MAJOR_THIRD, INTERVAL_PERFECT_FOURTH,
    INTERVAL_TRITONE, INTERVAL_PERFECT_FIFTH, INTERVAL_MINOR_SIXTH,
    INTERVAL_MAJOR_SIXTH, INTERVAL_MINOR_SEVENTH, INTERVAL_MAJOR_SEVENTH);

/* Phrygian Dominant = phrygian with major 3rd (flamenco/middle eastern) */
DEFINE_SCALE(SCALE_PHRYGIAN_DOMINANT, 7,
    INTERVAL_UNISON, INTERVAL_MINOR_SEC, INTERVAL_MAJOR_THIRD,
    INTERVAL_PERFECT_FOURTH, INTERVAL_PERFECT_FIFTH, INTERVAL_MINOR_SIXTH,
    INTERVAL_MINOR_SEVENTH);

/* Wholetone = whole steps only */
DEFINE_SCALE(SCALE_WHOLETONE, 6,
    INTERVAL_UNISON, INTERVAL_MAJOR_SEC, INTERVAL_MAJOR_THIRD,
    INTERVAL_TRITONE, INTERVAL_MINOR_SIXTH, INTERVAL_MINOR_SEVENTH);

/* Diminished = alternating whole/half steps (octatonic) */
DEFINE_SCALE(SCALE_DIMINISHED, 8,
    INTERVAL_UNISON, INTERVAL_MAJOR_SEC, INTERVAL_MINOR_THIRD,
    INTERVAL_PERFECT_FOURTH, INTERVAL_TRITONE, INTERVAL_PERFECT_FIFTH,
    INTERVAL_MAJOR_SIXTH, INTERVAL_MINOR_SEVENTH);

/* Lookup table */
static const ScaleDef *all_scales[SCALE_COUNT] = {
    &scale_SCALE_MAJOR,
    &scale_SCALE_NATURAL_MINOR,
    &scale_SCALE_DORIAN,
    &scale_SCALE_PHRYGIAN,
    &scale_SCALE_LYDIAN,
    &scale_SCALE_MIXOLYDIAN,
    &scale_SCALE_LOCRIAN,
    &scale_SCALE_MINOR_PENTATONIC,
    &scale_SCALE_MAJOR_PENTATONIC,
    &scale_SCALE_BLUES,
    &scale_SCALE_HARMONIC_MINOR,
    &scale_SCALE_MELODIC_MINOR,
    &scale_SCALE_CHROMATIC,
    &scale_SCALE_PHRYGIAN_DOMINANT,
    &scale_SCALE_WHOLETONE,
    &scale_SCALE_DIMINISHED
};

const ScaleDef *scale_get_def(ScaleType type) {
    if (type < 0 || type >= SCALE_COUNT)
        type = SCALE_MAJOR;
    return all_scales[type];
}

/* ----------------------------------------------------------------- */
/*  Frequency generation                                              */
/* ----------------------------------------------------------------- */

void scale_generate_freqs(const ScaleDef *scale,
                           float root_freq,
                           float *freqs_out,
                           int use_equal_temperament)
{
    if (!scale || !freqs_out) return;
    for (int i = 0; i < scale->num_degrees && i < SCALE_MAX_DEGREES; i++) {
        if (use_equal_temperament) {
            /* Equal temperament: semitone steps */
            int semitone = (int)scale->degrees[i]; /* interval enum = semitone count */
            freqs_out[i] = root_freq * powf(2.0f, semitone / 12.0f);
        } else {
            /* Just intonation: pure frequency ratio */
            JustRatio r = scale_just_ratio(scale->degrees[i]);
            freqs_out[i] = root_freq * scale_ratio_float(r);
        }
    }
}

float scale_midi_to_freq(int midi_note) {
    return 440.0f * powf(2.0f, (midi_note - 69) / 12.0f);
}

float scale_interval_to_freq(float root_freq, Interval interval) {
    JustRatio r = scale_just_ratio(interval);
    return root_freq * scale_ratio_float(r);
}

/* ----------------------------------------------------------------- */
/*  Cent utilities                                                    */
/* ----------------------------------------------------------------- */

float scale_cents_between(float f1, float f2) {
    if (f1 <= 0.0f || f2 <= 0.0f) return 0.0f;
    return 1200.0f * logf(f1 / f2) / logf(2.0f);
}

float scale_cent_deviation(Interval interval) {
    /* Just intonation ratio in cents */
    JustRatio r = scale_just_ratio(interval);
    float just_cents = 1200.0f * logf((float)r.numerator / r.denominator) / logf(2.0f);
    /* Equal temperament semitone in cents */
    float et_cents = (float)interval * 100.0f;
    return just_cents - et_cents;
}
