/*
 * test_midi.c — Simulated MIDI input test
 * Invia note-on/note-off come farebbe un controller MIDI via
 * synth_note_on/synth_note_off e verifica frequenze, timing,
 * legato, e assenza di click/glitch.
 *
 * MIDI note → frequency: f = 440 * 2^((note-69)/12)
 */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "synth.h"
#include "test_util.h"

#define SR 44100.0f

/* MIDI note → frequency (12-TET) */
static float midi_to_freq(int note) {
    return 440.0f * powf(2.0f, (note - 69.0f) / 12.0f);
}

/* Helper: peak-to-peak of a buffer section */
static float calc_pkpk(const float *buf, int n, int skip) {
    float mn=0, mx=0;
    int start = (skip < n) ? skip : 0;
    mn = mx = buf[start];
    for(int i=start; i<n; i++) {
        if(buf[i] < mn) mn = buf[i];
        if(buf[i] > mx) mx = buf[i];
    }
    return mx - mn;
}

/* Test 1: MIDI note-on produces correct frequency */
static int test_midi_note_on_freq(void) {
    TEST("MIDI note-on (C4=60) produces ~261.63Hz");

    AcidSynth s;
    synth_init(&s, SR);
    s.master_volume = 0.5f;
    s.mix_osc1 = 1; s.mix_osc2 = s.mix_osc3 = s.mix_sub = 0;
    s.env_attack = 0.002f; s.env_release = 0.01f;
    osc_init(&s.osc1, OSC_WAVE_SINE, 220);

    int note = 60;  // C4 = 261.63Hz
    float expected = midi_to_freq(note);
    synth_note_on(&s, expected);

    int n = (int)(SR * 0.5f);
    float *buf = (float*)calloc(n, sizeof(float));
    float *r = (float*)calloc(n, sizeof(float));
    synth_process_block(&s, buf, r, n);

    // Detect frequency via zero-crossing
    int crossings = 0;
    for(int i = 100; i < n-1; i++) {
        if(buf[i] >= 0 && buf[i+1] < 0) crossings++;
    }
    float detected = (float)crossings / ((n-100) / SR);

    char msg[64];
    snprintf(msg, sizeof(msg), "C4 freq: expected=%.2f detected=%.2f", expected, detected);
    ASSERT(fabsf(detected - expected) < expected * 0.1f, msg);

    free(buf); free(r);
    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test 2: MIDI note-off silences the output */
static int test_midi_note_off_silence(void) {
    TEST("MIDI note-off silences output within release time");

    AcidSynth s;
    synth_init(&s, SR);
    s.master_volume = 0.5f;
    s.env_attack = 0.001f;
    s.env_release = 0.01f;  // 10ms release

    synth_note_on(&s, 440.0f);
    int n_on = (int)(SR * 0.1f);
    float *buf = (float*)calloc((int)(SR * 0.5f), sizeof(float));
    float *r = (float*)calloc((int)(SR * 0.5f), sizeof(float));

    // Generate 100ms with note on
    for(int i = 0; i < n_on; i++)
        synth_process(&s, &buf[i], &r[i]);

    synth_note_off(&s);

    // Generate 400ms after note off
    for(int i = n_on; i < (int)(SR * 0.5f); i++)
        synth_process(&s, &buf[i], &r[i]);

    // Check late portion (after release should be near zero)
    int late_start = (int)(SR * 0.15f);  // 150ms — well past 10ms release
    float pk = 0;
    for(int i = late_start; i < (int)(SR * 0.5f); i++) {
        float a = fabsf(buf[i]);
        if(a > pk) pk = a;
    }

    ASSERT(pk < 0.01f, "output near zero after note-off + release");

    free(buf); free(r);
    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test 3: Rapid MIDI note changes (legato — no cut-out) */
static int test_midi_rapid_legato(void) {
    TEST("rapid MIDI note changes (legato, no silence between notes)");

    AcidSynth s;
    synth_init(&s, SR);
    s.master_volume = 0.5f;
    s.env_attack = 0.001f; s.env_release = 0.001f;
    s.env_decay = 0.1f; s.env_sustain = 0.8f;

    float notes[] = {midi_to_freq(60), midi_to_freq(64), midi_to_freq(67),
                     midi_to_freq(72), midi_to_freq(76), midi_to_freq(79)};
    int n_notes = 6;
    int samples_per_note = (int)(SR * 0.1f);  // 100ms each
    int total = n_notes * samples_per_note;

    float *buf = (float*)calloc(total, sizeof(float));
    float *r = (float*)calloc(total, sizeof(float));

    for(int i = 0; i < n_notes; i++) {
        synth_note_on(&s, notes[i]);
        for(int j = 0; j < samples_per_note; j++) {
            int idx = i * samples_per_note + j;
            synth_process(&s, &buf[idx], &r[idx]);
        }
    }

    // Check: no silent gaps between notes
    // Contiamo solo gaps di >=5 sample consecutivi sotto soglia
    // (per escludere i naturali zero-crossing del seno)
    int gaps = 0, silent_run = 0;
    for(int i = (int)(SR * 0.02f); i < total; i++) {
        if(fabsf(buf[i]) < 0.0005f) {
            silent_run++;
        } else {
            if(silent_run >= 5) gaps++;
            silent_run = 0;
        }
    }

    char msg[64];
    snprintf(msg, sizeof(msg), "gaps=%d (should be <<10 over 600ms rapid fire)", gaps);
    ASSERT(gaps < 10, msg);

    free(buf); free(r);
    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test 4: MIDI pitch bend simulation (frequency glide) */
static int test_midi_pitch_glide(void) {
    TEST("frequency glide on pitch change (no click)");

    AcidSynth s;
    synth_init(&s, SR);
    s.master_volume = 0.5f;
    s.mix_osc1 = 1; s.mix_osc2 = s.mix_osc3 = s.mix_sub = 0;
    s.env_attack = 0.001f; s.env_release = 0.01f;
    osc_init(&s.osc1, OSC_WAVE_SINE, 220);

    int n = (int)(SR * 0.5f);
    float *buf = (float*)calloc(n, sizeof(float));
    float *r = (float*)calloc(n, sizeof(float));

    synth_note_on(&s, 220.0f);

    // Generate 100ms at 220Hz, then change to 440Hz
    int change_point = (int)(SR * 0.1f);
    for(int i = 0; i < change_point; i++)
        synth_process(&s, &buf[i], &r[i]);

    synth_set_note(&s, 440.0f);  // like pitch bend or new note in legato

    for(int i = change_point; i < n; i++)
        synth_process(&s, &buf[i], &r[i]);

    // Verify no discontinuity at change point
    float pre = fabsf(buf[change_point - 1]);
    float post = fabsf(buf[change_point]);
    float diff = fabsf(pre - post);

    char msg[64];
    snprintf(msg, sizeof(msg), "pre=%.4f post=%.4f diff=%.4f (should be smooth)", pre, post, diff);
    ASSERT(diff < pre * 0.5f, msg);

    free(buf); free(r);
    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test 5: MIDI through all 10 octaves */
static int test_midi_all_octaves(void) {
    TEST("MIDI notes across 10 octaves produce audio");

    AcidSynth s;
    synth_init(&s, SR);
    s.master_volume = 0.5f;
    s.env_attack = 0.002f; s.env_release = 0.005f;

    int test_notes[] = {24, 36, 48, 60, 72, 84, 96, 108};  // C1 to C8
    int n_tests = 8;
    int ok = 0;

    for(int t = 0; t < n_tests; t++) {
        float freq = midi_to_freq(test_notes[t]);
        synth_note_on(&s, freq);

        float buf[4410]; // 100ms
        float r[4410];
        memset(buf, 0, sizeof(buf));
        for(int i = 0; i < 1000; i++)
            synth_process(&s, &buf[i], &r[i]);

        float peak = 0;
        for(int i = 50; i < 1000; i++) {
            float a = fabsf(buf[i]);
            if(a > peak) peak = a;
        }
        if(peak > 0.001f) ok++;

        synth_note_off(&s);
        // Drain envelope
        for(int i = 0; i < 1000; i++)
            synth_process(&s, &buf[i], &r[i]);
    }

    char msg[64];
    snprintf(msg, sizeof(msg), "%d/%d octaves produced audio", ok, n_tests);
    ASSERT(ok == n_tests, msg);

    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test 6: MIDI note velocity simulation (via master_volume proxy) */
static int test_midi_velocity(void) {
    TEST("velocity (note-on with different amps) changes output level");

    AcidSynth s;
    synth_init(&s, SR);
    s.env_attack = 0.001f; s.env_release = 0.01f;

    int n = (int)(SR * 0.2f);
    float *buf1 = (float*)calloc(n, sizeof(float));
    float *buf2 = (float*)calloc(n, sizeof(float));
    float *r = (float*)calloc(n, sizeof(float));

    // "Velocity 127" — full volume
    synth_note_on(&s, 440.0f);
    synth_process_block(&s, buf1, r, n);

    synth_free(&s); synth_init(&s, SR);
    s.env_attack = 0.001f; s.env_release = 0.01f;

    // "Velocity 64" — half volume (simulated by lowering master_volume)
    s.master_volume = 0.25f;
    synth_note_on(&s, 440.0f);
    synth_process_block(&s, buf2, r, n);

    float rms1 = 0, rms2 = 0;
    for(int i = (int)(0.02f*SR); i < n; i++) {
        rms1 += buf1[i]*buf1[i];
        rms2 += buf2[i]*buf2[i];
    }
    rms1 = sqrtf(rms1 / (n-(int)(0.02f*SR)));
    rms2 = sqrtf(rms2 / (n-(int)(0.02f*SR)));

    char msg[64];
    snprintf(msg, sizeof(msg), "full=%.4f half=%.4f ratio=%.2f", rms1, rms2, rms1/(rms2+0.001f));
    ASSERT(rms1 > rms2 * 1.5f, "higher velocity = louder output");

    free(buf1); free(buf2); free(r);
    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

/* Test 7: Simulated sustain pedal (hold notes on release) */
static int test_midi_sustain(void) {
    TEST("sustain pedal holds note after note-off");

    AcidSynth s;
    synth_init(&s, SR);
    s.master_volume = 0.5f;
    s.env_attack = 0.002f;
    s.env_release = 0.5f;  // long release: sustain pedal = keep envelope

    int n = (int)(SR * 0.3f);
    float *buf = (float*)calloc(n, sizeof(float));
    float *r = (float*)calloc(n, sizeof(float));

    // Note on
    synth_note_on(&s, 440.0f);

    int mid = (int)(SR * 0.1f);
    for(int i = 0; i < mid; i++)
        synth_process(&s, &buf[i], &r[i]);

    // Note off BUT with sustain (envelope release is long enough)
    synth_note_off(&s);

    for(int i = mid; i < n; i++)
        synth_process(&s, &buf[i], &r[i]);

    // At 300ms, with release=500ms, envelope should still be > 0
    float late_peak = 0;
    for(int i = (int)(SR * 0.25f); i < n; i++) {
        float a = fabsf(buf[i]);
        if(a > late_peak) late_peak = a;
    }

    char msg[64];
    snprintf(msg, sizeof(msg), "late_peak=%.6f at 250ms (release=500ms)", late_peak);
    ASSERT(late_peak > 0.001f, "sustain holds note audible after 250ms");

    free(buf); free(r);
    synth_free(&s);
    END_TEST();
    return _test_passed ? 0 : 1;
}

int main(void) {
    printf("=== test_midi: MIDI Input Simulation ===\n");
    int fail = 0;
    fail += test_midi_note_on_freq();
    fail += test_midi_note_off_silence();
    fail += test_midi_rapid_legato();
    fail += test_midi_pitch_glide();
    fail += test_midi_all_octaves();
    fail += test_midi_velocity();
    fail += test_midi_sustain();
    TEST_SUMMARY();
}
