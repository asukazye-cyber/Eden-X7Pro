// SPDX-License-Identifier: GPL-3.0-or-later
#include "video_core/renderer_vulkan/x7nx_gpu_backend.h"
#include "common/android/x7nx_device_profile.h"
#include "common/logging.h"
#include "video_core/vulkan_common/vulkan_device.h"

namespace Vulkan::X7NX {
std::unique_ptr<X7GpuBackend> CreateGpuBackend(const Device& device) {
    auto& profile = Common::Android::X7NX::DeviceProfile::Instance();
    profile.UpdateVulkanCapabilities({
        .model_name = std::string(device.GetModelName()), .driver_name = device.GetDriverName(),
        .api_version = device.ApiVersion(), .driver_version = device.GetDriverVersion(),
        .arm_proprietary_driver = device.GetDriverID() == VK_DRIVER_ID_ARM_PROPRIETARY,
        .synchronization2 = device.HasSynchronization2(), .null_descriptor = device.HasNullDescriptor(),
        // API version alone is NOT proof an optional feature was enabled on the logical device.
        .dynamic_rendering = false, .memory_budget = device.CanReportMemoryUsage()});
    const auto target = profile.Snapshot();
    if (target.kind != Common::Android::X7NX::DeviceKind::PocoX7ProDimensity8400 ||
        !target.mali_g720 || !target.vulkan.arm_proprietary_driver) {
        return std::make_unique<X7GpuBackend>();
    }
    const auto properties = device.GetPhysical().GetProperties();
    const auto memory = device.GetPhysical().GetMemoryProperties().memoryProperties;
    LOG_INFO(Render_Vulkan,
        "[X7NX] MaliG720Backend: API={} driver={} max2D={} UBO={} SSBO={} memoryTypes={} heaps={} extensions={}",
        device.ApiVersion(), device.GetDriverVersion(), properties.limits.maxImageDimension2D,
        properties.limits.maxUniformBufferRange, properties.limits.maxStorageBufferRange,
        memory.memoryTypeCount, memory.memoryHeapCount, device.GetAvailableExtensions().size());
    return std::make_unique<MaliG720Backend>();
}
} // namespace Vulkan::X7NX
