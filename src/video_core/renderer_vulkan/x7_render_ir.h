// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace Vulkan::X7NX {
constexpr bool SemanticModeEnabled(std::uint32_t mode, bool target_device) {
    return mode == 1 || (mode == 0 && target_device);
}
struct MemoryRange {
    std::uint64_t address{}, size{};
    bool Valid() const {
        return address && size && size <= std::numeric_limits<std::uint64_t>::max() - address;
    }
    bool Disjoint(const MemoryRange& other) const {
        return Valid() && other.Valid() &&
            (address + size <= other.address || other.address + other.size <= address);
    }
};
struct DirtyRect {
    std::uint32_t x{}, y{}, width{}, height{};
    bool Contains(const DirtyRect& other) const {
        return width && height && other.width && other.height && x <= other.x && y <= other.y &&
            std::uint64_t{x} + width >= std::uint64_t{other.x} + other.width &&
            std::uint64_t{y} + height >= std::uint64_t{other.y} + other.height;
    }
};
struct RenderResource {
    std::uint64_t backing{};
    MemoryRange guest_memory{}, cpu_memory{};
    std::uint32_t format{}, width{}, height{}, mip{}, layer{}, layers{1}, aspect{1};
    bool trustworthy{};
};
enum class AliasRelation : std::uint8_t { Disjoint, ExactSubresource, Possible };
struct ResourceAliasAnalyzer {
    static AliasRelation Analyze(const RenderResource& a, const RenderResource& b) {
        if (!a.trustworthy || !b.trustworthy || !a.backing || !b.backing ||
            !a.layers || !b.layers || !a.aspect || !b.aspect)
            return AliasRelation::Possible;
        if (a.backing == b.backing) {
            if (a.mip != b.mip || (std::uint64_t{a.layer} + a.layers <= b.layer) ||
                (std::uint64_t{b.layer} + b.layers <= a.layer) || !(a.aspect & b.aspect))
                return AliasRelation::Disjoint;
            if (a.format == b.format && a.mip == b.mip && a.layer == b.layer &&
                a.layers == b.layers && a.aspect == b.aspect)
                return AliasRelation::ExactSubresource;
            return AliasRelation::Possible;
        }
        // Check both address spaces: remapped/overlapping guest ranges must never be guessed safe.
        if (a.cpu_memory.Disjoint(b.cpu_memory) && a.guest_memory.Disjoint(b.guest_memory))
            return AliasRelation::Disjoint;
        return AliasRelation::Possible;
    }
};
enum class RenderOp : std::uint8_t { Clear, CopyImage, Observe };
struct RenderOperation {
    RenderOp kind{RenderOp::Observe};
    RenderResource read{}, write{};
    DirtyRect written_region{};
    bool writes{}, reads{};
};
// Bounded semantic block, not Vulkan commands. Values/handles are live bindings, never cached.
struct X7RenderIR {
    std::array<RenderOperation, 2> operations{};
    std::uint32_t size{};
};
enum Hazard : std::uint8_t { RAW = 1, WAR = 2, WAW = 4 };
struct ResourceDependencyGraph {
    std::uint8_t hazards{};
    AliasRelation overwrite{AliasRelation::Possible};
    bool covers_dirty{};
    static ResourceDependencyGraph Build(const X7RenderIR& ir) {
        ResourceDependencyGraph result;
        if (ir.size != 2) return result;
        const auto& a = ir.operations[0];
        const auto& b = ir.operations[1];
        if (a.writes && b.reads && ResourceAliasAnalyzer::Analyze(a.write, b.read) != AliasRelation::Disjoint)
            result.hazards |= RAW;
        if (a.reads && b.writes && ResourceAliasAnalyzer::Analyze(a.read, b.write) != AliasRelation::Disjoint)
            result.hazards |= WAR;
        if (a.writes && b.writes) {
            result.overwrite = ResourceAliasAnalyzer::Analyze(a.write, b.write);
            if (result.overwrite != AliasRelation::Disjoint) result.hazards |= WAW;
            result.covers_dirty = b.written_region.Contains(a.written_region);
        }
        return result;
    }
};
enum class Materialization : std::uint8_t { NoPendingContent, PendingClear, Materialized };
// Virtualizes pending CONTENT, not the already allocated VkImage. No claim of lazy allocation.
struct VirtualFramebuffer {
    RenderResource resource{};
    DirtyRect dirty{};
    Materialization state{Materialization::NoPendingContent};
    void Clear(const RenderResource& target, DirtyRect region) {
        resource = target;
        dirty = region;
        state = Materialization::PendingClear;
    }
    void Materialize() { state = Materialization::Materialized; }
    void DiscardUnobserved() { state = Materialization::NoPendingContent; }
};
struct MaliRenderProgram {
    bool emit_clear{true};
    bool emit_copy{};
    // Existing copy barriers and copy extent are deliberately retained in this first lowering.
    template <typename Clear, typename Copy>
    void Execute(Clear&& clear, Copy&& copy) const {
        clear(emit_clear);
        if (emit_copy) copy();
    }
};
struct RenderSignature {
    RenderOp first{}, second{};
    std::uint8_t hazards{};
    AliasRelation overwrite{};
    bool covers{}, well_formed{};
    bool operator==(const RenderSignature&) const = default;
};
class RenderGraphCompiler {
public:
    static RenderSignature Analyze(const X7RenderIR& ir) {
        if (ir.size != 2) return {};
        const auto graph = ResourceDependencyGraph::Build(ir);
        return {ir.operations[0].kind, ir.operations[1].kind, graph.hazards,
                graph.overwrite, graph.covers_dirty,
                ir.operations[0].writes && !ir.operations[0].reads &&
                ir.operations[1].reads && ir.operations[1].writes};
    }
    static MaliRenderProgram Compile(RenderSignature signature) {
        const bool copy = signature.second == RenderOp::CopyImage;
        const bool dead_clear = signature.well_formed && signature.first == RenderOp::Clear && copy &&
            signature.overwrite == AliasRelation::ExactSubresource && signature.covers &&
            !(signature.hazards & (RAW | WAR));
        return {!dead_clear, copy};
    }
};
// Small scheduler/session-local structural cache. Recheck resource relationships on every bind;
// never retain guest addresses, VkImage handles, clear values, or command buffers across frames.
class RenderGraphTemplateCache {
public:
    MaliRenderProgram Get(const X7RenderIR& ir) {
        const auto signature = RenderGraphCompiler::Analyze(ir);
        for (std::size_t i = 0; i < count; ++i) {
            if (entries[i].signature == signature) {
                ++hits;
                return entries[i].program;
            }
        }
        auto program = RenderGraphCompiler::Compile(signature);
        entries[cursor] = {signature, program};
        cursor = (cursor + 1) % entries.size();
        if (count < entries.size()) ++count;
        return program;
    }
    std::size_t Hits() const { return hits; }
private:
    struct Entry { RenderSignature signature{}; MaliRenderProgram program{}; };
    std::array<Entry, 16> entries{};
    std::size_t count{}, cursor{}, hits{};
};
} // namespace Vulkan::X7NX
