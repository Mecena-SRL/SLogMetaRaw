// SPDX-License-Identifier: GPL-3.0-or-later
// Runs the Creative node's CPU pipeline (src/creative/CreativeEngine.cpp) for tests/test_creative.py.
// stdin:  W H space gamma frame frames threads
//         name=value ... (panel controls by their OFX names, one line)
//         frames * W*H lines "r g b" (encoded input; frame 0 is the current one, then the following ones; when the
//         medium shows the previous picture and frames > 1, the last frame is the previous one; when a pickup tube
//         remembers the frames before, frames 1, 2, 3 are 1, 2, 3 frames before this one)
// stdout: W*H lines "r g b"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "../ofx/SLogMetaRaw/src/creative/CreativeEngine.h"
#include "../ofx/SLogMetaRaw/src/creative/CreativePanel.h"

static void set(CreativeControls& c, const std::string& name, double v)
{
    if (name == "process") { c.process = (int)v; return; }
    if (name == "mediumLoaded") { c.mediumLoaded = (int)v; return; }
    if (name == "deviceLoaded") { c.deviceLoaded = (int)v; return; }
    for (int i = 0; i < kCrMakerCount; ++i)
        if (name == kCrMakers[i].param) { c.stock[i] = (int)v; return; }
    for (const CrToggleDef& d : kCrToggles)
        if (name == d.name) { c.*(d.field) = (int)v; return; }
    for (const CrChoiceDef& d : kCrChoices)
        if (name == d.name) { c.*(d.field) = (int)v; return; }
    for (const CrSliderDef& d : kCrSliders)
        if (name == d.name) { c.*(d.field) = v; return; }
    fprintf(stderr, "controllo sconosciuto %s\n", name.c_str());
    exit(2);
}

int main()
{
    int W, H, space, gamma, frame, frames, threads;
    char line[8192];
    if (!fgets(line, sizeof(line), stdin)
        || sscanf(line, "%d %d %d %d %d %d %d", &W, &H, &space, &gamma, &frame, &frames, &threads) != 7 || frames < 1)
        return 2;
    if (!fgets(line, sizeof(line), stdin)) return 2;   // the controls, possibly none
    CreativeControls c;
    std::istringstream in(line);
    std::string kv;
    while (in >> kv) {
        const size_t eq = kv.find('=');
        set(c, kv.substr(0, eq), atof(kv.substr(eq + 1).c_str()));
    }
    const size_t N = (size_t)W * H;
    std::vector<std::vector<float>> src(frames, std::vector<float>(N * 4));
    for (int f = 0; f < frames; ++f)
        for (size_t i = 0; i < N; ++i) {
            float* px = &src[f][i * 4];
            if (!fgets(line, sizeof(line), stdin) || sscanf(line, "%f %f %f", px, px + 1, px + 2) != 3) return 2;
            px[3] = 1.0f;
        }
    const bool previous = creativeNeedsPrevious(c) && frames > 1;
    const int ahead = previous ? frames - 1 : frames;
    double t[3];
    creativeRecordTimes(c, t);
    const float* inputs[3];
    for (int k = 0; k < 3; ++k) {
        const int f = (int)std::lround(t[k]);
        inputs[k] = src[f >= 0 && f < ahead ? f : 0].data();   // past the clip's end: this frame, as the plugin
    }
    const int history = std::min(creativeHistory(c), frames - 1);
    CreativeHistory hist = {};
    for (int k = 0; k < history; ++k) hist.src[k] = src[1 + k].data();
    CreativeParams p;
    creativePrepare(p, c, W, H, frame, space, gamma, history);
    CreativePrevious prev;
    if (previous) {
        creativePrepare(prev.p, c, W, H, frame - 1, space, gamma);
        for (int k = 0; k < 3; ++k) prev.src[k] = src[frames - 1].data();
    }
    CreativeParallel par = [threads](int n, const std::function<void(int, int)>& fn) {
        std::vector<std::thread> pool;
        for (int i = 0; i < threads; ++i)
            pool.emplace_back([&, i] { fn((int)((long long)n * i / threads), (int)((long long)n * (i + 1) / threads)); });
        for (std::thread& th : pool) th.join();
    };
    CreativeScratch s;
    std::vector<float> dst(N * 4);
    CreativeParams copy;
    const bool transfer = creativePrepareTransfer(copy, c, W, H, frame);
    creativeRenderCPU(p, inputs, (size_t)W * 4, dst.data(), (size_t)W * 4, par, s, previous ? &prev : nullptr, &hist,
                      transfer ? &copy : nullptr);
    for (size_t i = 0; i < N; ++i) printf("%.7g %.7g %.7g\n", dst[i * 4], dst[i * 4 + 1], dst[i * 4 + 2]);
    return 0;
}
