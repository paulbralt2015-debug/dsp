#include "surround.h"

#include <algorithm>
#include <cmath>

namespace {
const float PI_F = 3.14159265358979f;
}

void Biquad::lowpass(float fc, float fs) {
    float w = 2 * PI_F * fc / fs, c = std::cos(w), al = std::sin(w) / (2 * 0.7071f), a0 = 1 + al;
    b0 = (1 - c) / 2 / a0; b1 = (1 - c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
}
void Biquad::highpass(float fc, float fs) {
    float w = 2 * PI_F * fc / fs, c = std::cos(w), al = std::sin(w) / (2 * 0.7071f), a0 = 1 + al;
    b0 = (1 + c) / 2 / a0; b1 = -(1 + c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
}

void Biquad::lowshelf(float fc, float fs, float gdb) {
    float A = std::pow(10.0f, gdb / 40), w = 2 * PI_F * fc / fs, cs = std::cos(w), sn = std::sin(w);
    float al = sn / 2 * std::sqrt(2.0f), sa = 2 * std::sqrt(A) * al;
    float a0 = (A + 1) + (A - 1) * cs + sa;
    b0 = A * ((A + 1) - (A - 1) * cs + sa) / a0; b1 = 2 * A * ((A - 1) - (A + 1) * cs) / a0; b2 = A * ((A + 1) - (A - 1) * cs - sa) / a0;
    a1 = -2 * ((A - 1) + (A + 1) * cs) / a0; a2 = ((A + 1) + (A - 1) * cs - sa) / a0;
}

void Surround::configure(Listen m, int rate) {
    mode_ = m; rate_ = rate;
    back_ = m == Listen::PL2xMovie || m == Listen::PL2xMusic || m == Listen::PL2xGame;
    steer_ = m != Listen::DolbySurround && m != Listen::PL2Matrix && m != Listen::Standard;
    bool pl2 = m >= Listen::PL2Movie;
    stereoSur_ = pl2;
    bandLimit_ = !pl2;                       // Dolby Surround / Pro Logic : surround limité à 7 kHz
    lfe_ = pl2;
    cLevel_ = 1; sLevel_ = 1; cWidth_ = 0; strength_ = pl2 ? 0.75f : 1.0f; lfeGain_ = 0.7f;
    switch (m) {
        case Listen::DolbySurround: cLevel_ = 0; break;
        case Listen::PL2Music: case Listen::PL2xMusic: cLevel_ = 0.8f; sLevel_ = 0.75f; cWidth_ = 0.35f; break;
        case Listen::PL2Game: case Listen::PL2xGame: cLevel_ = 0.5f; sLevel_ = 1.3f; lfeGain_ = 1.2f; break;
        case Listen::PL2Matrix: sLevel_ = 0.9f; break;
        default: break;
    }
    cLevel_ *= userC_; sLevel_ *= userS_;
    surDelay_ = userDelayMs_ >= 0 ? (int)(rate * userDelayMs_ / 1000.0) : (int)(rate * (pl2 ? 0.010 : 0.020));
    if (surDelay_ > rate / 20) surDelay_ = rate / 20;
    backDelay_ = (int)(rate * 0.006);
    eps_ = 1e-9f;
    kE_ = 1.0f - std::exp(-1.0f / (rate * 0.030f));
    kAtt_ = 1.0f - std::exp(-1.0f / (rate * 0.005f));
    kRel_ = 1.0f - std::exp(-1.0f / (rate * 0.060f));
    lp7_.lowpass(7000, (float)rate); hp100_.highpass(100, (float)rate); lfeLp_.lowpass(120, (float)rate);
    const int N = 63;
    hil_.assign(N, 0.0f);
    for (int k = 0; k < N; k++) {
        int n = k - N / 2;
        if (n & 1) hil_[k] = (2.0f / (PI_F * n)) * (0.54f + 0.46f * std::cos(2 * PI_F * n / N));
    }
    hist_.assign(N, 0.0f); hpos_ = 0;
    dS_.resize(64); for (auto& d : dFront_) d.resize(64);
    dSurL_.resize(rate / 20 + 4); dSurR_.resize(rate / 20 + 4); dBackL_.resize(rate / 20 + 4); dBackR_.resize(rate / 20 + 4);
    reset();
}

void Surround::reset() {
    pl_ = pr_ = pc_ = ps_ = 0; gL_ = gR_ = gC_ = gS_ = 1;
    lp7_.reset(); hp100_.reset(); lfeLp_.reset();
    std::fill(hist_.begin(), hist_.end(), 0.0f); hpos_ = 0;
    dS_.reset(); for (auto& d : dFront_) d.reset(); dSurL_.reset(); dSurR_.reset(); dBackL_.reset(); dBackR_.reset();
}

void Surround::process(const float* in, size_t frames, std::vector<float>& out) {
    const int oc = outChannels();
    out.assign(frames * oc, 0.0f);
    if (mode_ == Listen::Standard) { std::copy(in, in + frames * 2, out.begin()); return; }
    const float k = 0.7071f, N = (float)hil_.size();
    const int D = (int)hil_.size() / 2;   // retard du filtre de Hilbert
    for (size_t i = 0; i < frames; i++) {
        float lt = in[2 * i], rt = in[2 * i + 1];
        float sum = (lt + rt) * k, dif = (lt - rt) * k;
        // pilotage (logique de direction)
        float tL = 1, tR = 1, tC = 1, tS = 1;
        if (steer_) {
            pl_ += (lt * lt - pl_) * kE_; pr_ += (rt * rt - pr_) * kE_;
            pc_ += (sum * sum - pc_) * kE_; ps_ += (dif * dif - ps_) * kE_;
            float dLR = (pl_ - pr_) / (pl_ + pr_ + eps_), dCS = (pc_ - ps_) / (pc_ + ps_ + eps_);
            float aLR = std::fabs(dLR), aCS = std::fabs(dCS);
            float fc = std::max(0.0f, dCS) * (1 - aLR), fs = std::max(0.0f, -dCS) * (1 - aLR);
            float fl = std::max(0.0f, dLR) * (1 - aCS), fr = std::max(0.0f, -dLR) * (1 - aCS);
            float s = strength_;
            tL = (1 - 0.75f * s * fc) * (1 - 0.5f * s * fs) * (1 - 0.6f * s * fr);
            tR = (1 - 0.75f * s * fc) * (1 - 0.5f * s * fs) * (1 - 0.6f * s * fl);
            tC = (1 - 0.9f * s * fs) * (1 - 0.6f * s * (fl + fr));
            tS = (1 - 0.9f * s * fc) * (1 - 0.6f * s * (fl + fr));
        }
        gL_ += (tL - gL_) * (tL < gL_ ? kAtt_ : kRel_); gR_ += (tR - gR_) * (tR < gR_ ? kAtt_ : kRel_);
        gC_ += (tC - gC_) * (tC < gC_ ? kAtt_ : kRel_); gS_ += (tS - gS_) * (tS < gS_ ? kAtt_ : kRel_);
        float C = sum * gC_ * cLevel_, S = dif * gS_ * sLevel_;
        float fL = lt * gL_, fR = rt * gR_;
        float Ls, Rs;
        if (stereoSur_) {
            // deux surrounds décorrélés (déphasage de 90°) : transformée de Hilbert + retard aligné
            hist_[hpos_] = S;
            float h = 0;
            size_t n = hil_.size();
            for (size_t j = 0; j < n; j++) h += hil_[j] * hist_[(hpos_ + n - j) % n];
            float sd = hist_[(hpos_ + n - D) % n];
            hpos_ = (hpos_ + 1) % n;
            Ls = k * (sd + h); Rs = k * (sd - h);
            fL = dFront_[0].process(fL, D); fR = dFront_[1].process(fR, D); C = dFront_[2].process(C, D);
        } else {
            float m = S;
            if (bandLimit_) m = lp7_.process(hp100_.process(m));
            Ls = Rs = m;
        }
        float lf = lfe_ ? lfeLp_.process((lt + rt) * 0.5f) * lfeGain_ : 0.0f;
        if (cWidth_ > 0) { fL += cWidth_ * 0.5f * C; fR += cWidth_ * 0.5f * C; C *= (1 - cWidth_ * 0.5f); }
        Ls = dSurL_.process(Ls, surDelay_); Rs = dSurR_.process(Rs, surDelay_);
        float* o = &out[i * oc];
        o[0] = fL * 0.85f; o[1] = fR * 0.85f; o[2] = C * 0.85f; o[3] = lf * 0.85f;
        if (back_) {
            o[6] = 0.7f * dBackL_.process(Ls, backDelay_) * 0.85f; o[7] = 0.7f * dBackR_.process(Rs, backDelay_ + 40) * 0.85f;
            Ls *= 0.85f; Rs *= 0.85f;
        }
        o[4] = Ls * 0.85f; o[5] = Rs * 0.85f;
    }
    (void)N;
}

void Virtualizer::configure(int nch, int rate) {
    nch_ = nch;
    spk_.clear();
    spk_.resize(nch);
    // ordre : L R C LFE Ls Rs [Lb Rb]
    const float az[8] = {-30, 30, 0, 0, -110, 110, -150, 150};
    for (int c = 0; c < nch && c < 8; c++) {
        Spk& s = spk_[c];
        s.az = az[c]; s.lfe = c == 3;
        float a = std::fabs(az[c]); if (a > 90) a = 180 - a;
        float rad = a * PI_F / 180;
        float itd = 0.000656f * (rad + std::sin(rad)) / (PI_F / 2 + 1);
        s.d = (int)(itd * rate);
        s.gC = 1.0f - 0.45f * std::sin(rad);
        s.a = 1.0f - std::exp(-2 * PI_F * (a > 60 ? 1500.0f : 3500.0f) / rate);
        s.dl.resize(64);
    }
}
void Virtualizer::reset() { for (auto& s : spk_) { s.z = 0; s.dl.reset(); } }

void Virtualizer::process(const float* in, size_t frames, std::vector<float>& out) {
    out.assign(frames * 2, 0.0f);
    for (size_t i = 0; i < frames; i++) {
        float l = 0, r = 0;
        for (int c = 0; c < nch_ && c < 8; c++) {
            Spk& s = spk_[c];
            float x = in[i * nch_ + c];
            if (c == 2) { l += 0.707f * x; r += 0.707f * x; continue; }
            if (s.lfe) { l += 0.5f * x; r += 0.5f * x; continue; }
            float del = s.dl.process(x, s.d);
            s.z += (del - s.z) * s.a;          // ombre de la tête : l'oreille opposée est retardée et adoucie
            float contra = s.z * s.gC;
            if (s.az < 0) { l += x; r += contra; } else { r += x; l += contra; }
        }
        float g = nch_ >= 6 ? 0.42f : 0.6f;
        out[2 * i] = l * g; out[2 * i + 1] = r * g;
    }
}
