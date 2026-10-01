/*
 * ui_win32.c — Acid Synth v4 Dreamscape
 * Win32 GUI with waveOut audio via CALLBACK_FUNCTION.
 * Piano keys: polled via GetAsyncKeyState in WM_TIMER (bypasses edit box focus).
 * Sliders: TBM_SETPAGESIZE=10 so channel-click moves 1% of range.
 * Edit boxes: write value, press Enter or click elsewhere to apply.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <commctrl.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "synth.h"
#include "scale.h"

#define SR 44100
#define NP 9
#define EB 20000
#define NW 14
#define NB 10
#define KY 360
#define KW 28
#define KH 80
#define BH 54
#define BW 18

static CRITICAL_SECTION cs;
static AcidSynth gn;
static volatile int go=1;
static WAVEHDR wh[6];
static short wd[6][1024];
static HWAVEOUT wo;
static const char*pn[]={"Cutoff","Reso","DlyMix","DlyFb","RevMix","RevDcy","Vol","Rate","Drive"};
static float pmin[]={100,0,0,0,0,0,0,0.5f,0};
static float pmax[]={12000,1,0.5f,0.8f,0.5f,1,1,8.5f,10};
static const char*pfmt[]={"%.0f","%.3f","%.3f","%.3f","%.3f","%.3f","%.3f","%.3f","%.3f"};
static HWND sl[9],ev[9],sus,eq,sc,rt;
static int ks[24],kp[24];
static int wm[]={'Z','X','C','V','B','N','M','Q','E','T','Y','U','I','P'};
static int wmi[]={48,50,52,53,55,57,59,60,62,64,65,67,69,71};
static int bm[]={'S','D','G','H','J','W','R','F','O','A'};
static int bmi[]={49,51,54,56,58,61,63,66,68,70};
static int cm=-1,sm=-1,gdk=-1,sw=0,grd=0;
static float sp=0;
static UINT_PTR tm;

/* MIDI input globals */
static HMIDIIN g_midi_in=NULL;
static int g_midi_dev=-1;

static float p2v(int i,int p){return pmin[i]+(p/1000.0f)*(pmax[i]-pmin[i]);}
static int v2s(int i,float v){if(v<pmin[i])v=pmin[i];if(v>pmax[i])v=pmax[i];
    return(int)((v-pmin[i])/(pmax[i]-pmin[i])*1000.0f+0.5f);}
static void upd(int i){
    int p=(int)SendMessage(sl[i],TBM_GETPOS,0,0);float v=p2v(i,p);
    char b[32];_snprintf(b,32,pfmt[i],v);SetWindowText(ev[i],b);}
static void app(int i){
    int p=(int)SendMessage(sl[i],TBM_GETPOS,0,0);float v=p2v(i,p);
    EnterCriticalSection(&cs);
    switch(i){case 0:gn.cutoff_base=v;filter_set_freq(&gn.filter,v);break;
    case 1:gn.reso_base=v;filter_set_resonance(&gn.filter,v);break;
    case 2:delay_set_mix(&gn.delay,v);break;case 3:delay_set_feedback(&gn.delay,v);break;
    case 4:reverb_set_mix(&gn.reverb,v);break;case 5:reverb_set_decay(&gn.reverb,v);break;
    case 6:gn.master_volume=v;break;case 7:synth_set_seq_rate(&gn,v);break;
    case 8:wavefolder_set_drive(&gn.wavefolder,v);break;}
    LeaveCriticalSection(&cs);upd(i);}
static void sk(int i,int o){
    if(i<0||i>=24)return;ks[i]=o?1:0;
    if(o){int m=i<NW?wmi[i]:bmi[i-NW];
        if(sm>=0){sm=-1;}  /* Legato: non fare note_off, solo cambia frequenza — continuità */
        float f=440.0f*powf(2.0f,(m-69.0f)/12.0f);
        EnterCriticalSection(&cs);synth_note_on(&gn,f);cm=m;LeaveCriticalSection(&cs);}
    else{int sbt=IsDlgButtonChecked(GetParent(sus),2003);
        if(sbt){sm=i<NW?wmi[i]:bmi[i-NW];}else{if(cm>=0){EnterCriticalSection(&cs);synth_note_off(&gn);cm=-1;LeaveCriticalSection(&cs);}}}}
static void aps(){
    int s=(int)SendMessage(sc,CB_GETCURSEL,0,0);if(s<0)s=0;
    int r=(int)SendMessage(rt,CB_GETCURSEL,0,0);if(r<0)r=12;
    EnterCriticalSection(&cs);synth_set_scale(&gn,(ScaleType)s);synth_set_scale_root(&gn,
        (float[]){65.41f,69.30f,73.42f,77.78f,82.41f,87.31f,92.50f,98.00f,103.83f,110.00f,
        116.54f,123.47f,130.81f,138.59f,146.83f,155.56f,164.81f,174.61f,185.00f,196.00f,
        207.65f,220.00f,233.08f,246.94f,261.63f}[r]);LeaveCriticalSection(&cs);}
static void rst(HWND h){
    EnterCriticalSection(&cs);synth_free(&gn);synth_init(&gn,SR);
    gn.master_volume=0.8f;gn.seq_mode=0;LeaveCriticalSection(&cs);
    for(int i=0;i<9;i++){
        static int d[]={118,250,600,562,800,750,500,187,0};
        SendMessage(sl[i],TBM_SETPOS,1,d[i]);app(i);}
    SendMessage(sc,CB_SETCURSEL,2,0);SendMessage(rt,CB_SETCURSEL,12,0);aps();
    CheckDlgButton(h,2002,BST_UNCHECKED);CheckDlgButton(h,2003,BST_UNCHECKED);
    CheckDlgButton(h,2000,BST_UNCHECKED);CheckDlgButton(h,2005,BST_CHECKED);
    memset(kp,0,sizeof(kp));sw=0;sm=-1;cm=-1;}
static void drk(HDC h,HWND hw){
    RECT r;GetClientRect(hw,&r);int x0=(r.right-NW*KW)/2;
    for(int i=0;i<NW;i++){int x=x0+i*KW;
        HBRUSH b=CreateSolidBrush(ks[i]?0x00B4C8FF:0x00FFFFFF);
        SelectObject(h,b);Rectangle(h,x,KY,x+KW,KY+KH);DeleteObject(b);
        SetTextAlign(h,TA_CENTER);SetBkMode(h,TRANSPARENT);
        char l[8];_snprintf(l,8,"%s%d",(char*[]){"C","D","E","F","G","A","B",
            "C","D","E","F","G","A","B"}[i],wmi[i]/12-1);
        TextOut(h,x+KW/2,KY+60,l,(int)strlen(l));}
    int bo[10]={0,1,3,4,5,7,8,10,11,12};
    for(int i=0;i<NB;i++){int wi=bo[i],x=x0+wi*KW+KW-BW;
        HBRUSH b=CreateSolidBrush(ks[NW+i]?0x00C88264:0x00000000);
        SelectObject(h,b);Rectangle(h,x,KY,x+BW,KY+BH);DeleteObject(b);}}
static int hk(int mx,int my){
    int x0=(680-NW*KW)/2;if(my<KY||my>KY+KH)return-1;
    int bo[10]={0,1,3,4,5,7,8,10,11,12};
    /* Black keys have priority in their top portion */
    for(int i=0;i<NB;i++){int wi=bo[i],x=x0+wi*KW+KW-BW;
        if(mx>=x&&mx<x+BW&&my<KY+BH)return NW+i;}
    /* White keys everywhere (full height) */
    for(int i=0;i<NW;i++){int x=x0+i*KW;if(mx>=x&&mx<x+KW)return i;}
    return-1;}
static void fb(short*d,int n){
    float l[256],r[256];EnterCriticalSection(&cs);synth_process_block(&gn,l,r,n);LeaveCriticalSection(&cs);
    for(int i=0;i<n;i++){d[i*2]=(short)(l[i]*32767.0f);d[i*2+1]=(short)(r[i]*32767.0f);}}
static void CALLBACK wcb(HWAVEOUT h,UINT m,DWORD_PTR a,DWORD_PTR p1,DWORD_PTR p2){
    (void)a;(void)p2;if(m==WOM_DONE&&go){WAVEHDR*d=(WAVEHDR*)p1;
    waveOutUnprepareHeader(h,d,sizeof(WAVEHDR));fb((short*)d->lpData,256);
    waveOutPrepareHeader(h,d,sizeof(WAVEHDR));waveOutWrite(h,d,sizeof(WAVEHDR));}}
static void sa(){
    WAVEFORMATEX w={0};w.wFormatTag=WAVE_FORMAT_PCM;w.nChannels=2;w.nSamplesPerSec=SR;w.wBitsPerSample=16;
    w.nBlockAlign=4;w.nAvgBytesPerSec=SR*4;
    if(waveOutOpen(&wo,WAVE_MAPPER,&w,(DWORD_PTR)wcb,0,CALLBACK_FUNCTION)!=MMSYSERR_NOERROR)return;
    for(int i=0;i<6;i++){wh[i].lpData=(LPSTR)wd[i];wh[i].dwBufferLength=1024;wh[i].dwFlags=0;
    fb(wd[i],256);waveOutPrepareHeader(wo,&wh[i],sizeof(WAVEHDR));waveOutWrite(wo,&wh[i],sizeof(WAVEHDR));}}
static void spa(){go=0;if(wo){waveOutReset(wo);
    for(int i=0;i<6;i++)waveOutUnprepareHeader(wo,&wh[i],sizeof(WAVEHDR));waveOutClose(wo);wo=0;}}

/* ----------------------------------------------------------------- */
/*  MIDI Input                                                        */
/* ----------------------------------------------------------------- */
static void CALLBACK midi_proc(HMIDIIN h,UINT msg,DWORD_PTR inst,DWORD_PTR data,DWORD_PTR ts){
    (void)h;(void)inst;(void)ts;
    if(msg==MIM_DATA){
        int status=(int)(data&0xFF);
        int note=(int)((data>>8)&0x7F);
        int vel=(int)((data>>16)&0x7F);
        int cmd=status&0xF0;
        if(cmd==0x90&&vel>0){
            float f=440.0f*powf(2.0f,(float)(note-69)/12.0f);
            EnterCriticalSection(&cs);synth_note_on(&gn,f);cm=note;LeaveCriticalSection(&cs);
        }else if(cmd==0x80||(cmd==0x90&&vel==0)){
            EnterCriticalSection(&cs);synth_note_off(&gn);cm=-1;LeaveCriticalSection(&cs);
        }
    }
}

static void midi_open(int dev){
    if(g_midi_in){midiInClose(g_midi_in);g_midi_in=NULL;}
    if(dev<0)return;
    if(midiInOpen(&g_midi_in,dev,(DWORD_PTR)midi_proc,0,CALLBACK_FUNCTION)!=MMSYSERR_NOERROR){
        g_midi_in=NULL;return;}
    midiInStart(g_midi_in);g_midi_dev=dev;
}

static void midi_close(){
    if(g_midi_in){midiInStop(g_midi_in);midiInClose(g_midi_in);g_midi_in=NULL;g_midi_dev=-1;}
}

/* ----------------------------------------------------------------- */
/*  WAV Loader for Attack Layer                                       */
/* ----------------------------------------------------------------- */
static void load_attack_wav(HWND h){
    OPENFILENAMEA ofn={0};char path[260]={0};
    ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=h;
    ofn.lpstrFilter="WAV Files\0*.wav\0All Files\0*.*\0";
    ofn.lpstrFile=path;ofn.nMaxFile=sizeof(path);
    ofn.Flags=OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;
    if(!GetOpenFileNameA(&ofn))return;
    /* Parse simple WAV — PCM, any bit depth/channels */
    FILE*fp=fopen(path,"rb");if(!fp)return;
    char id[4];uint32_t sz;uint16_t fmt,ch;uint32_t sr,brate;uint16_t bps,bsize;
    fread(id,1,4,fp);fread(&sz,4,1,fp);fread(id,1,4,fp); /* RIFF + size + WAVE */
    fread(id,1,4,fp);fread(&sz,4,1,fp); /* "fmt " + chunk size */
    fread(&fmt,2,1,fp);fread(&ch,2,1,fp);fread(&sr,4,1,fp);fread(&brate,4,1,fp);
    fread(&bsize,2,1,fp);fread(&bps,2,1,fp);
    if(fmt!=1){fclose(fp);return;} /* only PCM */
    /* Skip any extra fmt bytes */
    if(sz>16)fseek(fp,sz-16,SEEK_CUR);
    /* Find data chunk */
    while(fread(id,1,4,fp)==4){
        fread(&sz,4,1,fp);
        if(memcmp(id,"data",4)==0)break;
        fseek(fp,sz,SEEK_CUR);
    }
    if(sz==0||feof(fp)){fclose(fp);return;}
    /* Read up to 1.5s of samples */
    int max_frames=(int)(sr*1.5f);
    int read_bytes=(sz>max_frames*ch*2)?max_frames*ch*2:(int)sz;
    unsigned char*buf=(unsigned char*)malloc(read_bytes);
    fread(buf,1,read_bytes,fp);fclose(fp);
    int total_frames=read_bytes/(ch*(bps/8));
    if(total_frames>max_frames)total_frames=max_frames;
    /* Convert to float mono */
    float*samples=(float*)malloc(total_frames*sizeof(float));
    for(int i=0;i<total_frames;i++){
        double s=0;
        for(int c=0;c<ch;c++){
            int src_idx=i*ch*(bps/8)+c*(bps/8);
            if(bps==16){
                short val=*(short*)(buf+src_idx);
                s+=(double)val/32768.0;
            }else if(bps==8){
                s+=(double)(buf[src_idx]-128)/128.0;
            }else if(bps==24){
                int val=(buf[src_idx]|(buf[src_idx+1]<<8)|(buf[src_idx+2]<<16));
                if(val&0x800000)val|=0xFF000000;
                s+=(double)val/8388608.0;
            }
        }
        samples[i]=(float)(s/ch);
    }
    free(buf);
    EnterCriticalSection(&cs);
    attack_load(&gn.attack,samples,total_frames,(int)sr,440.0f);
    gn.attack.enabled=1;
    LeaveCriticalSection(&cs);
    free(samples);
    CheckDlgButton(h,2007,BST_CHECKED);
    /* Show filename in static text */
    SetDlgItemTextA(h,2010,path);
}

/* Edit box: read text, validate, apply */
static void apply_edit(int pi){
    char b[64];GetWindowText(ev[pi],b,64);
    if(strlen(b)==0)return;
    char*e=0;float v=strtof(b,&e);
    if(e==b||*e!=0)return;
    if(v>pmax[pi])v=pmax[pi];if(v<pmin[pi])v=pmin[pi];
    int pt=v2s(pi,v);SendMessage(sl[pi],TBM_SETPOS,1,pt);
    EnterCriticalSection(&cs);
    switch(pi){case 0:gn.cutoff_base=v;filter_set_freq(&gn.filter,v);break;
    case 1:gn.reso_base=v;filter_set_resonance(&gn.filter,v);break;
    case 2:delay_set_mix(&gn.delay,v);break;case 3:delay_set_feedback(&gn.delay,v);break;
    case 4:reverb_set_mix(&gn.reverb,v);break;case 5:reverb_set_decay(&gn.reverb,v);break;
    case 6:gn.master_volume=v;break;case 7:synth_set_seq_rate(&gn,v);break;
    case 8:wavefolder_set_drive(&gn.wavefolder,v);break;}
    LeaveCriticalSection(&cs);
    upd(pi);}
static void hw_edit(HWND h,int pi,int cd){
    (void)h;if(!grd)return;
    if(cd==EN_KILLFOCUS){apply_edit(pi);}
    else if(cd==EN_SETFOCUS){SendMessage(ev[pi],EM_SETSEL,0,-1);}}

/* Poll piano keys via GetAsyncKeyState — bypasses edit-control focus issue */
static void poll_keys(HWND h){
    for(int i=0;i<NW;i++){
        int down=!!(GetAsyncKeyState(wm[i])&0x8000);
        if(down&&!kp[i]){sk(i,1);InvalidateRect(h,0,0);}
        else if(!down&&kp[i]){sk(i,0);InvalidateRect(h,0,0);}
        kp[i]=down;}
    for(int i=0;i<NB;i++){
        int down=!!(GetAsyncKeyState(bm[i])&0x8000);
        int idx=NW+i;
        if(down&&!kp[idx]){sk(idx,1);InvalidateRect(h,0,0);}
        else if(!down&&kp[idx]){sk(idx,0);InvalidateRect(h,0,0);}
        kp[idx]=down;}}

LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_CREATE:{int y=5;
        for(int i=0;i<9;i++){
            CreateWindowA("STATIC",pn[i],WS_CHILD|WS_VISIBLE,5,y,100,18,h,0,0,0);
            sl[i]=CreateWindowA("msctls_trackbar32","",WS_CHILD|WS_VISIBLE,110,y-2,210,22,h,(HMENU)(1000+i),0,0);
            SendMessage(sl[i],TBM_SETRANGE,1,MAKELONG(0,1000));
            SendMessage(sl[i],TBM_SETPAGESIZE,0,10);
            ev[i]=CreateWindowA("EDIT","",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_LEFT|ES_AUTOHSCROLL,330,y,70,18,h,(HMENU)(EB+i),0,0);
            SendMessage(ev[i],EM_SETLIMITTEXT,10,0);y+=26;}
        y+=4;CreateWindowA("STATIC","Scale:",WS_CHILD|WS_VISIBLE,5,y,50,20,h,0,0,0);
        sc=CreateWindowA("COMBOBOX","",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,55,y,220,300,h,(HMENU)3000,0,0);
        for(int i=0;i<SCALE_COUNT;i++)SendMessage(sc,CB_ADDSTRING,0,(LPARAM)scale_names[i]);
        SendMessage(sc,CB_SETCURSEL,2,0);
        CreateWindowA("STATIC","Root:",WS_CHILD|WS_VISIBLE,290,y,40,20,h,0,0,0);
        rt=CreateWindowA("COMBOBOX","",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST|WS_VSCROLL,330,y,90,300,h,(HMENU)3001,0,0);
        for(int i=0;i<25;i++)SendMessage(rt,CB_ADDSTRING,0,(LPARAM)(char*[]){"C2","C#2","D2","D#2","E2",
            "F2","F#2","G2","G#2","A2","A#2","B2","C3","C#3","D3","D#3","E3","F3","F#3","G3",
            "G#3","A3","A#3","B3","C4"}[i]);
        SendMessage(rt,CB_SETCURSEL,12,0);y+=24;
        eq=CreateWindowA("BUTTON","Eq Temp",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,400,y,70,22,h,(HMENU)2005,0,0);
        CheckDlgButton(h,2005,BST_CHECKED);
        CreateWindowA("BUTTON","Demo",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,5,y,60,22,h,(HMENU)2002,0,0);
        sus=CreateWindowA("BUTTON","Sustain",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,70,y,70,22,h,(HMENU)2003,0,0);
        CreateWindowA("BUTTON","Sweep",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,145,y,65,22,h,(HMENU)2000,0,0);
        CreateWindowA("BUTTON","Reset",WS_CHILD|WS_VISIBLE,220,y,55,22,h,(HMENU)2004,0,0);
        CreateWindowA("BUTTON","Quit",WS_CHILD|WS_VISIBLE,330,y,60,22,h,(HMENU)2001,0,0);y+=28;
        /* Row: MIDI + Attack controls */
        CreateWindowA("BUTTON","MIDI",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,5,y,55,22,h,(HMENU)2006,0,0);
        CreateWindowA("BUTTON","Attack",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,65,y,65,22,h,(HMENU)2007,0,0);
        CreateWindowA("BUTTON","Load WAV",WS_CHILD|WS_VISIBLE,135,y,75,22,h,(HMENU)2008,0,0);
        /* WAV path display */
        CreateWindowA("STATIC","(no attack WAV loaded)",WS_CHILD|WS_VISIBLE,215,y,420,22,h,(HMENU)2010,0,0);
        y+=26;
        CreateWindowA("STATIC","Z:C3 X:D3 C:E3 V:F3 B:G3 N:A3 M:B3  |  Q:C4 W:C# E:D4 R:D# T:E4 Y:F4 U:G4 I:A4 O:G# P:B4  S/D/G/H/J=C#/D#/F#/G#/A#  F=F#4  [MIDI in attesa su dispositivo #0]",
            WS_CHILD|WS_VISIBLE,5,y,655,14,h,0,0,0);
        synth_init(&gn,SR);gn.master_volume=0.8f;gn.seq_mode=0;
        int dd[]={118,250,600,562,800,750,500,187,0};
        for(int i=0;i<9;i++){SendMessage(sl[i],TBM_SETPOS,1,dd[i]);}
        for(int i=0;i<9;i++){app(i);}
        aps();grd=1;memset(kp,0,sizeof(kp));
        tm=SetTimer(h,1,30,0);sa();break;}
    case WM_HSCROLL:
        if((HWND)l)for(int i=0;i<9;i++)if(sl[i]==(HWND)l){app(i);break;}
        break;
    case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);drk(dc,h);EndPaint(h,&ps);break;}
    case WM_LBUTTONDOWN:{int mx=(short)LOWORD(l),my=(short)HIWORD(l);int idx=hk(mx,my);
        if(idx>=0){gdk=idx;sk(idx,1);InvalidateRect(h,0,0);SetFocus(h);}break;}
    case WM_MOUSEMOVE:{if(gdk>=0&&(w&MK_LBUTTON)){int mx=(short)LOWORD(l),my=(short)HIWORD(l);int idx=hk(mx,my);
        if(idx!=gdk){sk(gdk,0);gdk=-1;if(idx>=0){gdk=idx;sk(idx,1);}InvalidateRect(h,0,0);}}break;}
    case WM_LBUTTONUP:{if(gdk>=0){sk(gdk,0);gdk=-1;InvalidateRect(h,0,0);}break;}
    case WM_KEYDOWN:{int vk=(int)w;if(vk==VK_RETURN){HWND f=GetFocus();for(int i=0;i<9;i++){if(f==ev[i]){apply_edit(i);break;}}break;}
        /* poll_keys in WM_TIMER handles the actual note triggers */ break;}
    case WM_KEYUP:{break;}
    case WM_COMMAND:{
        int id=(int)LOWORD(w);
        if(id>=EB&&id<EB+NP){hw_edit(h,id-EB,(int)HIWORD(w));break;}
        switch(id){
        case 2000:sw=IsDlgButtonChecked(h,2000);break;
        case 2001:DestroyWindow(h);PostQuitMessage(0);break;
        case 2002:{int e=IsDlgButtonChecked(h,2002);
            EnterCriticalSection(&cs);gn.seq_mode=e?1:0;if(!e){synth_note_off(&gn);cm=-1;}LeaveCriticalSection(&cs);break;}
        case 2003:{int s=IsDlgButtonChecked(h,2003);if(!s&&sm>=0){
            if(cm>=0){EnterCriticalSection(&cs);synth_note_off(&gn);cm=-1;LeaveCriticalSection(&cs);}sm=-1;}break;}
        case 2005:{int e=IsDlgButtonChecked(h,2005);
            EnterCriticalSection(&cs);synth_set_equal_temp(&gn,e?1:0);LeaveCriticalSection(&cs);break;}
        case 2004:rst(h);break;
        case 2006:{int m=IsDlgButtonChecked(h,2006);
            if(m){midiInGetNumDevs()>0?midi_open(0):(void)0;}else{midi_close();}break;}
        case 2007:{int a=IsDlgButtonChecked(h,2007);
            EnterCriticalSection(&cs);gn.attack.enabled=a;LeaveCriticalSection(&cs);break;}
        case 2008:load_attack_wav(h);break;
        case 3000:case 3001:if(HIWORD(w)==CBN_SELCHANGE)aps();break;}
        break;}
    case WM_TIMER:{
        poll_keys(h);
        if(sw&&gn.seq_mode){sp+=0.003f;if(sp>1.0f)sp=0.0f;
        float f=100.0f+powf(sp*79.0f+1.0f,2.0f)*10.0f;if(f>12000.0f)f=12000.0f;
        EnterCriticalSection(&cs);filter_set_freq(&gn.filter,f);LeaveCriticalSection(&cs);
        SendMessage(sl[0],TBM_SETPOS,1,(int)(((f-100.0f)/11900.0f)*1000.0f));app(0);}break;}
    case WM_SIZE:{
        RECT r;GetClientRect(h,&r);r.top=KY;InvalidateRect(h,&r,1);break;}
    case WM_DESTROY:midi_close();spa();if(tm)KillTimer(h,tm);
        DeleteCriticalSection(&cs);synth_free(&gn);PostQuitMessage(0);break;
    default:return DefWindowProc(h,m,w,l);}
    return 0;}

int WINAPI WinMain(HINSTANCE hI,HINSTANCE hP,LPSTR c,int s){
    (void)hP;(void)c;
    InitCommonControls();InitializeCriticalSection(&cs);
    WNDCLASS wc={0};wc.lpfnWndProc=WndProc;wc.hInstance=hI;
    wc.hCursor=LoadCursor(0,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
    wc.lpszClassName="AcidSynth4";
    if(!RegisterClass(&wc))return 1;
    RECT rc={0,0,680,546};AdjustWindowRect(&rc,WS_OVERLAPPEDWINDOW,0);
    HWND h=CreateWindowA("AcidSynth4","Acid Synth v4 -- Dreamscape",
        WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,
        rc.right-rc.left,rc.bottom-rc.top,0,0,hI,0);
    if(!h)return 1;
    MSG m;while(GetMessage(&m,0,0,0)){TranslateMessage(&m);DispatchMessage(&m);}
    return (int)m.wParam;}
