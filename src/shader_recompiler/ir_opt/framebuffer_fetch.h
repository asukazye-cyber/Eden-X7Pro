// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/frontend/ir/modifiers.h"

namespace Shader::Optimization {
// Proof for the unscaled path only. No offset, normalized coordinates, filtering, arrays,
// reinterpretation or dynamic LOD is accepted. Runtime must additionally prove attachment alias.
inline bool IsLocalFramebufferFetch(const IR::Program& program) {
    using IR::Opcode;
    const auto& info = program.info;
    if (program.stage != Stage::Fragment || info.texture_descriptors.size() != 1 ||
        !info.texture_buffer_descriptors.empty() || !info.image_descriptors.empty() ||
        !info.image_buffer_descriptors.empty()) return false;
    const auto& desc = info.texture_descriptors.front();
    if (desc.type != TextureType::Color2D || desc.count != 1 || desc.is_depth ||
        desc.is_integer || desc.is_multisample) return false;
    const auto opcode = [](const IR::Value& v, Opcode op) {
        return !v.IsImmediate() && v.InstRecursive()->GetOpcode() == op;
    };
    const auto pixel_axis = [&](IR::Value v, IR::Attribute axis) {
        // Rescaling inserts Select(IsTextureScaled, scaled, original). Runtime forbids scaling.
        if (opcode(v, Opcode::SelectU32)) {
            auto* select = v.InstRecursive();
            if (!opcode(select->Arg(0), Opcode::IsTextureScaled)) return false;
            v = select->Arg(2);
        }
        if (!opcode(v, Opcode::ConvertU32F32) && !opcode(v, Opcode::ConvertS32F32)) return false;
        v = v.InstRecursive()->Arg(0);
        if (opcode(v, Opcode::FPMul32)) {
            auto* mul = v.InstRecursive();
            if (!opcode(mul->Arg(1), Opcode::ResolutionDownFactor)) return false;
            v = mul->Arg(0);
        }
        return opcode(v, Opcode::GetAttribute) && v.InstRecursive()->Arg(0).Attribute() == axis;
    };
    unsigned fetches = 0;
    for (const auto* block : program.blocks) {
        for (const auto& inst : block->Instructions()) {
            const auto op = inst.GetOpcode();
            if (op < Opcode::ImageSampleImplicitLod || op > Opcode::ImageWrite) continue;
            if (op != Opcode::ImageFetch) return false;
            const auto flags = inst.Flags<IR::TextureInstInfo>();
            if (flags.descriptor_index != 0 || flags.type != TextureType::Color2D ||
                flags.is_depth || flags.is_integer || inst.HasAssociatedPseudoOperation() ||
                !inst.Arg(0).IsEmpty() ||
                !inst.Arg(2).IsEmpty() || !inst.Arg(4).IsEmpty() ||
                !inst.Arg(3).IsImmediate() || inst.Arg(3).U32() != 0) return false;
            const auto coords = inst.Arg(1);
            if (!opcode(coords, Opcode::CompositeConstructU32x2)) return false;
            auto* pair = coords.InstRecursive();
            if (!pixel_axis(pair->Arg(0), IR::Attribute::PositionX) ||
                !pixel_axis(pair->Arg(1), IR::Attribute::PositionY)) return false;
            ++fetches;
        }
    }
    return fetches != 0;
}
} // namespace Shader::Optimization
