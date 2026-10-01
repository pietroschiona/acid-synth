#ifndef SCALE_H
#define SCALE_H

#ifdef __cplusplus
extern "C" {
#endif


/* ----------------------------------------------------------------- */
/*  Scale module — Just Intonation scale generation                  */
/*  Based on Musimathics (Loy Vol 1): pure frequency ratios.         */
/*  Every note frequency = root_freq * ratio.                        */
/* ----------------------------------------------------------------- */

/* Maximum scale degrees supported */
#define SCALE_MAX_DEGREES 16

/* Interval ratios from musimathics Section 6.2 */
typedef enum {
    /* Basic intervals */
    INTERVAL_UNISON      = 0,
    INTERVAL_MINOR_SEC   = 1,
    INTERVAL_MAJOR_SEC   = 2,
    INTERVAL_MINOR_THIRD = 3,
    INTERVAL_MAJOR_THIRD = 4,
    INTERVAL_PERFECT_FOURTH = 5,
    INTERVAL_TRITONE     = 6,
    INTERVAL_PERFECT_FIFTH  = 7,
    INTERVAL_MINOR_SIXTH = 8,
    INTERVAL_MAJOR_SIXTH = 9,
    INTERVAL_MINOR_SEVENTH = 10,
    INTERVAL_MAJOR_SEVENTH = 11,
    INTERVAL_OCTAVE      = 12,
    INTERVAL_COUNT
} Interval;

/* Just intonation frequency ratios (musimathics table 6.2) */
/* ratio = numerator/denominator */
typedef struct {
    int numerator;
    int denominator;
    const char *name;
} JustRatio;

/* Get the pure ratio for an interval */
JustRatio scale_just_ratio(Interval interval);

/* Scale types */
typedef enum {
    SCALE_MAJOR,           /* Ionian    */
    SCALE_NATURAL_MINOR,   /* Aeolian   */
    SCALE_DORIAN,
    SCALE_PHRYGIAN,
    SCALE_LYDIAN,
    SCALE_MIXOLYDIAN,
    SCALE_LOCRIAN,
    SCALE_MINOR_PENTATONIC,
    SCALE_MAJOR_PENTATONIC,
    SCALE_BLUES,
    SCALE_HARMONIC_MINOR,
    SCALE_MELODIC_MINOR,
    SCALE_CHROMATIC,
    SCALE_PHRYGIAN_DOMINANT, /* aka Spanish gypsy */
    SCALE_WHOLETONE,
    SCALE_DIMINISHED,
    SCALE_COUNT
} ScaleType;

extern const char *scale_names[SCALE_COUNT];

/* A complete scale definition */
typedef struct {
    ScaleType type;
    int num_degrees;           /* how many notes in the scale (e.g. 7 for heptatonic) */
    Interval degrees[SCALE_MAX_DEGREES]; /* intervals from root */
} ScaleDef;

/* Get a scale definition by type */
const ScaleDef *scale_get_def(ScaleType type);

/* Generate frequencies for a scale given root frequency.
 * Fills freq[] with num_degrees frequencies.
 * Uses just intonation by default.
 * If use_equal_temperament is non-zero, uses 12-TET instead. */
void scale_generate_freqs(const ScaleDef *scale,
                           float root_freq,
                           float *freqs_out,
                           int use_equal_temperament);

/* Convert MIDI note to frequency (equal temperament, standard)
 * f = 440 * 2^((midi - 69) / 12) */
float scale_midi_to_freq(int midi_note);

/* Convert root_freq + interval → frequency using just intonation ratio */
float scale_interval_to_freq(float root_freq, Interval interval);

/* Return the ratio value as a float: numerator/denominator */
float scale_ratio_float(JustRatio r);

/* Return the cent deviation from equal temperament for a just interval
 * Positive = sharper than ET, negative = flatter */
float scale_cent_deviation(Interval interval);

/* Return the cent value between two frequencies */
float scale_cents_between(float f1, float f2);

#ifdef __cplusplus
}
#endif

#endif
