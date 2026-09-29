// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace Vulkan {
class Device;
}

namespace Vulkan::X7NX {

enum class DescriptorLegalization {
    NativeNullDescriptor,
    DummyDescriptor,
};

// Kept next to the guest-IR-to-SPIR-V pipeline so future rewrites (image access bounds and
// unsupported descriptor forms) have a single ownership point. The current valid rewrite is the
// existing Eden dummy image descriptor fallback.
[[nodiscard]] DescriptorLegalization SelectDescriptorLegalization(const Device& device);
[[nodiscard]] bool NeedsDummyDescriptor(const Device& device);

} // namespace Vulkan::X7NX
