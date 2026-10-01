/*
 * standalone_portaudio.c — Acid Synth standalone application
 * Uses PortAudio for real-time audio output.
 * Compile: gcc -O2 standalone_portaudio.c src/*.c -Isrc -lportaudio -lm -o acid_synth
 *
 * Controls via terminal (no GUI):
 *   - Press 'q' to quit
 *   - Press 'f' to toggle filter cutoff sweep
 *   - Press 'r' to toggle reverb
 *   - Press 'd' to toggle delay
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <portaudio.h>
#include "synth.h"

#define SAMPLE_RATE 44100.0f
#define FRAMES_PER_BUFFER 256

static AcidSynth synth;
static volatile int running = 1;

void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

/* PortAudio callback */
static int pa_callback(const void *input, void *output,
                       unsigned long framesPerBuffer,
                       const PaStreamCallbackTimeInfo *timeInfo,
                       PaStreamCallbackFlags statusFlags,
                       void *userData) {
    (void)input;
    (void)timeInfo;
    (void)statusFlags;
    (void)userData;

    float *out = (float*)output;
    float left[256], right[256];

    synth_process_block(&synth, left, right, framesPerBuffer);

    // Interleave stereo output
    for (unsigned long i = 0; i < framesPerBuffer; i++) {
        *out++ = left[i];   // Left
        *out++ = right[i];  // Right
    }

    return paContinue;
}

int main(void) {
    PaError err;
    PaStream *stream;

    printf("=== Acid Synth Standalone ===\n");
    printf("Sample rate: %d Hz\n", (int)SAMPLE_RATE);
    printf("Controls:\n");
    printf("  q  - Quit\n");
    printf("  f  - Toggle filter sweep\n");
    printf("  r  - Toggle reverb\n");
    printf("  d  - Toggle delay\n\n");

    // Init synth
    synth_init(&synth, SAMPLE_RATE);
    synth_gate(&synth, 1);

    // Init PortAudio
    err = Pa_Initialize();
    if (err != paNoError) {
        fprintf(stderr, "PortAudio init error: %s\n", Pa_GetErrorText(err));
        return 1;
    }

    // Open stereo output stream
    err = Pa_OpenDefaultStream(&stream,
        0,                  // no input
        2,                  // stereo output
        paFloat32,          // 32-bit float
        (int)SAMPLE_RATE,
        FRAMES_PER_BUFFER,
        pa_callback,
        NULL);
    if (err != paNoError) {
        fprintf(stderr, "PortAudio open error: %s\n", Pa_GetErrorText(err));
        Pa_Terminate();
        return 1;
    }

    // Start stream
    err = Pa_StartStream(stream);
    if (err != paNoError) {
        fprintf(stderr, "PortAudio start error: %s\n", Pa_GetErrorText(err));
        Pa_CloseStream(stream);
        Pa_Terminate();
        return 1;
    }

    printf("Playing! Press 'q' + Enter or Ctrl-C to quit.\n\n");

    // Simple terminal control loop
    signal(SIGINT, handle_signal);
    
    float filter_freq = 800.0f;
    int filter_sweep = 0;
    float sweep_pos = 0.0f;

    while (running) {
        int c = getchar();
        if (c == EOF) continue;
        switch (c) {
            case 'q': case 'Q':
                running = 0;
                break;
            case 'f': case 'F':
                filter_sweep = !filter_sweep;
                printf("Filter sweep: %s\n", filter_sweep ? "ON" : "OFF");
                break;
            case 'r': case 'R':
                reverb_set_mix(&synth.reverb, synth.reverb.mix > 0.1f ? 0.0f : 0.3f);
                printf("Reverb: %s\n", synth.reverb.mix > 0.1f ? "ON" : "OFF");
                break;
            case 'd': case 'D':
                delay_set_mix(&synth.delay, synth.delay.mix > 0.1f ? 0.0f : 0.35f);
                printf("Delay: %s\n", synth.delay.mix > 0.1f ? "ON" : "OFF");
                break;
        }
        
        // Filter sweep
        if (filter_sweep) {
            sweep_pos += 0.005f;
            if (sweep_pos > 1.0f) sweep_pos = 0.0f;
            filter_freq = 100.0f + powf(sweep_pos * 79.0f + 1.0f, 2.0f) * 10.0f;
            if (filter_freq > 12000.0f) filter_freq = 12000.0f;
            filter_set_freq(&synth.filter, filter_freq);
        }
    }

    // Cleanup
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    synth_free(&synth);

    printf("Done.\n");
    return 0;
}
