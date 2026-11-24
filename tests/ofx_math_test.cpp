// SPDX-License-Identifier: GPL-3.0-or-later
// Runs gen/DevelopMath.h on the cases read from stdin (one per line) for tests/test_ofx_math.py.
//   default:     r g b nodeSpace nodeGamma outSpace outGamma shotK shotTint shotEI k tint ei
//                levelFix levelSpace levelGamma levelGain levelOffset fcMode fcU fcV fcKu fcKv fcTu fcTv TONE
//                [drt [hlVib [gamutCompress gamutMap [thrC thrM thrY limC limM limY power]]]] -> the developed pixel
//                (gamutCompress 1 = reference, 2 = the 7 values; without them the pair the rendering proposes)
//   --dump-tone: outSpace convert expo TONE -> every tone field sm_set_tone writes
//   --curve lo hi n: expo TONE -> n + 1 lines "e d hl hw" of sm_t5_curve at e = lo..hi (float32), then "end"
//   --fma a b c: a*b+c, exactly 0 for the probe values only without contraction
//   --noise: "r g b drt edge invFade" (S-Gamut3.Cine / S-Log3 -> Rec.709 / Gamma 2.4) -> "same mask view":
//            same = 1 when the RGB with the Maschera rumore on is the input, bit for bit (Resolve keys the node
//            with the alpha), view = 1 when the Vedi maschera picture is the mask
// TONE = contrast highlights shadows whites blacks vibrance saturation zonePivot zoneExp[4] zoneSat[4]
//        zoneRange[4] zoneFalloff[4] softClip softClipLevel softClipColor   (27 values)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "gen/DevelopMath.h"

static const size_t kToneValues = 27;

static SMToneControls tone(const double* v)
{
    SMToneControls c = sm_tone_defaults();
    c.contrast = v[0]; c.highlights = v[1]; c.shadows = v[2]; c.whites = v[3];
    c.blacks = v[4]; c.vibrance = v[5]; c.saturation = v[6]; c.zonePivot = v[7];
    for (int z = 0; z < 4; ++z) {
        c.zoneExp[z] = v[8 + z];
        c.zoneSat[z] = v[12 + z];
        c.zoneRange[z] = v[16 + z];
        c.zoneFalloff[z] = v[20 + z];
    }
    c.softClip = (int)v[24];
    c.softClipLevel = v[25];
    c.softClipColor = v[26];
    return c;
}

static void printArray(const char* name, const float* a, int n)
{
    printf("%s", name);
    for (int i = 0; i < n; ++i) printf(" %.9g", a[i]);
    printf("\n");
}

static void dumpTone(const DevelopParams& p)
{
    printf("toneOn %d\nblkA %.9g\nblkRenorm %.9g\nconC %.9g\nconPivot %.9g\n", p.toneOn, p.blkA, p.blkRenorm,
           p.curve.conC, p.curve.conPivot);
    printf("hlE %.9g\nhlStart %.9g\n", p.curve.hlE, p.curve.hlStart);
    printArray("zoneE", p.curve.zoneE, SM_T5_SLOTS);
    printArray("zoneEdge", p.curve.zoneEdge, SM_T5_SLOTS);
    printArray("zoneK", p.curve.zoneK, SM_T5_SLOTS);
    printArray("zoneFill", p.curve.zoneFill, SM_T5_SLOTS);
    printf("roofOn %d\nroofStops %.9g\nroofRenorm %.9g\nroofPurity %.9g\nroofLin %.9g\n", p.curve.roofOn,
           p.curve.roofStops, p.curve.roofRenorm, p.roofPurity, p.roofLin);
    printf("hlAmount %.9g\nhlWhiteLin %.9g\nhlTaperSpace %d\n", p.hlAmount, p.hlWhiteLin, p.hlTaperSpace);
    printf("colorOn %d\nrefSpace %d\nsat %.9g\nvib %.9g\n", p.colorOn, p.refSpace, p.sat, p.vib);
    printArray("zSat", p.zSat, 4);
    printArray("zNomEdge", p.zNomEdge, 4);
    printArray("zNomInvF", p.zNomInvF, 4);
    printf("end\n");
}

int main(int argc, char** argv)
{
    if (argc == 5 && !strcmp(argv[1], "--fma")) {
        volatile float a = strtof(argv[2], nullptr), b = strtof(argv[3], nullptr), c = strtof(argv[4], nullptr);
        printf("%.10g\n", a * b + c);
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--wb-pick")) {
        // "shotK shotTint r g b" (linear Rec.709): the eyedropper's answer, then the sample through the develop with it
        double shotK, shotT, r, g, b;
        while (scanf("%lf %lf %lf %lf %lf", &shotK, &shotT, &r, &g, &b) == 5) {
            DevelopParams p = {};
            p.nodeSpace = p.outSpace = 1;
            p.nodeGamma = p.outGamma = 1;
            p.expo = 1.0f;
            const SMf3 xyz = sm_to_xyz(smf3((float)r, (float)g, (float)b), 1);
            const double sample[3] = { xyz.x, xyz.y, xyz.z };
            double k = 0.0, tint = 0.0;
            const int ok = sm_wb_from_xyz(shotK, shotT, sample, &k, &tint);
            sm_set_white_balance(&p, shotK, shotT, k, tint);
            const SMf3 o = sm_develop(smf3((float)r, (float)g, (float)b), p);
            printf("%d %.6g %.6g %.9g %.9g %.9g\n", ok, k, tint, o.x, o.y, o.z);
        }
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--noise")) {
        double r, g, b, drt, edge, inv;
        while (scanf("%lf %lf %lf %lf %lf %lf", &r, &g, &b, &drt, &edge, &inv) == 6) {
            DevelopParams p = {};
            p.convert = 1;
            p.nodeSpace = 8; p.nodeGamma = 9; p.outSpace = 1; p.outGamma = 3;
            p.drt = (int)drt;
            p.expo = 1.0f; p.ratioL = p.ratioM = p.ratioS = p.norm = 1.0f;
            const SMf3 in = smf3((float)r, (float)g, (float)b);
            p.nmOn = 1; p.nmEdge = (float)edge; p.nmInvFade = (float)inv;
            float mask = -1.0f, viewMask = -1.0f;
            const SMf3 on = sm_develop_a(in, p, mask);
            p.nmOn = 2;
            const SMf3 view = sm_develop_a(in, p, viewMask);
            printf("%d %.9g %d\n", memcmp(&in, &on, sizeof(SMf3)) == 0 ? 1 : 0, mask,
                   view.x == mask && view.y == mask && view.z == mask && viewMask == mask ? 1 : 0);
        }
        return 0;
    }
    const bool dump = argc == 2 && !strcmp(argv[1], "--dump-tone");
    const bool curve = argc == 5 && !strcmp(argv[1], "--curve");
    std::string line;
    char buf[8192];
    while (fgets(buf, sizeof(buf), stdin)) {
        std::istringstream in(buf);
        std::vector<double> v;
        double x;
        while (in >> x) v.push_back(x);
        if (dump) {
            if (v.size() != 3 + kToneValues) continue;
            DevelopParams p = {};
            const SMToneControls c = tone(&v[3]);
            sm_set_tone(&p, &c, (int)v[0], (int)v[1], v[2]);
            dumpTone(p);
            continue;
        }
        if (curve) {
            if (v.size() != 1 + kToneValues) continue;
            DevelopParams p = {};
            const SMToneControls c = tone(&v[1]);
            sm_set_tone(&p, &c, SM_T5_REF, 0, v[0]);
            const double lo = atof(argv[2]), hi = atof(argv[3]);
            const int n = atoi(argv[4]);
            for (int i = 0; i <= n; ++i) {
                const float e = (float)(lo + (hi - lo) * i / n);
                const SMToneOut o = sm_t5_curve(e, p.curve);
                printf("%.9g %.9g %.9g %.9g\n", e, o.d, o.hl, o.hw);
            }
            printf("end\n");
            continue;
        }
        const size_t n = v.size(), base = 25 + kToneValues;
        if (n < base || (n > base + 2 && n != base + 4 && n != base + 11)) continue;
        DevelopParams p = {};
        p.nodeSpace = (int)v[3]; p.nodeGamma = (int)v[4]; p.outSpace = (int)v[5]; p.outGamma = (int)v[6];
        p.convert = (p.outSpace != p.nodeSpace || p.outGamma != p.nodeGamma) ? 1 : 0;
        p.expo = (float)(v[12] / v[9]);
        sm_set_white_balance(&p, v[7], v[8], v[10], v[11]);
        p.levelFix = (int)v[13]; p.levelSpace = (int)v[14]; p.levelGamma = (int)v[15];
        p.levelGain = (float)v[16]; p.levelOffset = (float)v[17];
        p.fcMode = (int)v[18];
        p.fcU = (float)v[19]; p.fcV = (float)v[20]; p.fcKu = (float)v[21]; p.fcKv = (float)v[22];
        p.fcTu = (float)v[23]; p.fcTv = (float)v[24];
        const SMToneControls c = tone(&v[25]);
        sm_set_tone(&p, &c, p.outSpace, p.convert, p.expo);
        if (n > base) p.drt = (int)v[base];
        if (n > base + 1) p.hlVib = (float)v[base + 1];
        int gc = SM_DRT_GAMUT_COMPRESS[p.drt], gm = SM_DRT_GAMUT_MAP[p.drt];
        if (n > base + 2) { gc = (int)v[base + 2]; gm = (int)v[base + 3]; }
        if (gc == 1) sm_set_gamut_compress(&p, SM_RGC_REFERENCE);
        if (gc == 2) sm_set_gamut_compress(&p, &v[base + 4]);
        p.gmOn = gm;
        SMf3 o = sm_develop(smf3((float)v[0], (float)v[1], (float)v[2]), p);
        printf("%.9g %.9g %.9g\n", o.x, o.y, o.z);
    }
    return 0;
}
