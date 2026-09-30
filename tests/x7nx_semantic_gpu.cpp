// SPDX-License-Identifier: GPL-3.0-or-later
#include <cassert>
#include <array>
#include <iostream>
#include <random>
#include "video_core/renderer_vulkan/x7_render_ir.h"
using namespace Vulkan::X7NX;
RenderResource Resource(std::uint64_t id) {
    return {id, {id * 4096, 256}, {id * 8192, 256}, 1, 8, 8, 0, 0, 1, 1, true};
}
X7RenderIR Block(RenderResource clear, RenderResource source, RenderResource destination,
                 DirtyRect region = {0, 0, 8, 8}) {
    X7RenderIR ir;
    ir.operations[0] = {RenderOp::Clear, {}, clear, {0, 0, 8, 8}, true, false};
    ir.operations[1] = {RenderOp::CopyImage, source, destination, region, true, true};
    ir.size = 2;
    return ir;
}
int main() {
    static_assert(SemanticModeEnabled(0, true) && !SemanticModeEnabled(0, false));
    static_assert(SemanticModeEnabled(1, false) && !SemanticModeEnabled(2, true));
    static_assert(!SemanticModeEnabled(99, true));
    auto a = Resource(1), b = Resource(2);
    RenderGraphTemplateCache cache;
    auto ir = Block(a, b, a);
    assert(ResourceDependencyGraph::Build(ir).hazards == WAW);
    assert(!cache.Get(ir).emit_clear);
    assert(cache.Get(Block(a, a, a)).emit_clear); // RAW + WAW: source observes the clear
    auto raw = ResourceDependencyGraph::Build(Block(a, a, b));
    assert(raw.hazards == RAW);
    ir.operations[0] = {RenderOp::Observe, a, {}, {}, false, true};
    assert(ResourceDependencyGraph::Build(ir).hazards == WAR);
    auto unknown = b;
    unknown.trustworthy = false;
    assert(cache.Get(Block(a, unknown, a)).emit_clear);
    unknown = b;
    unknown.cpu_memory = a.cpu_memory;
    assert(cache.Get(Block(a, unknown, a)).emit_clear); // distinct images, overlapping guest backing
    unknown = b;
    unknown.guest_memory.address = std::numeric_limits<std::uint64_t>::max() - 1;
    assert(cache.Get(Block(a, unknown, a)).emit_clear); // overflow is unknown, not disjoint
    auto different_mip = a;
    different_mip.mip = 1;
    assert(cache.Get(Block(a, b, different_mip)).emit_clear);
    auto different_format = a;
    different_format.format = 9;
    assert(cache.Get(Block(a, b, different_format)).emit_clear);
    assert(cache.Get(Block(a, b, a, {1, 0, 7, 8})).emit_clear);
    assert(cache.Get(Block(a, b, a, {})).emit_clear);
    ir = Block(a, b, a);
    ir.operations[1] = {RenderOp::Observe, a, {}, {}, false, true};
    auto observe = cache.Get(ir);
    assert(observe.emit_clear && !observe.emit_copy);
    const auto before = cache.Hits();
    assert(!cache.Get(Block(Resource(7), Resource(8), Resource(7))).emit_clear);
    assert(cache.Hits() > before); // structural reuse, no cached handles/addresses
    VirtualFramebuffer vf;
    vf.Clear(a, {0, 0, 8, 8});
    assert(vf.state == Materialization::PendingClear);
    vf.Materialize();
    assert(vf.state == Materialization::Materialized);
    vf.Clear(a, {0, 0, 8, 8});
    vf.DiscardUnobserved();
    assert(vf.state == Materialization::NoPendingContent);

    std::mt19937 rng{8400};
    unsigned eliminated = 0;
    for (unsigned trial = 0; trial < 10000; ++trial) {
        const bool self_copy = rng() % 2;
        const unsigned width = 1 + rng() % 8, height = 1 + rng() % 8;
        std::array<unsigned, 64> baseline, optimized, input;
        for (unsigned i = 0; i < 64; ++i) {
            baseline[i] = optimized[i] = rng();
            input[i] = rng();
        }
        const unsigned clear = rng();
        baseline.fill(clear);
        const auto source_pixels = self_copy ? baseline : input;
        for (unsigned y = 0; y < height; ++y)
            for (unsigned x = 0; x < width; ++x) baseline[y * 8 + x] = source_pixels[y * 8 + x];
        const auto program = cache.Get(Block(a, self_copy ? a : b, a, {0, 0, width, height}));
        unsigned copies = 0;
        program.Execute([&](bool emit_clear) { if (emit_clear) optimized.fill(clear); }, [&] {
            ++copies;
            const auto current_source = self_copy ? optimized : input;
            for (unsigned y = 0; y < height; ++y)
                for (unsigned x = 0; x < width; ++x) optimized[y * 8 + x] = current_source[y * 8 + x];
        });
        assert(copies == 1 && baseline == optimized);
        eliminated += !program.emit_clear;
    }
    assert(eliminated > 0);
    std::cout << "Semantic GPU: RAW/WAR/WAW, alias guards, materialization, cache reuse and 10000 differential blocks passed; "
              << eliminated << " dead clears removed in generated tests (not game measurements)\n";
}
