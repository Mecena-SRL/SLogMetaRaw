// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Common.h"

// codes: 0 DaVinci Intermediate, 1 Linear, 2 Gamma 2.2, 3 Gamma 2.4, 4 Gamma 2.6,
//        5 Rec.709 (BT.709 OETF), 6 sRGB, 7 SLog, 8 SLog2, 9 SLog3, 10 ACEScct,
//        11 V-Log, 12 Apple Log, 13 D-Log M, 14 D-Log, 15 F-Log, 16 F-Log2, 17 N-Log, 18 C-Log2, 19 C-Log3,
//        20 LogC3 (EI 800), 21 LogC4, 22 Log3G10, 23 BMD Film Gen5, 24 HLG (18% grey at 38%, BT.2408)
// Codes 11+ are the other cameras' curves, each from its maker's published formula (sources in
// docs/CAMERAS.md). Canon's and Apple's are defined on the legal range, the others on full code values.

SM_FN float sm_spow(float x, float p) { return x < 0.0f ? -SM_POW(-x, p) : SM_POW(x, p); }

SM_FN float sm_slog_enc(float r) {  // Sony S-Log core, r = reflection / 0.9 (Sony papers)
    float y = r >= 0.0f ? 0.432699f * SM_LOG10(r + 0.037584f) + 0.616596f + 0.03f
                        : r * 5.0f + 0.030001222851889303f;
    return (y * 876.0f + 64.0f) / 1023.0f;
}
SM_FN float sm_slog_dec(float x) {
    float y = (x * 1023.0f - 64.0f) / 876.0f;
    return y >= 0.030001222851889303f ? SM_POW(10.0f, (y - 0.616596f - 0.03f) / 0.432699f) - 0.037584f
                                      : (y - 0.030001222851889303f) / 5.0f;
}

// Canon C-Log2 / C-Log3 on x = reflection / 0.9
SM_FN float sm_clog_enc(float x, float a, float k, float o) {
    return x < 0.0f ? -a * SM_LOG10(-x * k + 1.0f) + o : a * SM_LOG10(x * k + 1.0f) + o;
}
SM_FN float sm_clog_dec(float y, float a, float k, float o) {
    return y < o ? -(SM_POW(10.0f, (o - y) / a) - 1.0f) / k : (SM_POW(10.0f, (y - o) / a) - 1.0f) / k;
}
SM_FN float sm_clog3_enc(float x) {
    if (x < -0.014f) return -0.36726845f * SM_LOG10(-x * 14.98325f + 1.0f) + 0.12783901f;
    if (x <= 0.014f) return 1.9754798f * x + 0.12512219f;
    return 0.36726845f * SM_LOG10(x * 14.98325f + 1.0f) + 0.12240537f;
}
SM_FN float sm_clog3_dec(float y) {
    if (y < 0.097465473f) return -(SM_POW(10.0f, (0.12783901f - y) / 0.36726845f) - 1.0f) / 14.98325f;
    if (y <= 0.15277891f) return (y - 0.12512219f) / 1.9754798f;
    return (SM_POW(10.0f, (y - 0.12240537f) / 0.36726845f) - 1.0f) / 14.98325f;
}
// "c * log10(a * y + b) + d" above cut, "e * y + f" below: F-Log, F-Log2, LogC3
SM_FN float sm_cine_enc(float y, float a, float b, float c, float d, float e, float f, float cut) {
    return y >= cut ? c * SM_LOG10(a * y + b) + d : e * y + f;
}
SM_FN float sm_cine_dec(float x, float a, float b, float c, float d, float e, float f, float cut) {
    return x >= e * cut + f ? (SM_POW(10.0f, (x - d) / c) - b) / a : (x - f) / e;
}
// ARRI LogC4: a = (2^18 - 16) / 117.45, b = 928 / 1023, c = 95 / 1023, s and t from them
#define SM_LC4_A 2231.826309067688f
#define SM_LC4_B 0.9071358748778103f
#define SM_LC4_C 0.0928641251221896f
#define SM_LC4_S 0.1135972086105891f
#define SM_LC4_T -0.0180569961199113f
// DJI D-Log M: DJI publishes no formula, only LUTs. Community fit by Thatcher Freeman (dwg-transforms,
// Mini 4 Pro footage, 18% grey near code 0.40), on the legal range. Its midtone slope matches the neutral axis of DJI's
// own D-Log M -> Rec.709 LUT (which adds a display toe and shoulder), so it is kept.
SM_FN float sm_dlogm_dec(float x) {
    float t = SM_EXP2(x * 5.403507709503174f + 0.9523495435714722f) - 2.4300594329833984f;
    return (t < 0.5263257622718811f ? t * 0.9165859222412109f + 0.44757312536239624f : t * 1.7669587135314941f)
           * 0.016361355781555176f;
}
SM_FN float sm_dlogm_enc(float y) {
    float o = y / 0.016361355781555176f;
    float t = o < 0.9299839f ? (o - 0.44757312536239624f) / 0.9165859222412109f : o / 1.7669587135314941f;
    return (SM_LOG2(SM_MAX(t + 2.4300594329833984f, 1e-6f)) - 0.9523495435714722f) / 5.403507709503174f;
}
// HLG: BT.2100 inverse OETF scaled so that BT.2408's 38% signal is 18% grey
#define SM_HLG_A 0.17883277f
#define SM_HLG_B 0.28466892f
#define SM_HLG_C 0.55991073f
#define SM_HLG_GREY 3.7396122f
SM_FN float sm_hlg_enc(float y) {
    float e = SM_MAX(y, 0.0f) / SM_HLG_GREY;
    return e <= 1.0f / 12.0f ? SM_SQRT(3.0f * e) : SM_HLG_A * SM_LOG(12.0f * e - SM_HLG_B) + SM_HLG_C;
}
SM_FN float sm_hlg_dec(float x) {
    float e = x <= 0.5f ? x * x / 3.0f : (SM_EXP((x - SM_HLG_C) / SM_HLG_A) + SM_HLG_B) / 12.0f;
    return e * SM_HLG_GREY;
}

SM_FN float sm_encode(float y, int g) {
    if (g == 0) return y <= 0.00262409f ? y * 10.44426855f : (SM_LOG2(y + 0.0075f) + 7.0f) * 0.07329248f;
    if (g == 2) return sm_spow(y, 1.0f / 2.2f);
    if (g == 3) return sm_spow(y, 1.0f / 2.4f);
    if (g == 4) return sm_spow(y, 1.0f / 2.6f);
    if (g == 5) return y < 0.018f ? 4.5f * y : 1.099f * SM_POW(y, 0.45f) - 0.099f;
    if (g == 6) return y <= 0.0031308f ? 12.92f * y : 1.055f * SM_POW(y, 1.0f / 2.4f) - 0.055f;
    if (g == 7) return sm_slog_enc(y / 0.9f);
    if (g == 8) return sm_slog_enc(y * 155.0f / 219.0f / 0.9f);
    if (g == 9) return y >= 0.01125f ? (420.0f + SM_LOG10((y + 0.01f) / 0.19f) * 261.5f) / 1023.0f
                                     : (y * (171.2102946929f - 95.0f) / 0.01125f + 95.0f) / 1023.0f;
    if (g == 10) return y <= 0.0078125f ? 10.5402377416545f * y + 0.0729055341958355f : (SM_LOG2(y) + 9.72f) / 17.52f;
    if (g == 11) return y < 0.01f ? 5.6f * y + 0.125f : 0.241514f * SM_LOG10(y + 0.00873f) + 0.598206f;
    if (g == 12) return y >= 0.01f ? 0.08550479f * SM_LOG2(y + 0.00964052f) + 0.69336945f
                                   : (y >= -0.05641088f ? 47.28711236f * (y + 0.05641088f) * (y + 0.05641088f) : 0.0f);
    if (g == 13) return sm_dlogm_enc(y);
    if (g == 14) return y <= 0.0078f ? 6.025f * y + 0.0929f : SM_LOG10(y * 0.9892f + 0.0108f) * 0.256663f + 0.584555f;
    if (g == 15) return sm_cine_enc(y, 0.555556f, 0.009468f, 0.344676f, 0.790453f, 8.735631f, 0.092864f, 0.00089f);
    if (g == 16) return sm_cine_enc(y, 5.555556f, 0.064829f, 0.245281f, 0.384316f, 8.799461f, 0.092864f, 0.000889f);
    if (g == 17) return y < 0.328f ? 650.0f * sm_spow(y + 0.0075f, 1.0f / 3.0f) / 1023.0f : (150.0f * SM_LOG(y) + 619.0f) / 1023.0f;
    if (g == 18) return sm_clog_enc(y / 0.9f, 0.24136077f, 87.09937546f, 0.092864125f);
    if (g == 19) return sm_clog3_enc(y / 0.9f);
    if (g == 20) return sm_cine_enc(y, 5.555556f, 0.052272f, 0.247190f, 0.385537f, 5.367655f, 0.092809f, 0.010591f);
    if (g == 21) return y >= SM_LC4_T ? (SM_LOG2(SM_LC4_A * y + 64.0f) - 6.0f) / 14.0f * SM_LC4_B + SM_LC4_C
                                      : (y - SM_LC4_T) / SM_LC4_S;
    if (g == 22) return y + 0.01f < 0.0f ? (y + 0.01f) * 15.1927f : 0.224282f * SM_LOG10((y + 0.01f) * 155.975327f + 1.0f);
    if (g == 23) return y < 0.005f ? 8.283605932402494f * y + 0.09246575342465753f
                                   : 0.08692876065491224f * SM_LOG(y + 0.005494072432257808f) + 0.5300133392291939f;
    if (g == 24) return sm_hlg_enc(y);
    return y;
}
SM_FN float sm_decode(float x, int g) {
    if (g == 0) return x <= 0.02740668f ? x / 10.44426855f : SM_EXP2(x / 0.07329248f - 7.0f) - 0.0075f;
    if (g == 2) return sm_spow(x, 2.2f);
    if (g == 3) return sm_spow(x, 2.4f);
    if (g == 4) return sm_spow(x, 2.6f);
    if (g == 5) return x < 0.081f ? x / 4.5f : SM_POW((x + 0.099f) / 1.099f, 1.0f / 0.45f);
    if (g == 6) return x <= 0.04045f ? x / 12.92f : SM_POW((x + 0.055f) / 1.055f, 2.4f);
    if (g == 7) return sm_slog_dec(x) * 0.9f;
    if (g == 8) return sm_slog_dec(x) * 0.9f * 219.0f / 155.0f;
    if (g == 9) return x >= 171.2102946929f / 1023.0f ? SM_POW(10.0f, (x * 1023.0f - 420.0f) / 261.5f) * 0.19f - 0.01f
                                                      : (x * 1023.0f - 95.0f) * 0.01125f / (171.2102946929f - 95.0f);
    if (g == 10) return x <= 0.155251141552511f ? (x - 0.0729055341958355f) / 10.5402377416545f : SM_EXP2(x * 17.52f - 9.72f);
    if (g == 11) return x < 0.181f ? (x - 0.125f) / 5.6f : SM_POW(10.0f, (x - 0.598206f) / 0.241514f) - 0.00873f;
    if (g == 12) return x >= 0.20855532f ? SM_EXP2((x - 0.69336945f) / 0.08550479f) - 0.00964052f
                                        : (x > 0.0f ? SM_SQRT(x / 47.28711236f) - 0.05641088f : -0.05641088f);
    if (g == 13) return sm_dlogm_dec(x);
    if (g == 14) return x <= 0.14f ? (x - 0.0929f) / 6.025f : (SM_POW(10.0f, 3.89616f * x - 2.27752f) - 0.0108f) / 0.9892f;
    if (g == 15) return sm_cine_dec(x, 0.555556f, 0.009468f, 0.344676f, 0.790453f, 8.735631f, 0.092864f, 0.00089f);
    if (g == 16) return sm_cine_dec(x, 5.555556f, 0.064829f, 0.245281f, 0.384316f, 8.799461f, 0.092864f, 0.000889f);
    if (g == 17) { float v = x * 1023.0f; return v < 452.0f ? SM_POW(v / 650.0f, 3.0f) - 0.0075f : SM_EXP((v - 619.0f) / 150.0f); }
    if (g == 18) return sm_clog_dec(x, 0.24136077f, 87.09937546f, 0.092864125f) * 0.9f;
    if (g == 19) return sm_clog3_dec(x) * 0.9f;
    if (g == 20) return sm_cine_dec(x, 5.555556f, 0.052272f, 0.247190f, 0.385537f, 5.367655f, 0.092809f, 0.010591f);
    if (g == 21) return x >= 0.0f ? (SM_EXP2(14.0f * (x - SM_LC4_C) / SM_LC4_B + 6.0f) - 64.0f) / SM_LC4_A
                                  : x * SM_LC4_S + SM_LC4_T;
    if (g == 22) return x < 0.0f ? x / 15.1927f - 0.01f : (SM_POW(10.0f, x / 0.224282f) - 1.0f) / 155.975327f - 0.01f;
    if (g == 23) return x < 0.13388378308667f ? (x - 0.09246575342465753f) / 8.283605932402494f
                                                 : SM_EXP((x - 0.5300133392291939f) / 0.08692876065491224f) - 0.005494072432257808f;
    if (g == 24) return sm_hlg_dec(x);
    return x;
}
