// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>

// "1.2.3" or "v1.2.3" -> true and the three numbers. ASCII digits only, at most 9 per part.
bool parseSemver(const std::string& text, int out[3]);
bool isNewer(const std::string& current, const std::string& latest);
// Only the project's own release installers for this system may be opened: update.json is a writable file.
bool isTrustedInstallerUrl(const std::string& url);
