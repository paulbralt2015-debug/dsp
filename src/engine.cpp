#include "engine.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
bool Settings::load(const std::string& p) {
    const char* f = p.c_str();
    listen = (int)GetPrivateProfileIntA("twins", "listen", 0, f);
    mix = (int)GetPrivateProfileIntA("twins", "mix", 0, f);
    drc = (int)GetPrivateProfileIntA("twins", "drc", 1, f);
    virt = (int)GetPrivateProfileIntA("twins", "virt", 1, f);
    bass = (int)GetPrivateProfileIntA("twins", "bass", 100, f) - 100;
    reverb = (int)GetPrivateProfileIntA("twins", "reverb", 0, f);
    centerTrim = (int)GetPrivateProfileIntA("twins", "center", 100, f) - 100;
    surTrim = (int)GetPrivateProfileIntA("twins", "surround", 100, f) - 100;
    surDelay = (int)GetPrivateProfileIntA("twins", "delay", 100, f) - 101;
    listen = std::min(9, std::max(0, listen)); mix = std::min(2, std::max(0, mix)); drc = std::min(3, std::max(0, drc));
    return true;
}
void Settings::save(const std::string& p) const {
    auto w = [&](const char* k, int v) { WritePrivateProfileStringA("twins", k, std::to_string(v).c_str(), p.c_str()); };
    w("listen", listen); w("mix", mix); w("drc", drc); w("virt", virt); w("bass", bass + 100); w("reverb", reverb);
    w("center", centerTrim + 100); w("surround", surTrim + 100); w("delay", surDelay + 101);
}
#else
bool Settings::load(const std::string&) { return false; }
void Settings::save(const std::string&) const {}
#endif

void DolbyEngine::configure(const Settings& s) {
    s_ = s;
    fxCh_ = 0;  // les effets sont recalculés au prochain bloc
}

void DolbyEngine::reset() {
    sur_.reset(); virt_.reset(); env_ = 0; fxCh_ = 0; curListen_ = -1;
}

void DolbyEngine::configureFx(int ch, int rate) {
    fxCh_ = ch; fxRate_ = rate;
    shelf_.assign(ch, Biquad());
    for (auto& b : shelf_) if (s_.bass != 0) b.lowshelf(140.0f, (float)rate, (float)s_.bass);
    static const double cm[4] = {29.7, 37.1, 41.1, 43.7}, am[2] = {5.0, 1.7};
    for (int s = 0; s < 2; s++) {
        for (int c = 0; c < 4; c++) { comb_[s][c].b.assign((size_t)(rate * (cm[c] + s * 0.7) / 1000.0) + 1, 0.0f); comb_[s][c].i = 0; comb_[s][c].z = 0; }
        for (int a = 0; a < 2; a++) { ap_[s][a].b.assign((size_t)(rate * (am[a] + s * 0.3) / 1000.0) + 1, 0.0f); ap_[s][a].i = 0; }
    }
}

void DolbyEngine::process(const float* in, size_t frames, int ch, int rate, bool multiOk, std::vector<float>& out, int& outCh) {
    std::vector<float> a;
    int oc = ch;
    Listen m = (Listen)s_.listen;
    float cm = std::pow(10.0f, s_.centerTrim / 20.0f), sm = std::pow(10.0f, s_.surTrim / 20.0f);
    if (ch == 2 && m != Listen::Standard) {
        if ((int)m != curListen_ || rate != surRate_ || s_.centerTrim != curCT_ || s_.surTrim != curST_ || s_.surDelay != curSD_) {
            sur_.setTrims(cm, sm, s_.surDelay);
            sur_.configure(m, rate);
            surRate_ = rate; curListen_ = (int)m; curCT_ = s_.centerTrim; curST_ = s_.surTrim; curSD_ = s_.surDelay; virtCh_ = 0;
        }
        sur_.process(in, frames, a);
        oc = sur_.outChannels();
    } else {
        a.assign(in, in + frames * ch);
    }
    bool discrete = multiOk && !s_.virt;
    if (oc > 2) {
        if (discrete) {
            if (oc == 8) for (size_t i = 0; i < frames; i++) {   // ordre hôte 7.1 : L R C LFE Lb Rb Ls Rs
                float* f = &a[i * 8];
                std::swap(f[4], f[6]); std::swap(f[5], f[7]);
            }
        } else {
            if (oc != virtCh_ || rate != virtRate_) { virt_.configure(oc, rate); virtCh_ = oc; virtRate_ = rate; }
            std::vector<float> st;
            virt_.process(a.data(), frames, st);
            a.swap(st); oc = 2;
        }
    }
    // effets : basses et salle
    if (s_.bass != 0 || s_.reverb > 0) {
        if (fxCh_ != oc || fxRate_ != rate) configureFx(oc, rate);
        float wet = s_.reverb / 100.0f * 0.35f;
        for (size_t i = 0; i < frames; i++) {
            float* f = &a[i * oc];
            if (s_.bass != 0) for (int c = 0; c < oc; c++) f[c] = shelf_[c].process(f[c]);
            if (wet > 0) {
                float inL = 0, inR = 0;
                for (int c = 0; c < oc; c++) {
                    if (oc >= 4 && c == 3) continue;
                    if (oc >= 3 && c == 2) { inL += 0.5f * f[c]; inR += 0.5f * f[c]; }
                    else if (c & 1) inR += f[c]; else inL += f[c];
                }
                float rl = 0, rr = 0;
                for (int c = 0; c < 4; c++) { rl += comb_[0][c].run(inL * 0.15f); rr += comb_[1][c].run(inR * 0.15f); }
                for (int p = 0; p < 2; p++) { rl = ap_[0][p].run(rl); rr = ap_[1][p].run(rr); }
                for (int c = 0; c < oc; c++) {
                    if (oc >= 4 && c == 3) continue;
                    if (oc >= 3 && c == 2) f[c] += wet * 0.5f * (rl + rr);
                    else f[c] += wet * ((c & 1) ? rr : rl);
                }
            }
        }
    }
    // compression de dynamique (DRC « films » / « mode nuit »)
    if (s_.drc >= 2) {
        bool night = s_.drc == 3;
        float thr = night ? 0.0079f : 0.0501f, ratio = night ? 10.0f : 3.0f, makeup = night ? 2.0f : 1.2f;
        float att = 1.0f - std::exp(-1.0f / (rate * (night ? 0.002f : 0.005f))), rel = 1.0f - std::exp(-1.0f / (rate * (night ? 0.15f : 0.25f)));
        for (size_t i = 0; i < frames; i++) {
            float* f = &a[i * oc], pk = 0;
            for (int c = 0; c < oc; c++) pk = std::max(pk, std::fabs(f[c]));
            env_ += (pk - env_) * (pk > env_ ? att : rel);
            float g = makeup;
            if (env_ > thr) g *= std::pow(env_ / thr, 1.0f / ratio - 1.0f);
            for (int c = 0; c < oc; c++) f[c] *= g;
        }
    }
    for (auto& x : a) x = x > 1.0f ? 1.0f : x < -1.0f ? -1.0f : x;
    out.swap(a);
    outCh = oc;
}
