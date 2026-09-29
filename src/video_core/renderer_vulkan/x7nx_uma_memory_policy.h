// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_types.h"

namespace Vulkan::X7NX {

enum class ResourceLifetime : u8 { Persistent, Ring, ReusableTransient, OneShotReadback };
enum class ReadbackReason : u8 { ExplicitCapture, GuestCorrectness, HotPath }; 

struct UmaMemoryPolicy {
    bool active{};
    bool prefer_persistent_mapped{};
    bool prefer_ring_allocators{};
    bool defer_noncoherent_flush{};
    bool allow_hot_path_readback{};
};

[[nodiscard]] UmaMemoryPolicy BuildUmaMemoryPolicy(bool x7nx_enabled);
[[nodiscard]] ResourceLifetime ClassifyResource(bool per_frame, bool cpu_visible, bool readback);
[[nodiscard]] bool IsReadbackAllowed(ReadbackReason reason);

} // namespace Vulkan::X7NX
