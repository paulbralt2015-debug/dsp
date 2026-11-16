// Décodeurs matriciels Dolby Surround / Pro Logic / Pro Logic II / IIx (approximations à logique de pilotage)
// et virtualiseur casque. Aucune dépendance.
#pragma once
#include <algorithm>
#include <cstddef>
#include <vector>

enum class Listen {
    Standard = 0,   // stéréo sans traitement
    DolbySurround,  // matriciel passif : surround mono
    ProLogic,       // Pro Logic : pilotage actif, centre + surround mono (7 kHz, retard 20 ms)
    PL2Movie, PL2Music, PL2Game, PL2Matrix,
    PL2xMovie, PL2xMusic, PL2xGame          // IIx : ajoute deux canaux arrière (7.1)
};

struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void lowpass(float fc, float fs);
    void highpass(float fc, float fs);
    void lowshelf(float fc, float fs, float gainDb);
    void reset() { z1 = z2 = 0; }
    float process(float x) { float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};

class DelayLine {
public:
    void resize(size_t n) { b_.assign(n + 2, 0.0f); w_ = 0; }
    float process(float x, size_t d) {
        b_[w_] = x;
        size_t r = (w_ + b_.size() - d) % b_.size();
        w_ = (w_ + 1) % b_.size();
        return b_[r];
    }
    void reset() { std::fill(b_.begin(), b_.end(), 0.0f); w_ = 0; }
private:
    std::vector<float> b_;
    size_t w_ = 0;
};

class Surround {
public:
    void setTrims(float centerMul, float surMul, int delayMs) { userC_ = centerMul; userS_ = surMul; userDelayMs_ = delayMs; }
    void configure(Listen m, int rate);
    Listen mode() const { return mode_; }
    int outChannels() const { return mode_ == Listen::Standard ? 2 : back_ ? 8 : 6; }
    // in : stéréo entrelacé ; out : outChannels() canaux, ordre L R C LFE Ls Rs [Lb Rb]
    void process(const float* in, size_t frames, std::vector<float>& out);
    void reset();

private:
    Listen mode_ = Listen::Standard;
    int rate_ = 48000;
    bool back_ = false, steer_ = false, stereoSur_ = false, lfe_ = false, bandLimit_ = false;
    float userC_ = 1, userS_ = 1;
    int userDelayMs_ = -1;
    float cLevel_ = 1, sLevel_ = 1, cWidth_ = 0, strength_ = 1, lfeGain_ = 0.7f;
    int surDelay_ = 0, backDelay_ = 0;
    float eps_ = 0, kE_ = 0, kAtt_ = 0, kRel_ = 0;
    float pl_ = 0, pr_ = 0, pc_ = 0, ps_ = 0, gL_ = 1, gR_ = 1, gC_ = 1, gS_ = 1;
    Biquad lp7_, hp100_, lfeLp_;
    DelayLine dS_, dFront_[3], dSurL_, dSurR_, dBackL_, dBackR_, dHil_;
    std::vector<float> hil_, hist_;
    size_t hpos_ = 0;
};

// Virtualisation sur casque / 2 enceintes : chaque canal est placé par différence de temps et de niveau.
class Virtualizer {
public:
    void configure(int nch, int rate);
    void process(const float* in, size_t frames, std::vector<float>& out);
    void reset();
private:
    struct Spk { float az = 0; float gI = 1, gC = 0.5f; int d = 0; float a = 0.3f, z = 0; bool lfe = false; DelayLine dl; };
    std::vector<Spk> spk_;
    int nch_ = 2;
};
