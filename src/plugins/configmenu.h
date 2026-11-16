// Menu de réglages commun (Winamp / foobar2000) : modes d'écoute, downmix, DRC, effets.
#pragma once
#include <windows.h>
#include "../engine.h"

// Affiche le menu au curseur. Renvoie true si un réglage a changé (s est mis à jour).
inline bool showSettingsMenu(HWND owner, Settings& s) {
    enum { LST = 1000, MIX = 1100, DRC = 1200, OUT = 1300, BAS = 1400, REV = 1500, CTR = 1600, SUR = 1700, DLY = 1800 };
    static const wchar_t* listen[10] = {L"Standard (stéréo, sans traitement)", L"Dolby Surround", L"Dolby Pro Logic", L"Pro Logic II — Films",
        L"Pro Logic II — Musique", L"Pro Logic II — Jeux", L"Pro Logic II — Matrix", L"Pro Logic IIx — Films (7.1)", L"Pro Logic IIx — Musique (7.1)", L"Pro Logic IIx — Jeux (7.1)"};
    static const wchar_t* mix[3] = {L"Stéréo (Lo/Ro)", L"Dolby Surround (Lt/Rt)", L"5.1 discret"};
    static const wchar_t* drc[4] = {L"Désactivée", L"Standard (métadonnées Dolby)", L"Films", L"Mode nuit"};
    static const int bass[4] = {-6, 0, 6, 12}, rev[4] = {0, 15, 30, 50}, trim[5] = {-6, -3, 0, 3, 6}, dly[5] = {-1, 0, 10, 20, 30};
    HMENU m = CreatePopupMenu(), l = CreatePopupMenu(), x = CreatePopupMenu(), d = CreatePopupMenu(), o = CreatePopupMenu(),
          b = CreatePopupMenu(), r = CreatePopupMenu(), c = CreatePopupMenu(), u = CreatePopupMenu(), y = CreatePopupMenu();
    wchar_t t[64];
    for (int i = 0; i < 10; i++) { AppendMenuW(l, MF_STRING | (s.listen == i ? MF_CHECKED : 0), LST + i, listen[i]); if (i == 0 || i == 2 || i == 6) AppendMenuW(l, MF_SEPARATOR, 0, nullptr); }
    for (int i = 0; i < 3; i++) AppendMenuW(x, MF_STRING | (s.mix == i ? MF_CHECKED : 0), MIX + i, mix[i]);
    for (int i = 0; i < 4; i++) AppendMenuW(d, MF_STRING | (s.drc == i ? MF_CHECKED : 0), DRC + i, drc[i]);
    AppendMenuW(o, MF_STRING | (s.virt ? MF_CHECKED : 0), OUT + 1, L"Surround virtuel (casque / 2 enceintes)");
    AppendMenuW(o, MF_STRING | (!s.virt ? MF_CHECKED : 0), OUT, L"Sortie multicanale (si le lecteur l'accepte)");
    for (int i = 0; i < 4; i++) { swprintf(t, 64, L"%+d dB", bass[i]); AppendMenuW(b, MF_STRING | (s.bass == bass[i] ? MF_CHECKED : 0), BAS + i, bass[i] ? t : L"Aucun"); }
    for (int i = 0; i < 4; i++) { swprintf(t, 64, L"%d %%", rev[i]); AppendMenuW(r, MF_STRING | (s.reverb == rev[i] ? MF_CHECKED : 0), REV + i, rev[i] ? t : L"Aucun"); }
    for (int i = 0; i < 5; i++) { swprintf(t, 64, L"%+d dB", trim[i]); AppendMenuW(c, MF_STRING | (s.centerTrim == trim[i] ? MF_CHECKED : 0), CTR + i, trim[i] ? t : L"0 dB"); }
    for (int i = 0; i < 5; i++) { swprintf(t, 64, L"%+d dB", trim[i]); AppendMenuW(u, MF_STRING | (s.surTrim == trim[i] ? MF_CHECKED : 0), SUR + i, trim[i] ? t : L"0 dB"); }
    for (int i = 0; i < 5; i++) { swprintf(t, 64, L"%d ms", dly[i]); AppendMenuW(y, MF_STRING | (s.surDelay == dly[i] ? MF_CHECKED : 0), DLY + i, dly[i] < 0 ? L"Automatique" : t); }
    AppendMenuW(m, MF_POPUP, (UINT_PTR)l, L"Mode d'écoute surround");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)x, L"Mode downmix (AC-3)");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)d, L"Compression (DRC / mode nuit)");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)o, L"Sortie");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_POPUP, (UINT_PTR)b, L"Effet : renfort des basses");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)r, L"Effet : salle (réverbération)");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)c, L"Niveau du centre");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)u, L"Niveau des surrounds");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)y, L"Retard des surrounds");
    POINT p; GetCursorPos(&p);
    if (owner) SetForegroundWindow(owner);
    int id = TrackPopupMenu(m, TPM_RETURNCMD | TPM_NONOTIFY, p.x, p.y, 0, owner ? owner : GetDesktopWindow(), nullptr);
    DestroyMenu(m);   // détruit aussi les sous-menus
    if (!id) return false;
    if (id >= DLY) s.surDelay = dly[id - DLY]; else if (id >= SUR) s.surTrim = trim[id - SUR]; else if (id >= CTR) s.centerTrim = trim[id - CTR];
    else if (id >= REV) s.reverb = rev[id - REV]; else if (id >= BAS) s.bass = bass[id - BAS]; else if (id >= OUT) s.virt = id - OUT;
    else if (id >= DRC) s.drc = id - DRC; else if (id >= MIX) s.mix = id - MIX; else s.listen = id - LST;
    return true;
}
