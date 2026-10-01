#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <stdint.h>

/*
 * test_util.h — Minimal C test framework for audio DSP modules.
 * No external dependencies. Writes WAV files, runs assertions.
 */

#ifndef TEST_UTIL_H
#define TEST_UTIL_H

/* ----------------------------------------------------------------- */
/*  Assertion macros                                                  */
/* ----------------------------------------------------------------- */
static int _test_passed = 1;
static int _test_count = 0;
static int _fail_count = 0;

#define TEST(name) do { \
    _test_count++; \
    printf("  TEST %d: %s ... ", _test_count, name); \
    _test_passed = 1; \
} while(0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("FAIL [%s]\n", msg); \
        _test_passed = 0; \
        _fail_count++; \
    } \
} while(0)

#define ASSERT_FLOAT_EQ(a, b, tol, msg) do { \
    float _diff = fabsf((float)(a) - (float)(b)); \
    if (_diff > (tol)) { \
        printf("FAIL [%s: %.6f != %.6f (diff=%.6f > %.6f)]\n", msg, (float)(a), (float)(b), _diff, (float)(tol)); \
        _test_passed = 0; \
        _fail_count++; \
    } \
} while(0)

#define END_TEST() do { \
    if (_test_passed) printf("OK\n"); \
} while(0)

#define TEST_SUMMARY() do { \
    printf("\n=== RESULTS: %d tests, %d failed ===\n", _test_count, _fail_count); \
    return _fail_count > 0 ? 1 : 0; \
} while(0)

/* ----------------------------------------------------------------- */
/*  WAV file writer                                                   */
/* ----------------------------------------------------------------- */
typedef struct {
    FILE *fp;
    uint32_t sample_rate;
    uint32_t num_channels;
    uint32_t num_samples;
} WavWriter;

static WavWriter wav_create(const char *path, uint32_t sr, uint32_t ch) {
    WavWriter w = {0};
    w.fp = fopen(path, "wb");
    if (!w.fp) { perror("fopen"); return w; }
    w.sample_rate = sr;
    w.num_channels = ch;
    w.num_samples = 0;

    // Write placeholder header (44 bytes)
    uint16_t bps = 16; // bits per sample
    uint32_t data_size = 0;
    uint16_t fmt = 1;  // PCM

    fwrite("RIFF", 1, 4, w.fp);
    fwrite(&(uint32_t){36}, 4, 1, w.fp); // file size - 8 (placeholder)
    fwrite("WAVE", 1, 4, w.fp);
    fwrite("fmt ", 1, 4, w.fp);
    fwrite(&(uint32_t){16}, 4, 1, w.fp); // chunk size
    fwrite(&fmt, 2, 1, w.fp);
    fwrite(&ch, 2, 1, w.fp);
    fwrite(&sr, 4, 1, w.fp);
    uint32_t byte_rate = sr * ch * bps / 8;
    fwrite(&byte_rate, 4, 1, w.fp);
    uint16_t block_align = ch * bps / 8;
    fwrite(&block_align, 2, 1, w.fp);
    fwrite(&bps, 2, 1, w.fp);
    fwrite("data", 1, 4, w.fp);
    fwrite(&data_size, 4, 1, w.fp); // data size (placeholder)

    return w;
}

static void wav_write_sample(WavWriter *w, float sample) {
    // Clip to [-1, 1]
    if (sample > 1.0f) sample = 1.0f;
    if (sample < -1.0f) sample = -1.0f;
    int16_t s = (int16_t)(sample * 32767.0f);
    fwrite(&s, 2, 1, w->fp);
    w->num_samples++;
}

static void wav_close(WavWriter *w) {
    if (!w->fp) return;
    // Fix header sizes
    uint32_t data_size = w->num_samples * 2; // 16-bit
    uint32_t file_size = 36 + data_size;

    fseek(w->fp, 4, SEEK_SET);
    fwrite(&file_size, 4, 1, w->fp);
    fseek(w->fp, 40, SEEK_SET); // data chunk size
    fwrite(&data_size, 4, 1, w->fp);
    fclose(w->fp);
    w->fp = NULL;
}

/* ----------------------------------------------------------------- */
/*  Simple FFT magnitude (for frequency detection)                    */
/* ----------------------------------------------------------------- */
// Compute magnitude spectrum via naive DFT (small N only)
static void compute_spectrum(const float *samples, int n, float *mag, int sr) {
    for (int k = 0; k < n/2; k++) {
        double re = 0, im = 0;
        for (int i = 0; i < n; i++) {
            double angle = 2.0 * M_PI * k * i / n;
            re += samples[i] * cos(angle);
            im -= samples[i] * sin(angle);
        }
        mag[k] = (float)(sqrt(re*re + im*im) / n);
    }
}

// Find dominant frequency from magnitude spectrum
static float detect_frequency(const float *samples, int n, int sr) {
    float *mag = (float*)calloc(n/2, sizeof(float));
    compute_spectrum(samples, n, mag, sr);

    int peak_bin = 0;
    float peak_mag = 0;
    for (int k = 1; k < n/2; k++) {
        if (mag[k] > peak_mag) {
            peak_mag = mag[k];
            peak_bin = k;
        }
    }
    float freq = (float)peak_bin * sr / n;
    free(mag);
    return freq;
}

/* ----------------------------------------------------------------- */
/*  Signal generation helper (samples a signal generator callback)    */
/* ----------------------------------------------------------------- */
typedef void (*SignalGenFn)(void *state, float *out, int n, float sr);
typedef float (*SampleFn)(void *state, float sr);

static void generate_samples(void *state, SampleFn fn, float *out, int n, float sr) {
    for (int i = 0; i < n; i++) {
        out[i] = fn(state, sr);
    }
}

#endif // TEST_UTIL_H
