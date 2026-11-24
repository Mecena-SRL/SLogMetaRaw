// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "motion.hpp"
#include <optional>

namespace motion {
// A moving step edge seen through a box shutter becomes a ramp. Its width
// divided by its displacement on the SAME scanline estimates the fraction of
// the sampling interval exposed. This also works on slanted edges. Real scene
// edges are not always steps; only use mutually consistent, isolated tracks.
struct ExposureEdge {
    float center, width, left, right, farLeft, farRight;
};

inline std::vector<ExposureEdge> exposureEdges(Image im, bool horizontal, int line, int curve) {
    int length = horizontal ? im.w : im.h;
    std::vector<float> values(length), smooth(length);
    for (int i = 0; i < length; ++i) {
        auto p = im.at(im.x0 + (horizontal ? i : line), im.y0 + (horizontal ? line : i));
        values[i] = .25f * decode(p[0], curve) + .5f * decode(p[1], curve) + .25f * decode(p[2], curve);
    }
    for (int i = 1; i + 1 < length; ++i)
        smooth[i] = .25f * (values[i-1] + 2 * values[i] + values[i+1]);

    std::vector<ExposureEdge> edges;
    for (int i = 4; i + 5 < length;) {
        const int start = i++;
        float diff = smooth[i] - smooth[start];
        float floor = .00004f * (1.f + std::abs(smooth[start]));
        if (!std::isfinite(diff) || std::abs(diff) <= floor) continue;
        float sign = diff > 0 ? 1.f : -1.f;
        while (i + 5 < length && sign * (smooth[i+1] - smooth[i]) > floor) ++i;
        float left = smooth[start], right = smooth[i], contrast = std::abs(right - left);
        if (contrast < .009f + .07f * std::max(std::abs(left), std::abs(right))) continue;
        if (std::max(std::abs(smooth[start-3]-left), std::abs(smooth[i+3]-right)) > .09f * contrast) continue;

        auto crossing = [&](float fraction) {
            float level = left + fraction * (right-left);
            int j = start;
            while (j+1 < i && sign * (smooth[j+1]-level) < 0) ++j;
            return j + (level-smooth[j]) / (smooth[j+1]-smooth[j]);
        };
        float center = crossing(.5f), q1 = crossing(.25f), q3 = crossing(.75f);
        float width = 2.f * (q3-q1);
        if (std::abs(q1+q3-2*center) > .08f * width + .3f) continue;
        if (crossing(.9f)-crossing(.1f) > .93f * width + .6f) continue;
        edges.push_back({center, width, left, right, smooth[std::max(1,start-12)], smooth[std::min(length-2,i+12)]});
    }
    return edges;
}

inline float edgeCost(const ExposureEdge& a, const ExposureEdge& b) {
    if ((a.right-a.left)*(b.right-b.left) <= 0) return 100.f;
    float contrast = std::abs(a.right-a.left);
    return (std::abs(a.left-b.left) + std::abs(a.right-b.right)
          + .25f*(std::abs(a.farLeft-b.farLeft)+std::abs(a.farRight-b.farRight))) / contrast
          + .35f * std::abs(a.width-b.width)/std::max(a.width,2.f);
}

inline void exposureTracks(const std::vector<ExposureEdge>& before, const std::vector<ExposureEdge>& now,
                           const std::vector<ExposureEdge>& after, int range, std::vector<float>& fractions) {
    auto first = [](const auto& edges, float x) {
        return std::lower_bound(edges.begin(), edges.end(), x,
                                [](const ExposureEdge& edge, float pos){return edge.center < pos;});
    };
    for (const auto& edge : now) {
        float best = .55f, second = 100.f, displacement = 0;
        for (auto prev=first(before,edge.center-range); prev!=before.end() && prev->center<=edge.center+range; ++prev) {
            float pastCost = edgeCost(edge,*prev);
            if (pastCost > best+.06f) continue;
            float dx = edge.center-prev->center, expected = edge.center+dx;
            float tolerance = 1.25f + .12f * std::abs(dx);
            for (auto next=first(after,expected-tolerance); next!=after.end() && next->center<=expected+tolerance; ++next) {
                float cost = pastCost + edgeCost(edge,*next) + .3f*std::abs(next->center-expected)/tolerance;
                if (cost < best) {
                    second = best;
                    best = cost;
                    displacement = .5f * std::abs(next->center-prev->center);
                } else second = std::min(second,cost);
            }
        }
        // Repetitive decoration, subpixel motions and stationary edges cannot
        // establish a shutter length independently of sampling and lens softness.
        if (displacement < 5.f || second-best < .06f) continue;
        // Below the sampling footprint this is only an upper bound, not a
        // measured zero exposure that could contradict a camera tag.
        if (edge.width >= 2.4f && edge.width <= 1.6f * displacement)
            fractions.push_back(edge.width/displacement);
    }
}

inline std::optional<double> imageExposureFraction(Image before, Image now, Image after, int curve) {
    std::vector<float> fractions;
    int supportedLines = 0;
    for (bool horizontal : {true, false}) {
        int cross = horizontal ? now.h : now.w, length = horizontal ? now.w : now.h;
        int lines = std::min(16,cross/4);
        for (int i = 0; i < lines; ++i) {
            int line = (2*i+1)*cross/(2*lines);
            auto center = exposureEdges(now,horizontal,line,curve);
            if (center.empty()) continue;
            auto prev = exposureEdges(before,horizontal,line,curve);
            auto next = exposureEdges(after,horizontal,line,curve);
            auto size = fractions.size();
            exposureTracks(prev,center,next,std::min(384,length/3),fractions);
            if (fractions.size() > size) ++supportedLines;
        }
    }
    if (fractions.size() < 8 || supportedLines < 3) return std::nullopt;
    std::sort(fractions.begin(),fractions.end());
    float lower = fractions[fractions.size()/4], median = fractions[fractions.size()/2];
    float upper = fractions[3*fractions.size()/4];
    if (upper-lower > .045f + .20f*median) return std::nullopt;
    return median;
}
}
