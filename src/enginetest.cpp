// Test du moteur commun : ac3 -> modes d'écoute -> effets -> DRC. Usage : enginetest fichier.ac3
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "ac3file.h"
#include "engine.h"
int main(int argc, char** argv) {
    if (argc < 2) return 1;
    int bad = 0;
    for (int mix = 0; mix < 3; mix++)
      for (int listen = 0; listen < 10; listen++)
        for (int virt = 0; virt < 2; virt++) {
            Ac3File f;
            if (!f.openFile(argv[1])) { printf("ouverture impossible\n"); return 1; }
            Settings s; s.mix = mix; s.listen = listen; s.virt = virt; s.drc = listen % 4; s.bass = listen % 2 ? 6 : 0; s.reverb = listen % 3 ? 20 : 0;
            f.configure(s.mixMode(), s.drcScale());
            DolbyEngine e; e.configure(s);
            std::vector<float> pcm, out; int oc = 0; size_t total = 0; double peak = 0; bool nan = false;
            f.seek(0.5);
            while (true) {
                pcm.clear();
                size_t n = f.read(pcm);
                if (!n) break;
                e.process(pcm.data(), n, f.channels(), f.sampleRate(), true, out, oc);
                total += n;
                for (float x : out) { if (x != x) nan = true; peak = std::max(peak, (double)std::fabs(x)); }
            }
            if (nan || total == 0 || peak > 1.0001) { bad++; printf("ECHEC mix=%d listen=%d virt=%d nan=%d total=%zu peak=%.3f\n", mix, listen, virt, nan, total, peak); }
            else if (listen == 0 || listen == 9) printf("mix=%d listen=%d virt=%d : %d canaux, %zu trames, crête %.2f\n", mix, listen, virt, oc, total, peak);
        }
    printf(bad ? "%d echecs\n" : "tout est bon\n", bad);
    return bad != 0;
}
