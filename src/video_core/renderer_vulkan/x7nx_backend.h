// SPDX-FileCopyrightText: Copyright 2026 X7NX Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>

#include "video_core/renderer_vulkan/x7nx_mali_policy.h"

namespace Vulkan {
class Device;
}

namespace Vulkan::X7NX {

// The fork-owned Vulkan boundary. Eden's proven rasterizer, cache and scheduler live behind this
// object; all X7-specific capability probing and future replacement paths enter through here.
class Backend final {
public:
    void Initialize(const Device& device);
    void BeginFrame();
    void EndFrame(const Device& device);

    [[nodiscard]] const MaliG7xxPolicy& Policy() const { return mali_policy; }

private:
    MaliG7xxPolicy mali_policy{};
    std::chrono::steady_clock::time_point frame_start{};
};

} // namespace Vulkan::X7NX
