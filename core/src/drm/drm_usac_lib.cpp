// Run-time loader for the optional libfdk-aac USAC decoder (see drm_usac_lib.h). Not shipped, not linked.
#include "dect2/drm/drm_usac_lib.h"
#include "../source/native_common.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace dect2 { namespace drm {

UsacLib UsacLib::fromResolver(const std::function<void*(const char*)>& sym) {
    UsacLib l;
    l.open = (OpenFn)sym("aacDecoder_Open"); l.configRaw = (ConfigRawFn)sym("aacDecoder_ConfigRaw"); l.fill = (FillFn)sym("aacDecoder_Fill");
    l.decode = (DecodeFn)sym("aacDecoder_DecodeFrame"); l.info = (InfoFn)sym("aacDecoder_GetStreamInfo"); l.close = (CloseFn)sym("aacDecoder_Close");
    return l;
}

const char* UsacLib::libraryHint() { return "libfdk-aac (libfdk-aac.so.2, libfdk-aac.2.dylib or libfdk-aac-2.dll)"; }

// The libfdk-aac files in the OnAir codecs folders (docs/CODECS.md): the usual names first, then any file with "fdk" and "aac" in its
// name and a library extension, so a differently named build (fdk-aac-2.dll, libfdk-aac.2.2.0.dylib) is found too.
std::vector<std::string> UsacLib::codecFolderCandidates() {
    std::vector<std::string> v;
    for (const std::string& dir : native::codecFolders()) {
        for (const char* n : {"libfdk-aac-2.dll", "libfdk-aac.dll", "fdk-aac.dll", "libfdk-aac-1.dll", "fdk-aac-2.dll", "libfdk-aac.so.2", "libfdk-aac.so",
                              "libfdk-aac.2.dylib", "libfdk-aac.dylib"}) v.push_back(dir + n);
        std::error_code ec;
        std::vector<std::string> extra;
        for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
            const std::string f = e.path().filename().string();
            std::string low = f;
            std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return (char)std::tolower(c); });
            if (low.find("fdk") == std::string::npos || low.find("aac") == std::string::npos) continue;
            if (low.find(".dll") == std::string::npos && low.find(".dylib") == std::string::npos && low.find(".so") == std::string::npos) continue;
            if (std::find(v.begin(), v.end(), dir + f) == v.end()) extra.push_back(dir + f);
        }
        std::sort(extra.begin(), extra.end());
        v.insert(v.end(), extra.begin(), extra.end());
    }
    return v;
}

UsacLib UsacLib::load(const std::string& path) {
    std::vector<std::string> names;
    if (!path.empty()) names.push_back(path);
    if (const char* e = std::getenv("DECT2_FDKAAC_LIB")) if (*e) names.push_back(e);
    for (const std::string& n : codecFolderCandidates()) names.push_back(n);   // a copy in a codecs folder wins over the system's
    void* h = nullptr;
#ifdef _WIN32
    for (const char* n : {"libfdk-aac-2.dll", "libfdk-aac.dll", "fdk-aac.dll", "libfdk-aac-1.dll"}) names.push_back(n);
    for (const std::string& n : names) {
        const bool full = n.find('\\') != std::string::npos || n.find('/') != std::string::npos;   // a full path: its own folder is searched for the DLLs it needs
        if ((h = full ? (void*)LoadLibraryExA(n.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH) : (void*)LoadLibraryA(n.c_str()))) break;
    }
    auto sym = [&](const char* s) -> void* { return h ? (void*)GetProcAddress((HMODULE)h, s) : nullptr; };
#else
    for (const char* n : {"libfdk-aac.so.2", "libfdk-aac.so", "libfdk-aac.2.dylib", "libfdk-aac.dylib", "/opt/homebrew/lib/libfdk-aac.dylib", "/usr/local/lib/libfdk-aac.dylib"}) names.push_back(n);
    for (const std::string& n : names) if ((h = dlopen(n.c_str(), RTLD_NOW | RTLD_LOCAL))) break;
    auto sym = [&](const char* s) -> void* { return h ? dlsym(h, s) : nullptr; };
#endif
    return fromResolver(sym);   // the library handle stays open for the life of the program
}

bool UsacSession::open(const std::vector<uint8_t>& asc) {
    close();
    if (!lib_.ok() || asc.empty()) return false;
    h_ = lib_.open(0 /* TT_MP4_RAW */, 1);
    if (!h_) return false;
    uint8_t* conf[1] = {const_cast<uint8_t*>(asc.data())};
    const unsigned len[1] = {(unsigned)asc.size()};
    if (lib_.configRaw(h_, conf, len) != 0) { close(); return false; }
    return true;
}

void UsacSession::close() { if (h_ && lib_.close) lib_.close(h_); h_ = nullptr; }

bool UsacSession::decode(const std::vector<uint8_t>& au, std::vector<int16_t>& pcm, int& rate, int& channels) {
    pcm.clear();
    if (!h_ || au.empty()) return false;
    uint8_t* buf[1] = {const_cast<uint8_t*>(au.data())};
    const unsigned size[1] = {(unsigned)au.size()};
    unsigned valid = (unsigned)au.size();
    if (lib_.fill(h_, buf, size, &valid) != 0) return false;
    std::vector<int16_t> tmp(8 * 4096);
    if (lib_.decode(h_, tmp.data(), (int)tmp.size(), 0) != 0) return false;
    const int* si = lib_.info(h_);
    if (!si) return false;
    rate = si[0]; channels = si[2];
    const int frame = si[1];
    if (rate <= 0 || channels <= 0 || frame <= 0 || (size_t)frame * (size_t)channels > tmp.size()) return false;
    pcm.assign(tmp.begin(), tmp.begin() + (size_t)frame * (size_t)channels);
    return true;
}

}} // namespace dect2::drm
