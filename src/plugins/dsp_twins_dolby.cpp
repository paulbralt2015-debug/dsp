// Winamp — plugin DSP « Twins Dolby Surround / Pro Logic » : s'applique à n'importe quel fichier lu par Winamp
// (stéréo 16 bits). Les modes surround sont rendus en surround virtuel stéréo (le DSP ne peut pas changer le
// nombre de canaux). Réglages : bouton « Configurer le plug-in ».
#include <string>
#include <vector>

#include "../engine.h"
#include "configmenu.h"
#include "winamp_sdk.h"

namespace {

HINSTANCE hInst;
DolbyEngine engine;
Settings settings;
std::string iniPath;
char desc[] = "Twins Dolby Surround / Pro Logic (DSP)";
char modDesc[] = "Twins Dolby — modes Pro Logic, effets, mode nuit";

void loadSettings() {
    settings.load(iniPath);
    settings.virt = 1;
    engine.configure(settings);
}

void config(winampDSPModule* m) {
    loadSettings();
    if (showSettingsMenu(m->hwndParent, settings)) { settings.save(iniPath); loadSettings(); }
}

int init(winampDSPModule*) {
    char p[MAX_PATH];
    GetModuleFileNameA(hInst, p, MAX_PATH);
    iniPath = p;
    size_t dot = iniPath.find_last_of('.');
    iniPath = (dot == std::string::npos ? iniPath : iniPath.substr(0, dot)) + ".ini";
    loadSettings();
    engine.reset();
    return 0;
}

int modify(winampDSPModule*, short* samples, int n, int bps, int nch, int rate) {
    if (bps != 16 || nch != 2 || n <= 0) return n;
    static std::vector<float> in, out;
    in.resize((size_t)n * 2);
    for (int i = 0; i < n * 2; i++) in[i] = samples[i] / 32768.0f;
    int oc = 0;
    engine.process(in.data(), n, 2, rate, false, out, oc);
    if (oc != 2) return n;
    for (int i = 0; i < n * 2; i++) samples[i] = (short)std::lrintf(out[i] * 32767.0f);
    return n;
}

void quit(winampDSPModule*) {}

winampDSPModule module = {modDesc, nullptr, nullptr, config, init, modify, quit, nullptr};
winampDSPModule* getModule(int n) { if (n == 0) { module.hDllInstance = hInst; return &module; } return nullptr; }
winampDSPHeader header = {DSP_HDRVER, desc, getModule, nullptr};

}  // namespace

BOOL WINAPI DllMain(HINSTANCE h, DWORD, LPVOID) { hInst = h; return TRUE; }

extern "C" __declspec(dllexport) winampDSPHeader* winampDSPGetHeader2() { return &header; }
