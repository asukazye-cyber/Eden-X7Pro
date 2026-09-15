// Minimal host-policy-test Vulkan declarations. Android/native builds use the real Vulkan header.
#pragma once
#include <cstdint>
using VkQueueFlags = std::uint32_t;
enum VkDriverId : std::uint32_t {
    VK_DRIVER_ID_AMD_PROPRIETARY = 1,
    VK_DRIVER_ID_NVIDIA_PROPRIETARY = 4,
    VK_DRIVER_ID_ARM_PROPRIETARY = 20,
};
inline constexpr VkQueueFlags VK_QUEUE_GRAPHICS_BIT = 0x1;
inline constexpr VkQueueFlags VK_QUEUE_COMPUTE_BIT = 0x2;
inline constexpr VkQueueFlags VK_QUEUE_TRANSFER_BIT = 0x4;
inline constexpr std::uint32_t VK_UUID_SIZE = 16;
