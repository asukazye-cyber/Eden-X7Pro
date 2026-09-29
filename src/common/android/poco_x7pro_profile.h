// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <string_view>

#include "common/common_types.h"

namespace Common::Android {

// Roles deliberately describe work, not CPU numbers. The Android topology implementation reads
// cpu_capacity/cpufreq and applies the matching mask only when the kernel exposes a safe split.
enum class PocoX7ProThreadRole {
    GpuSubmission,
    RendererCritical,
    CpuEmulation,
    ShaderCompiler,
    AsyncWorker,
    Background,
};

void DetectPocoX7ProDevice();
void UpdatePocoX7ProVulkanDevice(std::string_view driver_name, std::string_view model_name,
                                 u32 api_version, u32 driver_version);
void UpdatePocoX7ProFrameGenStatus(std::string_view phase, u64 real_frames, u64 generated_frames);
void RecordPocoX7ProRealFrame();
[[nodiscard]] bool ShouldGeneratePocoX7ProFrame();

[[nodiscard]] bool IsPocoX7ProProfileActive();
[[nodiscard]] std::string PocoX7ProProfileStatus();
[[nodiscard]] std::string PocoX7ProFrameGenDiagnostics();
[[nodiscard]] std::string PocoX7ProPerformanceDiagnostics();

// Safe no-op for every device other than a recognised POCO X7 Pro / MT6899 / Mali-G720. Affinity
// errors are logged by the central thread policy implementation and never make emulation fail.
void ApplyPocoX7ProThreadAffinity(PocoX7ProThreadRole role);

} // namespace Common::Android
