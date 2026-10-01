#include "synth.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

static float soft_clip(float x) {
    if (x > 1.0f) return 1.0f;
    if (x < -1.0f) return -1.0f;
    return x * (1.5f - 0.5f * x * x);
}

/* ----------------------------------------------------------------- */
/*  Attack Layer — sample-based attack transient                     */
/*  Plays a loaded WAV at the correct pitch at each note_on,         */
/*  crossfades into the synth body.                                  */
/* ----------------------------------------------------------------- */
void attack_init(AttackLayer *a) {
    a->samples = NULL;
    a->num_samples = 0;
    a->sample_rate = 44100;
    a->root_freq = 440.0f;
    a->loaded = 0;
    a->phase = 0.0;
    a->playing = 0;
    a->enabled = 0;
    a->note_finished = 0;
    a->crossfade_samples = 441; /* 10ms crossfade at 44.1kHz */
}

void attack_free(AttackLayer *a) {
    free(a->samples);
    a->samples = NULL;
    a->num_samples = 0;
    a->loaded = 0;
}

void attack_load(AttackLayer *a, const float *samples, int num_samples,
                 int sample_rate, float root_freq) {
    attack_free(a);
    if (num_samples > 0 && samples) {
        a->samples = (float*)malloc(num_samples * sizeof(float));
        if (a->samples) {
            memcpy(a->samples, samples, num_samples * sizeof(float));
            a->num_samples = num_samples;
            a->sample_rate = sample_rate;
            a->root_freq = root_freq;
            a->loaded = 1;
        }
    }
}

void attack_note_on(AttackLayer *a) {
    if (a->enabled && a->loaded) {
        a->phase = 0.0;
        a->playing = 1;
        a->note_finished = 0;
    }
}

float attack_process(AttackLayer *a, float synth_body, float note_freq, float sr) {
    if (!a->playing || !a->loaded) return synth_body;

    /* Compute playback rate: pitch-shift the attack sample to match note_freq */
    double pitch_ratio = (double)note_freq / (double)a->root_freq;
    double step = pitch_ratio * (double)a->sample_rate / (double)sr;

    /* Read current sample with linear interpolation */
    double pos = a->phase;
    int idx = (int)pos;
    if (idx >= a->num_samples - 1) {
        a->playing = 0;
        a->note_finished = 1;
        return synth_body;
    }

    float frac = (float)(pos - idx);
    float attack_sample = a->samples[idx] * (1.0f - frac)
                        + a->samples[idx + 1] * frac;

    /* Advance position */
    a->phase += step;

    /* If we've passed the end, stop */
    if (a->phase >= a->num_samples - 1) {
        a->playing = 0;
        a->note_finished = 1;
    }

    /* Compute crossfade: attack fades out, synth fades in */
    float fade = 1.0f;
    if (a->phase > a->num_samples - a->crossfade_samples) {
        float remaining = (float)(a->num_samples - 1 - a->phase);
        if (remaining > 0) {
            fade = remaining / (float)a->crossfade_samples;
            if (fade < 0.0f) fade = 0.0f;
        } else {
            fade = 0.0f;
        }
    }

    /* Attack first (in crossfade window, attack fades out), then synth takes over */
    return attack_sample * fade + synth_body * (1.0f - fade);
}

/* ----------------------------------------------------------------- */
/*  Dream house arpeggio pattern                                     */
/*  Smooth, step-wise, chord-tone emphasis for dreamscape           */
/*  Follows musimathics melodic principles:                         */
/*  - Emphasize root and fifth as tonal anchors                     */
/*  - Step-wise motion preferred over leaps                        */
/*  - Chord tones (root, 3rd, 5th, 7th) frame the melody          */
/*  - Passing tones (2nd, 6th) create forward motion              */
/*  - No octave jumps except gentle shift in second half          */
/* ----------------------------------------------------------------- */
static void generate_dreamscape_pattern(int *degrees, const ScaleDef *scale) {
    int n = scale->num_degrees;

    int deg0 = 0;
    int deg1 = (n >= 2) ? 1 : 0;
    int deg2 = (n >= 3) ? 2 : (n-1);
    int deg3 = (n >= 4) ? 3 : (n-2);
    int deg4 = (n >= 5) ? 4 : (n-3);
    int deg5 = (n >= 6) ? 5 : (n-4);
    int deg6 = (n >= 7) ? 6 : (n-5);

    /* Smooth chord-tone arpeggio with passing tones.
     * First 8 steps: ascending chord arpeggio (Root→3rd→5th→7th),
     * then descending through 6th→5th→3rd→Root.
     * Second 8 steps: Root→2nd→3rd→5th→6th→5th→3rd→Root with
     * a gentle octave-up shift for variety. */

    int pattern[16] = {
        deg0,    /* Root */
        deg2,    /* 3rd */
        deg4,    /* 5th */
        deg6,    /* 7th */
        deg5,    /* 6th */
        deg4,    /* 5th (return) */
        deg2,    /* 3rd */
        deg0,    /* Root */
        deg0,    /* Root */
        deg1,    /* 2nd (passing tone) */
        deg2,    /* 3rd */
        deg4,    /* 5th */
        deg5,    /* 6th */
        deg4,    /* 5th (return) */
        deg2,    /* 3rd */
        deg0,    /* Root */
    };

    /* Gentle octave shift: second half goes up one octave
     * for melodic variety without jarring leaps */
    for (int i = 0; i < 16; i++) {
        int oct_shift = (i >= 8) ? 1 : 0;
        degrees[i] = pattern[i] + oct_shift * n;
    }
}

void synth_init(AcidSynth *s, float sr) {
    memset(s, 0, sizeof(*s));
    s->sr = sr;

    /* --- Oscillators: triangle waves for warm, dreamy character ---
     * Triangle has the right harmonic content: warm fundamental,
     * gentle odd harmonics that make the filter sing, without
     * the harsh buzz of saw waves.                           */
    osc_init(&s->osc1, OSC_WAVE_TRI, 220.0f);
    /* Detune VCO2 by +5 cents (~0.3% = 1.003 multiplier) for
     * slow beat-frequency chorus effect — the classic dream
     * house pad texture.                                      */
    osc_init(&s->osc2, OSC_WAVE_TRI, 220.0f * 1.003f);
    /* VCO3: one octave down for body */
    osc_init(&s->osc3, OSC_WAVE_TRI, 110.0f);

    /* Sub oscillator: sine wave, two octaves below root for
     * deep, round sub-bass that doesn't compete with the
     * triangle harmonics.                                      */
    osc_init(&s->osc_sub, OSC_WAVE_SINE, 55.0f);
    osc_set_amp(&s->osc_sub, 0.3f);

    /* Mix levels: balanced for warmth. VCO1+2 share the lead,
     * VCO3 adds body, sub adds depth without muddiness. */
    s->mix_osc1 = 0.35f;
    s->mix_osc2 = 0.35f;
    s->mix_osc3 = 0.25f;
    s->mix_sub  = 0.30f;

    /* Wavefolder — OFF. Dreamscape is clean, no grit. */
    wavefolder_init(&s->wavefolder, 0.0f, 0.0f);

    /* Filter: open LP at 1500Hz, gentle resonance (0.25 ≈ Q~13).
     * Open enough to be airy, closed enough to shape the sound.
     * Resonance is musical but not squelchy.               */
    filter_init(&s->filter, FILTER_LP, 1500.0f, 0.25f, sr);
    s->cutoff_base = 1500.0f;
    s->reso_base = 0.25f;

    /* LFO: slow triangle wave for gentle, musical modulation.
     * Triangle has smoother transitions than sine for filter
     * modulation — no abrupt direction changes.            */
    lfo_init(&s->lfo, LFO_WAVE_TRI, 2.5f, sr);
    s->lfo_to_filter = 80.0f;   /* ±80Hz cutoff modulation = subtle, not squelchy */
    s->lfo_rate = 2.5f;

    /* Envelope: dreamy pad envelope.
     * 200ms attack: soft, not percussive
     * 800ms decay: gradual descent to sustain
     * 0.6 sustain: stays present
     * 1.2s release: long tail, pads don't cut off abruptly */
    s->env_attack  = 0.2f;
    s->env_decay   = 0.8f;
    s->env_sustain = 0.6f;
    s->env_release = 1.2f;
    s->envelope    = 0.0f;
    s->gate_on     = 0;

    /* Delay: 500ms = quarter note at 120 BPM.
     * Feedback 0.45 gives 3-4 clear echoes, musical for house.
     * Mix 0.30: present but not overwhelming.            */
    delay_init(&s->delay, 2000.0f, sr);
    delay_set_time(&s->delay, 500.0f);
    delay_set_feedback(&s->delay, 0.45f);
    delay_set_mix(&s->delay, 0.30f);

    /* Reverb: large room (1.2x), long decay (0.75).
     * Schroeder reverb with bigger comb filters + higher feedback.
     * Mix 0.40: the dreamscape signature — big and atmospheric. */
    reverb_init(&s->reverb, 1.2f, 0.75f, sr);
    reverb_set_mix(&s->reverb, 0.40f);

    /* Sequencer: slow house arpeggio at 2Hz (= 120 BPM half-notes).
     * Not the fast 4Hz acid squelch — this is dreamy and melodic. */
    sequencer_init(&s->seq, 16, 2.0f, sr);
    s->seq_mode = 0;  /* AUTO-SEQUENCER OFF by default — silent until "Demo" pressed */

    /* --- Scale system: Dorian + 12-TET ---
     * Dorian = minor with raised 6th — the quintessential
     * house scale: melancholic but warm.
     * 12-TET because Just Intonation sounds beautiful on drones
     * but "out of tune" to ears raised on equal temperament,
     * especially for melodic house. The user can toggle to JI
     * via the Eq Temp checkbox in the UI.                    */
    s->dreamscape_mode = 1;
    s->use_equal_temp = 1;           /* 12-TET by default for house */
    s->scale_type = SCALE_DORIAN;    /* warm, dreamy house scale */
    s->scale_root_freq = 130.81f;    /* C3 */
    const ScaleDef *sd = scale_get_def(s->scale_type);
    scale_generate_freqs(sd, s->scale_root_freq, s->scale_freqs, s->use_equal_temp);

    /* Generate dreamy chord-tone arpeggio */
    generate_dreamscape_pattern(s->seq_degrees, sd);

    /* Pre-fill sequencer notes */
    for (int step = 0; step < 16; step++) {
        int deg = s->seq_degrees[step];
        float freq = synth_scale_degree_freq(s, deg);
        sequencer_set_note(&s->seq, step, freq);
    }

    s->master_volume = 0.5f;

    /* Attack layer: off by default */
    attack_init(&s->attack);
}

void synth_free(AcidSynth *s) {
    delay_free(&s->delay);
    reverb_free(&s->reverb);
    sequencer_free(&s->seq);
    attack_free(&s->attack);
}

void synth_set_scale(AcidSynth *s, ScaleType type) {
    s->scale_type = type;
    const ScaleDef *sd = scale_get_def(type);
    scale_generate_freqs(sd, s->scale_root_freq, s->scale_freqs, s->use_equal_temp);

    /* Regenerate dreamy pattern for new scale */
    generate_dreamscape_pattern(s->seq_degrees, sd);

    /* Update sequencer note frequencies */
    for (int step = 0; step < 16; step++) {
        int deg = s->seq_degrees[step];
        float freq = synth_scale_degree_freq(s, deg);
        sequencer_set_note(&s->seq, step, freq);
    }
}

void synth_set_scale_root(AcidSynth *s, float root_freq) {
    s->scale_root_freq = root_freq;
    const ScaleDef *sd = scale_get_def(s->scale_type);
    scale_generate_freqs(sd, root_freq, s->scale_freqs, s->use_equal_temp);

    /* Update sequencer note frequencies with new root */
    for (int step = 0; step < 16; step++) {
        int deg = s->seq_degrees[step];
        float freq = synth_scale_degree_freq(s, deg);
        sequencer_set_note(&s->seq, step, freq);
    }
}

float synth_scale_degree_freq(const AcidSynth *s, int degree) {
    const ScaleDef *sd = scale_get_def(s->scale_type);
    int n = sd->num_degrees;
    if (n <= 0) return s->scale_root_freq;

    /* Handle octave wrapping: degree 0..n-1 = root octave
     * degree n..2n-1 = one octave up, etc. */
    int octave = degree / n;
    int index = degree % n;
    float freq = s->scale_freqs[index];
    /* Multiply by 2 for each octave shift */
    for (int i = 0; i < octave; i++) freq *= 2.0f;
    return freq;
}

void synth_set_note(AcidSynth *s, float freq_hz) {
    /* Detune: VCO2 is slightly sharp (+0.3% for chorus effect).
     * Use osc_set_target_freq for smooth 2ms glide — eliminates
     * the filter transient from instant frequency change.
     * From Pirkle: frequency slewing prevents click through
     * resonant filters and effects chains. */
    osc_set_target_freq(&s->osc1, freq_hz, s->sr);
    osc_set_target_freq(&s->osc2, freq_hz * 1.003f, s->sr);
    osc_set_target_freq(&s->osc3, freq_hz * 0.5f, s->sr);
    osc_set_target_freq(&s->osc_sub, freq_hz * 0.25f, s->sr);
}

void synth_gate(AcidSynth *s, int on) {
    s->gate_on = on;
}

void synth_note_on(AcidSynth *s, float freq_hz) {
    /* Pure legato always: just change frequency and re-activate gate.
     * The envelope continues from wherever it is (sustain, decaying, or 0).
     * - If envelope = 0 (fully released): attack starts from 0 → no click
     * - If envelope = 0.15 (decaying): attack continues from 0.15 → no click
     * - If envelope = 0.6 (sustain): just stays at sustain → no click
     * Do NOT reset envelope, phase, filter, delay, or reverb.
     * Reset causes C0 discontinuity → click. */
    synth_set_note(s, freq_hz);
    s->gate_on = 1;
    /* Trigger attack sample layer if enabled */
    attack_note_on(&s->attack);
}

void synth_note_off(AcidSynth *s) {
    s->gate_on = 0;
}

void synth_set_seq_rate(AcidSynth *s, float rate_hz) {
    sequencer_set_rate(&s->seq, rate_hz);
}

/* ----------------------------------------------------------------- */
/*  Toggle dreamscape mode                                           */
/* ----------------------------------------------------------------- */
void synth_set_dreamscape(AcidSynth *s, int on) {
    s->dreamscape_mode = on;
    if (on) {
        /* Switch to triangle waves with detune */
        float current_freq = s->osc1.freq;
        osc_init(&s->osc1, OSC_WAVE_TRI, current_freq);
        osc_init(&s->osc2, OSC_WAVE_TRI, current_freq * 1.003f);
        osc_init(&s->osc3, OSC_WAVE_TRI, current_freq * 0.5f);
        osc_init(&s->osc_sub, OSC_WAVE_SINE, current_freq * 0.25f);
        osc_set_amp(&s->osc_sub, 0.3f);

        s->mix_osc1 = 0.35f;
        s->mix_osc2 = 0.35f;
        s->mix_osc3 = 0.25f;
        s->mix_sub  = 0.30f;

        wavefolder_set_drive(&s->wavefolder, 0.0f);

        s->cutoff_base = 1500.0f;
        s->reso_base   = 0.25f;
        filter_set_freq(&s->filter, 1500.0f);
        filter_set_resonance(&s->filter, 0.25f);

        lfo_set_wave(&s->lfo, LFO_WAVE_TRI);
        s->lfo_to_filter = 80.0f;
        s->lfo_rate = 2.5f;
        lfo_set_rate(&s->lfo, 2.5f);

        s->env_attack  = 0.2f;
        s->env_decay   = 0.8f;
        s->env_sustain = 0.6f;
        s->env_release = 1.2f;

        delay_set_time(&s->delay, 500.0f);
        delay_set_feedback(&s->delay, 0.45f);
        delay_set_mix(&s->delay, 0.30f);

        reverb_init(&s->reverb, 1.2f, 0.75f, s->sr);
        reverb_set_mix(&s->reverb, 0.40f);

        synth_set_seq_rate(s, 2.0f);
    } else {
        /* Switch back to acid defaults (saw waves, faster envelope) */
        float current_freq = s->osc1.freq;
        osc_init(&s->osc1, OSC_WAVE_SAW, current_freq);
        osc_init(&s->osc2, OSC_WAVE_SAW, current_freq + 0.3f);
        osc_init(&s->osc3, OSC_WAVE_SAW, current_freq * 0.5f);
        osc_init(&s->osc_sub, OSC_WAVE_SINE, current_freq * 0.25f);
        osc_set_amp(&s->osc_sub, 0.4f);

        s->mix_osc1 = 0.30f;
        s->mix_osc2 = 0.30f;
        s->mix_osc3 = 0.25f;
        s->mix_sub  = 0.30f;

        wavefolder_set_drive(&s->wavefolder, 1.0f);

        s->cutoff_base = 800.0f;
        s->reso_base   = 0.4f;
        filter_set_freq(&s->filter, 800.0f);
        filter_set_resonance(&s->filter, 0.4f);

        lfo_set_wave(&s->lfo, LFO_WAVE_SINE);
        s->lfo_to_filter = 300.0f;
        s->lfo_rate = 4.0f;
        lfo_set_rate(&s->lfo, 4.0f);

        s->env_attack  = 0.005f;
        s->env_decay   = 0.2f;
        s->env_sustain = 0.5f;
        s->env_release = 0.05f;

        delay_set_time(&s->delay, 300.0f);
        delay_set_feedback(&s->delay, 0.25f);
        delay_set_mix(&s->delay, 0.2f);

        reverb_init(&s->reverb, 0.6f, 0.3f, s->sr);
        reverb_set_mix(&s->reverb, 0.12f);

        synth_set_seq_rate(s, 4.0f);
    }
}

/* ----------------------------------------------------------------- */
/*  Toggle equal temperament vs Just Intonation                      */
/* ----------------------------------------------------------------- */
void synth_set_equal_temp(AcidSynth *s, int on) {
    s->use_equal_temp = on;
    const ScaleDef *sd = scale_get_def(s->scale_type);
    scale_generate_freqs(sd, s->scale_root_freq, s->scale_freqs, on);

    /* Update sequencer frequencies */
    for (int step = 0; step < 16; step++) {
        int deg = s->seq_degrees[step];
        float freq = synth_scale_degree_freq(s, deg);
        sequencer_set_note(&s->seq, step, freq);
    }
}

/* ----------------------------------------------------------------- */
/*  Envelope update                                                  */
/* ----------------------------------------------------------------- */
static float update_envelope(AcidSynth *s, float dt) {
    if (s->gate_on) {
        if (s->envelope < 1.0f) {
            s->envelope += dt / s->env_attack;
            if (s->envelope > 1.0f) s->envelope = 1.0f;
        } else if (s->envelope > s->env_sustain) {
            s->envelope -= dt / s->env_decay;
            if (s->envelope < s->env_sustain) s->envelope = s->env_sustain;
        }
    } else {
        if (s->envelope > 0.0f) {
            s->envelope -= dt / s->env_release;
            if (s->envelope < 0.0f) s->envelope = 0.0f;
        }
    }
    return s->envelope;
}

void synth_process(AcidSynth *s, float *outL, float *outR) {
    float sr = s->sr;
    float dt = 1.0f / sr;

    /* --- Sequencer (auto-trigger) ---
     * Resetting filter + delay on new note prevents click from
     * stale filter ringing or old delay buffer against new waveform. */
    if (sequencer_tick(&s->seq)) {
        if (s->seq_mode) {
            int step = sequencer_get_step(&s->seq);
            int degree = s->seq_degrees[step];
            float note_freq = synth_scale_degree_freq(s, degree);

            /* Pure legato: just change frequency.
             * Do NOT reset envelope, oscillator phase, or filter.
             * - Envelope stays at sustain → no C0 discontinuity → no click
             * - Oscillator phase continues → no phase discontinuity → no click
             * - Filter state continues → no transient → no click
             * The new frequency takes over naturally from the current
             * oscillator position. For a dreamy pad this is correct:
             * notes blend smoothly with no stepped volume recovery. */
            synth_set_note(s, note_freq);
            s->gate_on = 1;
        }
    }

    /* --- Envelope --- */
    float env = update_envelope(s, dt);
    if (env <= 0.0f && !s->gate_on) {
        s->gate_on = 0;
        /* When note fully released, clear filter, delay and reverb to prevent
         * stale ringing interfering with next note_on */
        filter_reset(&s->filter);
        delay_reset(&s->delay);
        reverb_reset(&s->reverb);
    }

    /* --- LFO --- */
    float lfo_val = lfo_process(&s->lfo);

    /* --- Oscillators --- */
    float osc1 = osc_process(&s->osc1, sr);
    float osc2 = osc_process(&s->osc2, sr);
    float osc3 = osc_process(&s->osc3, sr);
    float sub  = osc_process(&s->osc_sub, sr);

    /* --- Mix --- */
    float mix = osc1 * s->mix_osc1
              + osc2 * s->mix_osc2
              + osc3 * s->mix_osc3
              + sub  * s->mix_sub;

    /* --- Envelope --- */
    mix *= env;

    /* --- Wavefolder (bypassed at drive=0) --- */
    float folded = wavefolder_process(&s->wavefolder, mix);

    /* --- Filter (cutoff modulated by LFO, base from slider) --- */
    float cutoff = s->cutoff_base + lfo_val * s->lfo_to_filter;
    if (cutoff < 50.0f) cutoff = 50.0f;
    if (cutoff > sr * 0.45f) cutoff = sr * 0.45f;
    filter_set_freq(&s->filter, cutoff);
    filter_set_resonance(&s->filter, s->reso_base);
    float filtered = filter_process(&s->filter, folded);

    /* Soft-clip post-filtro: cattura picchi di risonanza prima che
     * entrino in delay/reverb. Il filtro con alta risonanza può produrre
     * overshoot > ±1.0 che, amplificato dal wavefolder + env, crea
     * intermodulazione sibilante. */
    if (filtered > 1.0f) filtered = 1.0f;
    else if (filtered < -1.0f) filtered = -1.0f;

    /* --- Delay --- */
    float delayed = delay_process(&s->delay, filtered);

    /* --- Reverb --- */
    float wet = reverb_process(&s->reverb, delayed);

    /* --- Attack layer (if enabled, crossfades with synth body) --- */
    float attack = attack_process(&s->attack, wet, s->osc1.target_freq, sr);

    /* --- Master volume + soft-clip --- */
    float out = attack * s->master_volume;
    out = soft_clip(out);

    *outL = out;
    *outR = out;
}

void synth_process_block(AcidSynth *s, float *outL, float *outR, int n) {
    for (int i = 0; i < n; i++) {
        synth_process(s, &outL[i], &outR[i]);
    }
}
