// SPDX-License-Identifier: GPL-3.0-or-later
#include "LutExport.h"

#include <cstdio>

bool writeCubeLut(const DevelopParams& params, int size, const std::string& title, const std::string& path,
                  std::string& error)
{
    if (size < 2 || size > 129) {
        error = "dimensione LUT non valida";
        return false;
    }
    DevelopParams p = params;
    p.fcMode = 0;   // a LUT of the false colour view would be a lie
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) {
        error = "impossibile scrivere il file";
        return false;
    }
    std::string t = title;
    for (char& c : t)
        if (c == '"' || c == '\n' || c == '\r') c = ' ';
    fprintf(f, "TITLE \"%s\"\n", t.c_str());
    fprintf(f, "# S-Log MetaRaw: ingresso = segnale del nodo, uscita = Color Space / Gamma scelti\n");
    fprintf(f, "LUT_3D_SIZE %d\nDOMAIN_MIN 0.0 0.0 0.0\nDOMAIN_MAX 1.0 1.0 1.0\n", size);
    const float step = 1.0f / (float)(size - 1);
    bool ok = true;
    // .cube order: red varies fastest
    for (int b = 0; b < size && ok; ++b)
        for (int g = 0; g < size && ok; ++g)
            for (int r = 0; r < size; ++r) {
                const SMf3 o = sm_develop(smf3(r * step, g * step, b * step), p);
                if (fprintf(f, "%.6f %.6f %.6f\n", o.x, o.y, o.z) < 0) { ok = false; break; }
            }
    if (fclose(f) != 0) ok = false;
    if (!ok) error = "scrittura del file non riuscita";
    return ok;
}
