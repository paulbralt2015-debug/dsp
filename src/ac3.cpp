#include "ac3.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ac3 {
namespace {

#include "ac3_tables.inc"

const double PI = 3.14159265358979323846;
const int SLOWDEC[4] = {0x0f, 0x11, 0x13, 0x15}, FASTDEC[4] = {0x3f, 0x53, 0x67, 0x7b};
const int SLOWGAIN[4] = {0x540, 0x4d8, 0x478, 0x410}, DBPB[4] = {0x000, 0x700, 0x900, 0xb00};
const int FLOORT[8] = {0x2f0, 0x2b0, 0x270, 0x230, 0x1f0, 0x170, 0x0f0, -2048};
const int FASTGAIN[8] = {0x080, 0x100, 0x180, 0x200, 0x280, 0x300, 0x380, 0x400};
const int KBPS[19] = {32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 448, 512, 576, 640};
const int FSR[3] = {48000, 44100, 32000};
const int NFCH[8] = {2, 1, 2, 3, 3, 4, 4, 5};
const int QB[16] = {0, 0, 0, 0, 0, 0, 5, 6, 7, 8, 9, 10, 11, 12, 14, 16};
const int REMAT[5] = {13, 25, 37, 61, 253};
const double COUPSCALE = 8.0, IMS = -2.0;

struct Tables {
    int bndtab[50], bndsz[50];
    uint8_t masktab[260];
    double win[256], pow2[40];
    Tables() {
        static const int hi[22] = {28, 31, 34, 37, 40, 43, 46, 49, 55, 61, 67, 73, 79, 85, 97, 109, 121, 133, 157, 181, 205, 229};
        for (int i = 0; i < 28; i++) bndtab[i] = i;
        for (int i = 0; i < 22; i++) bndtab[28 + i] = hi[i];
        std::memset(masktab, 0, sizeof masktab);
        for (int i = 0; i < 50; i++) {
            bndsz[i] = (i < 49 ? bndtab[i + 1] : 253) - bndtab[i];
            for (int k = bndtab[i]; k < bndtab[i] + bndsz[i]; k++) masktab[k] = (uint8_t)i;
        }
        for (int i = 0; i < 40; i++) pow2[i] = std::pow(2.0, -i);
        // fenêtre KBD alpha = 5, 256 points montants
        auto i0 = [](double x) {
            double s = 1, t = 1;
            for (int n = 1; t > 1e-12 * s; n++) { t *= (x / 2 / n) * (x / 2 / n); s += t; }
            return s;
        };
        double kk[257], sum = 0;
        for (int j = 0; j <= 256; j++) {
            double r = 2.0 * j / 256 - 1;
            kk[j] = i0(PI * 5 * std::sqrt(1 - r * r));
            sum += kk[j];
        }
        double acc = 0;
        for (int j = 0; j < 256; j++) { acc += kk[j]; win[j] = std::sqrt(acc / sum); }
    }
};
const Tables& T() { static const Tables t; return t; }

struct Fft {
    int n;
    std::vector<int> rev;
    std::vector<double> cs, sn;
    explicit Fft(int n_) : n(n_), rev(n_), cs(n_ / 2), sn(n_ / 2) {
        int lg = 0;
        while ((1 << lg) < n) lg++;
        for (int j = 0; j < n; j++) {
            int r = 0;
            for (int b = 0; b < lg; b++) if (j & (1 << b)) r |= 1 << (lg - 1 - b);
            rev[j] = r;
        }
        for (int j = 0; j < n / 2; j++) { cs[j] = std::cos(-2 * PI * j / n); sn[j] = std::sin(-2 * PI * j / n); }
    }
    void run(double* re, double* im) const {
        for (int a = 0; a < n; a++) {
            int b = rev[a];
            if (b > a) { std::swap(re[a], re[b]); std::swap(im[a], im[b]); }
        }
        for (int len = 2; len <= n; len <<= 1) {
            int h = len >> 1, st = n / len;
            for (int a = 0; a < n; a += len)
                for (int bb = 0; bb < h; bb++) {
                    int x = a + bb, y = x + h;
                    double u = cs[bb * st], v = sn[bb * st];
                    double tr = re[y] * u - im[y] * v, ti = re[y] * v + im[y] * u;
                    re[y] = re[x] - tr; im[y] = im[x] - ti; re[x] += tr; im[x] += ti;
                }
        }
    }
};

struct Dct4 {  // DCT-IV de taille M via FFT complexe de M/2 points
    int M;
    Fft f;
    std::vector<double> cr, ci, re, im;
    explicit Dct4(int M_) : M(M_), f(M_ / 2), cr(M_ / 2), ci(M_ / 2), re(M_ / 2), im(M_ / 2) {
        for (int n = 0; n < M / 2; n++) { double a = -PI * (4 * n + 1) / (4.0 * M); cr[n] = std::cos(a); ci[n] = std::sin(a); }
    }
    void run(const double* x, double* u) {
        int n2 = M / 2;
        for (int n = 0; n < n2; n++) {
            double tr = x[2 * n], ti = x[M - 1 - 2 * n];
            re[n] = tr * cr[n] - ti * ci[n];
            im[n] = tr * ci[n] + ti * cr[n];
        }
        f.run(re.data(), im.data());
        for (int n = 0; n < n2; n++) {
            u[2 * n] = re[n] * cr[n] - im[n] * ci[n];
            u[M - 1 - 2 * n] = -(re[n] * ci[n] + im[n] * cr[n]);
        }
    }
};

struct BR {
    const uint8_t* d;
    size_t n, p;
    uint32_t get(int k) {
        uint32_t v = 0;
        while (k > 0) {
            size_t b = p >> 3;
            int o = (int)(p & 7), t = 8 - o, m = k < t ? k : t;
            uint32_t byte = b < n ? d[b] : 0;
            v = (v << m) | ((byte >> (t - m)) & ((1u << m) - 1));
            p += m; k -= m;
        }
        return v;
    }
};

struct Delta { int n = 0, off[8] = {0}, len[8] = {0}, ba[8] = {0}; };
struct AllocP { int sdecay, fdecay, sgain, dbknee, floor, fgain, snr, fleak, sleak; bool snr0; const Delta* delta; };

inline int logadd(int a, int b) {
    int c = a - b, ad = (c < 0 ? -c : c) >> 1;
    if (ad > 255) ad = 255;
    return c >= 0 ? a + LATAB[ad] : b + LATAB[ad];
}
inline int lowcomp(int a, int b0, int b1, int bin) {
    if (bin < 7) { if (b0 + 256 == b1) a = 384; else if (b0 > b1) a = std::max(0, a - 64); }
    else if (bin < 20) { if (b0 + 256 == b1) a = 320; else if (b0 > b1) a = std::max(0, a - 64); }
    else a = std::max(0, a - 128);
    return a;
}

void allocate(const int8_t* exps, int start, int end, int fscod, const AllocP& p, bool lfe, uint8_t* bap) {
    const Tables& t = T();
    int psd[260], bndpsd[52], excite[52], mask[52];
    std::memset(bndpsd, 0, sizeof bndpsd); std::memset(excite, 0, sizeof excite); std::memset(mask, 0, sizeof mask);
    for (int i = start; i < end; i++) psd[i] = 3072 - (exps[i] << 7);
    int j = start, k = t.masktab[start], lastbin;
    do {
        lastbin = std::min(t.bndtab[k] + t.bndsz[k], end);
        bndpsd[k] = psd[j]; j++;
        for (; j < lastbin; j++) bndpsd[k] = logadd(bndpsd[k], psd[j]);
        k++;
    } while (end > lastbin);
    int bs = t.masktab[start], be = t.masktab[end - 1] + 1, fl = 0, sl = 0, lc = 0, begin, bin;
    if (bs == 0) {
        lc = lowcomp(0, bndpsd[0], bndpsd[1], 0); excite[0] = bndpsd[0] - p.fgain - lc;
        lc = lowcomp(lc, bndpsd[1], bndpsd[2], 1); excite[1] = bndpsd[1] - p.fgain - lc;
        begin = 7;
        for (bin = 2; bin < 7; bin++) {
            if (!(lfe && bin == 6)) lc = lowcomp(lc, bndpsd[bin], bndpsd[bin + 1], bin);
            fl = bndpsd[bin] - p.fgain; sl = bndpsd[bin] - p.sgain; excite[bin] = fl - lc;
            if (!(lfe && bin == 6) && bndpsd[bin] <= bndpsd[bin + 1]) { begin = bin + 1; break; }
        }
        for (bin = begin; bin < std::min(be, 22); bin++) {
            if (!(lfe && bin == 6)) lc = lowcomp(lc, bndpsd[bin], bndpsd[bin + 1], bin);
            fl -= p.fdecay; fl = std::max(fl, bndpsd[bin] - p.fgain);
            sl -= p.sdecay; sl = std::max(sl, bndpsd[bin] - p.sgain);
            excite[bin] = std::max(fl - lc, sl);
        }
        begin = 22;
    } else { begin = bs; fl = p.fleak; sl = p.sleak; }
    for (bin = begin; bin < be; bin++) {
        fl -= p.fdecay; fl = std::max(fl, bndpsd[bin] - p.fgain);
        sl -= p.sdecay; sl = std::max(sl, bndpsd[bin] - p.sgain);
        excite[bin] = std::max(fl, sl);
    }
    for (bin = bs; bin < be; bin++) {
        if (bndpsd[bin] < p.dbknee) excite[bin] += (p.dbknee - bndpsd[bin]) >> 2;
        mask[bin] = std::max(excite[bin], (int)HTH[fscod][bin]);
    }
    if (p.delta && p.delta->n) {
        int band = bs;
        for (int s = 0; s < p.delta->n; s++) {
            band += p.delta->off[s];
            int dl = p.delta->ba[s] >= 4 ? (p.delta->ba[s] - 3) << 7 : (p.delta->ba[s] - 4) << 7;
            for (int q = 0; q < p.delta->len[s] && band < 50; q++) mask[band++] += dl;
        }
    }
    int i = start; j = t.masktab[start];
    do {
        lastbin = std::min(t.bndtab[j] + t.bndsz[j], end);
        mask[j] -= p.snr; mask[j] -= p.floor;
        if (mask[j] < 0) mask[j] = 0;
        mask[j] &= 0x1fe0; mask[j] += p.floor;
        for (; i < lastbin; i++) {
            int a = (psd[i] - mask[j]) >> 5;
            a = a < 0 ? 0 : a > 63 ? 63 : a;
            bap[i] = p.snr0 ? 0 : BAPTAB[a];
        }
        j++;
    } while (end > lastbin);
}

double dynrng(uint32_t v) {
    int x = (int)((v >> 5) & 7);
    if (x > 3) x -= 8;
    return std::pow(2.0, x) * (((v & 31) | 32) / 32.0);
}

}  // namespace

struct Decoder::Impl {
    int cplinu = 0, chincpl[5] = {0}, cplbegf = 0, cplendf = 0, nsub = 0, ncplbnd = 0, cplbndstrc[20] = {0}, phsflginu = 0, phsflg[20] = {0};
    int rematflg[4] = {0};
    double cplco[5][20] = {{0}};
    int8_t exps[7][300];
    int endmant[5] = {0};
    uint8_t bap[7][300];
    double coef[7][300];
    double delay[7][128];
    Delta delta[7];
    int sdcycod = 0, fdcycod = 0, sgaincod = 0, dbpbcod = 0, floorcod = 0, csnroffst = 0, fsnroffst[7] = {0}, fgaincod[7] = {0};
    int cplfleak = 0, cplsleak = 0, cplstart = 0, cplend = 0, strat[7] = {0};
    uint32_t rseed = 1;
    Dct4 d256{256}, d128{128};
    double u256[256], u128[128], xs[128];
    Impl() { reset(); }
    void reset() {
        cplinu = 0; std::memset(chincpl, 0, sizeof chincpl); cplbegf = cplendf = nsub = ncplbnd = 0; phsflginu = 0;
        std::memset(cplbndstrc, 0, sizeof cplbndstrc); std::memset(phsflg, 0, sizeof phsflg); std::memset(rematflg, 0, sizeof rematflg);
        std::memset(cplco, 0, sizeof cplco); std::memset(exps, 0, sizeof exps); std::memset(endmant, 0, sizeof endmant);
        std::memset(bap, 0, sizeof bap); std::memset(coef, 0, sizeof coef); std::memset(delay, 0, sizeof delay);
        for (auto& d : delta) d = Delta();
        sdcycod = fdcycod = sgaincod = dbpbcod = floorcod = csnroffst = 0;
        std::memset(fsnroffst, 0, sizeof fsnroffst); std::memset(fgaincod, 0, sizeof fgaincod);
        cplfleak = cplsleak = cplstart = cplend = 0; std::memset(strat, 0, sizeof strat); rseed = 1;
    }
    void block(BR& b, int blk, const Header& h, float out[6][1536], double drcs);
};

Decoder::Decoder() : p_(new Impl) { std::memset(out_, 0, sizeof out_); }
Decoder::~Decoder() {}
void Decoder::reset() { p_->reset(); }

bool Decoder::parseHeader(const uint8_t* d, size_t n, Header& h) {
    if (n < 8 || d[0] != 0x0b || d[1] != 0x77) return false;
    BR b{d, n, 32};
    int fscod = (int)b.get(2), fsz = (int)b.get(6);
    if (fscod == 3 || fsz > 37) return false;
    h = Header();
    h.fscod = fscod; h.fs = FSR[fscod];
    h.bsid = (int)b.get(5); b.get(3);
    h.acmod = (int)b.get(3); h.nf = NFCH[h.acmod];
    if ((h.acmod & 1) && h.acmod != 1) { static const float c[4] = {0.707f, 0.595f, 0.5f, 0.595f}; h.cmix = c[b.get(2)]; }
    if (h.acmod & 4) { static const float s[4] = {0.707f, 0.5f, 0.0f, 0.5f}; h.smix = s[b.get(2)]; }
    if (h.acmod == 2) b.get(2);
    h.lfe = (int)b.get(1); h.nch = h.nf + h.lfe;
    h.kbps = KBPS[fsz >> 1];
    int kb = h.kbps;
    int words = fscod == 0 ? kb * 2 : fscod == 2 ? kb * 3 : (int)std::floor(kb * 1536000.0 / 44100 / 16) + (fsz & 1);
    h.size = words * 2;
    h.bitpos = (int)b.p;
    return true;
}

bool Decoder::decodeFrame(const uint8_t* d, size_t n, Header& h) {
    if (!parseHeader(d, n, h) || h.bsid > 8 || n < (size_t)h.size) return false;
    BR b{d, (size_t)h.size, (size_t)h.bitpos};
    b.get(5);
    if (b.get(1)) b.get(8);
    if (b.get(1)) b.get(8);
    if (b.get(1)) { b.get(5); b.get(2); }
    if (h.acmod == 0) {
        b.get(5);
        if (b.get(1)) b.get(8);
        if (b.get(1)) b.get(8);
        if (b.get(1)) { b.get(5); b.get(2); }
    }
    b.get(2);
    if (b.get(1)) b.get(14);
    if (b.get(1)) b.get(14);
    if (b.get(1)) { int k = (int)b.get(6) + 1; while (k--) b.get(8); }
    for (int blk = 0; blk < 6; blk++) p_->block(b, blk, h, out_, drcs_);
    return true;
}

void Decoder::Impl::block(BR& b, int blk, const Header& h, float out[6][1536], double drcs) {
    const Tables& t = T();
    int nf = h.nf, acmod = h.acmod, lfe = h.lfe, ch, bnd, i;
    int blksw[5] = {0}, dith[5] = {0};
    double gain[2] = {1, 1};
    for (ch = 0; ch < nf; ch++) blksw[ch] = (int)b.get(1);
    for (ch = 0; ch < nf; ch++) dith[ch] = (int)b.get(1);
    if (b.get(1)) gain[0] = std::pow(dynrng(b.get(8)), drcs);
    if (acmod == 0 && b.get(1)) gain[1] = std::pow(dynrng(b.get(8)), drcs);
    int cplco0[5] = {0};
    if (b.get(1)) {  // stratégie de couplage
        cplinu = (int)b.get(1);
        if (cplinu) {
            for (ch = 0; ch < nf; ch++) chincpl[ch] = (int)b.get(1);
            phsflginu = acmod == 2 ? (int)b.get(1) : 0;
            cplbegf = (int)b.get(4); cplendf = (int)b.get(4);
            int ns = 3 + cplendf - cplbegf;
            if (ns < 1) ns = 1;
            if (ns > 18) ns = 18;
            ncplbnd = ns; cplbndstrc[0] = 0;
            for (bnd = 1; bnd < ns; bnd++) { cplbndstrc[bnd] = (int)b.get(1); if (cplbndstrc[bnd]) ncplbnd--; }
            cplstart = 37 + 12 * cplbegf; cplend = 37 + 12 * (cplendf + 3); nsub = ns;
            if (cplend > 253) cplend = 253;
        }
    }
    if (cplinu) {
        for (ch = 0; ch < nf; ch++) if (chincpl[ch]) {
            if (b.get(1)) {
                cplco0[ch] = 1;
                int ms = (int)b.get(2);
                for (bnd = 0; bnd < ncplbnd; bnd++) {
                    int ex = (int)b.get(4), mt = (int)b.get(4);
                    cplco[ch][bnd] = (ex == 15 ? mt / 16.0 : (mt + 16) / 32.0) * std::pow(2.0, -(ex + 3 * ms));
                }
            }
        }
        if (acmod == 2 && phsflginu && (cplco0[0] || cplco0[1])) for (bnd = 0; bnd < ncplbnd; bnd++) phsflg[bnd] = (int)b.get(1);
    }
    if (acmod == 2) {
        if (b.get(1)) {
            int nrb = (!cplinu || cplbegf > 2) ? 4 : (cplbegf > 0 ? 3 : 2);
            for (bnd = 0; bnd < nrb; bnd++) rematflg[bnd] = (int)b.get(1);
        }
    }
    if (cplinu) strat[5] = (int)b.get(2);
    for (ch = 0; ch < nf; ch++) strat[ch] = (int)b.get(2);
    if (lfe) strat[6] = (int)b.get(1);
    for (ch = 0; ch < nf; ch++) {
        if (strat[ch] && (!chincpl[ch] || !cplinu)) endmant[ch] = (int)b.get(6) * 3 + 73;
        if (cplinu && chincpl[ch]) endmant[ch] = cplstart;
    }
    auto grp = [&](int8_t* ex, int from, int ngrp, int gs, int prev) {
        for (int k = 0; k < ngrp; k++) {
            int v = (int)b.get(7);
            for (int m = 0; m < 3; m++) {
                int dx = (m == 0 ? v / 25 : m == 1 ? (v % 25) / 5 : v % 5) - 2;
                prev += dx;
                int c = prev < 0 ? 0 : prev > 24 ? 24 : prev;
                for (int q = 0; q < gs; q++) if (from < 296) ex[from++] = (int8_t)c;
            }
        }
    };
    if (cplinu && strat[5]) {
        int gs = strat[5] == 1 ? 1 : strat[5] == 2 ? 2 : 4;
        int ng = (cplend - cplstart) / (3 * gs);
        int ab = (int)b.get(4) << 1;
        grp(exps[5], cplstart, ng, gs, ab);
    }
    for (ch = 0; ch < nf; ch++) if (strat[ch]) {
        int gs = strat[ch] == 1 ? 1 : strat[ch] == 2 ? 2 : 4, em = endmant[ch];
        int ng = (em + 3 * gs - 4) / (3 * gs);
        int ab = (int)b.get(4);
        exps[ch][0] = (int8_t)ab;
        grp(exps[ch], 1, ng, gs, ab);
        b.get(2);
    }
    if (lfe && strat[6]) { int ab = (int)b.get(4); exps[6][0] = (int8_t)ab; grp(exps[6], 1, 2, 1, ab); }
    // paramètres d'allocation binaire
    if (b.get(1)) { sdcycod = (int)b.get(2); fdcycod = (int)b.get(2); sgaincod = (int)b.get(2); dbpbcod = (int)b.get(2); floorcod = (int)b.get(3); }
    if (b.get(1)) {
        csnroffst = (int)b.get(6);
        if (cplinu) { fsnroffst[5] = (int)b.get(4); fgaincod[5] = (int)b.get(3); }
        for (ch = 0; ch < nf; ch++) { fsnroffst[ch] = (int)b.get(4); fgaincod[ch] = (int)b.get(3); }
        if (lfe) { fsnroffst[6] = (int)b.get(4); fgaincod[6] = (int)b.get(3); }
    }
    if (cplinu && b.get(1)) { cplfleak = ((int)b.get(3) << 8) + 768; cplsleak = ((int)b.get(3) << 8) + 768; }
    if (b.get(1)) {
        int dbe[7] = {0};
        if (cplinu) dbe[5] = (int)b.get(2);
        for (ch = 0; ch < nf; ch++) dbe[ch] = (int)b.get(2);
        auto rd = [&](Delta& dl, int e) {
            if (e == 1) {
                dl.n = (int)b.get(3) + 1;
                for (int q = 0; q < dl.n; q++) { dl.off[q] = (int)b.get(5); dl.len[q] = (int)b.get(4); dl.ba[q] = (int)b.get(3); }
            } else if (e == 2) dl.n = 0;
        };
        if (cplinu) rd(delta[5], dbe[5]);
        for (ch = 0; ch < nf; ch++) rd(delta[ch], dbe[ch]);
    }
    if (b.get(1)) { int sk = (int)b.get(9); b.p += (size_t)sk * 8; }
    // allocation binaire
    AllocP P;
    P.sdecay = SLOWDEC[sdcycod]; P.fdecay = FASTDEC[fdcycod]; P.sgain = SLOWGAIN[sgaincod]; P.dbknee = DBPB[dbpbcod]; P.floor = FLOORT[floorcod];
    auto run = [&](int c, int start, int end, bool lf) {
        P.fgain = FASTGAIN[fgaincod[c]];
        P.snr = (((csnroffst - 15) << 4) + fsnroffst[c]) << 2;
        P.snr0 = csnroffst == 0 && fsnroffst[c] == 0;
        P.delta = c < 5 ? &delta[c] : nullptr;
        P.fleak = cplfleak; P.sleak = cplsleak;
        allocate(exps[c], start, end, h.fscod, P, lf, bap[c]);
    };
    for (ch = 0; ch < nf; ch++) run(ch, 0, endmant[ch], false);
    if (cplinu) run(5, cplstart, cplend, false);
    if (lfe) run(6, 0, 7, true);
    // mantisses
    int g1[3] = {0}, g2[3] = {0}, g4[2] = {0}, n1 = 0, n2 = 0, n4 = 0;
    auto mant = [&](int bp, int ex, bool dt) -> double {
        int v;
        if (bp == 0) {
            if (!dt) return 0;
            rseed = rseed * 1103515245u + 12345u;
            return ((rseed >> 8) / 8388608.0 - 1) * 0.7071 * t.pow2[ex];
        }
        if (bp == 1) { if (!n1) { v = (int)b.get(5); g1[0] = v / 9; g1[1] = (v / 3) % 3; g1[2] = v % 3; n1 = 3; } return (g1[3 - n1--] - 1) * 2.0 / 3 * t.pow2[ex]; }
        if (bp == 2) { if (!n2) { v = (int)b.get(7); g2[0] = v / 25; g2[1] = (v / 5) % 5; g2[2] = v % 5; n2 = 3; } return (g2[3 - n2--] - 2) * 2.0 / 5 * t.pow2[ex]; }
        if (bp == 3) return ((int)b.get(3) - 3) * 2.0 / 7 * t.pow2[ex];
        if (bp == 4) { if (!n4) { v = (int)b.get(7); g4[0] = v / 11; g4[1] = v % 11; n4 = 2; } return (g4[2 - n4--] - 5) * 2.0 / 11 * t.pow2[ex]; }
        if (bp == 5) return ((int)b.get(4) - 7) * 2.0 / 15 * t.pow2[ex];
        int q = QB[bp];
        v = (int)b.get(q);
        if (v >= (1 << (q - 1))) v -= (1 << q);
        return v / (double)(1 << (q - 1)) * t.pow2[ex];
    };
    int got = 0;
    double cpl[300];
    std::memset(cpl, 0, sizeof cpl);
    for (ch = 0; ch < nf; ch++) {
        int em = endmant[ch];
        std::memset(coef[ch], 0, sizeof coef[ch]);
        for (i = 0; i < em; i++) coef[ch][i] = mant(bap[ch][i], exps[ch][i], dith[ch] != 0);
        if (cplinu && chincpl[ch] && !got) {
            got = 1;
            for (i = cplstart; i < cplend; i++) cpl[i] = mant(bap[5][i], exps[5][i], false);
        }
    }
    if (lfe) {
        std::memset(coef[6], 0, sizeof coef[6]);
        for (i = 0; i < 7; i++) coef[6][i] = mant(bap[6][i], exps[6][i], false);
    }
    // couplage
    if (cplinu) {
        for (ch = 0; ch < nf; ch++) if (chincpl[ch]) {
            int bn = -1, pos = cplstart;
            for (int sb = 0; sb < nsub; sb++) {
                if (!cplbndstrc[sb]) bn++;
                double cc = cplco[ch][bn < 0 ? 0 : bn];
                if (ch == 1 && acmod == 2 && phsflginu && phsflg[bn < 0 ? 0 : bn]) cc = -cc;
                for (i = 0; i < 12 && pos < 300; i++, pos++) coef[ch][pos] = cpl[pos] * cc * COUPSCALE;
            }
        }
    }
    // rematriçage
    if (acmod == 2) {
        int e2 = std::min(endmant[0], endmant[1]), nb = (!cplinu || cplbegf > 2) ? 4 : (cplbegf > 0 ? 3 : 2);
        for (bnd = 0; bnd < nb; bnd++) if (rematflg[bnd]) {
            int s0 = REMAT[bnd], s1 = std::min(REMAT[bnd + 1], e2);
            if (cplinu && bnd == nb - 1) s1 = std::min(s1, cplstart);
            for (i = s0; i < s1; i++) { double l = coef[0][i], r = coef[1][i]; coef[0][i] = l + r; coef[1][i] = l - r; }
        }
    }
    // IMDCT + fenêtrage + recouvrement
    const double* WIN = t.win;
    for (ch = 0; ch < h.nch; ch++) {
        int cidx = ch < nf ? ch : 6;
        double* cf = coef[cidx];
        double g = ch < nf ? (acmod == 0 && ch == 1 ? gain[1] : gain[0]) : 1.0;
        float* o = out[ch] + blk * 256;
        double* dl = delay[cidx];
        double tmp[256];
        bool sw = ch < nf && blksw[ch];
        if (g != 1.0) for (i = 0; i < 256; i++) cf[i] *= g;
        if (!sw) {
            d256.run(cf, u256);
            for (i = 0; i < 256; i++) tmp[i] = -IMS * u256[255 - i];
            for (i = 0; i < 128; i++) {
                o[i] = (float)(dl[i] * WIN[255 - i] - tmp[127 - i] * WIN[i]);
                o[255 - i] = (float)(dl[i] * WIN[i] + tmp[127 - i] * WIN[255 - i]);
            }
            for (i = 0; i < 128; i++) dl[i] = tmp[128 + i];
        } else {
            for (i = 0; i < 128; i++) xs[i] = cf[2 * i];
            d128.run(xs, u128);
            for (i = 0; i < 128; i++) tmp[i] = IMS * -u128[191 - (64 + i)];
            for (i = 0; i < 128; i++) {
                o[i] = (float)(dl[i] * WIN[255 - i] - tmp[127 - i] * WIN[i]);
                o[255 - i] = (float)(dl[i] * WIN[i] + tmp[127 - i] * WIN[255 - i]);
            }
            for (i = 0; i < 128; i++) xs[i] = cf[2 * i + 1];
            d128.run(xs, u128);
            for (i = 0; i < 128; i++) dl[i] = IMS * -u128[191 - (64 + i)];
        }
    }
}

int mixFrame(const Header& h, const Decoder& d, Mix mode, float* dst) {
    int A = h.acmod, nf = h.nf;
    const float *L = nullptr, *R = nullptr, *C = nullptr, *Ls = nullptr, *Rs = nullptr, *LF = h.lfe ? d.channel(nf) : nullptr;
    auto o = [&](int i) { return d.channel(i); };
    if (A == 0 || A == 2) { L = o(0); R = o(1); }
    else if (A == 1) C = o(0);
    else if (A == 3) { L = o(0); C = o(1); R = o(2); }
    else if (A == 4) { L = o(0); R = o(1); Ls = o(2); }
    else if (A == 5) { L = o(0); C = o(1); R = o(2); Ls = o(3); }
    else if (A == 6) { L = o(0); R = o(1); Ls = o(2); Rs = o(3); }
    else { L = o(0); C = o(1); R = o(2); Ls = o(3); Rs = o(4); }
    bool ms = A == 4 || A == 5;
    if (ms) Rs = Ls;
    bool six = mode == Mix::Surround51 && (h.nf > 2 || h.lfe);
    int oc = six ? 6 : 2;
    float cm = h.cmix, sm = h.smix;
    float nrLo = 1.0f / (1.0f + (C ? cm : 0.0f) + (Ls ? sm : 0.0f));
    float nrLt = 1.0f / (1.0f + (C ? 0.707f : 0.0f) + (Ls ? 0.707f : 0.0f));
    for (int i = 0; i < 1536; i++) {
        float l = L ? L[i] : 0, r = R ? R[i] : 0, c = C ? C[i] : 0, ls = Ls ? Ls[i] : 0, rs = Rs ? Rs[i] : 0;
        float* w = dst + i * oc;
        if (six) {
            float k = ms ? 0.707f : 1.0f;
            w[0] = l; w[1] = r; w[2] = c; w[3] = LF ? LF[i] : 0; w[4] = ls * k; w[5] = rs * k;
        } else if (A == 1) { w[0] = w[1] = c; }
        else if (mode == Mix::LtRt) {
            float s = ms ? 0.707f * ls : 0.707f * (ls + rs);
            w[0] = (l + 0.707f * c - s) * nrLt; w[1] = (r + 0.707f * c + s) * nrLt;
        } else {
            w[0] = (l + cm * c + sm * ls) * nrLo; w[1] = (r + cm * c + sm * rs) * nrLo;
        }
    }
    return oc;
}

Feeder::Feeder() {}
void Feeder::reset() { buf_.clear(); dec_.reset(); }

void Feeder::push(const uint8_t* p, size_t n, std::vector<float>& pcm) {
    buf_.insert(buf_.end(), p, p + n);
    size_t pos = 0;
    Header h;
    while (pos + 8 <= buf_.size()) {
        if (!Decoder::parseHeader(buf_.data() + pos, buf_.size() - pos, h) || h.bsid > 8) { pos++; continue; }
        if (pos + (size_t)h.size > buf_.size()) break;
        if (dec_.decodeFrame(buf_.data() + pos, h.size, h)) {
            tmp_.resize(1536 * 6);
            int oc = mixFrame(h, dec_, mix_, tmp_.data());
            oc_ = oc; fs_ = h.fs;
            pcm.insert(pcm.end(), tmp_.begin(), tmp_.begin() + 1536 * oc);
        }
        pos += h.size;
    }
    buf_.erase(buf_.begin(), buf_.begin() + (std::ptrdiff_t)pos);
    if (buf_.size() > (1u << 20)) buf_.clear();
}

}  // namespace ac3
