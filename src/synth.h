#ifndef SYNTH_H
#define SYNTH_H

#ifdef __cplusplus
extern "C" {
#endif


/* ----------------------------------------------------------------- */
/*  Synth module — Integrates osc + filter + wavefolder + delay     */
/*  + reverb + sequencer + scale + MIDI attack layer into a         */
/*  complete acid synth. Pure C, no dependencies.                    */
/* ----------------------------------------------------------------- */

#include "osc.h"
#include "filter.h"
#include "delay.h"
#include "wavefolder.h"
#include "reverb.h"
#include "sequencer.h"
#include "lfo.h"
#include "scale.h"

#define SEQ_STEPS 16

/* ----------------------------------------------------------------- */
/*  Attack layer — sample-based attack transient layer               */
/*  Loads a WAV file, plays it pitch-shifted at each note_on,       */
/*  crossfades into the synth body.                                  */
/* ----------------------------------------------------------------- */
typedef struct {
    float* samples;       /* loaded attack samples (mono, mixed down) */
    int    num_samples;   /* total samples loaded */
    int    sample_rate;   /* sample rate of the loaded WAV */
    float  root_freq;     /* the MIDI note frequency this sample was recorded at */
    int    loaded;        /* 1 if samples are loaded */

    /* Playback state */
    double phase;         /* current playback position in samples */
    int    playing;       /* 1 if attack is currently playing */
    int    enabled;       /* 0 = synth only, 1 = attack layer active */
    int    note_finished; /* flag: attack finished playing for current note */

    /* Crossfade */
    int    crossfade_samples; /* length of crossfade region in output samples */
} AttackLayer;

/* Initialize attack layer (call once) */
void attack_init(AttackLayer *a);

/* Free loaded samples */
void attack_free(AttackLayer *a);

/* Load attack WAV from memory buffer */
void attack_load(AttackLayer *a, const float *samples, int num_samples,
                 int sample_rate, float root_freq);

/* Process attack layer: returns the attack contribution to mix.
 * Call once per sample. When attack finishes, returns 0 and synth takes over. */
float attack_process(AttackLayer *a, float synth_body, float note_freq, float sr);

/* Trigger attack on note_on */
void attack_note_on(AttackLayer *a);

/* ----------------------------------------------------------------- */
/*  Main synth structure                                             */
/* ----------------------------------------------------------------- */
typedef struct {
    // 3 VCO
    Osc osc1, osc2, osc3;

    // Sub oscillator
    Osc osc_sub;

    // Mixer
    float mix_osc1, mix_osc2, mix_osc3, mix_sub;

    // Wavefolder
    Wavefolder wavefolder;

    // Filter
    Filter filter;

    // LFO for filter modulation
    Lfo lfo;

    // Modulation amounts
    float lfo_to_filter;  // LFO → filter cutoff modulation depth
    float lfo_rate;

    // Filter base values (UI sliders set these, synth_process applies them)
    float cutoff_base;    // base filter cutoff in Hz (set by slider)
    float reso_base;      // base filter resonance 0..1 (set by slider)

    // Envelope
    float envelope;
    float env_attack, env_decay, env_sustain, env_release;

    // Delay
    Delay delay;

    // Reverb
    Reverb reverb;

    // Sequencer
    Sequencer seq;
    int seq_mode;       // 1 = auto-sequencer, 0 = keyboard input

    // --- Scale system (musimathics Just Intonation) ---
    ScaleType scale_type;       // which scale (major, phrygian, etc.)
    float     scale_root_freq;  // root frequency of the scale (e.g. 220.0 for A3)
    float     scale_freqs[SCALE_MAX_DEGREES]; // precomputed scale frequencies
    int       seq_degrees[SEQ_STEPS]; // scale degree for each sequencer step

    // Dreamscape mode
    int dreamscape_mode;    // 1 = dreamy (triangle waves, slow env, big verb)
    int use_equal_temp;     // 1 = 12-TET, 0 = Just Intonation (musimathics pure ratios)

    // Master
    float master_volume;

    // Attack layer
    AttackLayer attack;

    // Internal state
    float sr;
    int gate_on;  // note gate
} AcidSynth;

/* Initialize complete synth with default scale */
void synth_init(AcidSynth *s, float sr);

/* Free dynamically allocated resources */
void synth_free(AcidSynth *s);

/* Set the scale type (Just Intonation) */
void synth_set_scale(AcidSynth *s, ScaleType type);

/* Set the scale root frequency */
void synth_set_scale_root(AcidSynth *s, float root_freq);

/* Get the current scale's frequency for a given degree */
float synth_scale_degree_freq(const AcidSynth *s, int degree);

/* Set note frequency (Hz) directly */
void synth_set_note(AcidSynth *s, float freq_hz);

/* Gate on/off */
void synth_gate(AcidSynth *s, int on);

/* Note on — sets freq, triggers envelope + attack layer, sets gate */
void synth_note_on(AcidSynth *s, float freq_hz);

/* Note off — releases envelope */
void synth_note_off(AcidSynth *s);

/* Set sequencer tick rate */
void synth_set_seq_rate(AcidSynth *s, float rate_hz);

/* Toggle dreamscape mode (triangle waves, slow envelope, big reverb) */
void synth_set_dreamscape(AcidSynth *s, int on);

/* Toggle equal temperament (1) vs Just Intonation (0) for scale frequencies */
void synth_set_equal_temp(AcidSynth *s, int on);

/* Process one sample — returns stereo pair in outL, outR */
void synth_process(AcidSynth *s, float *outL, float *outR);

/* Process block of stereo samples */
void synth_process_block(AcidSynth *s, float *outL, float *outR, int n);

#ifdef __cplusplus
}
#endif

#endif
