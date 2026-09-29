// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "video_core/renderer_vulkan/x7nx_shader_legalizer.h"

#include "video_core/vulkan_common/vulkan_device.h"
#include "shader_recompiler/frontend/ir/ir_emitter.h"
#include "shader_recompiler/ir_opt/passes.h"

namespace Vulkan::X7NX {

DescriptorLegalization SelectDescriptorLegalization(const Device& device) {
    return device.X7Backend().UseNativeNullDescriptors(device.HasNullDescriptor()) ? DescriptorLegalization::NativeNullDescriptor
                                      : DescriptorLegalization::DummyDescriptor;
}

bool NeedsDummyDescriptor(const Device& device) {
    return SelectDescriptorLegalization(device) == DescriptorLegalization::DummyDescriptor;
}

void LegalizeShader(const Device& device, Shader::IR::Program& program) {
    if (!device.X7Backend().LegalizeBitfields()) return;
    namespace IR = Shader::IR;
    bool changed = false;
    for (auto* block : program.blocks) {
        for (auto& inst : block->Instructions()) {
            const bool sign = inst.GetOpcode() == IR::Opcode::BitFieldSExtract;
            if (!sign && inst.GetOpcode() != IR::Opcode::BitFieldUExtract) continue;
            if (!inst.Arg(1).IsImmediate() || !inst.Arg(2).IsImmediate()) continue;
            const u32 offset = inst.Arg(1).U32(), width = inst.Arg(2).U32();
            // Preserve all undefined/dynamic cases for the original translator. No shift by 32.
            if (offset > 32 || width > 32 - offset) continue;
            IR::IREmitter ir(*block, IR::Block::InstructionList::s_iterator_to(inst));
            const IR::U32 source{inst.Arg(0)};
            if (!width) inst.ReplaceUsesWith(ir.Imm32(0));
            else if (width == 32) inst.ReplaceUsesWith(source);
            else if (sign) {
                const IR::U32 shifted = ir.ShiftLeftLogical(source, ir.Imm32(32 - offset - width));
                inst.ReplaceUsesWith(ir.ShiftRightArithmetic(shifted, ir.Imm32(32 - width)));
            } else {
                const IR::U32 shifted = ir.ShiftRightLogical(source, ir.Imm32(offset));
                inst.ReplaceUsesWith(ir.BitwiseAnd(shifted, ir.Imm32((u32{1} << width) - 1)));
            }
            changed = true;
        }
    }
    if (changed) {
        Shader::Optimization::IdentityRemovalPass(program);
        Shader::Optimization::DeadCodeEliminationPass(program);
    }
}

} // namespace Vulkan::X7NX
