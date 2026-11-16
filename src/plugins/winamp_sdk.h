// Déclarations minimales de l'API de plugins Winamp 2/5 (reproduites d'après in2.h, out.h et dsp.h du SDK Winamp).
#pragma once
#include <windows.h>

#define IN_VER 0x100
#define WM_WA_MPEG_EOF (WM_USER + 2)

struct Out_Module {
    int version;
    char* description;
    int id;
    HWND hMainWindow;
    HINSTANCE hDllInstance;
    void (*Config)(HWND hwndParent);
    void (*About)(HWND hwndParent);
    void (*Init)();
    void (*Quit)();
    int (*Open)(int samplerate, int numchannels, int bitspersamp, int bufferlenms, int prebufferms);
    void (*Close)();
    int (*Write)(char* buf, int len);
    int (*CanWrite)();
    int (*IsPlaying)();
    int (*Pause)(int pause);
    void (*SetVolume)(int volume);
    void (*SetPan)(int pan);
    void (*Flush)(int t);
    int (*GetOutputTime)();
    int (*GetWrittenTime)();
};

struct In_Module {
    int version;
    char* description;
    HWND hMainWindow;
    HINSTANCE hDllInstance;
    char* FileExtensions;
    int is_seekable;
    int UsesOutputPlug;
    void (*Config)(HWND hwndParent);
    void (*About)(HWND hwndParent);
    void (*Init)();
    void (*Quit)();
    void (*GetFileInfo)(const char* file, char* title, int* length_in_ms);
    int (*InfoBox)(const char* file, HWND hwndParent);
    int (*IsOurFile)(const char* fn);
    int (*Play)(const char* fn);
    void (*Pause)();
    void (*UnPause)();
    int (*IsPaused)();
    void (*Stop)();
    int (*GetLength)();
    int (*GetOutputTime)();
    void (*SetOutputTime)(int time_in_ms);
    void (*SetVolume)(int volume);
    void (*SetPan)(int pan);
    void (*SAVSAInit)(int maxlatency_in_ms, int srate);
    void (*SAVSADeInit)();
    void (*SAAddPCMData)(void* PCMData, int nch, int bps, int timestamp);
    int (*SAGetMode)();
    void (*SAAdd)(void* data, int timestamp, int csa);
    void (*VSAAddPCMData)(void* PCMData, int nch, int bps, int timestamp);
    int (*VSAGetMode)(int* specNch, int* waveNch);
    void (*VSAAdd)(void* data, int timestamp);
    void (*VSASetInfo)(int nch, int srate);
    int (*dsp_isactive)();
    int (*dsp_dosamples)(short int* samples, int numsamples, int bps, int nch, int srate);
    void (*EQSet)(int on, char data[10], int preamp);
    void (*SetInfo)(int bitrate, int srate, int stereo, int synched);
    Out_Module* outMod;
};

// Plugin DSP
struct winampDSPModule {
    char* description;
    HWND hwndParent;
    HINSTANCE hDllInstance;
    void (*Config)(winampDSPModule* this_mod);
    int (*Init)(winampDSPModule* this_mod);
    int (*ModifySamples)(winampDSPModule* this_mod, short int* samples, int numsamples, int bps, int nch, int srate);
    void (*Quit)(winampDSPModule* this_mod);
    void* userData;
};

struct winampDSPHeader {
    int version;
    char* description;
    winampDSPModule* (*getModule)(int);
    int (*sf)(int key);
};
#define DSP_HDRVER 0x20
