// SPDX-License-Identifier: GPL-3.0-or-later
#include <cassert>
#include <iostream>
#include "shader_recompiler/frontend/ir/ir_emitter.h"
#include "shader_recompiler/ir_opt/framebuffer_fetch.h"

namespace IR = Shader::IR;
int main() {
    unsigned accepted = 0;
    for (unsigned scenario = 0; scenario < 18; ++scenario) {
        Shader::ObjectPool<IR::Inst> pool{64};
        IR::Block block{pool};
        IR::Program program;
        program.blocks.push_back(&block);
        program.stage = Shader::Stage::Fragment;
        Shader::TextureDescriptor desc{};
        desc.type = Shader::TextureType::Color2D;
        desc.count = 1;
        program.info.texture_descriptors.push_back(desc);
        IR::IREmitter ir{block};
        IR::F32 x = ir.GetAttribute(IR::Attribute::PositionX);
        IR::F32 y = ir.GetAttribute(IR::Attribute::PositionY);
        if (scenario == 1 || scenario == 2) {
            x = ir.FPMul(x, ir.ResolutionDownFactor());
            y = ir.FPMul(y, ir.ResolutionDownFactor());
        }
        IR::U32 ix = ir.ConvertFToU(32, x);
        IR::U32 iy = ir.ConvertFToU(32, y);
        if (scenario == 2) {
            ix = IR::U32{ir.Select(ir.IsTextureScaled(ir.Imm32(0)), ir.Imm32(100), ix)};
            iy = IR::U32{ir.Select(ir.IsTextureScaled(ir.Imm32(0)), ir.Imm32(200), iy)};
        }
        if (scenario == 3) ix = ir.IAdd(ix, ir.Imm32(1));
        if (scenario == 4) iy = ix;
        IR::TextureInstInfo flags{};
        flags.type.Assign(Shader::TextureType::Color2D);
        if (scenario == 5) flags.descriptor_index.Assign(1);
        if (scenario == 6) flags.is_integer.Assign(1);
        const auto coords = ir.CompositeConstruct(ix, iy);
        block.AppendNewInst(IR::Opcode::ImageFetch,
            {scenario == 7 ? IR::Value{ir.Imm32(0)} : IR::Value{}, coords,
             scenario == 8 ? IR::Value{ir.Imm32(0)} : IR::Value{},
             ir.Imm32(scenario == 9 ? 1 : 0),
             scenario == 10 ? IR::Value{ir.Imm32(0)} : IR::Value{}});
        block.back().SetFlags(flags);
        if (scenario == 11) program.stage = Shader::Stage::VertexB;
        if (scenario == 12) program.info.texture_descriptors[0].is_multisample = true;
        if (scenario == 13) program.info.texture_descriptors[0].is_integer = true;
        if (scenario == 14) program.info.texture_descriptors[0].is_depth = true;
        if (scenario == 15) program.info.texture_descriptors[0].count = 2;
        if (scenario == 16) program.info.texture_descriptors[0].type = Shader::TextureType::ColorArray2D;
        if (scenario == 17) block.AppendNewInst(IR::Opcode::ImageQueryDimensions,
                                               {IR::Value{}, ir.Imm32(0), ir.Imm1(false)});
        const bool result = Shader::Optimization::IsLocalFramebufferFetch(program);
        assert(result == (scenario <= 2));
        accepted += result;
    }
    assert(accepted == 3);
    std::cout << "Native framebuffer fetch: 3 exact IR forms accepted, 15 unsafe forms rejected\n";
}
