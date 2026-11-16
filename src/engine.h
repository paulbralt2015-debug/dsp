// Moteur audio commun aux plugins (Winamp, foobar2000) : modes d'écoute Dolby, virtualisation,
// effets (basses, salle), compression de dynamique (DRC / mode nuit).
#pragma once
#include <string>
#include <vector>

#include "ac3.h"
#include "surround.h"

struct Settings {
    int listen = 0;       // Listen : 0 Standard, 1 Dolby Surround, 2 Pro Logic, 3..6 Pro Logic II (Films, Musique, Jeux, Matrix), 7..9 IIx
    int mix = 0;          // downmix AC-3 : 0 Stéréo Lo/Ro, 1 Dolby Surround Lt/Rt, 2 5.1 discret
    int drc = 1;          // 0 désactivée, 1 standard (métadonnées Dolby), 2 films, 3 mode nuit
    int virt = 1;         // 1 : surround virtuel sur 2 canaux ; 0 : sortie multicanale si l'hôte l'accepte
    int bass = 0;         // renfort des basses, dB (-6 .. +12)
    int reverb = 0;       // effet de salle, % (0 .. 100)
    int centerTrim = 0;   // niveau du centre, dB
    int surTrim = 0;      // niveau des surrounds, dB
    int surDelay = -1;    // retard des surrounds, ms (-1 = automatique)

    ac3::Mix mixMode() const { return mix == 1 ? ac3::Mix::LtRt : mix == 2 ? ac3::Mix::Surround51 : ac3::Mix::Stereo; }
    double drcScale() const { return drc == 0 ? 0.0 : 1.0; }
    bool load(const std::string& iniPath);        // Windows : fichier .ini
    void save(const std::string& iniPath) const;  // Windows : fichier .ini
};

class DolbyEngine {
public:
    void configure(const Settings& s);
    const Settings& settings() const { return s_; }
    void reset();
    // in : ch = 2 ou 6 (ordre L R C LFE Ls Rs) ; multiOk : l'hôte accepte plus de 2 canaux.
    // out : outCh canaux (ordre hôte : L R C LFE [Lb Rb] Ls Rs pour 8 canaux).
    void process(const float* in, size_t frames, int ch, int rate, bool multiOk, std::vector<float>& out, int& outCh);

private:
    struct Comb { std::vector<float> b; size_t i = 0; float z = 0;
        float run(float x) { float y = b[i]; z = y * 0.8f + z * 0.2f; b[i] = x + z * 0.82f; i = (i + 1) % b.size(); return y; } };
    struct Allpass { std::vector<float> b; size_t i = 0;
        float run(float x) { float d = b[i], y = -x + d; b[i] = x + d * 0.5f; i = (i + 1) % b.size(); return y; } };
    void configureFx(int ch, int rate);
    Settings s_;
    Surround sur_;
    Virtualizer virt_;
    int surRate_ = 0, virtCh_ = 0, virtRate_ = 0, fxCh_ = 0, fxRate_ = 0;
    int curListen_ = -1, curCT_ = 99, curST_ = 99, curSD_ = -99;
    std::vector<Biquad> shelf_;
    Comb comb_[2][4];
    Allpass ap_[2][2];
    float env_ = 0;
};
