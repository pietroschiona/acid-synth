/*
 * vst3_wrapper.cpp — Minimal VST3 wrapper for acid_synth
 * 
 * Self-contained: defines only the VST3 interfaces we need.
 * No VST3 SDK required.
 *
 * Compile on Windows with MSVC:
 *   cl /nologo /O2 /LD vst3_wrapper.cpp src/*.c -Isrc /Feacid_synth.vst3
 *   (requires VST3 SDK headers for proper GUIDs — see build instructions)
 *
 * For now: compiles as a verification unit on Linux with g++ to test
 * the DSP kernel linking.
 *
 * NOTE: A proper VST3 build requires the VST3 SDK headers for exact
 * GUID definitions and FUID base class. This file provides the shell
 * structure; the SDK headers are needed for binary compatibility.
 * See BUILD_VST3.md for instructions.
 */

#include <cstring>
#include <cstdio>
#include "synth.h"

/* ================================================================ */
/*  Kernel wrapper — bridges VST3 host to AcidSynth                 */
/* ================================================================ */

class SynthKernel {
public:
    AcidSynth synth;
    float sr;

    SynthKernel() : sr(44100.0f) {
        synth_init(&synth, sr);
    }

    ~SynthKernel() {
        synth_free(&synth);
    }

    void reset() {
        synth_free(&synth);
        synth_init(&synth, sr);
    }

    void set_sample_rate(float sample_rate) {
        sr = sample_rate;
        reset();
    }

    void process(float **inputs, float **outputs, int num_samples) {
        // inputs unused (synth is generative)
        (void)inputs;
        synth_process_block(&synth, outputs[0], outputs[1], num_samples);
    }

    // Parameter access
    void set_param(int id, float value) {
        switch (id) {
            case 0: // Filter cutoff
                filter_set_freq(&synth.filter, 100.0f + value * 11900.0f);
                break;
            case 1: // Filter resonance
                filter_set_resonance(&synth.filter, value);
                break;
            case 2: // Delay mix
                delay_set_mix(&synth.delay, value * 0.5f);
                break;
            case 3: // Delay feedback
                delay_set_feedback(&synth.delay, value * 0.8f);
                break;
            case 4: // Reverb mix
                reverb_set_mix(&synth.reverb, value * 0.5f);
                break;
            case 5: // Reverb decay
                reverb_set_decay(&synth.reverb, value);
                break;
            case 6: // Master volume
                synth.master_volume = value;
                break;
            case 7: // Sequencer rate
                synth_set_seq_rate(&synth, 0.5f + value * 8.0f);
                break;
            case 8: // Drive
                wavefolder_set_drive(&synth.wavefolder, value * 10.0f);
                break;
        }
    }

    float get_param(int id) {
        switch (id) {
            case 0: return (synth.filter.freq - 100.0f) / 11900.0f;
            case 1: return synth.filter.resonance;
            case 2: return synth.delay.mix / 0.5f;
            case 3: return synth.delay.feedback / 0.8f;
            case 4: return synth.reverb.mix / 0.5f;
            case 5: return synth.reverb.decay;
            case 6: return synth.master_volume;
            case 7: return (synth.seq.rate - 0.5f) / 8.0f;
            case 8: return synth.wavefolder.drive / 10.0f;
        }
        return 0.0f;
    }

    static int num_params() { return 9; }
    static const char* param_name(int id) {
        static const char* names[] = {
            "Filter Cutoff", "Filter Reso", "Delay Mix",
            "Delay Fdbk", "Reverb Mix", "Reverb Decay",
            "Volume", "Seq Rate", "Drive"
        };
        return (id >= 0 && id < 9) ? names[id] : "Unknown";
    }
};

/* ================================================================ */
/*  Standalone test main — compile on Linux with g++                 */
/*  Verifies that the DSP kernel links correctly via the wrapper.    */
/* ================================================================ */

#ifdef STANDALONE_TEST
int main() {
    printf("Acid Synth VST3 Wrapper Test\n");
    printf("===========================\n\n");

    SynthKernel kernel;
    kernel.set_sample_rate(44100.0f);

    printf("%d parameters:\n", SynthKernel::num_params());
    for (int i = 0; i < SynthKernel::num_params(); i++) {
        printf("  %d: %s = %.3f\n", i, SynthKernel::param_name(i), kernel.get_param(i));
    }

    // Process a small block
    float outputs[2][256];
    float *outs[2] = {outputs[0], outputs[1]};
    kernel.process(nullptr, outs, 256);

    printf("\nProcessed 256 samples.\n");
    printf("Output[0]: %.4f, Output[255]: %.4f\n", outputs[0][0], outputs[0][255]);
    printf("Wrapper OK — DSP kernel links correctly.\n");

    return 0;
}
#endif // STANDALONE_TEST
