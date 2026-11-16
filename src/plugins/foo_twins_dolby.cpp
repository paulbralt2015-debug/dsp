// foobar2000 — composant « Twins Dolby » : décodeur d'entrée AC-3 (.ac3 / .a52) + DSP « Dolby Surround / Pro Logic ».
// Nécessite le SDK foobar2000 (voir README). Le DSP s'applique à tous les formats (stéréo ou 5.1) ;
// la sortie est multicanale (foobar2000 sait ensuite l'adapter à l'appareil) ou virtuelle stéréo.
#include <foobar2000.h>

#include <vector>

#include "../ac3file.h"
#include "../engine.h"
#include "configmenu.h"

DECLARE_COMPONENT_VERSION("Twins Dolby", "1.0",
    "Décodeur Dolby Digital (AC-3) intégré, modes Dolby Surround / Pro Logic / Pro Logic II / IIx,\n"
    "mode nuit (DRC) et effets audio.\nRéglages : DSP « Twins Dolby Surround / Pro Logic ».");
VALIDATE_COMPONENT_FILENAME("foo_twins_dolby.dll");
DECLARE_FILE_TYPE("Fichiers Dolby Digital (AC-3)", "*.AC3;*.A52");

namespace {

std::string iniFile() {
    pfc::string8 p = core_api::get_my_full_path();
    std::string s = p.c_str();
    size_t dot = s.find_last_of('.');
    return (dot == std::string::npos ? s : s.substr(0, dot)) + ".ini";
}

unsigned chanConfig(int n) {
    return n == 8 ? audio_chunk::channel_config_7point1 : n == 6 ? audio_chunk::channel_config_5point1 : audio_chunk::channel_config_stereo;
}

void fillChunk(audio_chunk& c, const std::vector<float>& d, size_t frames, int ch, int rate) {
    c.set_data_size(frames * ch);
    audio_sample* p = c.get_data();
    for (size_t i = 0; i < frames * ch; i++) p[i] = (audio_sample)d[i];
    c.set_sample_count(frames);
    c.set_srate(rate);
    c.set_channels(ch, chanConfig(ch));
}

// ---------- décodeur d'entrée ----------
class input_ac3 {
public:
    void open(service_ptr_t<file> p_filehint, const char* p_path, t_input_open_reason p_reason, abort_callback& p_abort) {
        if (p_reason == input_open_info_write) throw exception_io_unsupported_format();
        m_file = p_filehint;
        input_open_file_helper(m_file, p_path, p_reason, p_abort);
        t_filesize sz = m_file->get_size(p_abort);
        if (sz == filesize_invalid || sz > (t_filesize)1 << 30) throw exception_io_unsupported_format();
        std::vector<uint8_t> data((size_t)sz);
        m_file->read_object(data.data(), data.size(), p_abort);
        if (!m_ac3.open(data.data(), data.size())) throw exception_io_unsupported_format();
        m_stats = m_file->get_stats(p_abort);
    }
    void get_info(file_info& p_info, abort_callback&) {
        p_info.set_length(m_ac3.duration());
        p_info.info_set_int("samplerate", m_ac3.sampleRate());
        p_info.info_set_int("channels", m_ac3.srcChannels());
        p_info.info_set_int("bitrate", m_ac3.kbps());
        p_info.info_set("codec", "AC-3 (Dolby Digital)");
    }
    t_filestats get_file_stats(abort_callback&) { return m_stats; }
    void decode_initialize(unsigned, abort_callback&) {
        Settings s; s.load(iniFile());
        m_ac3.configure(s.mixMode(), s.drcScale());
        m_ac3.seek(0);
    }
    bool decode_run(audio_chunk& p_chunk, abort_callback&) {
        std::vector<float> pcm;
        size_t n = m_ac3.read(pcm);
        if (!n) return false;
        fillChunk(p_chunk, pcm, n, m_ac3.channels(), m_ac3.sampleRate());
        return true;
    }
    void decode_seek(double p_seconds, abort_callback&) { m_ac3.seek(p_seconds); }
    bool decode_can_seek() { return true; }
    bool decode_get_dynamic_info(file_info&, double&) { return false; }
    bool decode_get_dynamic_info_track(file_info&, double&) { return false; }
    void decode_on_idle(abort_callback&) {}
    void retag_set_info(t_uint32, const file_info&, abort_callback&) { throw exception_io_unsupported_format(); }
    void retag_commit(abort_callback&) { throw exception_io_unsupported_format(); }
    static bool g_is_our_content_type(const char* p) { return !stricmp_utf8(p, "audio/ac3"); }
    static bool g_is_our_path(const char*, const char* ext) { return !stricmp_utf8(ext, "ac3") || !stricmp_utf8(ext, "a52"); }
    static const char* g_get_name() { return "Twins Dolby Digital (AC-3)"; }
    static GUID g_get_guid() { static const GUID g = { 0x3e43b960, 0x49fd, 0x5caf, { 0xb5, 0x50, 0x91, 0xa4, 0xb3, 0xed, 0x0b, 0x5e } }; return g; }
    static GUID g_get_preferences_guid() { static const GUID g = { 0xbcf12053, 0xde0f, 0x5a54, { 0x84, 0x3e, 0x71, 0xd1, 0xf4, 0x4b, 0x28, 0x29 } }; return g; }
    static bool g_is_low_merit() { return false; }

private:
    service_ptr_t<file> m_file;
    Ac3File m_ac3;
    t_filestats m_stats;
};

static input_singletrack_factory_t<input_ac3> g_input_ac3_factory;

// ---------- DSP : modes d'écoute, effets, DRC ----------
void toPreset(const Settings& s, dsp_preset& p, const GUID& g) {
    p.set_owner(g);
    p.set_data(&s, sizeof(Settings));
}
Settings fromPreset(const dsp_preset& p) {
    Settings s;
    if (p.get_data_size() == sizeof(Settings)) memcpy(&s, p.get_data(), sizeof(Settings));
    return s;
}

class dsp_twins : public dsp_impl_base {
public:
    dsp_twins(const dsp_preset& p) { m_eng.configure(fromPreset(p)); }
    static GUID g_get_guid() { static const GUID g = { 0xb9160103, 0x0014, 0x5aaa, { 0x8b, 0xd0, 0x3f, 0x2b, 0xe7, 0xdb, 0x3f, 0x84 } }; return g; }
    static void g_get_name(pfc::string_base& out) { out = "Twins Dolby Surround / Pro Logic"; }
    bool on_chunk(audio_chunk* c, abort_callback&) {
        int nch = (int)c->get_channel_count();
        if (nch != 2 && nch != 6) return true;
        size_t n = c->get_sample_count();
        const audio_sample* d = c->get_data();
        m_in.resize(n * nch);
        for (size_t i = 0; i < n * nch; i++) m_in[i] = (float)d[i];
        int oc = 0;
        m_eng.process(m_in.data(), n, nch, (int)c->get_srate(), true, m_out, oc);
        fillChunk(*c, m_out, n, oc, (int)c->get_srate());
        return true;
    }
    void on_endofplayback(abort_callback&) {}
    void on_endoftrack(abort_callback&) {}
    void flush() { m_eng.reset(); }
    double get_latency() { return 0; }
    bool need_track_change_mark() { return false; }
    static bool g_get_default_preset(dsp_preset& p) { toPreset(Settings(), p, g_get_guid()); return true; }
    static bool g_have_config_popup() { return true; }
    static void g_show_config_popup(const dsp_preset& p, HWND parent, dsp_preset_edit_callback& cb) {
        Settings s = fromPreset(p);
        if (showSettingsMenu(parent, s)) {
            s.save(iniFile());   // le décodeur AC-3 relit ce fichier au prochain morceau
            dsp_preset_impl np;
            toPreset(s, np, g_get_guid());
            cb.on_preset_changed(np);
        }
    }

private:
    DolbyEngine m_eng;
    std::vector<float> m_in, m_out;
};

static dsp_factory_t<dsp_twins> g_dsp_twins_factory;

}  // namespace
