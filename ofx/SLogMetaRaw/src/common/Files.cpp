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
#include <fstream>
#include <sstream>
#include <vector>

std::string homeDir()
{
#ifdef _WIN32
    const char* h = getenv("USERPROFILE");
#else
    const char* h = getenv("HOME");
    if (!h || !*h) {   // a host started without HOME: the password database knows
        struct passwd pwd, *found = nullptr;
        char buf[4096];
        if (getpwuid_r(getuid(), &pwd, buf, sizeof(buf), &found) == 0 && found && found->pw_dir) return found->pw_dir;
    }
#endif
    return h ? h : "";
}

// Where the plugin and the Python library share their state; slogmetaraw/paths.py mirrors this.
std::string supportDir()
{
#if defined(__APPLE__)
    return homeDir() + "/Library/Application Support/SLogMetaRaw";
#elif defined(_WIN32)
    const char* a = getenv("APPDATA");
    return (a && *a ? std::string(a) : homeDir() + "/AppData/Roaming") + "/SLogMetaRaw";
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
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
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
    char buf[MAX_PATH * 4];
    std::string full = _fullpath(buf, path.c_str(), sizeof(buf)) ? std::string(buf) : path;
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
    struct stat st;
    if (stat(path.c_str(), &st) != 0) { why = "file non trovato"; return false; }
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
            if (_mkdir(part.c_str()) != 0 && errno != EEXIST) return false;
#else
            if (mkdir(part.c_str(), 0755) != 0 && errno != EEXIST) return false;
#endif
        }
    }
    return true;
}

bool fileExists(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool removeFile(const std::string& path) { return remove(path.c_str()) == 0; }
