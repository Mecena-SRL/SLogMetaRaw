// SPDX-License-Identifier: GPL-3.0-or-later
// The "Esporta LUT" button: the node's current look as a .cube file.
#include "DevelopEffect.h"

#include <cstdio>

#include "LutExport.h"
#include "../common/Child.h"

static std::string appleScriptQuote(const std::string& s)
{
    std::string o = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') o += '\\';
        o += (c == '\n' || c == '\r') ? ' ' : c;
    }
    return o + "\"";
}

static std::string baseName(const std::string& path)
{
    std::string n = path.substr(path.find_last_of('/') == std::string::npos ? 0 : path.find_last_of('/') + 1);
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
    const std::string script = "POSIX path of (choose file name with prompt \"Salva la LUT\" default name "
                               + appleScriptQuote(name) + ")";
    const ChildResult r = runProcess({ "/usr/bin/osascript", "-e", script }, EnvSnapshot::capture(), 10 * 60 * 1000);
    if (!r.started || !r.finished) {
        say(OFX::Message::eMessageError, "Impossibile aprire la finestra di salvataggio.");
        return;
    }
    if (r.exitCode != 0) return;   // cancelled
    std::string path = r.out;
    while (!path.empty() && (path.back() == '\n' || path.back() == '\r')) path.pop_back();
    if (path.empty()) return;
    if (path.size() < 5 || path.compare(path.size() - 5, 5, ".cube") != 0) path += ".cube";

    std::string error;
    if (writeCubeLut(p, kLutSizes[sizeIdx], baseName(path), path, error))
        say(OFX::Message::eMessageMessage, "LUT salvata: " + path);
    else
        say(OFX::Message::eMessageError, "LUT non salvata: " + error);
}
