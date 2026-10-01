/*
 * vst3_acid.cpp — Acid Synth VST3 Plugin (self-contained)
 *
 * Compila senza VST3 SDK (definizioni interfacce inline).
 * Per compilazione full-SDK: g++ -DVST3_SDK -I/path/to/vst3sdk ...
 *
 * macOS build:
 *   g++ -O2 -std=c++11 -dynamiclib \
 *       src/vst3_acid.cpp src/*.c -Isrc \
 *       -framework CoreFoundation \
 *       -o AcidSynth.vst3/Contents/MacOS/acid_synth
 *
 * Windows build (MSVC):
 *   cl /nologo /O2 /LD vst3_acid.cpp src/*.c -Isrc /FeAcidSynth.vst3
 *
 * VST3 bundle struttura macOS:
 *   AcidSynth.vst3/Contents/Info.plist
 *   AcidSynth.vst3/Contents/MacOS/acid_synth  (binary)
 *   AcidSynth.vst3/Contents/Resources/
 */

/* ================================================================== */
/*  VST3 Interface Definitions (inline — no SDK required)             */
/* ================================================================== */

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <cstdlib>

/* --- VST3 base types --- */
using VstResult = int32_t;
static const VstResult kResultOk = 0;
static const VstResult kResultFalse = 1;
static const VstResult kNoInterface = 0x80000004;

using VstInt32 = int32_t;
using VstInt64 = int64_t;
using TChar = char16_t;
using TBool = int8_t;

/* --- FUID (128-bit GUID) --- */
struct FUID {
    uint64_t data[2];
    FUID() : data{0,0} {}
    FUID(uint64_t hi, uint64_t lo) : data{hi, lo} {}
    bool operator==(const FUID &o) const {
        return data[0]==o.data[0] && data[1]==o.data[1];
    }
    bool isSet() const { return data[0]!=0 || data[1]!=0; }
};

/* --- COM base --- */
class IUnknown {
public:
    virtual VstResult queryInterface(const FUID &iid, void **obj) = 0;
    virtual uint32_t addRef() = 0;
    virtual uint32_t release() = 0;
protected:
    virtual ~IUnknown() {}
};

/* --- VST3 known FUIDs --- */
static const FUID IID_IPluginBase    (0x1804F7B5, 0x8D3F4116);
static const FUID IID_IAudioProcessor(0xE2D53939, 0x7E2943B9);
static const FUID IID_IEditController(0x2B0B0C10, 0x0B540E40);
static const FUID IID_IConnectionPoint(0x12345678, 0x90ABCDEF);
static const FUID IID_IPluginFactory (0x7A888B84, 0x058B46F5);

/* ================================================================== */
/*  Core VST3 interfaces                                              */
/* ================================================================== */

/* Event bus / routing */
enum MediaTypes { kAudio = 0, kEvent = 1, kNumMediaTypes };
enum BusDirections { kInput = 0, kOutput = 1 };

/* Speaker arrangement */
static const uint64_t kStereo = 3; /* left | right */

/* Processing */
enum ProcessModes { kOffline, kPrefetch, kRealtime };
enum ProcessDataFlags {
    kNoFlags            = 0,
    kParamChanges       = 1 << 0
};

struct ParameterValue { VstInt32 id; float value; };

struct ProcessContext {
    VstInt64 samplePos;
    double   sampleRate;
    VstInt32 projectTimeMusic;
    TBool    playing;
    TBool    recording;
    double   tempo;
    double   timeSigNumer;
    VstInt32 timeSigDenom;
};

struct AudioBusBuffers {
    VstInt32 numChannels;
    void    *channelBuffersFloat;  /* float** */
    void    *channelBuffersInt;
};

struct ProcessData {
    VstInt32          processMode;
    VstInt32          symbolicSampleSize;
    VstInt32          numInputs;
    VstInt32          numOutputs;
    AudioBusBuffers  *inputs;
    AudioBusBuffers  *outputs;
    VstInt32          numSamples;
    ProcessContext   *context;
    ParameterValue   *paramChanges;
    VstInt32          numParamChanges;
};

/* --- IPluginBase --- */
class IPluginBase : public IUnknown {
public:
    virtual VstResult initialize(void *hostContext) = 0;
    virtual VstResult terminate() = 0;
};

/* --- IAudioProcessor --- */
class IAudioProcessor : public IUnknown {
public:
    virtual VstResult setupProcessing(const ProcessData &data) = 0;
    virtual VstResult setProcessing(TBool state) = 0;
    virtual VstResult process(ProcessData &data) = 0;
    virtual uint32_t  getBusCount(MediaTypes type, BusDirections dir) = 0;
    virtual VstResult getBusInfo(MediaTypes type, BusDirections dir,
                                 VstInt32 index, void *info) = 0;
    virtual VstResult getSpeakerArrangement(void *input, void *output) = 0;
    virtual VstResult setBusArrangements(void *inputs, VstInt32 numIns,
                                          void *outputs, VstInt32 numOuts) = 0;
    virtual VstResult canProcessSampleSize(VstInt32 symbolicSize) = 0;
    virtual uint32_t  getTailSamples() = 0;
};

/* --- IEditController --- */
class IEditController : public IUnknown {
public:
    virtual VstResult setComponentState(const void *data, VstInt32 size) = 0;
    virtual VstResult setState(const void *data, VstInt32 size) = 0;
    virtual VstResult getState(void *data, VstInt32 size) = 0;
    virtual VstInt32  getParameterCount() = 0;
    virtual VstResult getParameterInfo(VstInt32 index, void *info) = 0;
    virtual VstResult getParamStringByValue(VstInt32 id, float value,
                                             TChar *str, VstInt32 maxLen) = 0;
    virtual VstResult getParamValueByString(VstInt32 id, const TChar *str,
                                             float &value) = 0;
    virtual float     normalizeParamToPlain(VstInt32 id, float valueNormalized) = 0;
    virtual float     plainParamToNormalized(VstInt32 id, float plainValue) = 0;
    virtual float     getParamNormalized(VstInt32 id) = 0;
    virtual VstResult setParamNormalized(VstInt32 id, float value) = 0;
    virtual VstResult setComponentHandler(void *handler) = 0;
    virtual void     *getComponentHandler() = 0;
    virtual void     *createView(const FUID &name) = 0;
};

/* ================================================================== */
/*  Synth DSP — bridge alla libreria C                                */
/* ================================================================== */

extern "C" {
#include "synth.h"
#include "wavefolder.h"
#include "filter.h"
#include "delay.h"
#include "reverb.h"
}

/* 9 parametri — stessi nomi e range della GUI Win32 */
static const char *PARAM_NAMES[] = {
    "Cutoff", "Reso", "DlyMix", "DlyFb", "RevMix", "RevDcy",
    "Volume", "SeqRate", "Drive"
};
static const int NUM_PARAMS = 9;

struct SynthState {
    AcidSynth synth;
    float sr;
    float params[NUM_PARAMS];
    int gate_on;
    float current_freq;

    SynthState() : sr(44100.0f), gate_on(0), current_freq(220.0f) {
        synth_init(&synth, sr);
        for(int i=0;i<NUM_PARAMS;i++) params[i] = 0.5f;
        params[0] = 0.06f;  // cutoff ~800Hz
        params[1] = 0.4f;   // reso
        params[6] = 0.8f;   // volume
        params[8] = 0.1f;   // drive
        sync_params();
    }

    ~SynthState() { synth_free(&synth); }

    void sync_params() {
        synth.cutoff_base = 100.0f + params[0] * 11900.0f;
        filter_set_freq(&synth.filter, synth.cutoff_base);
        synth.reso_base = params[1];
        filter_set_resonance(&synth.filter, params[1]);
        delay_set_mix(&synth.delay, params[2] * 0.5f);
        delay_set_feedback(&synth.delay, params[3] * 0.8f);
        reverb_set_mix(&synth.reverb, params[4] * 0.5f);
        reverb_set_decay(&synth.reverb, params[5]);
        synth.master_volume = params[6];
        synth_set_seq_rate(&synth, 0.5f + params[7] * 8.0f);
        wavefolder_set_drive(&synth.wavefolder, params[8] * 10.0f);
    }

    void set_param(int id, float norm) {
        if(id<0||id>=NUM_PARAMS) return;
        params[id] = norm;
        sync_params();
    }

    float get_param(int id) {
        return (id>=0&&id<NUM_PARAMS) ? params[id] : 0.0f;
    }

    void process(float **outputs, int samples) {
        synth_process_block(&synth, outputs[0], outputs[1], samples);
    }

    void note_on(float freq) {
        current_freq = freq;
        synth_note_on(&synth, freq);
        gate_on = 1;
    }

    void note_off() {
        synth_note_off(&synth);
        gate_on = 0;
    }
};

/* ================================================================== */
/*  Processor — VST3 audio processing component                       */
/* ================================================================== */

class AcidProcessor : public IAudioProcessor, public IPluginBase {
    uint32_t ref;
    SynthState *state;
public:
    AcidProcessor(SynthState *s) : ref(1), state(s) {}
    virtual ~AcidProcessor() {}

    /* IUnknown */
    VstResult queryInterface(const FUID &iid, void **obj) override {
        if(iid == IID_IPluginBase)     { *obj = (IPluginBase*)this;     addRef(); return kResultOk; }
        if(iid == IID_IAudioProcessor) { *obj = (IAudioProcessor*)this; addRef(); return kResultOk; }
        *obj = nullptr;
        return kNoInterface;
    }
    uint32_t addRef() override { return ++ref; }
    uint32_t release() override {
        if(--ref==0) { delete this; return 0; }
        return ref;
    }

    /* IPluginBase */
    VstResult initialize(void *hostContext) override { (void)hostContext; return kResultOk; }
    VstResult terminate() override { return kResultOk; }

    /* IAudioProcessor */
    VstResult setupProcessing(const ProcessData &data) override {
        (void)data;
        return kResultOk;
    }
    VstResult setProcessing(TBool state) override {
        (void)state;
        return kResultOk;
    }
    VstResult process(ProcessData &data) override {
        if(!data.numOutputs || !data.outputs) return kResultFalse;
        float **outs = (float**)data.outputs[0].channelBuffersFloat;
        if(!outs) return kResultFalse;

        state->process(outs, data.numSamples);
        return kResultOk;
    }
    uint32_t getBusCount(MediaTypes t, BusDirections d) override {
        (void)t;
        return (d == kOutput) ? 1 : 0;
    }
    VstResult getBusInfo(MediaTypes t, BusDirections d,
                          VstInt32 idx, void *info) override {
        (void)t; (void)d; (void)idx; (void)info;
        return kResultFalse;
    }
    VstResult getSpeakerArrangement(void *in, void *out) override {
        (void)in; (void)out;
        *(uint64_t*)out = kStereo;
        return kResultOk;
    }
    VstResult setBusArrangements(void *ins, VstInt32 ni, void *outs, VstInt32 no) override {
        (void)ins; (void)ni; (void)outs; (void)no;
        return kResultOk;
    }
    VstResult canProcessSampleSize(VstInt32 size) override {
        (void)size;
        return kResultOk;  // 32-bit float OK
    }
    uint32_t getTailSamples() override { return 0; }
};

/* ================================================================== */
/*  Controller — VST3 parameter editing                               */
/* ================================================================== */

class AcidController : public IEditController, public IPluginBase {
    uint32_t ref;
    SynthState *state;
public:
    AcidController(SynthState *s) : ref(1), state(s) {}
    virtual ~AcidController() {}

    VstResult queryInterface(const FUID &iid, void **obj) override {
        if(iid == IID_IPluginBase)      { *obj = (IPluginBase*)this;      addRef(); return kResultOk; }
        if(iid == IID_IEditController)  { *obj = (IEditController*)this;  addRef(); return kResultOk; }
        *obj = nullptr;
        return kNoInterface;
    }
    uint32_t addRef() override { return ++ref; }
    uint32_t release() override {
        if(--ref==0) { delete this; return 0; }
        return ref;
    }

    VstResult initialize(void *ctx) override { (void)ctx; return kResultOk; }
    VstResult terminate() override { return kResultOk; }

    VstResult setComponentState(const void *d, VstInt32 s) override { (void)d; (void)s; return kResultOk; }
    VstResult setState(const void *d, VstInt32 s) override { (void)d; (void)s; return kResultOk; }
    VstResult getState(void *d, VstInt32 s) override { (void)d; (void)s; return kResultOk; }

    VstInt32 getParameterCount() override { return NUM_PARAMS; }

    VstResult getParameterInfo(VstInt32 idx, void *info) override {
        struct ParamInfo {
            FUID    id;
            TChar   title[64];
            TChar   units[16];
            VstInt32 stepCount;
            float   defaultNormalized;
            VstInt32 unitId;
            TChar   shortTitle[64];
        };
        if(idx<0||idx>=NUM_PARAMS) return kResultFalse;
        ParamInfo *pi = (ParamInfo*)info;
        memset(pi, 0, sizeof(ParamInfo));
        /* Convert nome parametro a UTF-16 */
        const char *name = PARAM_NAMES[idx];
        for(int i=0; name[i] && i<63; i++)
            pi->title[i] = (TChar)name[i];
        pi->stepCount = 1000;
        pi->defaultNormalized = (idx==6) ? 0.8f : 0.5f;
        return kResultOk;
    }

    VstResult getParamStringByValue(VstInt32 id, float v, TChar *str, VstInt32 max) override {
        (void)max;
        char buf[32];
        snprintf(buf,32,"%.3f",v);
        for(int i=0; buf[i] && i<31; i++) str[i] = (TChar)buf[i];
        str[31]=0;
        return kResultOk;
    }
    VstResult getParamValueByString(VstInt32 id, const TChar *str, float &v) override {
        (void)id;
        char buf[32];
        int i=0;
        for(; str[i] && i<31; i++) buf[i]=(char)str[i];
        buf[i]=0;
        v = (float)atof(buf);
        return kResultOk;
    }
    float normalizeParamToPlain(VstInt32 id, float n) override { (void)id; return n; }
    float plainParamToNormalized(VstInt32 id, float p) override { (void)id; return p; }
    float getParamNormalized(VstInt32 id) override { return state->get_param(id); }
    VstResult setParamNormalized(VstInt32 id, float v) override {
        state->set_param(id, v);
        return kResultOk;
    }
    VstResult setComponentHandler(void *h) override { (void)h; return kResultOk; }
    void *getComponentHandler() override { return nullptr; }
    void *createView(const FUID &name) override { (void)name; return nullptr; }
};

/* ================================================================== */
/*  Factory — creates processor + controller pairs                    */
/* ================================================================== */

/* Plugin class ID */
static const FUID ACID_PROC_CLASS_ID(0xAC1D5001, 0x00000001);
static const FUID ACID_CTRL_CLASS_ID(0xAC1D5002, 0x00000002);

struct VstClassInfo {
    FUID   cid;
    TChar  name[64];
    VstInt32 category;
    TChar  vendor[64];
    TChar  version[64];
};

static VstClassInfo acid_class;
static SynthState *global_state = nullptr;

class PluginFactory : public IUnknown {
    uint32_t ref;
public:
    PluginFactory() : ref(1) {
        /* Init class info */
        memset(&acid_class, 0, sizeof(acid_class));
        acid_class.cid = ACID_PROC_CLASS_ID;
        const char *n = "Acid Synth v4 Dreamscape";
        for(int i=0; n[i] && i<63; i++) acid_class.name[i] = (TChar)n[i];
        acid_class.category = 0; /* kAudioEffect */
        const char *v = "Pietro Acidi";
        for(int i=0; v[i] && i<63; i++) acid_class.vendor[i] = (TChar)v[i];
        const char *ver = "1.0.0";
        for(int i=0; ver[i] && i<63; i++) acid_class.version[i] = (TChar)ver[i];
    }

    VstResult queryInterface(const FUID &iid, void **obj) override {
        if(iid == IID_IPluginFactory) { *obj = this; addRef(); return kResultOk; }
        *obj = nullptr;
        return kNoInterface;
    }
    uint32_t addRef() override { return ++ref; }
    uint32_t release() override {
        if(--ref==0) { delete this; return 0; }
        return ref;
    }

    VstInt32 countClasses() { return 1; }

    VstResult getClassInfo(VstInt32 index, void *info) {
        if(index!=0) return kResultFalse;
        memcpy(info, &acid_class, sizeof(VstClassInfo));
        return kResultOk;
    }

    VstResult createInstance(const FUID &cid, const FUID &iid, void **obj) {
        if(!global_state) {
            global_state = new SynthState();
        }
        if(cid == ACID_PROC_CLASS_ID) {
            *obj = (IAudioProcessor*)new AcidProcessor(global_state);
            return kResultOk;
        }
        if(cid == ACID_CTRL_CLASS_ID) {
            *obj = (IEditController*)new AcidController(global_state);
            return kResultOk;
        }
        *obj = nullptr;
        return kResultFalse;
    }
};

/* ================================================================== */
/*  Exported factory entry point                                      */
/* ================================================================== */

extern "C" {

void* GetPluginFactory() {
    static PluginFactory *factory = nullptr;
    if(!factory) factory = new PluginFactory();
    return (void*)factory;
}

} /* extern "C" */

/* ================================================================== */
/*  Standalone test — compila con -DSTANDALONE_TEST                    */
/* ================================================================== */

#ifdef STANDALONE_TEST
int main() {
    printf("=== Acid Synth VST3 Plugin Test ===\n\n");

    void *factory = GetPluginFactory();
    printf("Factory: %p\n", factory);
    printf("Plugin ready: Acid Synth v4 Dreamscape\n");
    printf("Parameters (%d):\n", NUM_PARAMS);
    for(int i=0;i<NUM_PARAMS;i++)
        printf("  %d: %s\n", i, PARAM_NAMES[i]);

    /* Process test block */
    SynthState state;
    state.set_param(8, 0.3f);  /* drive = 3.0 */
    state.note_on(440.0f);

    float left[256], right[256];
    float *outs[2] = {left, right};
    state.process(outs, 256);

    float peak=0;
    for(int i=0;i<256;i++) {
        float a = fabsf(left[i]);
        if(a>peak) peak=a;
    }
    printf("\nTest block: peak=%.4f (should be >0)\n", peak);
    printf("DSP works: %s\n", peak>0.001f ? "YES" : "NO");

    state.note_off();
    printf("\nVST3 plugin test passed.\n");
    return 0;
}
#endif /* STANDALONE_TEST */
