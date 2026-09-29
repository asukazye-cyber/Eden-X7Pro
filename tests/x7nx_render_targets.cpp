// SPDX-License-Identifier: GPL-3.0-or-later
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include "video_core/renderer_vulkan/x7nx_render_targets.h"

using Targets = Vulkan::X7NX::MaliRenderTargetManager;
using Feedback = Vulkan::X7NX::MaliFramebufferFeedbackManager;

int main() {
    for (unsigned flags = 0; flags < 32; ++flags) {
        const bool eligible = Targets::Eligible(flags & 1, flags & 2,
            flags & 4 ? 4 : 1, flags & 8, flags & 16);
        assert(eligible == (flags == 3));
    }
    Targets targets;
    int framebuffer_a{}, framebuffer_b{};
    assert(!targets.CanContinue(&framebuffer_a, 1280, 720));
    targets.Begin(&framebuffer_a, 1280, 720, true);
    for (int draw = 0; draw < 1000; ++draw)
        assert(targets.CanContinue(&framebuffer_a, 1280, 720));
    assert(!targets.CanContinue(&framebuffer_b, 1280, 720));
    assert(!targets.CanContinue(&framebuffer_a, 1600, 900));
    targets.End(); // transfers, barriers, submissions and framebuffer switches use this path
    assert(!targets.CanContinue(&framebuffer_a, 1280, 720));
    targets.Begin(&framebuffer_a, 1280, 720, false); // non-target, MSAA, resolve, discard, base pass
    assert(!targets.CanContinue(&framebuffer_a, 1280, 720));

    constexpr Feedback::Range a{0, 1, 0, 1};
    static_assert(Feedback::MayOverlap(a, a, true));
    static_assert(!Feedback::MayOverlap(a, {1, 1, 0, 1}, true));
    static_assert(!Feedback::MayOverlap(a, {0, 1, 1, 1}, true));
    static_assert(Feedback::MayOverlap(a, {1, 1, 0, 1}, false));
    static_assert(Feedback::MayOverlap(a, {-1, 1, 0, 1}, true));
    static_assert(Feedback::MayOverlap(a, {0, 0, 0, 1}, true));
    static_assert(Feedback::MayOverlap(a, {0, 1, 0, -1}, true));
    constexpr auto maximum = std::numeric_limits<std::int32_t>::max();
    static_assert(Feedback::MayOverlap({maximum, maximum, 0, 1},
                                      {maximum - 1, 2, 0, 1}, true));
    std::mt19937 rng{720};
    for (int trial = 0; trial < 100000; ++trial) {
        const auto make_range = [&] {
            return Feedback::Range{int(rng() % 16), int(rng() % 8 + 1),
                                   int(rng() % 16), int(rng() % 8 + 1)};
        };
        const auto x = make_range(), y = make_range();
        bool overlap = false;
        for (int mip = x.mip; mip < x.mip + x.mips; ++mip)
            for (int layer = x.layer; layer < x.layer + x.layers; ++layer)
                overlap |= mip >= y.mip && mip < y.mip + y.mips &&
                           layer >= y.layer && layer < y.layer + y.layers;
        assert(Feedback::MayOverlap(x, y, true) == overlap);
        assert(Feedback::MayOverlap(y, x, true) == overlap);
    }
    std::cout << "X7NX render-target lifetime, eligibility and 100000 subresource pairs: OK\n";
}
