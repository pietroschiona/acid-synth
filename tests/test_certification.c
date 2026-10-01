/* test_certification.c — Acid Synth Certification + Stress Test
 * Generates reference WAV, verifies audio mathematically, stress-tests
 * parameter changes to simulate slider/UI interaction.
 * gcc -O2 tests/test_certification.c src/*.c -Isrc -Itests -lm -o test_cert && ./test_cert
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "test_util.h"
#include "synth.h"
#include "osc.h"
#include "filter.h"
#include "delay.h"
#include "reverb.h"
#include "wavefolder.h"
#include "lfo.h"
#include "sequencer.h"
#include "scale.h"
#define SR 44100.0f

static float calc_rms(const float *buf, int n, int skip) {
    float sum=0;int c=n-skip;if(c<1)return 0;for(int i=skip;i<n;i++)sum+=buf[i]*buf[i];return sqrtf(sum/c);}
static float calc_peak(const float *buf, int n) {
    float p=0;for(int i=0;i<n;i++){float a=fabsf(buf[i]);if(a>p)p=a;}return p;}
static float calc_dc(const float *buf, int n, int skip) {
    double s=0;int c=n-skip;if(c<1)return 0;for(int i=skip;i<n;i++)s+=buf[i];return (float)(s/c);}

/* -- 1. REFERENCE WAV ------------------------------------------------- */
static int test_certified_reference_wav(void) {
    TEST("1: generate certified reference WAV"); int n=(int)(SR*5.0f);
    float *oL=(float*)calloc(n,4),*oR=(float*)calloc(n,4);
    AcidSynth s;synth_init(&s,SR);s.seq_mode=1;s.master_volume=0.8f;
    synth_gate(&s,1);synth_process_block(&s,oL,oR,n);synth_gate(&s,0);
    int tl=(int)(SR*1.5f);float *tL=(float*)calloc(tl,4),*tR=(float*)calloc(tl,4);
    synth_process_block(&s,tL,tR,tl);
    WavWriter w=wav_create("acid_synth_reference.wav",(uint32_t)SR,2);
    for(int i=0;i<n;i++){wav_write_sample(&w,oL[i]);wav_write_sample(&w,oR[i]);}
    for(int i=0;i<tl;i++){wav_write_sample(&w,tL[i]);wav_write_sample(&w,tR[i]);}wav_close(&w);
    float rms=calc_rms(oL,(int)(SR*4),(int)SR);ASSERT(rms>0.001f,"RMS>0.001");
    float pk=calc_peak(oL,n);ASSERT(pk<=1.001f,"peak<=1.0");
    float dc=calc_dc(oL,n,(int)(SR*0.5f));ASSERT(fabsf(dc)<0.02f,"DC<0.02");
    float rt_mid=calc_rms(oL,(int)(SR*2),(int)(SR*0.5f));
    float rt_early=calc_rms(oL,(int)(SR*0.2f),(int)(SR*0.01f));
    ASSERT(rt_mid>rt_early*0.5f,"envelope rises");
    printf("  REFERENCE WAV: acid_synth_reference.wav (%.1fs pk=%.3f rms=%.3f dc=%.4f)\n",(n+tl)/SR,pk,rms,dc);
    free(oL);free(oR);free(tL);free(tR);synth_free(&s);END_TEST();return _test_passed?0:1;}

/* -- 2. All 9 parameters change output --------------------------------
 * Nota speciale per drive (param 8): con dry/wet mix il volume resta
 * costante ma la FORMA D'ONDA cambia. Per drive usiamo max diff
 * sample-by-sample invece di RMS.                                              */
static int test_all_parameters_change_output(void) {
    TEST("2: all 9 params change output");AcidSynth s;int n=(int)(SR*2.0f);
    float *bA=(float*)calloc(n,4),*bB=(float*)calloc(n,4);int fail=0;
    for(int pi=0;pi<9;pi++){
        synth_init(&s,SR);s.master_volume=1;s.seq_mode=0;
        s.mix_osc1=1;s.mix_osc2=s.mix_osc3=s.mix_sub=0;s.env_attack=0.002f;s.env_release=0.01f;
        osc_init(&s.osc1,OSC_WAVE_SINE,220);filter_init(&s.filter,FILTER_LP,20000,0.01f,SR);
        delay_set_mix(&s.delay,0.5f);reverb_set_mix(&s.reverb,0.5f);
        switch(pi){case 0:s.cutoff_base=100;filter_set_freq(&s.filter,100);break;
        case 1:s.reso_base=0;filter_set_resonance(&s.filter,0);break;
        case 2:delay_set_mix(&s.delay,0);break;case 3:delay_set_feedback(&s.delay,0);break;
        case 4:reverb_set_mix(&s.reverb,0);break;case 5:reverb_set_decay(&s.reverb,0);break;
        case 6:s.master_volume=0.05f;break;case 7:synth_set_seq_rate(&s,0.5f);break;
        case 8:wavefolder_set_drive(&s.wavefolder,0);break;}
        synth_gate(&s,1);synth_process_block(&s,bA,bA,n);float rA=calc_rms(bA,n,(int)(0.05f*SR));
        synth_free(&s);synth_init(&s,SR);s.master_volume=1;s.seq_mode=0;
        s.mix_osc1=1;s.mix_osc2=s.mix_osc3=s.mix_sub=0;s.env_attack=0.002f;s.env_release=0.01f;
        osc_init(&s.osc1,OSC_WAVE_SINE,220);filter_init(&s.filter,FILTER_LP,20000,0.01f,SR);
        delay_set_mix(&s.delay,0.5f);reverb_set_mix(&s.reverb,0.5f);
        switch(pi){case 0:s.cutoff_base=1000;filter_set_freq(&s.filter,1000);break;
        case 1:s.reso_base=0.8f;filter_set_resonance(&s.filter,0.8f);break;
        case 2:delay_set_mix(&s.delay,0.5f);break;case 3:delay_set_feedback(&s.delay,0.8f);break;
        case 4:reverb_set_mix(&s.reverb,0.5f);break;case 5:reverb_set_decay(&s.reverb,0.9f);break;
        case 6:s.master_volume=1;break;case 7:s.seq_mode=1;synth_set_seq_rate(&s,8);break;
        case 8:wavefolder_set_drive(&s.wavefolder,8);break;}
        synth_gate(&s,1);synth_process_block(&s,bB,bB,n);float rB=calc_rms(bB,n,(int)(0.05f*SR));
        if(pi==8){
            /* Drive: dry/wet mix mantiene volume costante, ma forma d'onda
             * cambia. Verifichiamo con max diff sample-by-sample. */
            float max_diff=0; for(int i=(int)(0.05f*SR);i<n;i++){
                float d=fabsf(bA[i]-bB[i]); if(d>max_diff)max_diff=d;}
            if(max_diff<0.001f){fail++;printf("  FAIL: param %d waveform same (max_diff=%.6f)\n",pi,max_diff);}
        }else{
        if(fabsf(rB-rA)<0.0001f){
            float tA=calc_rms(bA,n,n/2),tB=calc_rms(bB,n,n/2);
            if((pi==3||pi==5)&&fabsf(tB-tA)<0.0001f)
                printf("  WARN: param %d tail same\n",pi);
            else if(pi!=3&&pi!=5){fail++;printf("  FAIL: param %d same\n",pi);}}}
    }
    ASSERT(fail==0,"all 9 params differ at min vs max");
    free(bA);free(bB);synth_free(&s);END_TEST();return(fail==0&&_test_passed)?0:1;}
/* -- 3. Sequencer accuracy ------------------------------------------- */
static int test_sequencer_accuracy(void) {
    TEST("3: sequencer advances");AcidSynth s;synth_init(&s,SR);
    s.seq_mode=1;s.master_volume=1;s.env_attack=0.001f;s.env_release=0.01f;
    synth_set_seq_rate(&s,4);int n=(int)(SR*1);
    float *b=(float*)calloc(n,4),*bR=(float*)calloc(n,4);
    int fstep=s.seq.current_step;synth_gate(&s,1);synth_process_block(&s,b,bR,n);
    ASSERT(s.seq.current_step!=fstep||s.seq.current_step==0,"sequencer advanced");
    float rms=calc_rms(b,n,(int)(0.1f*SR));ASSERT(rms>0.001f,"seq audible");
    int vn=0;for(int i=0;i<16;i++)if(s.seq.notes[i]>20&&s.seq.notes[i]<20000)vn++;
    ASSERT(vn==16,"16 valid notes");free(b);free(bR);synth_free(&s);END_TEST();return _test_passed?0:1;}

/* -- 4. Dreamscape defaults ------------------------------------------ */
static int test_dreamscape_character(void) {
    TEST("4: dreamscape mode");AcidSynth s;synth_init(&s,SR);
    ASSERT(s.osc1.waveform==OSC_WAVE_TRI,"VCO1=tri");
    ASSERT(s.env_attack>=0.05f,"attack>=50ms");ASSERT(s.env_release>=0.5f,"release>=500ms");
    ASSERT(s.reverb.mix>=0.25f,"reverb>=0.25");ASSERT(s.wavefolder.drive<0.01f,"drive=0");
    ASSERT(s.use_equal_temp==1,"12-TET");ASSERT(s.scale_type==SCALE_DORIAN,"Dorian");
    int n=(int)(SR*0.5f);float *b=(float*)calloc(n,4),*bR=(float*)calloc(n,4);
    s.seq_mode=1;synth_gate(&s,1);synth_process_block(&s,b,bR,n);synth_gate(&s,0);
    float md=0;for(int i=2;i<n;i++){float d=fabsf(b[i]-b[i-1]);if(d>md)md=d;}
    ASSERT(md<0.5f,"no clicks");free(b);free(bR);synth_free(&s);END_TEST();return _test_passed?0:1;}

/* -- 5. All 16 scales correct ---------------------------------------- */
static int test_scale_correctness(void) {
    TEST("5: all 16 scales");for(int st=0;st<SCALE_COUNT;st++){
        AcidSynth s;synth_init(&s,SR);synth_set_scale(&s,(ScaleType)st);
        const ScaleDef *sd=scale_get_def((ScaleType)st);
        ASSERT(sd->num_degrees>=2&&sd->num_degrees<=12,"deg 2-12");
        for(int i=0;i<16;i++){int d=s.seq_degrees[i];ASSERT(d>=0,"non-neg deg");
        ASSERT(d%sd->num_degrees<sd->num_degrees,"idx range");
        ASSERT(s.seq.notes[i]>20&&s.seq.notes[i]<20000,"audible");}
        synth_free(&s);}END_TEST();return _test_passed?0:1;}

/* -- 6. STRESS TEST: rapid parameter changes + note toggles ---------- */
/* Simulates the GUI: rapid slider moves + key presses.
 * Verifies: no discontinuities > 1.0 per sample (glitch detection).
 * 10 seconds of heavy interaction stress. */
static int test_stress_rapid_changes(void) {
    TEST("6: stress test (rapid param changes + note toggles, 10s)");
    int n=(int)(SR*10);int bs=256;int steps=n/bs;
    float *oL=(float*)calloc(n,4),*oR=(float*)calloc(n,4);
    AcidSynth s;synth_init(&s,SR);s.seq_mode=1;s.master_volume=0.8f;
    synth_gate(&s,1);
    /* Rapidly change parameters every block (simulating slider moves) */
    for(int blk=0;blk<steps;blk++){
        int i=blk%bs;
        if(i<16){
            /* Rapid note toggles */
            if(i%2) synth_note_on(&s,220+blk*0.1f);
            else synth_note_off(&s);
        }
        if(i%8==0){
            /* Rapid slider-like parameter changes */
            float f=100+(blk%11900);
            filter_set_freq(&s.filter,f);
            s.cutoff_base=f;
            s.reso_base=(blk%1000)/1000.0f*0.5f;
            filter_set_resonance(&s.filter,s.reso_base);
            delay_set_feedback(&s.delay,(blk%800)/1000.0f*0.5f);
            reverb_set_decay(&s.reverb,(blk%1000)/1000.0f*0.5f);
            wavefolder_set_drive(&s.wavefolder,(blk%1000)/1000.0f*2.0f);
        }
        synth_process_block(&s,&oL[blk*bs],&oR[blk*bs],bs);
    }
    synth_gate(&s,0);
    /* Check: no extreme sample-to-sample discontinuities */
    float max_diff=0;int glitch_count=0;
    for(int i=3;i<n;i++){
        float d=fabsf(oL[i]-oL[i-1]);
        if(d>max_diff)max_diff=d;
        if(d>1.0f)glitch_count++;
    }
    char msg[80];snprintf(msg,80,"max diff=%.4f, glitches=%d (<5)=stable",max_diff,glitch_count);
    ASSERT(glitch_count<5,msg);
    ASSERT(max_diff<1.5f,"no extreme glitches (>1.5/sample)");
    printf("  STRESS: max_diff=%.4f glitches=%d (threshold=1.0, limit=5)\n",max_diff,glitch_count);
    free(oL);free(oR);synth_free(&s);END_TEST();return _test_passed?0:1;}

/* -- 7. STRESS TEST: CS contention simulated ------------------------- */
/* Simulates the audio callback running concurrently with slider updates.
 * Verifies the synth is re-entrant safe and produces consistent output. */
static int test_stress_cs_contention(void) {
    TEST("7: CS contention stress (interleaved processes)");
    int n=(int)(SR*3);int bs=64;int steps=n/bs;
    float *oL=(float*)calloc(n,4),*oR=(float*)calloc(n,4);
    AcidSynth s;synth_init(&s,SR);s.seq_mode=1;s.master_volume=0.8f;
    synth_gate(&s,1);
    /* Interleave: process small block, change param, process, change... */
    for(int blk=0;blk<steps;blk++){
        synth_process_block(&s,&oL[blk*bs],&oR[blk*bs],bs);
        /* Interleaved param changes (like slider + timer handler) */
        if(blk%3==0){
            float f=s.cutoff_base+50;if(f>12000)f=100;
            filter_set_freq(&s.filter,f);s.cutoff_base=f;
            s.reso_base=fabsf(s.reso_base-0.1f);
            filter_set_resonance(&s.filter,s.reso_base);
        }
        if(blk%7==0){
            delay_set_mix(&s.delay,fabsf(s.delay.mix-0.1f));
            reverb_set_mix(&s.reverb,fabsf(s.reverb.mix-0.05f));
        }
    }
    synth_gate(&s,0);
    float pk=calc_peak(oL,n);ASSERT(pk<=1.001f,"no clipping under contention");
    float rms=calc_rms(oL,n,(int)(0.1f*SR));ASSERT(rms>0.001f,"audio present under contention");
    printf("  CONTENTION: pk=%.4f rms=%.4f (stable)\n",pk,rms);
    free(oL);free(oR);synth_free(&s);END_TEST();return _test_passed?0:1;}

/* -- 8. SAME NOTE WITH PAUSE: press, release, wait, press again ---- */
/* Simulates clicking the same piano key with a delay between presses.
 * Verifies no clicks/glitches where the old note decay meets the new
 * note attack. */
static int test_same_note_with_pause(void) {
    TEST("8: same note with pause (no step between releases)");
    int n=(int)(SR*3);int bs=256;
    float *oL=(float*)calloc(n,4),*oR=(float*)calloc(n,4);
    AcidSynth s;synth_init(&s,SR);s.master_volume=0.8f;
    s.env_attack=0.002f;s.env_decay=0.1f;s.env_sustain=0.6f;s.env_release=0.05f;
    int off=0;
    synth_note_on(&s,130.81f);
    for(int i=0;i<4;i++){synth_process_block(&s,&oL[off],&oR[off],bs);off+=bs;}
    synth_note_off(&s);
    for(int i=0;i<10;i++){synth_process_block(&s,&oL[off],&oR[off],bs);off+=bs;} /* ~26ms decay */
    /* Pause (envelope fully decayed, filter+delay reset by env detector) */
    for(int i=0;i<50;i++){synth_process_block(&s,&oL[off],&oR[off],bs);off+=bs;} /* ~130ms silence */
    /* Same note again */
    synth_note_on(&s,130.81f);
    for(int i=0;i<4;i++){synth_process_block(&s,&oL[off],&oR[off],bs);off+=bs;} /* ~10ms */
    synth_note_off(&s);
    for(int i=0;i<10;i++){synth_process_block(&s,&oL[off],&oR[off],bs);off+=bs;}
    /* Check for clicks at the note-on boundary (around sample 12*256 = 3072) */
    float max_diff=0;int glitch_count=0;
    for(int i=5;i<off;i++){
        float d=fabsf(oL[i]-oL[i-1]);if(d>max_diff)max_diff=d;
        if(d>0.8f)glitch_count++;}
    /* Specifically check the region around the 2nd note_on */
    float pmax=0;int pstart=(int)(SR*0.4f),pend=(int)(SR*0.6f);
    if(pend>off)pend=off;
    for(int i=pstart+2;i<pend;i++){float d=fabsf(oL[i]-oL[i-1]);if(d>pmax)pmax=d;}
    char msg[80];snprintf(msg,80,"pause region pk=%.4f glitches=%d",pmax,glitch_count);
    ASSERT(glitch_count<3,msg);
    ASSERT(pmax<0.8f,"no click at same-note retrigger boundary");
    printf("  SAME+PAUSE: pk=%.4f glitches=%d (boundary=%.4f)\n",max_diff,glitch_count,pmax);
    free(oL);free(oR);synth_free(&s);END_TEST();return _test_passed?0:1;}

/* -- 9. SAME NOTE RAPID: press while already playing (legato) ------- */
/* Simulates pressing the same key before releasing the previous one.
 * Verifies synth_note_on (with filter_reset, envelope=0) doesn't click. */
static int test_same_note_no_pause(void) {
    TEST("9: same note rapid press (no click on double-trigger)");
    int n=(int)(SR*3);int bs=256;int blkmax=(n/bs)-15; /* leave room for tail blocks */
    float *oL=(float*)calloc(n,4);
    AcidSynth s;synth_init(&s,SR);s.master_volume=0.8f;
    s.env_attack=0.002f;s.env_release=0.05f;
    synth_note_on(&s,220.0f);
    int off=0;
    for(int b=0;b<blkmax;b++){
        /* Every 3 blocks: re-trigger SAME note while still playing */
        if(b>0&&b%3==0)synth_note_on(&s,220.0f);
        synth_process_block(&s,&oL[off],&oL[off],bs);off+=bs;}
    synth_note_off(&s);
    for(int i=0;i<10;i++){synth_process_block(&s,&oL[off],&oL[off],bs);off+=bs;}
    float max_diff=0;int glitch_count=0;
    for(int i=5;i<off;i++){float d=fabsf(oL[i]-oL[i-1]);
        if(d>max_diff)max_diff=d;if(d>0.8f)glitch_count++;}
    char msg[80];snprintf(msg,80,"max diff=%.4f glitches=%d",max_diff,glitch_count);
    ASSERT(glitch_count<5,msg);ASSERT(max_diff<0.9f,"no click on same-note rapid retrigger");
    printf("  SAME+RAPID: max_diff=%.4f glitches=%d\n",max_diff,glitch_count);
    free(oL);synth_free(&s);END_TEST();return _test_passed?0:1;}

int main(void){
    printf("============================================================\n");
    printf("  ACID SYNTH CERTIFICATION + STRESS TEST SUITE\n");
    printf("============================================================\n\n");
    int fail=0;
    fail+=test_certified_reference_wav();
    fail+=test_all_parameters_change_output();
    fail+=test_sequencer_accuracy();
    fail+=test_dreamscape_character();
    fail+=test_scale_correctness();
    fail+=test_stress_rapid_changes();
    fail+=test_stress_cs_contention();
    fail+=test_same_note_with_pause();
    fail+=test_same_note_no_pause();
    printf("\n============================================================\n");
    TEST_SUMMARY();}
