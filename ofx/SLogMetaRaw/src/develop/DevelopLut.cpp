// SPDX-License-Identifier: GPL-3.0-or-later
// The "Esporta LUT" button: the node's current look as a .cube file.
#include "DevelopEffect.h"

#include <cstdio>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

#include "LutExport.h"
#include "../common/Child.h"
#include "../common/Files.h"

#ifdef __APPLE__
static std::string appleScriptQuote(const std::string& s)
{
    std::string o = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') o += '\\';
        o += (c == '\n' || c == '\r') ? ' ' : c;
    }
    return o + "\"";
}
#endif

enum class SaveAsk { Chosen, Cancelled, Failed };

// The native "save as" dialog of the platform: osascript on macOS, GetSaveFileName on Windows, zenity/kdialog on Linux.
static SaveAsk askSavePath(const std::string& name, std::string& path)
{
#if defined(_WIN32)
    char buf[MAX_PATH * 2] = {};
    snprintf(buf, sizeof(buf), "%s", name.c_str());
    OPENFILENAMEA o = {};
    o.lStructSize = sizeof(o);
    o.lpstrFilter = "LUT (*.cube)\0*.cube\0\0";
    o.lpstrFile = buf;
    o.nMaxFile = sizeof(buf);
    o.lpstrDefExt = "cube";
    o.lpstrTitle = "Salva la LUT";
    o.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameA(&o)) return CommDlgExtendedError() == 0 ? SaveAsk::Cancelled : SaveAsk::Failed;
    path = buf;
    return SaveAsk::Chosen;
#else
    std::vector<std::string> argv;
#if defined(__APPLE__)
    argv = { "/usr/bin/osascript", "-e",
             "POSIX path of (choose file name with prompt \"Salva la LUT\" default name " + appleScriptQuote(name) + ")" };
#else
    const std::string zenity = findExecutable("zenity"), kdialog = findExecutable("kdialog");
    if (!zenity.empty())
        argv = { zenity, "--file-selection", "--save", "--confirm-overwrite", "--title=Salva la LUT",
                 "--filename=" + name };
    else if (!kdialog.empty())
        argv = { kdialog, "--getsavefilename", name, "*.cube", "--title", "Salva la LUT" };
    else
        return SaveAsk::Failed;
#endif
    const ChildResult r = runProcess(argv, EnvSnapshot::capture(), 10 * 60 * 1000);
    if (!r.started || !r.finished) return SaveAsk::Failed;
    if (r.exitCode != 0) return SaveAsk::Cancelled;
    path = r.out;
    while (!path.empty() && (path.back() == '\n' || path.back() == '\r')) path.pop_back();
    return path.empty() ? SaveAsk::Cancelled : SaveAsk::Chosen;
#endif
}

static std::string baseName(const std::string& path)
{
    std::string n = path.substr(path.find_last_of("/\\") == std::string::npos ? 0 : path.find_last_of("/\\") + 1);
    const size_t dot = n.find_last_of('.');
    if (dot != std::string::npos && dot > 0) n.resize(dot);
    return n.empty() ? "S-Log MetaRaw" : n;
}

void DevelopEffect::exportLut(double p_Time)
{
    if (!m_Ready) return;
    auto say = [this](OFX::Message::MessageTypeEnum type, const std::string& text) {
        try {
            sendMessage(type, "slogmetarawLut", text);
        } catch (...) {
        }
    };
    DevelopParams p;
    const bool active = buildParams(p_Time, p);
    if (!active && p.levelFix == 0) {
        say(OFX::Message::eMessageError,
            "Niente da esportare: il nodo e in pass-through (Decode Using = Camera metadata, oppure la clip non ha "
            "metadata). Leggi i metadata o sblocca i controlli e riprova.");
        return;
    }
    int sizeIdx = kLutDefaultSize;
    m_LutSize->getValue(sizeIdx);
    if (sizeIdx < 0 || sizeIdx >= kLutSizeCount) sizeIdx = kLutDefaultSize;

    const std::string name = baseName(sourcePath()) + " - S-Log MetaRaw.cube";
    std::string path;
    switch (askSavePath(name, path)) {
    case SaveAsk::Failed:
        say(OFX::Message::eMessageError, "Impossibile aprire la finestra di salvataggio.");
        return;
    case SaveAsk::Cancelled:
        return;
    case SaveAsk::Chosen:
        break;
    }
    if (path.empty()) return;
    if (path.size() < 5 || path.compare(path.size() - 5, 5, ".cube") != 0) path += ".cube";

    std::string error;
    if (writeCubeLut(p, kLutSizes[sizeIdx], baseName(path), path, error))
        say(OFX::Message::eMessageMessage, "LUT salvata: " + path);
    else
        say(OFX::Message::eMessageError, "LUT non salvata: " + error);
}
