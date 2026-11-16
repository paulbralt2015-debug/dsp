// Décodeur Dolby Digital (AC-3) écrit à la main — aucune dépendance (ni ffmpeg, ni bibliothèque tierce).
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace ac3 {

struct Header {
    int fscod = 0, fs = 48000, bsid = 0, acmod = 0, nf = 0, lfe = 0, nch = 0, kbps = 0, size = 0;
    int bitpos = 0;
    float cmix = 0.707f, smix = 0.707f;
};

enum class Mix { Stereo, LtRt, Surround51 };

class Decoder {
public:
    Decoder();
    ~Decoder();
    void reset();
    void setDrcScale(double s) { drcs_ = s; }  // 0 = ignorer la gamme dynamique Dolby, 1 = l'appliquer
    static bool parseHeader(const uint8_t* d, size_t n, Header& h);  // d[0..1] = 0x0B 0x77
    // Décode une trame (6 blocs de 256 échantillons). Canaux dans l'ordre de l'acmod, LFE en dernier.
    bool decodeFrame(const uint8_t* d, size_t n, Header& h);
    const float* channel(int c) const { return out_[c]; }  // 1536 échantillons

private:
    struct Impl;
    std::unique_ptr<Impl> p_;
    float out_[6][1536];
    double drcs_ = 1.0;
};

// Mixe une trame décodée en PCM float entrelacé (1536 trames). Renvoie le nombre de canaux (2 ou 6).
int mixFrame(const Header& h, const Decoder& d, Mix mode, float* dst);

// Alimente le décodeur avec des octets quelconques (échantillons MP4/MKV, fichier .ac3…).
class Feeder {
public:
    Feeder();
    void setMix(Mix m) { mix_ = m; }
    void setDrcScale(double s) { dec_.setDrcScale(s); }
    void reset();
    void push(const uint8_t* p, size_t n, std::vector<float>& pcmOut);  // ajoute du PCM entrelacé
    int channels() const { return oc_; }
    int sampleRate() const { return fs_; }

private:
    Decoder dec_;
    Mix mix_ = Mix::Stereo;
    std::vector<uint8_t> buf_;
    int oc_ = 2, fs_ = 48000;
    std::vector<float> tmp_;
};

}  // namespace ac3
