// Winamp — plugin d'entrée « Twins Dolby Digital » : lit les fichiers .ac3 / .a52 avec le décodeur AC-3
// intégré, avec tous les modes d'écoute Dolby, les effets et le mode nuit (clic sur « Configurer »).
#include <atomic>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "../ac3file.h"
#include "../engine.h"
#include "configmenu.h"
#include "winamp_sdk.h"

namespace {

In_Module mod;
Ac3File file;
DolbyEngine engine;
Settings settings;
HANDLE thread = nullptr;
std::atomic<bool> killed{false}, paused{false};
std::atomic<int> seekTo{-1};
int decodedMs = 0, lengthMs = 0, rate = 48000, outCh = 2;
std::string iniPath;
char extList[] = "AC3;A52\0Dolby Digital (*.AC3;*.A52)\0";
char desc[] = "Twins Dolby Digital (AC-3) + Pro Logic";

void loadSettings() { settings.load(iniPath); }

DWORD WINAPI decodeThread(LPVOID) {
    std::vector<float> pcm, out;
    std::vector<short> s16;
    bool eof = false;
    while (!killed) {
        int st = seekTo.exchange(-1);
        if (st >= 0) {
            decodedMs = st; file.seek(st / 1000.0); engine.reset(); mod.outMod->Flush(decodedMs); eof = false;
        }
        if (eof) {
            if (!mod.outMod->IsPlaying()) { PostMessage(mod.hMainWindow, WM_WA_MPEG_EOF, 0, 0); return 0; }
            Sleep(10); continue;
        }
        if (paused || mod.outMod->CanWrite() < (int)(1536 * 6 * outCh * 2)) { Sleep(10); continue; }
        pcm.clear();
        size_t n = file.read(pcm);
        if (!n) { eof = true; continue; }
        int oc = 0;
        engine.process(pcm.data(), n, file.channels(), rate, false, out, oc);   // Winamp : sortie stéréo
        s16.resize(out.size() * 2);
        for (size_t i = 0; i < out.size(); i++) s16[i] = (short)std::lrintf(out[i] * 32767.0f);
        int samples = (int)n;
        if (mod.dsp_isactive()) samples = mod.dsp_dosamples(s16.data(), (int)n, 16, oc, rate);
        int bytes = samples * oc * 2;
        mod.SAAddPCMData(s16.data(), oc, 16, mod.outMod->GetWrittenTime());
        mod.VSAAddPCMData(s16.data(), oc, 16, mod.outMod->GetWrittenTime());
        mod.outMod->Write((char*)s16.data(), bytes);
        decodedMs += (int)(n * 1000LL / rate);
    }
    return 0;
}

void config(HWND hwnd) {
    loadSettings();
    if (showSettingsMenu(hwnd, settings)) { settings.save(iniPath); loadSettings(); }
}

void about(HWND hwnd) {
    MessageBoxA(hwnd, "Twins Dolby Digital\n\nDécodeur AC-3 intégré, modes Dolby Surround / Pro Logic / Pro Logic II / IIx,\nmode nuit, effets audio. Réglages : bouton « Configurer ».", "À propos", MB_OK);
}

void init() {
    char p[MAX_PATH];
    GetModuleFileNameA(mod.hDllInstance, p, MAX_PATH);
    iniPath = p;
    size_t dot = iniPath.find_last_of('.');
    iniPath = (dot == std::string::npos ? iniPath : iniPath.substr(0, dot)) + ".ini";
    loadSettings();
}

void quit() {}

int isOurFile(const char*) { return 0; }

int play(const char* fn) {
    loadSettings();
    if (!file.openFile(fn)) return 1;
    file.configure(settings.mixMode(), settings.drcScale());
    file.seek(0);
    engine.configure(settings); engine.reset();
    rate = file.sampleRate();
    outCh = 2;
    lengthMs = (int)(file.duration() * 1000);
    decodedMs = 0; killed = false; paused = false; seekTo = -1;
    int maxlat = mod.outMod->Open(rate, outCh, 16, -1, -1);
    if (maxlat < 0) return 1;
    mod.SetInfo(file.kbps(), rate / 1000, outCh, 1);
    mod.SAVSAInit(maxlat, rate);
    mod.VSASetInfo(outCh, rate);
    mod.outMod->SetVolume(-666);
    thread = CreateThread(nullptr, 0, decodeThread, nullptr, 0, nullptr);
    return 0;
}

void pause() { paused = true; mod.outMod->Pause(1); }
void unpause() { paused = false; mod.outMod->Pause(0); }
int isPaused() { return paused ? 1 : 0; }

void stop() {
    if (thread) {
        killed = true;
        WaitForSingleObject(thread, 5000);
        CloseHandle(thread);
        thread = nullptr;
    }
    mod.outMod->Close();
    mod.SAVSADeInit();
}

int getLength() { return lengthMs; }
int getOutputTime() { return decodedMs + (mod.outMod->GetOutputTime() - mod.outMod->GetWrittenTime()); }
void setOutputTime(int ms) { seekTo = ms; }
void setVolume(int v) { mod.outMod->SetVolume(v); }
void setPan(int p) { mod.outMod->SetPan(p); }

void getFileInfo(const char* fn, char* title, int* length) {
    if (!fn || !*fn) { *length = lengthMs; if (title) *title = 0; return; }   // fichier en cours
    Ac3File f;
    if (!f.openFile(fn)) { if (length) *length = 0; return; }
    if (length) *length = (int)(f.duration() * 1000);
    if (title) {
        const char* b = strrchr(fn, '\\');
        strncpy(title, b ? b + 1 : fn, 255); title[255] = 0;
    }
}

int infoBox(const char*, HWND) { return 0; }
void eqSet(int, char*, int) {}

}  // namespace

extern "C" __declspec(dllexport) In_Module* winampGetInModule2() {
    mod.version = IN_VER;
    mod.description = desc;
    mod.FileExtensions = extList;
    mod.is_seekable = 1;
    mod.UsesOutputPlug = 1;
    mod.Config = config; mod.About = about; mod.Init = init; mod.Quit = quit;
    mod.GetFileInfo = getFileInfo; mod.InfoBox = infoBox; mod.IsOurFile = isOurFile;
    mod.Play = play; mod.Pause = pause; mod.UnPause = unpause; mod.IsPaused = isPaused; mod.Stop = stop;
    mod.GetLength = getLength; mod.GetOutputTime = getOutputTime; mod.SetOutputTime = setOutputTime;
    mod.SetVolume = setVolume; mod.SetPan = setPan; mod.EQSet = eqSet;
    return &mod;
}
