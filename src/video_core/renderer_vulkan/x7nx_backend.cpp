// SPDX-FileCopyrightText: Copyright 2026 X7NX Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "video_core/renderer_vulkan/x7nx_backend.h"

#include <string>

#include "common/android/x7nx_device_profile.h"
#include "common/android/x7nx_telemetry.h"
#include "common/logging.h"
#include "video_core/vulkan_common/vulkan_device.h"

namespace Vulkan::X7NX {

void Backend::Initialize(const Device& device) {
    Common::Android::X7NX::DeviceProfile::Instance().UpdateVulkanCapabilities({
        .model_name = std::string{device.GetModelName()},
        .driver_name = device.GetDriverName(),
        .api_version = device.ApiVersion(),
        .driver_version = device.GetDriverVersion(),
        .arm_proprietary_driver = device.GetDriverID() == VK_DRIVER_ID_ARM_PROPRIETARY,
        .synchronization2 = device.HasSynchronization2(),
        .null_descriptor = device.HasNullDescriptor(),
        .dynamic_rendering = device.ApiVersion() >= VK_API_VERSION_1_3,
        .memory_budget = device.CanReportMemoryUsage(),
    });
    mali_policy = BuildMaliG7xxPolicy(device);
    uma_policy = BuildUmaMemoryPolicy(mali_policy.enabled);
    Common::Android::X7NX::Telemetry::Instance().SetRendererPath(RendererPathName(mali_policy));
    LOG_INFO(Render_Vulkan, "[X7NX] backend initialized: active={} UMA={} feedback={}",
             mali_policy.enabled, uma_policy.active, RendererPathName(mali_policy));
}

void Backend::BeginFrame() {
    frame_start = std::chrono::steady_clock::now();
}

void Backend::EndFrame(const Device& device) {
    if (frame_start.time_since_epoch().count() == 0) {
        return;
    }
    const auto elapsed = std::chrono::duration<f64, std::milli>(
        std::chrono::steady_clock::now() - frame_start).count();
    auto& telemetry = Common::Android::X7NX::Telemetry::Instance();
    telemetry.RecordCpuFrame(elapsed);
    if (device.CanReportMemoryUsage()) {
        telemetry.SetDeviceMemoryBytes(device.GetDeviceMemoryUsage());
    }
    telemetry.LogSummaryIfDue();
}

} // namespace Vulkan::X7NX
