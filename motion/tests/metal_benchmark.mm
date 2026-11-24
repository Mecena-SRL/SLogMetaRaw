// Metal and CPU run the same engine source (blur_core.inc). Check that both
// produce the same picture, then time Full HD and UHD renders.
#import <Metal/Metal.h>
#include "blur.hpp"
#include "exposure.hpp"
#include <chrono>
#include <cstdio>
namespace motion { bool metalBlur(const blur::Job&, Image, const int*); }
using namespace motion;
struct Frames {
    std::vector<float> data[5];
    Image image[5];
    Frames(int w, int h) {
        for (int f = 0; f < 5; ++f) {
            data[f].resize(size_t(w) * h * 4);
            image[f] = {data[f].data(), 0, 0, w, h, w * 16};
            if (f >= 3) continue;
            for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
                // Camera pan plus a block moving the other way.
                bool block = std::abs(x - w / 2 + 6 * f) < w / 8 && std::abs(y - h / 2) < h / 5;
                int u = block ? x + 6 * f : x - 4 * f;
                unsigned seed = unsigned((u % w + w) % w) + unsigned(y) * 1009 + (block ? 77u : 0u);
                seed = (seed ^ (seed >> 16)) * 0x45d9f3bU;
                float v = float(seed % 251) / 255.f;
                auto px = image[f].at(x, y);
                px[0] = v * 1.6f; px[1] = v; px[2] = .5f * v; px[3] = 1.f;
            }
        }
    }
    blur::Job job(bool draft, int curve, const CameraPair* pairs) {
        blur::Job j;
        j.center = image[1]; j.prev = &image[0]; j.next = &image[2];
        j.exposure = .5f; j.curve = curve; j.draft = draft; j.curved = pairs != nullptr; j.camera = pairs;
        return j;
    }
};
int main() {
    @autoreleasepool {
        auto device = MTLCreateSystemDefaultDevice();
        if (!device) { std::puts("Metal non disponibile: percorso CPU."); return 0; }
        Frames small(320, 128);
        int window[] = {0, 0, 320, 128};
        CameraSamples center(small.image[1]);
        CameraPair pairs[2];
        for (int side = 0; side < 2; ++side) pairs[side] = analysePair(center, CameraSamples(small.image[2 * side]), 320, 128).camera;
        for (int cinema = 0; cinema < 2; ++cinema)
            for (bool draft : {false, true})
                for (int curve : {Linear, SLog3, Intermediate}) {
                    auto job = small.job(draft, curve, cinema ? pairs : nullptr);
                    if (!metalBlur(job, small.image[3], window)) { std::fputs("Errore nel render Metal\n", stderr); return 1; }
                    if (!blur::render(job, small.image[4], window)) return 2;
                    double error = 0; int count = 0;
                    for (size_t k = 0; k < small.data[3].size(); ++k) {
                        if (!std::isfinite(small.data[3][k])) return 3;
                        if (k % 4 == 3) { if (small.data[3][k] != small.data[4][k]) return 4; continue; }
                        error += std::abs(small.data[3][k] - small.data[4][k]); ++count;
                    }
                    std::printf("Metal / CPU | %s | %s | curva %d | MAE %.7f\n", cinema ? "Cinema" : "Base", draft ? "Fluida" : "Alta", curve, error / count);
                    if (error / count > .002) return 5;
                }
        for (int w : {1920, 3840}) {
            Frames f(w, w * 9 / 16);
            int bounds[] = {0, 0, w, w * 9 / 16};
            for (bool draft : {true, false}) {
                std::vector<double> elapsed;
                for (int i = 0; i < 4; ++i) {
                    auto start = std::chrono::steady_clock::now();
                    CameraSamples c(f.image[1]);
                    CameraPair p[] = {analysePair(c, CameraSamples(f.image[0]), w, w * 9 / 16).camera,
                                      analysePair(c, CameraSamples(f.image[2]), w, w * 9 / 16).camera};
                    if (!metalBlur(f.job(draft, Linear, p), f.image[3], bounds)) return 6;
                    if (i) elapsed.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
                }
                std::sort(elapsed.begin(), elapsed.end());
                std::printf("Cinema · analisi + Metal | %s | %dx%d | %s | %.1f ms/frame (%.1f fps)\n",
                            device.name.UTF8String, w, w * 9 / 16, draft ? "Fluida" : "Alta qualità", elapsed[1], 1000 / elapsed[1]);
            }
        }
        return 0;
    }
}
