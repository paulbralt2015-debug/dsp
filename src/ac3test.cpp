// Outil de test en ligne de commande : ac3test entree.ac3 sortie.f32 [stereo|lt|51]  (PCM float entrelacé)
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include "ac3.h"
int main(int argc, char** argv) {
    if (argc < 3) return 1;
    std::ifstream f(argv[1], std::ios::binary);
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::string m = argc > 3 ? argv[3] : "stereo";
    ac3::Feeder fd;
    fd.setMix(m == "lt" ? ac3::Mix::LtRt : m == "51" ? ac3::Mix::Surround51 : ac3::Mix::Stereo);
    std::vector<float> pcm;
    for (size_t i = 0; i < d.size(); i += 4000) fd.push(d.data() + i, std::min<size_t>(4000, d.size() - i), pcm);
    FILE* o = fopen(argv[2], "wb");
    fwrite(pcm.data(), 4, pcm.size(), o);
    fclose(o);
    printf("%zu floats, %d canaux, %d Hz\n", pcm.size(), fd.channels(), fd.sampleRate());
}
