// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>

std::string homeDir();
std::string supportDir();    // macOS ~/Library/Application Support/SLogMetaRaw, Windows %APPDATA%\SLogMetaRaw, Linux ~/.local/share/SLogMetaRaw
std::string logDir();        // macOS ~/Library/Logs/SLogMetaRaw, elsewhere <supportDir>/logs
bool readFile(const std::string& path, std::string& out);
std::string normalizeNFC(const std::string& s);
std::string canonicalPath(const std::string& path);   // realpath + NFC, as plugin_cache.canonical_path
std::string fnv1a64(const std::string& text);
std::string cacheRecordPath(const std::string& clipPath);
bool clipIsReadable(const std::string& path, std::string& why);
bool makeDirs(const std::string& path);   // mkdir -p
bool fileExists(const std::string& path);
std::string findExecutable(const std::string& name);   // first match on PATH (or one of the usual folders); "" if none
bool removeFile(const std::string& path);
