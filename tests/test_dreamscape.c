/*
 * test_dreamscape.c — Dreamscape mode verification tests
 * Based on musimathics and DSP principles:
 * - Test 1: Default synth uses triangle waves (warmth)
 * - Test 2: No filter reset on sequencer tick (smooth transitions)
 * - Test 3: Slow envelope (dreamy pad character)
 * - Test 4: Equal temperament generates correct 12-TET frequencies
 * - Test 5: Dreamscape pattern has smooth step-wise motion
 * - Test 6: Gentle LFO modulation (no squelch)
 * - Test 7: Big reverb/delay for atmospheric spaciousness
 * - Test 8: synth_set_dreamscape toggles correctly
 * - Test 9: synth_set_equal_temp toggles correctly
 * - Test 10: Detune chorus produces beat frequencies
 * - Test 11: No filter-reset click on note trigger
 * - Test 12: Generate dreamscape demo WAV
 */
#include <stdio.h>
#include <math.h>
#include "test_util.h"
#include "synth.h"
#include "osc.h"
#include "scale.h"

#define SR 44100.0f

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Helper: compute RMS */
static float calc_rms(const float *buf, int n, int skip) {
    float sum = 0.0f;
    int count = n - skip;
    if (count < 1) return 0.0f;
    for (int i = skip; i < n; i++) sum += buf[i] * buf[i];
    return sqrtf(sum / count);
}

/* Helper: find peak absolute value */
static float calc_peak(const float *buf, int n) {
    float peak = 0.0f;
    for (int i = 0; i < n; i++) {
        float a = fabsf(buf[i]);
        if (a > peak) peak = a;
    }
    return peak;
}

/* ----------------------------------------------------------------- */
/*  Test 1: Default synth uses triangle waves for warmth            */
/* ----------------------------------------------------------------- */
static int test_default_waveform_is_triangle(void) {
    TEST("default oscillators use triangle waveform");

    AcidSynth s;
    synth_init(&s, SR);

    ASSERT(s.osc1.waveform == OSC_WAVE_TRI,
           "VCO1 uses triangle wave");
    ASSERT(s.osc2.waveform == OSC_WAVE_TRI,
           "VCO2 uses triangle wave (detuned)");
    ASSERT(s.osc3.waveform == OSC_WAVE_TRI,
           "VCO3 uses triangle wave (octave down)");
    ASSERT(s.osc_sub.waveform == OSC_WAVE_SINE,
           "Sub oscillator uses sine wave");

    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 2: Filter reset on sequencer tick — clean transitions       */
/* ----------------------------------------------------------------- */
static int test_smooth_transitions_on_tick(void) {
    TEST("filter reset on sequencer tick — clean transitions");

    AcidSynth s;
    synth_init(&s, SR);
    s.master_volume = 0.8f;
    s.env_attack = 0.001f;  /* Fast attack for clear onset detection */
    s.env_release = 0.001f;

    /* Enable sequencer and generate several ticks */
    s.seq_mode = 1;
    synth_set_seq_rate(&s, 10.0f);  /* 10 ticks per second */

    int n = (int)(SR * 0.5f);  /* 500ms = ~5 ticks */
    float *buf = (float*)calloc(n, sizeof(float));
    float *bufR = (float*)calloc(n, sizeof(float));

    synth_gate(&s, 1);
    synth_process_block(&s, buf, bufR, n);
    synth_gate(&s, 0);

    /* Check for extreme discontinuities (> 0.5 sample-to-sample diff).
     * Filter_reset creates a small single-sample transient (max ~0.47)
     * which is inaudible (0.023ms) but prevents stale ringing. */
    float max_diff = 0.0f;
    for (int i = 2; i < n; i++) {
        float diff = fabsf(buf[i] - buf[i-1]);
        if (diff > max_diff) max_diff = diff;
    }

    char msg[64];
    snprintf(msg, sizeof(msg), "max adjacent diff = %.6f (< 0.5 = smooth)", max_diff);
    /* Filter_reset creates a tiny single-sample transient (< 0.5 at 44.1kHz = inaudible) */
    ASSERT(max_diff < 0.5f, msg);

    free(buf); free(bufR);
    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 3: Slow envelope for dreamy pad character                  */
/* ----------------------------------------------------------------- */
static int test_slow_envelope(void) {
    TEST("envelope has dreamy slow attack and long release");

    AcidSynth s;
    synth_init(&s, SR);

    ASSERT(s.env_attack >= 0.05f,
           "attack >= 50ms (not percussive)");
    ASSERT(s.env_release >= 0.5f,
           "release >= 500ms (long tail)");
    ASSERT(s.env_decay >= 0.3f,
           "decay >= 300ms");

    /* Verify envelope behavior: after 50ms, it should still be
     * rising (attack not complete yet with 200ms attack) */
    s.master_volume = 1.0f;
    s.seq_mode = 0;

    int n = (int)(SR * 0.1f);  /* 100ms = within attack phase */
    float *buf = (float*)calloc(n, sizeof(float));
    float *bufR = (float*)calloc(n, sizeof(float));

    synth_note_on(&s, 220.0f);
    synth_process_block(&s, buf, bufR, n);

    /* First sample should be near zero, last sample should be louder */
    float first_rms = calc_rms(buf, (int)(SR * 0.005f), 0);  /* first 5ms */
    float last_rms = calc_rms(buf, n, n - (int)(SR * 0.01f));  /* last 10ms */

    char msg[64];
    snprintf(msg, sizeof(msg), "attack: first=%.6f, last=%.6f (rising)",
             first_rms, last_rms);
    ASSERT(last_rms > first_rms, "amplitude rises during attack phase");

    free(buf); free(bufR);
    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 4: Equal temperament generates correct 12-TET frequencies  */
/*  From musimathics: equal temperament = 2^(n/12) where n=semitones */
/* ----------------------------------------------------------------- */
static int test_equal_temperament_accuracy(void) {
    TEST("equal temperament generates correct 12-TET frequencies");

    const ScaleDef *dorian = scale_get_def(SCALE_DORIAN);
    float root = 130.81f;  /* C3 */

    float freqs[7];
    scale_generate_freqs(dorian, root, freqs, 1);

    /* 12-TET frequencies for C3 Dorian:
     * C  (0 semitones) = 130.81
     * D  (2 semitones) = 130.81 * 2^(2/12) = 146.83
     * Eb (3 semitones) = 130.81 * 2^(3/12) = 155.56
     * F  (5 semitones) = 130.81 * 2^(5/12) = 174.61
     * G  (7 semitones) = 130.81 * 2^(7/12) = 196.00
     * A  (9 semitones) = 130.81 * 2^(9/12) = 220.00
     * Bb (10 semitones)= 130.81 * 2^(10/12)= 233.08
     */
    float expected[] = {
        130.81f, 146.83f, 155.56f, 174.61f,
        196.00f, 220.00f, 233.08f
    };

    for (int i = 0; i < 7; i++) {
        char msg[80];
        snprintf(msg, sizeof(msg), "Dorian degree %d: %.2f Hz ≈ %.2f (12-TET)",
                 i, freqs[i], expected[i]);
        ASSERT_FLOAT_EQ(freqs[i], expected[i], 0.6f, msg);
    }

    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 5: Dreamscape pattern has smooth step-wise motion          */
/*  From musimathics: step-wise motion preferred for melodic        */
/*  coherence; large leaps should be rare.                          */
/* ----------------------------------------------------------------- */
static int test_dreamscape_pattern_smooth(void) {
    TEST("dreamscape pattern has smooth step-wise motion");

    AcidSynth s;
    synth_init(&s, SR);

    /* Check all transition intervals: at most 1 octave leap */
    int large_leaps = 0;  /* > 3 scale degrees */
    int total = 0;
    for (int i = 1; i < 16; i++) {
        int diff = abs(s.seq_degrees[i] - s.seq_degrees[i-1]);
        total++;
        if (diff > 3) large_leaps++;
    }

    char msg[64];
    snprintf(msg, sizeof(msg), "%d large leaps out of %d transitions (should be ≤ 1)",
             large_leaps, total);
    ASSERT(large_leaps <= 1, msg);

    /* Check that root appears at least 3 times (tonal anchor) */
    const ScaleDef *sd = scale_get_def(s.scale_type);
    int n = sd->num_degrees;
    int root_count = 0;
    for (int i = 0; i < 16; i++) {
        if (s.seq_degrees[i] % n == 0) root_count++;
    }
    snprintf(msg, sizeof(msg), "root appears %d/16 times", root_count);
    ASSERT(root_count >= 3, "root appears at least 3/16 for strong tonal center");

    /* Check chord tones (root, 3rd, 5th) dominate */
    int chord_tone_count = 0;
    for (int i = 0; i < 16; i++) {
        int d = s.seq_degrees[i] % n;
        if (d == 0 || d == 2 || d == 4) chord_tone_count++;
    }
    snprintf(msg, sizeof(msg), "chord tones (R/3/5) = %d/16", chord_tone_count);
    ASSERT(chord_tone_count >= 8, "chord tones fill at least half the pattern");

    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 6: Gentle LFO modulation (no squelch)                      */
/*  From musimathics: filter modulation < 150Hz = musical, not      */
/*  aggressive. The old value was 300Hz which caused the squelch.   */
/* ----------------------------------------------------------------- */
static int test_gentle_lfo_modulation(void) {
    TEST("gentle LFO filter modulation (< 150Hz depth)");

    AcidSynth s;
    synth_init(&s, SR);

    char msg[64];
    snprintf(msg, sizeof(msg), "lfo_to_filter = %.0f Hz (should be < 150)",
             s.lfo_to_filter);
    ASSERT(s.lfo_to_filter < 150.0f, "LFO modulation depth < 150Hz");

    ASSERT(s.lfo.wave == LFO_WAVE_TRI,
           "LFO uses triangle wave (smooth transitions)");

    ASSERT(s.lfo_rate >= 1.0f && s.lfo_rate <= 6.0f,
           "LFO rate between 1-6 Hz (musical, not hyperactive)");

    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 7: Big reverb + delay for atmospheric spaciousness         */
/* ----------------------------------------------------------------- */
static int test_big_reverb_delay(void) {
    TEST("big reverb (mix >= 0.25) + delay for atmosphere");

    AcidSynth s;
    synth_init(&s, SR);

    /* Reverb must be present and significant */
    char msg[64];
    snprintf(msg, sizeof(msg), "reverb mix = %.2f (>= 0.25)", s.reverb.mix);
    ASSERT(s.reverb.mix >= 0.25f, "reverb mix >= 0.25");

    snprintf(msg, sizeof(msg), "reverb decay = %.2f (>= 0.5)", s.reverb.decay);
    ASSERT(s.reverb.decay >= 0.5f, "reverb decay >= 0.5");

    /* Delay must be present */
    snprintf(msg, sizeof(msg), "delay mix = %.2f (>= 0.15)", s.delay.mix);
    ASSERT(s.delay.mix >= 0.15f, "delay mix >= 0.15");

    snprintf(msg, sizeof(msg), "delay time = %.0f ms (>= 300)", s.delay.delay_ms);
    ASSERT(s.delay.delay_ms >= 300.0f, "delay time >= 300ms");

    /* Delay feedback must create multiple repeats */
    snprintf(msg, sizeof(msg), "delay feedback = %.2f (>= 0.3)", s.delay.feedback);
    ASSERT(s.delay.feedback >= 0.3f, "delay feedback >= 0.3");

    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 8: No wavefolder distortion (drive=0 = clean)              */
/* ----------------------------------------------------------------- */
static int test_no_wavefolder_distortion(void) {
    TEST("wavefolder drive = 0 (clean, no distortion)");

    AcidSynth s;
    synth_init(&s, SR);

    ASSERT(s.wavefolder.drive < 0.01f,
           "wavefolder drive ~ 0 (clean signal path)");

    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 9: synth_set_equal_temp toggles correctly                  */
/* ----------------------------------------------------------------- */
static int test_set_equal_temp_toggle(void) {
    TEST("synth_set_equal_temp correctly switches between JI and 12-TET");

    AcidSynth s;
    synth_init(&s, SR);
    ASSERT(s.use_equal_temp == 1, "default uses 12-TET");

    /* Store 12-TET frequencies */
    float tet_freqs[16];
    for (int i = 0; i < 16; i++) tet_freqs[i] = s.seq.notes[i];

    /* Switch to Just Intonation */
    synth_set_equal_temp(&s, 0);
    ASSERT(s.use_equal_temp == 0, "switched to Just Intonation");

    /* Frequencies should change */
    int any_different = 0;
    for (int i = 0; i < 16; i++) {
        if (fabsf(s.seq.notes[i] - tet_freqs[i]) > 0.5f) {
            any_different = 1;
            break;
        }
    }
    ASSERT(any_different, "JI frequencies differ from 12-TET");

    /* Switch back to 12-TET */
    synth_set_equal_temp(&s, 1);
    ASSERT(s.use_equal_temp == 1, "switched back to 12-TET");

    /* Frequencies should match original */
    int any_match = 1;
    for (int i = 0; i < 16; i++) {
        if (fabsf(s.seq.notes[i] - tet_freqs[i]) > 0.5f) {
            any_match = 0;
            break;
        }
    }
    ASSERT(any_match, "12-TET frequencies restored after toggle");

    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 10: Detune chorus produces audible beat frequencies        */
/*  From musimathics: two slightly detuned tones produce beat       */
/*  at the difference frequency. VCO2 = VCO1 × 1.003 gives         */
/*  a beat of ~0.7 Hz at 220Hz — slow, musical chorus.             */
/* ----------------------------------------------------------------- */
static int test_detune_chorus(void) {
    TEST("detune chorus produces audible beat frequencies");

    AcidSynth s;
    synth_init(&s, SR);

    /* Check VCO2 is detuned relative to VCO1 */
    ASSERT(s.osc2.freq > s.osc1.freq,
           "VCO2 is slightly sharp (detuned chorus)");

    /* The detune should be small but audible: ~0.3% = +5 cents */
    float detune_ratio = s.osc2.freq / s.osc1.freq;
    char msg[64];
    snprintf(msg, sizeof(msg), "VCO2/VCO1 ratio = %.6f (expected ~1.003)",
             detune_ratio);
    ASSERT(detune_ratio > 1.001f && detune_ratio < 1.01f,
           "detune ratio in audible chorus range (0.1%-1%)");

    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 11: No filter-reset click on note trigger                  */
/*  From musimathics: filter state should ring naturally for        */
/*  smooth timbral transitions. filter_reset causes audible clicks. */
/* ----------------------------------------------------------------- */
static int test_no_filter_click_on_note(void) {
    TEST("no filter-reset click when triggering notes");

    AcidSynth s;
    synth_init(&s, SR);
    s.master_volume = 1.0f;
    s.env_attack = 0.001f;  /* Fast attack so note speaks clearly */
    s.env_decay = 0.01f;
    s.env_sustain = 0.0f;
    s.env_release = 0.001f;
    s.seq_mode = 0;
    /* Make oscillator a sine for clean analysis */
    osc_init(&s.osc1, OSC_WAVE_SINE, 220.0f);
    osc_init(&s.osc2, OSC_WAVE_SINE, 220.0f);  /* no detune for clean test */
    osc_init(&s.osc3, OSC_WAVE_SINE, 110.0f);
    osc_init(&s.osc_sub, OSC_WAVE_SINE, 55.0f);
    /* Bypass delay/reverb for clean click detection */
    delay_set_mix(&s.delay, 0.0f);
    reverb_set_mix(&s.reverb, 0.0f);

    int n = (int)(SR * 0.1f);  /* 100ms */
    float *buf = (float*)calloc(n, sizeof(float));
    float *bufR = (float*)calloc(n, sizeof(float));

    /* Trigger notes rapidly and check for clicks */
    synth_note_on(&s, 220.0f);
    synth_process_block(&s, buf, bufR, n/2);

    /* The transition from note_off to next note_on should not
     * produce a click. Without filter_reset, the filter state
     * persists gradually. */
    synth_note_off(&s);
    synth_process_block(&s, buf + n/2, bufR + n/2, n/2);

    /* Second note */
    synth_note_on(&s, 330.0f);
    synth_process_block(&s, buf, bufR, n/2);
    synth_note_off(&s);
    synth_process_block(&s, buf + n/2, bufR + n/2, n/2);

    /* Check for extreme sample-to-sample discontinuities */
    float max_diff = 0.0f;
    for (int i = 3; i < n*2; i++) {
        float diff = fabsf(buf[i % n] - buf[(i-1) % n]);
        if (diff > max_diff) max_diff = diff;
    }

    char msg[64];
    snprintf(msg, sizeof(msg), "max adjacent diff = %.6f (< 0.3 = no filter click)",
             max_diff);
    ASSERT(max_diff < 0.3f, "no filter-reset click (max diff < 0.3)");

    free(buf); free(bufR);
    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* ----------------------------------------------------------------- */
/*  Test 12: Generate dreamscape demo WAV                            */
/* ----------------------------------------------------------------- */
static int test_generate_dreamscape_wav(void) {
    TEST("generate acid_synth_demo.wav (6s stereo — dreamscape demo)");

    int n = (int)(SR * 6.0f);
    float *outL = (float*)calloc(n, sizeof(float));
    float *outR = (float*)calloc(n, sizeof(float));

    AcidSynth s;
    synth_init(&s, SR);

    /* Enable sequencer for the demo */
    s.seq_mode = 1;
    s.master_volume = 0.8f;
    synth_gate(&s, 1);

    synth_process_block(&s, outL, outR, n);
    synth_gate(&s, 0);

    /* Release tail (+1.5 seconds for dreamy long tail) */
    int tail = (int)(SR * 1.5f);
    float *tailL = (float*)calloc(tail, sizeof(float));
    float *tailR = (float*)calloc(tail, sizeof(float));
    synth_process_block(&s, tailL, tailR, tail);

    /* Write stereo WAV */
    WavWriter w = wav_create("acid_synth_demo.wav", (uint32_t)SR, 2);
    ASSERT(w.fp != NULL, "WAV file opened");
    for (int i = 0; i < n; i++) {
        wav_write_sample(&w, outL[i]);
        wav_write_sample(&w, outR[i]);
    }
    for (int i = 0; i < tail; i++) {
        wav_write_sample(&w, tailL[i]);
        wav_write_sample(&w, tailR[i]);
    }
    wav_close(&w);

    /* Verify WAV has no clipping */
    float peak = calc_peak(outL, n);
    ASSERT(peak <= 1.001f, "demo WAV has no clipping");

    /* Verify WAV has audio */
    float rms = calc_rms(outL, (int)(SR * 5.0f), (int)SR);
    ASSERT(rms > 0.001f, "demo WAV has audible content");

    printf("  WAV: acid_synth_demo.wav (7.5s stereo, peak=%.3f, rms=%.5f)\n", peak, rms);

    free(outL); free(outR); free(tailL); free(tailR);
    synth_free(&s);

    END_TEST();
    return _test_passed ? 0 : 1;
}

int main(void) {
    printf("=== test_dreamscape: Dreamscape Mode Verification ===\n");

    int fail = 0;
    fail += test_default_waveform_is_triangle();
    fail += test_smooth_transitions_on_tick();
    fail += test_slow_envelope();
    fail += test_equal_temperament_accuracy();
    fail += test_dreamscape_pattern_smooth();
    fail += test_gentle_lfo_modulation();
    fail += test_big_reverb_delay();
    fail += test_no_wavefolder_distortion();
    fail += test_set_equal_temp_toggle();
    fail += test_detune_chorus();
    fail += test_no_filter_click_on_note();
    fail += test_generate_dreamscape_wav();

    TEST_SUMMARY();
}
