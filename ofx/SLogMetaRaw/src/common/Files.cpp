// SPDX-License-Identifier: GPL-3.0-or-later
#include "Files.h"

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif
#include <limits.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <pwd.h>
#include <unistd.h>
#endif
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#endif

#include <cerrno>

#include <cstdio>
#include <cstdlib>
#include <vector>

#ifdef _WIN32
std::wstring widen(const std::string& utf8)
{
    if (utf8.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), nullptr, 0);
    std::wstring w(n > 0 ? n : 0, L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), &w[0], n);
    return w;
}

std::string narrow(const wchar_t* wide)
{
    if (!wide || !*wide) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    std::string s(n > 1 ? n - 1 : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, wide, -1, &s[0], n, nullptr, nullptr);
    return s;
}

std::string envVar(const char* name) { return narrow(_wgetenv(widen(name).c_str())); }
FILE* openFile(const std::string& path, const char* mode) { return _wfopen(widen(path).c_str(), widen(mode).c_str()); }

using StatBuf = struct _stat64;   // MSVC's plain stat() fails on files over 2 GiB
static int statPath(const std::string& path, StatBuf* st) { return _wstat64(widen(path).c_str(), st); }
#else
std::string envVar(const char* name)
{
    const char* v = getenv(name);
    return v ? v : "";
}
FILE* openFile(const std::string& path, const char* mode) { return fopen(path.c_str(), mode); }

using StatBuf = struct stat;
static int statPath(const std::string& path, StatBuf* st) { return stat(path.c_str(), st); }
#endif

std::string homeDir()
{
#ifdef _WIN32
    return envVar("USERPROFILE");
#else
    const char* h = getenv("HOME");
    if (!h || !*h) {   // a host started without HOME: the password database knows
        struct passwd pwd, *found = nullptr;
        char buf[4096];
        if (getpwuid_r(getuid(), &pwd, buf, sizeof(buf), &found) == 0 && found && found->pw_dir) return found->pw_dir;
    }
    return h ? h : "";
#endif
}

// Where the plugin and the Python library share their state; slogmetaraw/paths.py mirrors this.
std::string supportDir()
{
#if defined(__APPLE__)
    return homeDir() + "/Library/Application Support/SLogMetaRaw";
#elif defined(_WIN32)
    const std::string a = envVar("APPDATA");
    return (a.empty() ? homeDir() + "/AppData/Roaming" : a) + "/SLogMetaRaw";
#else
    const char* x = getenv("XDG_DATA_HOME");
    return (x && *x ? std::string(x) : homeDir() + "/.local/share") + "/SLogMetaRaw";
#endif
}

std::string logDir()
{
#if defined(__APPLE__)
    return homeDir() + "/Library/Logs/SLogMetaRaw";
#else
    return supportDir() + "/logs";
#endif
}

bool readFile(const std::string& path, std::string& out)
{
    FILE* f = openFile(path, "rb");
    if (!f) return false;
    out.clear();
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    const bool ok = !ferror(f);
    fclose(f);
    return ok;
}

// Hosts hand out NFD or NFC spellings of the same path; the cache key must not depend on it.
std::string normalizeNFC(const std::string& s)
{
#ifndef __APPLE__
    return s;   // only HFS+/APFS hosts mix NFD and NFC spellings
#else
    if (s.empty()) return s;
    CFStringRef cf = CFStringCreateWithCString(kCFAllocatorDefault, s.c_str(), kCFStringEncodingUTF8);
    if (!cf) return s;
    CFMutableStringRef norm = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, cf);
    CFRelease(cf);
    if (!norm) return s;
    CFStringNormalize(norm, kCFStringNormalizationFormC);
    CFIndex max = CFStringGetMaximumSizeForEncoding(CFStringGetLength(norm), kCFStringEncodingUTF8) + 1;
    std::vector<char> buf(max);
    std::string out = CFStringGetCString(norm, buf.data(), max, kCFStringEncodingUTF8) ? buf.data() : s;
    CFRelease(norm);
    return out;
#endif
}

// Resolve may hand over a symlinked path while the script records the physical one.
std::string canonicalPath(const std::string& path)
{
#ifdef _WIN32
    wchar_t buf[MAX_PATH * 4];
    std::string full = _wfullpath(buf, widen(path).c_str(), MAX_PATH * 4) ? narrow(buf) : path;
    for (char& c : full)
        if (c == '\\') c = '/';
    return full;
#else
    char buf[PATH_MAX];
    return normalizeNFC(realpath(path.c_str(), buf) ? std::string(buf) : path);
#endif
}

std::string fnv1a64(const std::string& text)
{
    unsigned long long h = 0xcbf29ce484222325ULL;
    for (unsigned char c : text) {
        h ^= c;
        h *= 0x100000001b3ULL;
    }
    char buf[17];
    snprintf(buf, sizeof(buf), "%016llx", h);
    return buf;
}

std::string cacheRecordPath(const std::string& clipPath)
{
    return supportDir() + "/cache/" + fnv1a64(canonicalPath(clipPath)) + ".json";
}

// A cloud placeholder (SF_DATALESS) downloads when read: minutes of frozen UI and gigabytes.
bool clipIsReadable(const std::string& path, std::string& why)
{
    StatBuf st;
    if (statPath(path, &st) != 0) { why = "file non trovato"; return false; }
#ifdef __APPLE__
    if (st.st_flags & 0x40000000 /* SF_DATALESS */) { why = "il file non e in locale (non scaricato)"; return false; }
#endif
    return true;
}

bool makeDirs(const std::string& path)
{
    for (size_t i = 1; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/' || path[i] == '\\') {
            const std::string part = path.substr(0, i);
#ifdef _WIN32
            if (part.size() == 2 && part[1] == ':') continue;   // drive root
            if (_wmkdir(widen(part).c_str()) != 0 && errno != EEXIST) return false;
#else
            if (mkdir(part.c_str(), 0755) != 0 && errno != EEXIST) return false;
#endif
        }
    }
    return true;
}

bool fileExists(const std::string& path)
{
    StatBuf st;
    return statPath(path, &st) == 0;
}

#ifdef _WIN32
bool removeFile(const std::string& path) { return _wremove(widen(path).c_str()) == 0; }
#else
bool removeFile(const std::string& path) { return remove(path.c_str()) == 0; }
#endif

std::string findExecutable(const std::string& name)
{
#ifdef _WIN32
    wchar_t found[MAX_PATH];
    return SearchPathW(nullptr, widen(name).c_str(), L".exe", MAX_PATH, found, nullptr) ? narrow(found) : "";
#else
    std::string list = getenv("PATH") ? getenv("PATH") : "";
    list += ":/usr/bin:/usr/local/bin:/bin";   // a host can start with a stripped PATH
    size_t pos = 0;
    while (pos <= list.size()) {
        size_t end = list.find(':', pos);
        if (end == std::string::npos) end = list.size();
        if (end > pos) {
            const std::string candidate = list.substr(pos, end - pos) + "/" + name;
            if (access(candidate.c_str(), X_OK) == 0) return candidate;
        }
        pos = end + 1;
    }
    return "";
#endif
}
