// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "common/common_types.h"

namespace Vulkan {
class Device;
}

namespace Vulkan::X7NX {

struct GameProfile {
    std::string_view name{"default"};
    bool prewarm_pipeline_cache{};
    bool conservative_framebuffer_feedback{};
    bool conservative_depth_stencil{};
};

[[nodiscard]] GameProfile ProfileForTitle(u64 title_id);

// Vulkan driver changes can invalidate performance characteristics even when the guest shader
// cache is structurally valid. Keep X7NX's persistent cache partitioned by API/driver revision.
[[nodiscard]] std::string CacheNamespace(const Device& device);

} // namespace Vulkan::X7NX
