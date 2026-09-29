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
    // Device construction already captured enabled capabilities before any resource cache existed.
    mali_policy = BuildMaliG7xxPolicy(device);
    uma_policy = BuildUmaMemoryPolicy(mali_policy.enabled);
    Common::Android::X7NX::Telemetry::Instance().SetRendererPath(RendererPathName(mali_policy));
    LOG_INFO(Render_Vulkan, "[X7NX] backend initialized: active={} UMA={} feedback={}",
             mali_policy.enabled, uma_policy.active, RendererPathName(mali_policy));
}

void Backend::BeginFrame() {
#ifndef NDEBUG
    frame_start = std::chrono::steady_clock::now();
#endif
}

void Backend::EndFrame(const Device& device) {
#ifndef NDEBUG
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
#endif
}

} // namespace Vulkan::X7NX
