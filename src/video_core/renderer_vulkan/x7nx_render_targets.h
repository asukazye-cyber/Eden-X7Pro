// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>

namespace Vulkan::X7NX {
// Tracks only an eligible, already-open CLEAR/STORE scope. It never keeps a scope alive across
// transfers, feedback barriers, submissions, framebuffer changes, resolves or discarded stores.
class MaliRenderTargetManager {
public:
    static constexpr bool Eligible(bool enabled, bool clear_variant, std::uint32_t samples,
                                   bool resolves, bool discards) {
        return enabled && clear_variant && samples == 1 && !resolves && !discards;
    }
    void Begin(const void* framebuffer, std::uint32_t width, std::uint32_t height,
               bool clear_store_only) {
        identity = clear_store_only ? framebuffer : nullptr;
        area_width = width;
        area_height = height;
    }
    bool CanContinue(const void* framebuffer, std::uint32_t width, std::uint32_t height) const {
        return identity && identity == framebuffer && width == area_width && height == area_height;
    }
    void End() { identity = nullptr; }
private:
    const void* identity{};
    std::uint32_t area_width{}, area_height{};
};

class MaliFramebufferFeedbackManager {
public:
    struct Range {
        std::int32_t mip, mips, layer, layers;
    };
    // Unknown/reinterpreted/sliced resources retain the existing barrier. Caller must establish
    // that both ranges refer to the same physical 2D image and unchanged format/sample count.
    static constexpr bool MayOverlap(Range a, Range b, bool comparable) {
        if (!comparable || !Valid(a) || !Valid(b)) return true;
        return Intersects(a.mip, a.mips, b.mip, b.mips) &&
               Intersects(a.layer, a.layers, b.layer, b.layers);
    }
private:
    static constexpr bool Valid(Range r) {
        return r.mip >= 0 && r.layer >= 0 && r.mips > 0 && r.layers > 0;
    }
    static constexpr bool Intersects(std::int32_t a, std::int32_t n,
                                     std::int32_t b, std::int32_t m) {
        return std::int64_t{a} < std::int64_t{b} + m &&
               std::int64_t{b} < std::int64_t{a} + n;
    }
};
} // namespace Vulkan::X7NX
