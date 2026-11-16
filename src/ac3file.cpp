#include "ac3file.h"

#include <algorithm>
#include <cstdio>

bool Ac3File::open(const uint8_t* d, size_t n) {
    data_.assign(d, d + n);
    idx_.clear();
    ac3::Header h;
    size_t p = 0;
    while (p + 8 <= data_.size()) {
        if (!ac3::Decoder::parseHeader(data_.data() + p, data_.size() - p, h) || h.bsid > 8) { p++; continue; }
        if (idx_.empty()) { fs_ = h.fs; srcCh_ = h.nch; kbps_ = h.kbps; }
        idx_.push_back(p);
        p += h.size;
    }
    pos_ = 0;
    feeder_.reset();
    return !idx_.empty();
}

bool Ac3File::openFile(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    std::vector<uint8_t> b;
    uint8_t tmp[65536];
    size_t r;
    while ((r = fread(tmp, 1, sizeof tmp, f)) > 0) b.insert(b.end(), tmp, tmp + r);
    fclose(f);
    return open(b.data(), b.size());
}

void Ac3File::seek(double sec) {
    size_t k = (size_t)(std::max(0.0, sec) * fs_ / 1536.0);
    pos_ = std::min(k > 2 ? k - 2 : 0, idx_.size());   // 2 trames de recouvrement pour l'IMDCT
    feeder_.reset();
    skip_ = k - pos_;
}

size_t Ac3File::read(std::vector<float>& pcm) {
    if (pos_ >= idx_.size()) return 0;
    size_t last = std::min(pos_ + 4, idx_.size());
    size_t from = idx_[pos_], to = last < idx_.size() ? idx_[last] : data_.size();
    std::vector<float> tmp;
    feeder_.push(data_.data() + from, to - from, tmp);
    pos_ = last;
    int oc = feeder_.channels();
    size_t frames = oc ? tmp.size() / oc : 0;
    size_t drop = std::min(skip_ * 1536, frames);   // trames de recouvrement : décodées mais non restituées
    skip_ -= drop / 1536;
    pcm.insert(pcm.end(), tmp.begin() + drop * oc, tmp.end());
    return frames - drop;
}
