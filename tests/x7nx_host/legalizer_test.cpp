#include <cassert>
#include <cstdint>
#include <iostream>
#include <random>
#include "shader_recompiler/frontend/ir/ir_emitter.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/renderer_vulkan/x7nx_shader_legalizer.h"

namespace IR = Shader::IR;
u32 Evaluate(IR::Value value) {
    if (value.IsImmediate()) return value.U32();
    const auto& inst = *value.Inst();
    const u32 a = Evaluate(inst.Arg(0));
    const u32 b = Evaluate(inst.Arg(1));
    switch (inst.GetOpcode()) {
    case IR::Opcode::ShiftLeftLogical32: assert(b < 32); return a << b;
    case IR::Opcode::ShiftRightLogical32: assert(b < 32); return a >> b;
    case IR::Opcode::ShiftRightArithmetic32: assert(b < 32); return static_cast<s32>(a) >> b;
    case IR::Opcode::BitwiseAnd32: return a & b;
    default: throw std::runtime_error("Unexpected opcode after lowering");
    }
}

int main() {
    Vulkan::Device device;
    std::mt19937 random{720};
    size_t checks = 0;
    for (u32 offset = 0; offset <= 32; ++offset) {
        for (u32 width = 0; width <= 32 - offset; ++width) {
            for (bool sign : {false, true}) {
                for (unsigned iteration = 0; iteration < 32; ++iteration) {
                    const u32 source = iteration == 0 ? 0 : iteration == 1 ? ~u32{} : random();
                    Shader::ObjectPool<IR::Inst> pool{32};
                    IR::Block block{pool};
                    IR::Program program;
                    program.blocks.push_back(&block);
                    program.post_order_blocks.push_back(&block);
                    IR::IREmitter ir{block};
                    const auto output = ir.BitFieldExtract(ir.Imm32(source), ir.Imm32(offset), ir.Imm32(width), sign);
                    block.AppendNewInst(IR::Opcode::WriteStorage32, {ir.Imm32(0), ir.Imm32(0), output});
                    auto& consumer = block.back();
                    Vulkan::X7NX::LegalizeShader(device, program);
                    u64 expected = (static_cast<u64>(source) >> offset) & ((u64{1} << width) - 1);
                    if (sign && width && (expected & (u64{1} << (width - 1)))) expected |= ~((u64{1} << width) - 1);
                    assert(Evaluate(consumer.Arg(2)) == static_cast<u32>(expected));
                    ++checks;
                }
            }
        }
    }
    // Non-target backends, dynamic operands and invalid immediate ranges must remain unchanged.
    for (unsigned scenario = 0; scenario < 3; ++scenario) {
        Shader::ObjectPool<IR::Inst> pool{32};
        IR::Block block{pool};
        IR::Program program;
        program.blocks.push_back(&block);
        program.post_order_blocks.push_back(&block);
        IR::IREmitter ir{block};
        block.AppendNewInst(IR::Opcode::LoadStorage32, {ir.Imm32(0), ir.Imm32(0)});
        const IR::U32 dynamic{&block.back()};
        const auto offset = scenario == 1 ? dynamic : ir.Imm32(scenario == 2 ? 32u : 0u);
        const auto output = ir.BitFieldExtract(ir.Imm32(123u), offset, ir.Imm32(3u), false);
        block.AppendNewInst(IR::Opcode::WriteStorage32, {ir.Imm32(0), ir.Imm32(0), output});
        auto& consumer = block.back();
        device.backend.enabled = scenario != 0;
        Vulkan::X7NX::LegalizeShader(device, program);
        assert(consumer.Arg(2).Inst()->GetOpcode() == IR::Opcode::BitFieldUExtract);
    }
    std::cout << "Actual X7NX IR pass: " << checks << " cases passed\n";
}
