// Moteur de lecture : Media Foundation (codecs fournis par Windows) + WASAPI + décodeur AC-3 maison.
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

#include "ac3.h"
#include "surround.h"

struct AudioTrack {
    int stream = 0;          // index de flux Media Foundation (-1 : fichier .ac3 brut)
    std::wstring label;      // texte affiché dans le menu
    bool ownAc3 = false;     // décodé par le décodeur AC-3 intégré
};

enum class Drc { Off = 0, Standard = 1, Film = 2, Night = 3 };

class Player {
public:
    Player();
    ~Player();

    bool open(const std::wstring& path, std::wstring& err);
    void close();
    void play();
    void pause();
    bool playing() const { return playing_; }
    void seek(double sec);
    double position();
    double duration() const { return dur_; }
    bool ended();
    bool isOpen() const { return open_; }

    const std::vector<AudioTrack>& tracks() const { return tracks_; }
    int currentTrack() const { return curTrack_; }
    void selectTrack(int i);  // changement de piste audio, sans perdre la position

    void setVolume(float v) { vol_ = v; }
    float volume() const { return vol_; }
    void setMix(ac3::Mix m);
    ac3::Mix mix() const { return mix_; }
    void setListen(Listen l) { listen_ = (int)l; }
    Listen listen() const { return (Listen)listen_.load(); }
    void setVirtualOnly(bool v) { virtOnly_ = v; }
    void setDrc(Drc d) { drc_ = (int)d; }
    Drc drc() const { return (Drc)drc_.load(); }
    int deviceChannels() const { return devCh_; }

    bool hasVideo() const { return vs_ >= 0; }
    // Renvoie true si une nouvelle image est à afficher (px = BGRX, bottomUp = lignes de bas en haut).
    bool fetchFrame(std::vector<uint8_t>& px, int& w, int& h, bool& bottomUp);
    double displayAspect() const { return vh_ ? (double)vw_ * pn_ / pd_ / vh_ : 1.7778; }

private:
    struct VFrame { double ts; std::vector<uint8_t> px; };

    void workerLoop();
    void audioLoop();
    void doSeek(double sec);
    void doSelect(int i);
    bool configureAudio(int i);
    bool configureVideo();
    void handleVideo(IMFSample* s, double ts);
    void handleAudio(IMFSample* s, double ts);
    void handleRaw();
    void pushAudio(const float* pcm, size_t frames, int ch, int rate, double ts);
    void routeAudio(const float* pcm, size_t frames, int ch, int rate, double ts);
    size_t aqFrames() const { return aqCh_ ? (aq_.size() - aqRead_) / aqCh_ : 0; }
    double clockLocked();
    void flushLocked(double target);

    Microsoft::WRL::ComPtr<IMFSourceReader> rd_;
    std::vector<AudioTrack> tracks_;
    int vs_ = -1, curTrack_ = -1;
    int vw_ = 0, vh_ = 0, pn_ = 1, pd_ = 1;
    LONG vstride_ = 0;
    double dur_ = 0;
    bool open_ = false, raw_ = false;

    // fichier .ac3 brut
    std::vector<uint8_t> rawData_;
    std::vector<size_t> rawIdx_;
    size_t rawPos_ = 0;
    int rawFs_ = 48000;

    ac3::Feeder feeder_;
    bool ownAc3_ = false;
    int nativeCh_ = 2, nativeRate_ = 48000;

    std::thread worker_, audioT_;
    std::mutex mu_;
    std::condition_variable cv_;
    std::deque<VFrame> vq_;
    std::vector<float> aq_;
    size_t aqRead_ = 0;
    int aqCh_ = 2, aqRate_ = 48000;
    bool audioReinit_ = false, flushAudio_ = false, haveBase_ = false, needFirst_ = true, hasAudio_ = false;
    double base_ = 0, seekPos_ = 0, wallAcc_ = 0;
    std::chrono::steady_clock::time_point wallStart_;
    uint64_t written_ = 0;
    uint32_t padding_ = 0;
    double seekReq_ = -1;
    int trackReq_ = -1;
    bool streamEnded_[64] = {false};

    std::atomic<bool> quit_{false}, workerStop_{false}, playing_{false}, eos_{false};
    std::atomic<float> vol_{1.0f};
    std::atomic<int> drc_{(int)Drc::Standard}, devCh_{2}, listen_{(int)Listen::Standard};
    std::atomic<bool> virtOnly_{false};
    Surround sur_;
    Virtualizer virt_;
    int surRate_ = 0, virtCh_ = 0, virtRate_ = 0;
    ac3::Mix mix_ = ac3::Mix::Stereo;
};
