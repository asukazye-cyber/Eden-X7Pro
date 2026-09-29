// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <memory>
#include <string_view>
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {
class Device;
namespace X7NX {
// Device-owned dispatch boundary, initialized before buffers, textures and pipeline caches.
// Unsupported devices retain Eden behavior through the base implementation.
class X7GpuBackend {
public:
    virtual ~X7GpuBackend() = default;
    virtual bool IsMaliG720() const { return false; }
    virtual bool UseNativeNullDescriptors(bool supported) const { return supported; }
    virtual VkDeviceSize DescriptorArenaBytes(VkDeviceSize fallback) const { return fallback; }
    virtual bool LegalizeBitfields() const { return false; }
};
class MaliGpuBackend : public X7GpuBackend {
public:
    bool UseNativeNullDescriptors(bool) const override { return false; }
    bool LegalizeBitfields() const override { return true; }
};
class MaliG720Backend final : public MaliGpuBackend {
public:
    bool IsMaliG720() const override { return true; }
    VkDeviceSize DescriptorArenaBytes(VkDeviceSize) const override { return 3 * 1024 * 1024; }
};
std::unique_ptr<X7GpuBackend> CreateGpuBackend(const Device& device);
} // namespace X7NX
} // namespace Vulkan
