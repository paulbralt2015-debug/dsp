// Menu de réglages commun (Winamp / foobar2000) : modes d'écoute, downmix, ID_DRC, effets.
#pragma once
#include <windows.h>
#include "../engine.h"

// Affiche le menu au curseur. Renvoie true si un réglage a changé (s est mis à jour).
inline bool showSettingsMenu(HWND owner, Settings& s) {
    enum { ID_LST = 1000, ID_MIX = 1100, ID_DRC = 1200, ID_OUT = 1300, ID_BAS = 1400, ID_REV = 1500, ID_CTR = 1600, ID_SUR = 1700, ID_DLY = 1800 };
    static const wchar_t* k_listen[10] = {L"Standard (stéréo, sans traitement)", L"Dolby Surround", L"Dolby Pro Logic", L"Pro Logic II — Films",
        L"Pro Logic II — Musique", L"Pro Logic II — Jeux", L"Pro Logic II — Matrix", L"Pro Logic IIx — Films (7.1)", L"Pro Logic IIx — Musique (7.1)", L"Pro Logic IIx — Jeux (7.1)"};
    static const wchar_t* k_mix[3] = {L"Stéréo (Lo/Ro)", L"Dolby Surround (Lt/Rt)", L"5.1 discret"};
    static const wchar_t* k_drc[4] = {L"Désactivée", L"Standard (métadonnées Dolby)", L"Films", L"Mode nuit"};
    static const int k_bass[4] = {-6, 0, 6, 12}, k_rev[4] = {0, 15, 30, 50}, k_trim[5] = {-6, -3, 0, 3, 6}, k_dly[5] = {-1, 0, 10, 20, 30};
    HMENU m = CreatePopupMenu(), l = CreatePopupMenu(), x = CreatePopupMenu(), d = CreatePopupMenu(), o = CreatePopupMenu(),
          b = CreatePopupMenu(), r = CreatePopupMenu(), c = CreatePopupMenu(), u = CreatePopupMenu(), y = CreatePopupMenu();
    wchar_t t[64];
    for (int i = 0; i < 10; i++) { AppendMenuW(l, MF_STRING | (s.listen == i ? MF_CHECKED : 0), ID_LST + i, k_listen[i]); if (i == 0 || i == 2 || i == 6) AppendMenuW(l, MF_SEPARATOR, 0, nullptr); }
    for (int i = 0; i < 3; i++) AppendMenuW(x, MF_STRING | (s.mix == i ? MF_CHECKED : 0), ID_MIX + i, k_mix[i]);
    for (int i = 0; i < 4; i++) AppendMenuW(d, MF_STRING | (s.drc == i ? MF_CHECKED : 0), ID_DRC + i, k_drc[i]);
    AppendMenuW(o, MF_STRING | (s.virt ? MF_CHECKED : 0), ID_OUT + 1, L"Surround virtuel (casque / 2 enceintes)");
    AppendMenuW(o, MF_STRING | (!s.virt ? MF_CHECKED : 0), ID_OUT, L"Sortie multicanale (si le lecteur l'accepte)");
    for (int i = 0; i < 4; i++) { swprintf(t, 64, L"%+d dB", k_bass[i]); AppendMenuW(b, MF_STRING | (s.bass == k_bass[i] ? MF_CHECKED : 0), ID_BAS + i, k_bass[i] ? t : L"Aucun"); }
    for (int i = 0; i < 4; i++) { swprintf(t, 64, L"%d %%", k_rev[i]); AppendMenuW(r, MF_STRING | (s.reverb == k_rev[i] ? MF_CHECKED : 0), ID_REV + i, k_rev[i] ? t : L"Aucun"); }
    for (int i = 0; i < 5; i++) { swprintf(t, 64, L"%+d dB", k_trim[i]); AppendMenuW(c, MF_STRING | (s.centerTrim == k_trim[i] ? MF_CHECKED : 0), ID_CTR + i, k_trim[i] ? t : L"0 dB"); }
    for (int i = 0; i < 5; i++) { swprintf(t, 64, L"%+d dB", k_trim[i]); AppendMenuW(u, MF_STRING | (s.surTrim == k_trim[i] ? MF_CHECKED : 0), ID_SUR + i, k_trim[i] ? t : L"0 dB"); }
    for (int i = 0; i < 5; i++) { swprintf(t, 64, L"%d ms", k_dly[i]); AppendMenuW(y, MF_STRING | (s.surDelay == k_dly[i] ? MF_CHECKED : 0), ID_DLY + i, k_dly[i] < 0 ? L"Automatique" : t); }
    AppendMenuW(m, MF_POPUP, (UINT_PTR)l, L"Mode d'écoute surround");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)x, L"Mode downmix (AC-3)");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)d, L"Compression (ID_DRC / mode nuit)");
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
    if (id >= ID_DLY) s.surDelay = k_dly[id - ID_DLY]; else if (id >= ID_SUR) s.surTrim = k_trim[id - ID_SUR]; else if (id >= ID_CTR) s.centerTrim = k_trim[id - ID_CTR];
    else if (id >= ID_REV) s.reverb = k_rev[id - ID_REV]; else if (id >= ID_BAS) s.bass = k_bass[id - ID_BAS]; else if (id >= ID_OUT) s.virt = id - ID_OUT;
    else if (id >= ID_DRC) s.drc = id - ID_DRC; else if (id >= ID_MIX) s.mix = id - ID_MIX; else s.listen = id - ID_LST;
    return true;
}
