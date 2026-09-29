// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "video_core/renderer_vulkan/x7nx_uma_memory_policy.h"

namespace Vulkan::X7NX {

UmaMemoryPolicy BuildUmaMemoryPolicy(bool x7nx_enabled) {
    return {.active = x7nx_enabled,
            .prefer_persistent_mapped = x7nx_enabled,
            .prefer_ring_allocators = x7nx_enabled,
            .defer_noncoherent_flush = x7nx_enabled,
            .allow_hot_path_readback = false};
}

ResourceLifetime ClassifyResource(bool per_frame, bool cpu_visible, bool readback) {
    if (readback) return ResourceLifetime::OneShotReadback;
    if (cpu_visible) return ResourceLifetime::Ring;
    return per_frame ? ResourceLifetime::ReusableTransient : ResourceLifetime::Persistent;
}

bool IsReadbackAllowed(ReadbackReason reason) {
    return reason == ReadbackReason::ExplicitCapture || reason == ReadbackReason::GuestCorrectness;
}

} // namespace Vulkan::X7NX
