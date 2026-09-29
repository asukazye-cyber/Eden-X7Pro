// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace Shader::IR { struct Program; }

namespace Vulkan {
class Device;
}

namespace Vulkan::X7NX {

enum class DescriptorLegalization {
    NativeNullDescriptor,
    DummyDescriptor,
};

// Central policy for descriptor backing, separate from instruction-level IR legalization below.
[[nodiscard]] DescriptorLegalization SelectDescriptorLegalization(const Device& device);
[[nodiscard]] bool NeedsDummyDescriptor(const Device& device);
// An actual IR pass, called after translation and before SPIR-V for graphics and compute.
void LegalizeShader(const Device& device, Shader::IR::Program& program);

} // namespace Vulkan::X7NX
