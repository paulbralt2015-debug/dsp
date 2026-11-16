// Twins Video Player — interface Win32 native (aucun HTML, aucun ffmpeg).
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "player.h"

#if defined(_MSC_VER)
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

namespace {

enum {
    ID_OPEN = 101, ID_ADD, ID_EXIT, ID_PLAY = 110, ID_STOP, ID_PREV, ID_NEXT, ID_FULL, ID_LIST, ID_MUTE, ID_SEEKBACK, ID_SEEKFWD, ID_NEXTTRACK,
    ID_MIX_ST = 120, ID_MIX_LT, ID_MIX_51,
    ID_DRC_OFF = 130, ID_DRC_STD, ID_DRC_FILM, ID_DRC_NIGHT,
    ID_ABOUT = 140,
    ID_LS0 = 150, ID_LS9 = 159, ID_OUT_AUTO = 160, ID_OUT_VIRT = 161,
    IDC_SEEK = 201, IDC_VOL, IDC_TIME, IDC_LIST,
    ID_TRACK0 = 4000
};
const wchar_t* APP = L"Twins Video Player";
const int BAR_H = 44, LIST_W = 260;

struct App {
    HWND hwnd = nullptr, hSeek = nullptr, hVol = nullptr, hTime = nullptr, hList = nullptr;
    HWND btn[5] = {};
    HMENU menu = nullptr, trackMenu = nullptr, mixMenu = nullptr, drcMenu = nullptr, lsMenu = nullptr, outMenu = nullptr;
    std::unique_ptr<Player> pl;
    std::vector<std::wstring> list;
    int cur = -1;
    bool listVisible = false, full = false, seeking = false, muted = false;
    float lastVol = 1.0f;
    std::vector<uint8_t> frame;
    int fw = 0, fh = 0;
    bool fbu = true;
    WINDOWPLACEMENT wp = {sizeof(WINDOWPLACEMENT)};
    LONG savedStyle = 0;
    int tick = 0;
} A;

std::wstring fmt(double s) {
    if (s < 0) s = 0;
    int t = (int)s, h = t / 3600, m = (t / 60) % 60, c = t % 60;
    wchar_t b[32];
    if (h) swprintf(b, 32, L"%d:%02d:%02d", h, m, c); else swprintf(b, 32, L"%d:%02d", m, c);
    return b;
}

std::wstring baseName(const std::wstring& p) {
    size_t i = p.find_last_of(L"\\/");
    return i == std::wstring::npos ? p : p.substr(i + 1);
}

RECT videoRect() {
    RECT r; GetClientRect(A.hwnd, &r);
    if (!A.full) r.bottom -= BAR_H;
    if (A.listVisible && !A.full) r.right -= LIST_W;
    if (r.bottom < 0) r.bottom = 0;
    if (r.right < 0) r.right = 0;
    return r;
}

void layout() {
    RECT r; GetClientRect(A.hwnd, &r);
    int w = r.right, h = r.bottom, y = h - BAR_H + 8, x = 6;
    int show = A.full ? SW_HIDE : SW_SHOW;
    const int bw[5] = {64, 40, 40, 34, 34};
    for (int i = 0; i < 5; i++) { MoveWindow(A.btn[i], x, y, bw[i], 28, TRUE); ShowWindow(A.btn[i], show); x += bw[i] + 4; }
    int volW = 90, timeW = 120;
    int seekW = std::max(60, w - x - timeW - volW - 20);
    MoveWindow(A.hSeek, x, y - 2, seekW, 30, TRUE); ShowWindow(A.hSeek, show);
    MoveWindow(A.hTime, x + seekW + 6, y + 4, timeW, 22, TRUE); ShowWindow(A.hTime, show);
    MoveWindow(A.hVol, x + seekW + timeW + 10, y - 2, volW, 30, TRUE); ShowWindow(A.hVol, show);
    MoveWindow(A.hList, w - LIST_W, 0, LIST_W, h - BAR_H, TRUE);
    ShowWindow(A.hList, (A.listVisible && !A.full) ? SW_SHOW : SW_HIDE);
    InvalidateRect(A.hwnd, nullptr, FALSE);
}

void rebuildTrackMenu() {
    while (GetMenuItemCount(A.trackMenu) > 0) DeleteMenu(A.trackMenu, 0, MF_BYPOSITION);
    const auto& t = A.pl->tracks();
    if (t.empty()) AppendMenuW(A.trackMenu, MF_STRING | MF_GRAYED, 0, L"(aucune piste audio)");
    for (size_t i = 0; i < t.size(); i++) AppendMenuW(A.trackMenu, MF_STRING, ID_TRACK0 + (UINT)i, t[i].label.c_str());
    if (!t.empty()) CheckMenuRadioItem(A.trackMenu, 0, (UINT)t.size() - 1, (UINT)A.pl->currentTrack(), MF_BYPOSITION);
    DrawMenuBar(A.hwnd);
}

void updateTitle() {
    std::wstring t = A.cur >= 0 ? baseName(A.list[A.cur]) + L" — " + APP : APP;
    SetWindowTextW(A.hwnd, t.c_str());
}

void refreshList() {
    SendMessageW(A.hList, LB_RESETCONTENT, 0, 0);
    for (auto& p : A.list) SendMessageW(A.hList, LB_ADDSTRING, 0, (LPARAM)baseName(p).c_str());
    if (A.cur >= 0) SendMessageW(A.hList, LB_SETCURSEL, A.cur, 0);
}

bool openIndex(int i) {
    if (i < 0 || i >= (int)A.list.size()) return false;
    std::wstring err;
    A.frame.clear(); A.fw = A.fh = 0;
    if (!A.pl->open(A.list[i], err)) {
        A.cur = i; updateTitle();
        MessageBoxW(A.hwnd, (baseName(A.list[i]) + L"\n\n" + err).c_str(), APP, MB_ICONWARNING);
        rebuildTrackMenu();
        return false;
    }
    A.cur = i;
    A.pl->setVolume(A.muted ? 0.0f : A.lastVol);
    rebuildTrackMenu(); updateTitle();
    SendMessageW(A.hList, LB_SETCURSEL, i, 0);
    A.pl->play();
    SetWindowTextW(A.btn[1], L"❚❚");
    InvalidateRect(A.hwnd, nullptr, FALSE);
    return true;
}

void addFiles(const std::vector<std::wstring>& f, bool playFirst) {
    if (f.empty()) return;
    int first = (int)A.list.size();
    for (auto& p : f) A.list.push_back(p);
    refreshList();
    if (playFirst || A.cur < 0) openIndex(first);
}

void openDialog(bool add) {
    std::vector<wchar_t> buf(65536, 0);
    OPENFILENAMEW o = {sizeof(o)};
    o.hwndOwner = A.hwnd;
    o.lpstrFilter = L"Médias\0*.mp4;*.m4v;*.mov;*.mkv;*.avi;*.wmv;*.webm;*.ts;*.m2ts;*.mpg;*.mpeg;*.3gp;*.mp3;*.m4a;*.aac;*.wav;*.flac;*.ogg;*.opus;*.wma;*.ac3;*.a52\0Tous les fichiers\0*.*\0";
    o.lpstrFile = buf.data(); o.nMaxFile = (DWORD)buf.size();
    o.Flags = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&o)) return;
    std::vector<std::wstring> out;
    std::wstring dir = buf.data();
    const wchar_t* p = buf.data() + dir.size() + 1;
    if (*p == 0) out.push_back(dir);
    else while (*p) { std::wstring n = p; out.push_back(dir + L"\\" + n); p += n.size() + 1; }
    addFiles(out, !add);
}

void togglePlay() {
    if (!A.pl->isOpen()) { if (A.cur >= 0) openIndex(A.cur); else openDialog(false); return; }
    if (A.pl->ended()) { A.pl->seek(0); A.pl->play(); }
    else if (A.pl->playing()) A.pl->pause(); else A.pl->play();
    SetWindowTextW(A.btn[1], A.pl->playing() ? L"❚❚" : L"▶");
}

void stopPlay() {
    if (!A.pl->isOpen()) return;
    A.pl->pause(); A.pl->seek(0);
    SetWindowTextW(A.btn[1], L"▶");
}

void setVolume(float v) {
    A.lastVol = std::min(1.0f, std::max(0.0f, v));
    A.muted = false;
    A.pl->setVolume(A.lastVol);
    SendMessageW(A.hVol, TBM_SETPOS, TRUE, (LPARAM)(A.lastVol * 100));
}

void toggleFullscreen() {
    if (!A.full) {
        A.savedStyle = GetWindowLongW(A.hwnd, GWL_STYLE);
        GetWindowPlacement(A.hwnd, &A.wp);
        MONITORINFO mi = {sizeof(mi)};
        GetMonitorInfoW(MonitorFromWindow(A.hwnd, MONITOR_DEFAULTTONEAREST), &mi);
        A.full = true;
        SetMenu(A.hwnd, nullptr);
        SetWindowLongW(A.hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(A.hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_FRAMECHANGED);
    } else {
        A.full = false;
        SetWindowLongW(A.hwnd, GWL_STYLE, A.savedStyle);
        SetMenu(A.hwnd, A.menu);
        SetWindowPlacement(A.hwnd, &A.wp);
        SetWindowPos(A.hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
    layout();
}

void next(int d) {
    if (A.list.empty()) return;
    int i = A.cur + d;
    if (i < 0 || i >= (int)A.list.size()) return;
    openIndex(i);
}

void updateUi() {
    if (!A.pl->isOpen()) { SetWindowTextW(A.hTime, L"0:00 / 0:00"); return; }
    double p = A.pl->position(), d = A.pl->duration();
    if (!A.seeking && d > 0) SendMessageW(A.hSeek, TBM_SETPOS, TRUE, (LPARAM)(p / d * 1000));
    std::wstring s = fmt(p) + L" / " + fmt(d);
    SetWindowTextW(A.hTime, s.c_str());
}

void paint(HDC hdc) {
    RECT vr = videoRect();
    HBRUSH black = (HBRUSH)GetStockObject(BLACK_BRUSH);
    if (A.fw <= 0 || A.fh <= 0 || A.frame.empty()) {
        FillRect(hdc, &vr, black);
        SetTextColor(hdc, RGB(160, 160, 160)); SetBkMode(hdc, TRANSPARENT);
        DrawTextW(hdc, L"Glissez des fichiers ici ou appuyez sur O", -1, &vr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    int vw = vr.right - vr.left, vh = vr.bottom - vr.top;
    double ar = A.pl->displayAspect();
    int dw = vw, dh = (int)(vw / ar);
    if (dh > vh) { dh = vh; dw = (int)(vh * ar); }
    int dx = (vw - dw) / 2, dy = (vh - dh) / 2;
    RECT b1 = {vr.left, vr.top, vr.right, vr.top + dy}, b2 = {vr.left, vr.top + dy + dh, vr.right, vr.bottom};
    RECT b3 = {vr.left, vr.top + dy, vr.left + dx, vr.top + dy + dh}, b4 = {vr.left + dx + dw, vr.top + dy, vr.right, vr.top + dy + dh};
    FillRect(hdc, &b1, black); FillRect(hdc, &b2, black); FillRect(hdc, &b3, black); FillRect(hdc, &b4, black);
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = A.fw;
    bi.bmiHeader.biHeight = A.fbu ? A.fh : -A.fh;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    SetStretchBltMode(hdc, HALFTONE);
    SetBrushOrgEx(hdc, 0, 0, nullptr);
    StretchDIBits(hdc, vr.left + dx, vr.top + dy, dw, dh, 0, 0, A.fw, A.fh, A.frame.data(), &bi, DIB_RGB_COLORS, SRCCOPY);
}

void onTimer() {
    if (!A.pl) return;
    int w, h; bool bu;
    if (A.pl->fetchFrame(A.frame, w, h, bu)) {
        A.fw = w; A.fh = h; A.fbu = bu;
        RECT vr = videoRect();
        InvalidateRect(A.hwnd, &vr, FALSE);
    }
    if (++A.tick % 12 == 0) {
        updateUi();
        if (A.pl->isOpen() && A.pl->ended()) {
            if (A.cur + 1 < (int)A.list.size()) next(1);
            else { A.pl->pause(); SetWindowTextW(A.btn[1], L"▶"); }
        }
    }
}

void setMixChecks(int id) { CheckMenuRadioItem(A.mixMenu, ID_MIX_ST, ID_MIX_51, id, MF_BYCOMMAND); }
void setDrcChecks(int id) { CheckMenuRadioItem(A.drcMenu, ID_DRC_OFF, ID_DRC_NIGHT, id, MF_BYCOMMAND); }
void setLsChecks(int id) { CheckMenuRadioItem(A.lsMenu, ID_LS0, ID_LS9, id, MF_BYCOMMAND); }
void setOutChecks(int id) { CheckMenuRadioItem(A.outMenu, ID_OUT_AUTO, ID_OUT_VIRT, id, MF_BYCOMMAND); }
void reapplyAudio() { if (A.pl->isOpen()) A.pl->seek(A.pl->position()); }

HMENU buildMenu() {
    HMENU bar = CreateMenu(), f = CreatePopupMenu(), p = CreatePopupMenu(), a = CreatePopupMenu(), h = CreatePopupMenu();
    HMENU mix = CreatePopupMenu(), drc = CreatePopupMenu(), ls = CreatePopupMenu(), out = CreatePopupMenu();
    A.trackMenu = CreatePopupMenu();
    A.mixMenu = mix; A.drcMenu = drc; A.lsMenu = ls; A.outMenu = out;
    AppendMenuW(f, MF_STRING, ID_OPEN, L"Ouvrir…\tO");
    AppendMenuW(f, MF_STRING, ID_ADD, L"Ajouter à la liste…");
    AppendMenuW(f, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(f, MF_STRING, ID_EXIT, L"Quitter");
    AppendMenuW(p, MF_STRING, ID_PLAY, L"Lecture / Pause\tEspace");
    AppendMenuW(p, MF_STRING, ID_STOP, L"Arrêt");
    AppendMenuW(p, MF_STRING, ID_PREV, L"Précédent\tP");
    AppendMenuW(p, MF_STRING, ID_NEXT, L"Suivant\tN");
    AppendMenuW(p, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(p, MF_STRING, ID_FULL, L"Plein écran\tF");
    AppendMenuW(p, MF_STRING, ID_LIST, L"Liste de lecture\tL");
    AppendMenuW(mix, MF_STRING, ID_MIX_ST, L"Stéréo (Lo/Ro)");
    AppendMenuW(mix, MF_STRING, ID_MIX_LT, L"Dolby Surround (Lt/Rt)");
    AppendMenuW(mix, MF_STRING, ID_MIX_51, L"5.1 discret (si la sortie le permet)");
    AppendMenuW(drc, MF_STRING, ID_DRC_OFF, L"Désactivée");
    AppendMenuW(drc, MF_STRING, ID_DRC_STD, L"Standard (métadonnées Dolby)");
    AppendMenuW(drc, MF_STRING, ID_DRC_FILM, L"Films");
    AppendMenuW(drc, MF_STRING, ID_DRC_NIGHT, L"Mode nuit");
    AppendMenuW(a, MF_POPUP, (UINT_PTR)A.trackMenu, L"Piste audio");
    AppendMenuW(a, MF_STRING, ID_NEXTTRACK, L"Piste audio suivante\tA");
    AppendMenuW(a, MF_SEPARATOR, 0, nullptr);
    static const wchar_t* lsn[10] = {L"Standard (stéréo, sans traitement)", L"Dolby Surround", L"Dolby Pro Logic", L"Pro Logic II — Films",
        L"Pro Logic II — Musique", L"Pro Logic II — Jeux", L"Pro Logic II — Matrix", L"Pro Logic IIx — Films (7.1)", L"Pro Logic IIx — Musique (7.1)", L"Pro Logic IIx — Jeux (7.1)"};
    for (int i = 0; i < 10; i++) { AppendMenuW(ls, MF_STRING, ID_LS0 + i, lsn[i]); if (i == 1 || i == 2 || i == 6) AppendMenuW(ls, MF_SEPARATOR, 0, nullptr); }
    AppendMenuW(out, MF_STRING, ID_OUT_AUTO, L"Automatique (enceintes 5.1/7.1 si disponibles)");
    AppendMenuW(out, MF_STRING, ID_OUT_VIRT, L"Casque / 2 enceintes (surround virtuel)");
    AppendMenuW(a, MF_POPUP, (UINT_PTR)ls, L"Mode d'écoute surround");
    AppendMenuW(a, MF_POPUP, (UINT_PTR)out, L"Sortie surround");
    AppendMenuW(a, MF_POPUP, (UINT_PTR)mix, L"Mode downmix (AC-3 et pistes multicanales)");
    AppendMenuW(a, MF_POPUP, (UINT_PTR)drc, L"Compression (DRC)");
    AppendMenuW(a, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(a, MF_STRING, ID_MUTE, L"Muet\tM");
    AppendMenuW(h, MF_STRING, ID_ABOUT, L"À propos");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)f, L"Fichier");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)p, L"Lecture");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)a, L"Audio");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)h, L"Aide");
    return bar;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        A.hwnd = hwnd;
        A.menu = buildMenu();
        SetMenu(hwnd, A.menu);
        const wchar_t* cap[5] = {L"Ouvrir", L"▶", L"■", L"⏮", L"⏭"};
        const int ids[5] = {ID_OPEN, ID_PLAY, ID_STOP, ID_PREV, ID_NEXT};
        for (int i = 0; i < 5; i++)
            A.btn[i] = CreateWindowW(L"BUTTON", cap[i], WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)ids[i], nullptr, nullptr);
        A.hSeek = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_NOTICKS | TBS_HORZ, 0, 0, 10, 10, hwnd, (HMENU)IDC_SEEK, nullptr, nullptr);
        SendMessageW(A.hSeek, TBM_SETRANGE, TRUE, MAKELPARAM(0, 1000));
        A.hVol = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_NOTICKS | TBS_HORZ, 0, 0, 10, 10, hwnd, (HMENU)IDC_VOL, nullptr, nullptr);
        SendMessageW(A.hVol, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendMessageW(A.hVol, TBM_SETPOS, TRUE, 100);
        A.hTime = CreateWindowW(L"STATIC", L"0:00 / 0:00", WS_CHILD | WS_VISIBLE | SS_RIGHT, 0, 0, 10, 10, hwnd, (HMENU)IDC_TIME, nullptr, nullptr);
        A.hList = CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, 0, 0, 10, 10, hwnd, (HMENU)IDC_LIST, nullptr, nullptr);
        HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        for (HWND c : {A.btn[0], A.btn[1], A.btn[2], A.btn[3], A.btn[4], A.hTime, A.hList}) SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
        setMixChecks(ID_MIX_ST); setDrcChecks(ID_DRC_STD); setLsChecks(ID_LS0); setOutChecks(ID_OUT_AUTO);
        DragAcceptFiles(hwnd, TRUE);
        SetTimer(hwnd, 1, 15, nullptr);
        return 0;
    }
    case WM_SIZE: layout(); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(hwnd, &ps); paint(dc);
        if (!A.full) { RECT r; GetClientRect(hwnd, &r); RECT bar = {0, r.bottom - BAR_H, r.right, r.bottom}; FillRect(dc, &bar, (HBRUSH)(COLOR_BTNFACE + 1)); }
        EndPaint(hwnd, &ps); return 0; }
    case WM_TIMER: onTimer(); return 0;
    case WM_DROPFILES: {
        HDROP d = (HDROP)wp; UINT n = DragQueryFileW(d, 0xFFFFFFFF, nullptr, 0);
        std::vector<std::wstring> f;
        for (UINT i = 0; i < n; i++) { wchar_t b[MAX_PATH * 2]; DragQueryFileW(d, i, b, MAX_PATH * 2); f.push_back(b); }
        DragFinish(d);
        addFiles(f, true);
        return 0; }
    case WM_HSCROLL: {
        HWND src = (HWND)lp;
        if (src == A.hSeek) {
            int code = LOWORD(wp);
            double pos = SendMessageW(A.hSeek, TBM_GETPOS, 0, 0) / 1000.0 * A.pl->duration();
            if (code == TB_THUMBTRACK) { A.seeking = true; SetWindowTextW(A.hTime, (fmt(pos) + L" / " + fmt(A.pl->duration())).c_str()); }
            else if (code == TB_ENDTRACK) { A.seeking = false; A.pl->seek(pos); SetFocus(hwnd); }
        } else if (src == A.hVol) setVolume((float)SendMessageW(A.hVol, TBM_GETPOS, 0, 0) / 100.0f);
        return 0; }
    case WM_INITMENUPOPUP:
        EnableMenuItem(A.mixMenu, ID_MIX_51, MF_BYCOMMAND | (A.pl->deviceChannels() >= 6 ? MF_ENABLED : MF_GRAYED));
        break;
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id >= ID_TRACK0 && id < ID_TRACK0 + 64) {
            A.pl->selectTrack(id - ID_TRACK0);
            // la case cochée est mise à jour quand la piste change réellement
            Sleep(30); CheckMenuRadioItem(A.trackMenu, 0, (UINT)A.pl->tracks().size() - 1, (UINT)(id - ID_TRACK0), MF_BYPOSITION);
            SetFocus(hwnd); return 0;
        }
        if (id >= ID_LS0 && id <= ID_LS9) { A.pl->setListen((Listen)(id - ID_LS0)); setLsChecks(id); reapplyAudio(); SetFocus(hwnd); return 0; }
        switch (id) {
        case ID_OPEN: openDialog(false); break;
        case ID_ADD: openDialog(true); break;
        case ID_EXIT: DestroyWindow(hwnd); break;
        case ID_PLAY: togglePlay(); break;
        case ID_STOP: stopPlay(); break;
        case ID_PREV: next(-1); break;
        case ID_NEXT: next(1); break;
        case ID_FULL: toggleFullscreen(); break;
        case ID_LIST: A.listVisible = !A.listVisible; layout(); break;
        case ID_MUTE: A.muted = !A.muted; A.pl->setVolume(A.muted ? 0.0f : A.lastVol); break;
        case ID_NEXTTRACK: { int n = (int)A.pl->tracks().size(); if (n > 1) { int t = (A.pl->currentTrack() + 1) % n; SendMessageW(hwnd, WM_COMMAND, ID_TRACK0 + t, 0); } break; }
        case ID_MIX_ST: A.pl->setMix(ac3::Mix::Stereo); setMixChecks(id); reapplyAudio(); break;
        case ID_MIX_LT: A.pl->setMix(ac3::Mix::LtRt); setMixChecks(id); reapplyAudio(); break;
        case ID_MIX_51: A.pl->setMix(ac3::Mix::Surround51); setMixChecks(A.pl->mix() == ac3::Mix::Surround51 ? id : ID_MIX_ST); reapplyAudio(); break;
        case ID_DRC_OFF: case ID_DRC_STD: case ID_DRC_FILM: case ID_DRC_NIGHT:
            A.pl->setDrc((Drc)(id - ID_DRC_OFF)); setDrcChecks(id); reapplyAudio(); break;
        case ID_OUT_AUTO: case ID_OUT_VIRT:
            A.pl->setVirtualOnly(id == ID_OUT_VIRT); setOutChecks(id); reapplyAudio(); break;
        case ID_ABOUT:
            MessageBoxW(hwnd, L"Twins Video Player\n\nLecteur natif Windows : Media Foundation + WASAPI,\ndécodeur Dolby Digital (AC-3) intégré, sans ffmpeg.", APP, MB_OK | MB_ICONINFORMATION);
            break;
        case IDC_LIST: if (HIWORD(wp) == LBN_DBLCLK) { int i = (int)SendMessageW(A.hList, LB_GETCURSEL, 0, 0); if (i >= 0) openIndex(i); SetFocus(hwnd); } break;
        }
        if (id != IDC_LIST) SetFocus(hwnd);
        return 0; }
    case WM_LBUTTONDBLCLK: toggleFullscreen(); return 0;
    case WM_LBUTTONDOWN: SetFocus(hwnd); return 0;
    case WM_KEYDOWN: {
        bool sh = GetKeyState(VK_SHIFT) < 0;
        switch (wp) {
        case VK_SPACE: togglePlay(); break;
        case VK_LEFT: if (A.pl->isOpen()) A.pl->seek(A.pl->position() - (sh ? 30 : 5)); break;
        case VK_RIGHT: if (A.pl->isOpen()) A.pl->seek(A.pl->position() + (sh ? 30 : 5)); break;
        case VK_UP: setVolume(A.lastVol + 0.05f); break;
        case VK_DOWN: setVolume(A.lastVol - 0.05f); break;
        case VK_ESCAPE: if (A.full) toggleFullscreen(); break;
        case VK_RETURN: case 'F': toggleFullscreen(); break;
        case 'M': SendMessageW(hwnd, WM_COMMAND, ID_MUTE, 0); break;
        case 'N': next(1); break;
        case 'P': next(-1); break;
        case 'L': A.listVisible = !A.listVisible; layout(); break;
        case 'O': openDialog(false); break;
        case 'A': SendMessageW(hwnd, WM_COMMAND, ID_NEXTTRACK, 0); break;
        }
        return 0; }
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        A.pl.reset();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int show) {
    SetProcessDPIAware();
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    INITCOMMONCONTROLSEX ic = {sizeof(ic), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&ic);
    A.pl.reset(new Player());
    WNDCLASSW wc = {};
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"TwinsPlayerWnd";
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassW(&wc);
    HWND h = CreateWindowExW(0, wc.lpszClassName, APP, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1024, 640, nullptr, nullptr, hi, nullptr);
    ShowWindow(h, show);
    UpdateWindow(h);
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::wstring> files;
    for (int i = 1; argv && i < argc; i++) files.push_back(argv[i]);
    if (argv) LocalFree(argv);
    if (!files.empty()) addFiles(files, true);
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) { TranslateMessage(&m); DispatchMessageW(&m); }
    CoUninitialize();
    return (int)m.wParam;
}
