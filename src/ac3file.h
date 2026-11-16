// Lecture d'un fichier .ac3 brut en PCM float (indexation des trames, recherche, mixage). Portable.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "ac3.h"

class Ac3File {
public:
    bool open(const uint8_t* d, size_t n);
    bool openFile(const std::string& path);
    double duration() const { return idx_.empty() ? 0.0 : idx_.size() * 1536.0 / fs_; }
    int sampleRate() const { return fs_; }
    int srcChannels() const { return srcCh_; }
    int kbps() const { return kbps_; }
    void configure(ac3::Mix m, double drcScale) { feeder_.setMix(m); feeder_.setDrcScale(drcScale); }
    void seek(double sec);
    double position() const { return pos_ * 1536.0 / fs_; }
    // Ajoute du PCM entrelacé (2 ou 6 canaux) ; renvoie le nombre de trames audio ajoutées (0 = fin)
    size_t read(std::vector<float>& pcm);
    int channels() const { return feeder_.channels(); }

private:
    std::vector<uint8_t> data_;
    std::vector<size_t> idx_;
    size_t pos_ = 0, skip_ = 0;
    int fs_ = 48000, srcCh_ = 2, kbps_ = 0;
    ac3::Feeder feeder_;
};
