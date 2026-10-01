/*
 * standalone_mac.c — Acid Synth macOS standalone
 * Zero dipendenze esterne. Usa CoreAudio (AudioQueue) per l'audio
 * e CGEvent/Carbon per la tastiera MIDI.
 *
 * Compila su Mac con:
 *   gcc -O2 standalone_mac.c src/*.c -Isrc \
 *       -framework CoreAudio -framework AudioToolbox \
 *       -framework CoreFoundation -framework Carbon \
 *       -o acid_synth_mac
 *
 * Tastiera:
 *   Z=C3  X=D3  C=E3  V=F3  B=G3  N=A3  M=B3
 *   S=C#3 D=D#3 G=F#3 H=G#3 J=A#3
 *   Q=C4  W=C#4 E=D4  R=D#4 T=E4  Y=F4  U=G4  I=A4  O=G#4 P=B4
 *   SPACE = sustain toggle
 *   ESC   = quit
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Carbon/Carbon.h>  // per GetCurrentEventKeyModifiers

#include "synth.h"

#define SR 44100.0
#define FRAMES 256

static AcidSynth synth;
static volatile int running = 1;
static AudioQueueRef queue = NULL;

/* --- Tastiera MIDI virtuale --- */
static const int white_notes[] = {48,50,52,53,55,57,59,60,62,64,65,67,69,71}; /* C3..B4 */
static const int white_keys[] = {'Z','X','C','V','B','N','M','Q','E','T','Y','U','I','P'};
#define NW 14
static const int black_notes[] = {49,51,54,56,58,61,63,66,68,70};
static const int black_keys[] = {'S','D','G','H','J','W','R','F','O','A'};
#define NB 10

static int keys_down[24] = {0};
static int sustain_on = 0;
static int current_note = -1;
static int sustained_note = -1;

static int find_key(int c) {
    for(int i=0;i<NW;i++) if(white_keys[i]==c) return i;
    for(int i=0;i<NB;i++) if(black_keys[i]==c) return NW+i;
    return -1;
}

static int note_for_key(int idx) {
    if(idx<NW) return white_notes[idx];
    return black_notes[idx-NW];
}

static void key_down(int c) {
    int idx = find_key(c);
    if(idx<0) return;
    keys_down[idx]=1;

    /* Sustain release: se c'era una nota sostenuta, la sostituiamo */
    if(sustained_note>=0) sustained_note=-1;

    int m = note_for_key(idx);
    float f = 440.0f * powf(2.0f, (m-69.0f)/12.0f);
    synth_note_on(&synth, f);
    current_note = m;
}

static void key_up(int c) {
    int idx = find_key(c);
    if(idx<0) return;
    keys_down[idx]=0;

    if(sustain_on) {
        sustained_note = note_for_key(idx);
    } else {
        if(current_note>=0) {
            synth_note_off(&synth);
            current_note = -1;
        }
    }
}

/* --- Audio callback (AudioQueue) --- */
static void audio_callback(void *user, AudioQueueRef q, AudioQueueBufferRef buf) {
    (void)user; (void)q;
    float *out = (float*)buf->mAudioData;
    float left[FRAMES], right[FRAMES];

    synth_process_block(&synth, left, right, FRAMES);

    for(int i=0;i<FRAMES;i++) {
        out[i*2]   = left[i];   // L
        out[i*2+1] = right[i];  // R
    }
    buf->mAudioDataByteSize = FRAMES * 2 * sizeof(float);
    AudioQueueEnqueueBuffer(queue, buf, 0, NULL);
}

/* --- Init CoreAudio --- */
static int init_audio(void) {
    AudioStreamBasicDescription fmt = {0};
    fmt.mSampleRate       = SR;
    fmt.mFormatID         = kAudioFormatLinearPCM;
    fmt.mFormatFlags      = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    fmt.mBytesPerPacket   = 2 * sizeof(float);
    fmt.mFramesPerPacket  = 1;
    fmt.mBytesPerFrame    = 2 * sizeof(float);
    fmt.mChannelsPerFrame = 2;
    fmt.mBitsPerChannel   = 32;

    OSStatus err = AudioQueueNewOutput(&fmt, audio_callback, NULL,
                                       CFRunLoopGetMain(),
                                       kCFRunLoopCommonModes, 0, &queue);
    if(err) { fprintf(stderr,"AudioQueueNewOutput err=%d\n",(int)err); return -1; }

    /* Prepara 3 buffer */
    for(int i=0;i<3;i++) {
        AudioQueueBufferRef buf;
        err = AudioQueueAllocateBuffer(queue, FRAMES * 2 * sizeof(float), &buf);
        if(err) { fprintf(stderr,"AllocateBuffer err=%d\n",(int)err); return -1; }
        audio_callback(NULL, queue, buf);
    }

    err = AudioQueueStart(queue, NULL);
    if(err) { fprintf(stderr,"AudioQueueStart err=%d\n",(int)err); return -1; }
    return 0;
}

/* --- Main loop (CGEvent-based keyboard) --- */
static void print_help(void) {
    printf("\n=== Acid Synth macOS Standalone ===\n");
    printf("Piano keyboard: ZXCVBNM  |  QERTYUIOP  (C3..B4)\n");
    printf("Black keys: S D G H J  |  W R F O\n");
    printf("SPACE = sustain toggle   ESC = quit\n");
    printf("====================================\n\n");
}

int main(void) {
    synth_init(&synth, SR);
    synth.master_volume = 0.8f;

    print_help();

    if(init_audio()) {
        synth_free(&synth);
        return 1;
    }

    /* Poll loop: legge eventi tastiera via getchar() */
    printf("Playing! Type keys to play. SPACE=sustain ESC=quit.\n");

    while(running) {
        int c = getchar();
        if(c==EOF) continue;

        if(c==27 || c=='q') {  // ESC or q
            running=0;
            break;
        }

        if(c==' ') {
            sustain_on = !sustain_on;
            printf("Sustain: %s\n", sustain_on ? "ON" : "OFF");
            if(!sustain_on && sustained_note>=0) {
                synth_note_off(&synth);
                sustained_note = -1;
                current_note = -1;
            }
            continue;
        }

        /* Maiuscole */
        if(c>='a' && c<='z') c = c - 'a' + 'A';

        if(!keys_down[find_key(c)]) {
            key_down(c);
        } else {
            key_up(c);
        }
    }

    /* Cleanup */
    AudioQueueStop(queue, 1);
    AudioQueueDispose(queue, 1);
    synth_free(&synth);
    printf("Done.\n");
    return 0;
}
