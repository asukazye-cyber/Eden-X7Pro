// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_types.h"

namespace Vulkan {
class Device;
}

namespace Vulkan::X7NX {

enum class FeedbackPath : u8 {
    CopyClone,
};

struct MaliG7xxPolicy {
    bool enabled{};
    bool use_synchronization2{};
    bool use_dummy_descriptors{};
    FeedbackPath framebuffer_feedback{FeedbackPath::CopyClone};
};

// Central decision point for Mali-G7xx quirks. This type exposes no vendor fast path until the
// device capabilities and corresponding validation are available.
[[nodiscard]] MaliG7xxPolicy BuildMaliG7xxPolicy(const Device& device);
[[nodiscard]] const char* RendererPathName(const MaliG7xxPolicy& policy);

} // namespace Vulkan::X7NX
