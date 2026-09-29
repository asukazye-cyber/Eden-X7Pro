// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <string_view>

#include "common/common_types.h"

namespace Common::Android::X7NX {

enum class DeviceKind : u8 {
    SafeFallback,
    PocoX7ProDimensity8400,
};

struct VulkanCapabilities {
    std::string model_name;
    std::string driver_name;
    u32 api_version{};
    u32 driver_version{};
    bool arm_proprietary_driver{};
    bool synchronization2{};
    bool null_descriptor{};
    bool dynamic_rendering{};
    bool memory_budget{};
};

struct DeviceProfileSnapshot {
    DeviceKind kind{DeviceKind::SafeFallback};
    bool poco_x7_pro{};
    bool dimensity_8400{};
    bool mali_g7xx{};
    bool mali_g720{};
    bool all_big_core_hint{};
    VulkanCapabilities vulkan{};
};

// The profile never grants an unsupported Vulkan feature. It only enables optional policy paths
// after both the Android identity and the Vulkan device report match the target.
class DeviceProfile final {
public:
    static DeviceProfile& Instance();

    void DetectAndroidIdentity();
    void UpdateVulkanCapabilities(VulkanCapabilities capabilities);

    [[nodiscard]] DeviceProfileSnapshot Snapshot() const;
    [[nodiscard]] bool IsPocoX7ProDimensity8400() const;

private:
    DeviceProfile() = default;
};

} // namespace Common::Android::X7NX
