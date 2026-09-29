// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "video_core/renderer_vulkan/x7nx_shader_legalizer.h"

#include "video_core/vulkan_common/vulkan_device.h"

namespace Vulkan::X7NX {

DescriptorLegalization SelectDescriptorLegalization(const Device& device) {
    return device.HasNullDescriptor() ? DescriptorLegalization::NativeNullDescriptor
                                      : DescriptorLegalization::DummyDescriptor;
}

bool NeedsDummyDescriptor(const Device& device) {
    return SelectDescriptorLegalization(device) == DescriptorLegalization::DummyDescriptor;
}

} // namespace Vulkan::X7NX
