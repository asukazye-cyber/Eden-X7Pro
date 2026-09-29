// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>

#include <sys/system_properties.h>
#include <vulkan/vulkan_core.h>

#include "common/android/poco_x7pro_profile.h"
#include "common/logging.h"
#include "common/settings.h"
#include "common/thread.h"

namespace Common::Android {
namespace {

struct ProfileState {
    std::once_flag device_once;
    std::mutex mutex;
    bool poco_x7_pro{};
    bool dimensity_8400{};
    bool mali_g720{};
    std::string vulkan_model;
    std::string vulkan_driver;
    bool logged{};
    std::string status{"device-not-detected"};
    std::string frame_gen_phase{"inactive"};
    u64 real_frames{};
    u64 generated_frames{};
    u64 dropped_synthetic_frames{};
    std::chrono::steady_clock::time_point last_real_frame{};
    f64 real_frame_ms{};
    u64 real_frames_over_budget{};
    std::array<f64, 180> frame_samples{};
    size_t frame_sample_count{};
    size_t next_frame_sample{};
};

ProfileState& State() {
    static ProfileState state;
    return state;
}

std::string Property(const char* key) {
    char value[PROP_VALUE_MAX]{};
    return __system_property_get(key, value) > 0 ? value : "";
}

std::string Lower(std::string value) {
    std::ranges::transform(value, value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool Has(const std::string& value, std::string_view needle) {
    return value.find(needle) != std::string::npos;
}

std::string Compact(std::string value) {
    value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) {
        return !std::isalnum(c);
    }), value.end());
    return value;
}

bool HardwareOptimizationEnabledLocked(const ProfileState& state) {
    // 0 = auto, 1 = standard, 2 = manually request the target path. A manual request never
    // treats an incompatible GPU as a Mali-G720.
    const u32 mode = Settings::values.x7pro_hardware_optimization.GetValue();
    return mode != 1 && state.dimensity_8400 && state.mali_g720;
}

void RefreshProfileStatusLocked(ProfileState& state) {
    if (!state.mali_g720) {
        state.status = "gpu-mismatch";
    } else if (!state.dimensity_8400) {
        state.status = "soc-mismatch";
    } else if (!HardwareOptimizationEnabledLocked(state)) {
        state.status = "inactive-standard";
    } else {
        state.status = "active";
    }
}

std::string FormatMilliseconds(f64 value) {
    if (value <= 0.0) {
        return "N/A";
    }
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2) << value << " ms";
    return stream.str();
}

std::string FormatFps(f64 frame_time) {
    if (frame_time <= 0.0) {
        return "N/A";
    }
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1) << (1000.0 / frame_time) << " FPS";
    return stream.str();
}

f64 Percentile(const ProfileState& state, f64 percentile) {
    if (state.frame_sample_count == 0) {
        return 0.0;
    }
    std::array<f64, 180> samples{};
    std::copy_n(state.frame_samples.begin(), state.frame_sample_count, samples.begin());
    std::sort(samples.begin(), samples.begin() + state.frame_sample_count);
    const size_t index = std::min(state.frame_sample_count - 1,
                                  static_cast<size_t>((state.frame_sample_count - 1) * percentile));
    return samples[index];
}

const char* RoleName(PocoX7ProThreadRole role) {
    switch (role) {
    case PocoX7ProThreadRole::GpuSubmission:
        return "GpuSubmission";
    case PocoX7ProThreadRole::RendererCritical:
        return "RendererCritical";
    case PocoX7ProThreadRole::CpuEmulation:
        return "CpuEmulation";
    case PocoX7ProThreadRole::ShaderCompiler:
        return "ShaderCompiler";
    case PocoX7ProThreadRole::AsyncWorker:
        return "AsyncWorker";
    case PocoX7ProThreadRole::Background:
        return "Background";
    }
    return "Unknown";
}

} // Anonymous namespace

void DetectPocoX7ProDevice() {
    auto& state = State();
    std::call_once(state.device_once, [&] {
        const std::string soc = Lower(Property("ro.soc.model") + " " +
                                      Property("ro.board.platform") + " " +
                                      Property("ro.mediatek.platform") + " " +
                                      Property("ro.vendor.mediatek.platform") + " " +
                                      Property("ro.hardware"));
        const std::string product = Lower(Property("ro.product.manufacturer") + " " +
                                          Property("ro.product.brand") + " " +
                                          Property("ro.product.model") + " " +
                                          Property("ro.product.device") + " " +
                                          Property("ro.product.vendor.model") + " " +
                                          Property("ro.product.odm.model"));

        const bool mt6899 = Has(soc, "mt6899") || Has(soc, "dimensity 8400") ||
                            Has(soc, "dimensity8400");
        const bool poco_x7 = Has(product, "poco x7 pro") || Has(product, "x7 pro") ||
                             Has(product, "rodin") || Has(product, "2412dpc0a");
        const bool xiaomi = Has(product, "xiaomi") || Has(product, "poco") || Has(product, "redmi");

        std::scoped_lock lock{state.mutex};
        state.poco_x7_pro = poco_x7 && (xiaomi || Has(product, "rodin") ||
                                        Has(product, "2412dpc0a"));
        // A recognised POCO X7 Pro device signature is a reliable Dimensity 8400-family identity
        // even on firmware that omits the commercial SoC name from public system properties.
        state.dimensity_8400 = mt6899 || state.poco_x7_pro;
        state.status = state.dimensity_8400 ? "waiting-for-mali-g720" : "device-mismatch";
        LOG_INFO(Common, "[MALI-FG] device profile: soc='{}' product='{}' result={}", soc, product,
                 state.status);
    });
}

void UpdatePocoX7ProVulkanDevice(std::string_view driver_name, std::string_view model_name,
                                 u32 api_version, u32 driver_version) {
    DetectPocoX7ProDevice();
    auto& state = State();
    const std::string model = Lower(std::string{model_name});
    const std::string driver = Lower(std::string{driver_name});
    const std::string compact_identity = Compact(model + " " + driver);
    const bool reported_mali_g720 = Has(compact_identity, "malig720") ||
                                    Has(compact_identity, "g720mc");

    std::scoped_lock lock{state.mutex};
    // Identity is deliberately independent from capability checks: a valid Mali-G720 model name
    // activates the hardware profile, while unsupported Vulkan features disable only the feature
    // that needs them. This accepts names such as "Mali-G720 MC7" and vendor-prefixed variants.
    state.mali_g720 = reported_mali_g720;
    RefreshProfileStatusLocked(state);
    state.vulkan_model = std::string{model_name};
    state.vulkan_driver = std::string{driver_name};
    if (!state.logged) {
        LOG_INFO(Common,
                 "[MALI-FG] GPU detected: '{}' driver='{}' api={}.{}.{} driver_version={} profile={}",
                 model_name, driver_name, VK_VERSION_MAJOR(api_version), VK_VERSION_MINOR(api_version),
                 VK_VERSION_PATCH(api_version), driver_version, state.status);
        state.logged = true;
    }
}

void UpdatePocoX7ProFrameGenStatus(std::string_view phase, u64, u64 generated_frames) {
    auto& state = State();
    std::scoped_lock lock{state.mutex};
    state.frame_gen_phase = phase;
    state.generated_frames = generated_frames;
}

void RecordPocoX7ProRealFrame() {
    auto& state = State();
    const auto now = std::chrono::steady_clock::now();
    std::scoped_lock lock{state.mutex};
    if (state.last_real_frame.time_since_epoch().count() != 0) {
        const f64 frame_time =
            std::chrono::duration<f64, std::milli>(now - state.last_real_frame).count();
        state.real_frame_ms = frame_time;
        state.real_frames_over_budget += frame_time > 33.333 ? 1 : 0;
        state.frame_samples[state.next_frame_sample] = frame_time;
        state.next_frame_sample = (state.next_frame_sample + 1) % state.frame_samples.size();
        state.frame_sample_count = std::min(state.frame_sample_count + 1, state.frame_samples.size());
    }
    state.last_real_frame = now;
    ++state.real_frames;
}

bool ShouldGeneratePocoX7ProFrame() {
    DetectPocoX7ProDevice();
    auto& state = State();
    std::scoped_lock lock{state.mutex};
    RefreshProfileStatusLocked(state);
    if (!Settings::values.mali_native_frame_gen.GetValue() || !HardwareOptimizationEnabledLocked(state)) {
        return false;
    }
    if (Settings::values.x7pro_fg_governor.GetValue() == 0) {
        return true;
    }
    // Auto governor gives source frames priority. Locked 30 uses the actual 33.333 ms budget;
    // balanced has a small tolerance but still drops a synthetic frame before adding more work.
    const u32 performance_mode = Settings::values.x7pro_performance_governor.GetValue();
    // Off keeps the independent FG safety guard; Balanced relaxes it slightly, while Locked 30
    // makes the actual 33.333 ms source-frame budget the priority.
    const f64 budget = performance_mode == 2 ? 33.333 : performance_mode == 1 ? 36.0 : 34.5;
    const bool generate = state.real_frame_ms == 0.0 || state.real_frame_ms <= budget;
    if (!generate) {
        ++state.dropped_synthetic_frames;
    }
    return generate;
}

bool IsPocoX7ProProfileActive() {
    DetectPocoX7ProDevice();
    auto& state = State();
    std::scoped_lock lock{state.mutex};
    RefreshProfileStatusLocked(state);
    return HardwareOptimizationEnabledLocked(state);
}

std::string PocoX7ProProfileStatus() {
    DetectPocoX7ProDevice();
    auto& state = State();
    std::scoped_lock lock{state.mutex};
    RefreshProfileStatusLocked(state);
    return state.status;
}

std::string PocoX7ProFrameGenDiagnostics() {
    DetectPocoX7ProDevice();
    auto& state = State();
    std::scoped_lock lock{state.mutex};
    return "Mali FG: " + state.frame_gen_phase + " | profile=" + state.status +
           " | real=" + std::to_string(state.real_frames) +
           " | generated=" + std::to_string(state.generated_frames) +
           " | gpu=" + state.vulkan_model + " | driver=" + state.vulkan_driver;
}

std::string PocoX7ProPerformanceDiagnostics() {
    DetectPocoX7ProDevice();
    auto& state = State();
    std::scoped_lock lock{state.mutex};
    RefreshProfileStatusLocked(state);

    const bool profile_active = HardwareOptimizationEnabledLocked(state);
    const bool pacing_active = profile_active &&
                               Settings::values.x7pro_performance_governor.GetValue() != 0;
    const f64 p95 = Percentile(state, 0.95);
    const f64 p99 = Percentile(state, 0.99);
    std::ostringstream stream;
    stream << "Device: " << (state.poco_x7_pro ? "POCO X7 Pro / compatible target" : "N/A") << '\n'
           << "SoC profile: " << (state.dimensity_8400 ? "Dimensity 8400 family" : "N/A") << '\n'
           << "GPU: " << (state.vulkan_model.empty() ? "N/A" : state.vulkan_model) << '\n'
           << "Hardware Profile: " << (profile_active ? "ACTIVE" : "INACTIVE") << '\n'
           << "CPU Optimization: " << (profile_active ? "ACTIVE" : "INACTIVE") << '\n'
           << "Mali Optimization: " << (profile_active ? "ACTIVE" : "INACTIVE") << '\n'
           << "Frame Pacing: " << (pacing_active ? "ACTIVE" : "INACTIVE") << '\n'
           << "Real FPS: " << FormatFps(state.real_frame_ms) << '\n'
           << "Presented FPS: N/A" << '\n'
           << "Frame Time: " << FormatMilliseconds(state.real_frame_ms) << '\n'
           << "CPU Time: N/A" << '\n'
           << "GPU Time: N/A" << '\n'
           << "Shader Time: N/A" << '\n'
           << "Present Time: N/A" << '\n'
           << "FG Time: N/A" << '\n'
           << "1% Low (real): " << FormatFps(p99) << '\n'
           << "P95: " << FormatMilliseconds(p95) << '\n'
           << "P99: " << FormatMilliseconds(p99) << '\n'
           << "Bottleneck: N/A" << '\n'
           << "Synthetic Frames: " << state.generated_frames << '\n'
           << "Dropped Synthetic Frames: " << state.dropped_synthetic_frames;
    return stream.str();
}

void ApplyPocoX7ProThreadAffinity(PocoX7ProThreadRole role) {
    if (!IsPocoX7ProProfileActive()) {
        return;
    }

    LOG_DEBUG(Common, "[MALI-FG] applying topology-aware affinity role={}", RoleName(role));
    switch (role) {
    case PocoX7ProThreadRole::GpuSubmission:
    case PocoX7ProThreadRole::RendererCritical:
    case PocoX7ProThreadRole::CpuEmulation:
        SetCurrentThreadToPerformanceCores();
        break;
    case PocoX7ProThreadRole::ShaderCompiler:
    case PocoX7ProThreadRole::AsyncWorker:
        SetCurrentThreadToEfficiencyCores();
        break;
    case PocoX7ProThreadRole::Background:
        SetCurrentThreadToBackgroundWork();
        break;
    }
}

} // namespace Common::Android
