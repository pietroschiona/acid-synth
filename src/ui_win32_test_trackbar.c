/*
 * ui_win32_test_trackbar.c — Test if trackbar crashes
 */
#include <windows.h>
#include <mmsystem.h>
#include <commctrl.h>
#include <math.h>
#include "synth.h"

#define SAMPLE_RATE 44100
#define FRAMES_PER_BUF 256
#define NUM_BUFS 6

static AcidSynth g_synth;
static volatile int g_running = 1;
static WAVEHDR g_wavehdr[NUM_BUFS];
static short g_wavedata[NUM_BUFS][FRAMES_PER_BUF * 2];
static HWAVEOUT g_hwo = NULL;
static CRITICAL_SECTION g_audio_cs;

static void fill_buffer(short *data, int n) {
    float l[256], r[256];
    EnterCriticalSection(&g_audio_cs);
    synth_process_block(&g_synth, l, r, n);
    LeaveCriticalSection(&g_audio_cs);
    for (int i = 0; i < n; i++) {
        data[i*2]   = (short)(l[i] * 32767.0f);
        data[i*2+1] = (short)(r[i] * 32767.0f);
    }
}

static void CALLBACK wave_cb(HWAVEOUT hwo, UINT msg, DWORD_PTR inst, DWORD_PTR p1, DWORD_PTR p2) {
    (void)inst; (void)p2;
    if (msg == WOM_DONE && g_running) {
        WAVEHDR *done = (WAVEHDR *)p1;
        waveOutUnprepareHeader(hwo, done, sizeof(WAVEHDR));
        fill_buffer((short*)done->lpData, FRAMES_PER_BUF);
        waveOutPrepareHeader(hwo, done, sizeof(WAVEHDR));
        waveOutWrite(hwo, done, sizeof(WAVEHDR));
    }
}

static void start_audio(void) {
    WAVEFORMATEX wf = {0};
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 2;
    wf.nSamplesPerSec = SAMPLE_RATE;
    wf.wBitsPerSample = 16;
    wf.nBlockAlign = 4;
    wf.nAvgBytesPerSec = SAMPLE_RATE * 4;
    if (waveOutOpen(&g_hwo, WAVE_MAPPER, &wf, (DWORD_PTR)wave_cb, 0, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) return;
    for (int i = 0; i < NUM_BUFS; i++) {
        g_wavehdr[i].lpData = (LPSTR)g_wavedata[i];
        g_wavehdr[i].dwBufferLength = FRAMES_PER_BUF * 2 * 2;
        g_wavehdr[i].dwFlags = 0;
        fill_buffer(g_wavedata[i], FRAMES_PER_BUF);
        waveOutPrepareHeader(g_hwo, &g_wavehdr[i], sizeof(WAVEHDR));
        waveOutWrite(g_hwo, &g_wavehdr[i], sizeof(WAVEHDR));
    }
}

static void stop_audio(void) {
    g_running = 0;
    if (g_hwo) {
        waveOutReset(g_hwo);
        for (int i = 0; i < NUM_BUFS; i++) waveOutUnprepareHeader(g_hwo, &g_wavehdr[i], sizeof(WAVEHDR));
        waveOutClose(g_hwo); g_hwo = NULL;
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
        case WM_CREATE: {
            // Create ONE trackbar (to test comctl32)
            CreateWindow(TRACKBAR_CLASS, "", WS_CHILD|WS_VISIBLE|TBS_AUTOTICKS,
                10, 10, 200, 22, hwnd, (HMENU)1000, 0, 0);
            
            // Create ONE button
            CreateWindow("BUTTON", "Test", WS_CHILD|WS_VISIBLE,
                10, 40, 80, 22, hwnd, (HMENU)2000, 0, 0);
            
            synth_init(&g_synth, (float)SAMPLE_RATE);
            g_synth.master_volume = 0.8f;
            g_synth.seq_mode = 0;
            start_audio();
            break;
        }
        case WM_DESTROY:
            stop_audio();
            DeleteCriticalSection(&g_audio_cs);
            synth_free(&g_synth);
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, msg, w, l);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show) {
    (void)hPrev; (void)cmd;
    InitCommonControls();  // Init comctl32
    InitializeCriticalSection(&g_audio_cs);
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.lpszClassName = "TestTB";
    if (!RegisterClass(&wc)) return 1;
    HWND hwnd = CreateWindow("TestTB", "Test Trackbar",
        WS_OVERLAPPEDWINDOW|WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 300, 200, 0, 0, hInst, 0);
    if (!hwnd) return 1;
    MSG msg;
    while (GetMessage(&msg, 0,0,0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return (int)msg.wParam;
}
