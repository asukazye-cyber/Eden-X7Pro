// SPDX-License-Identifier: GPL-3.0-or-later
#include <cassert>
#include <iostream>
#include <set>
#include <string>

#define EDEN_GPU_PROFILE_POLICY_ONLY
#include "video_core/vulkan_common/vulkan_gpu_profile.cpp"
#include "common/android/x7pro_governor.h"

using namespace Common::Android::X7Pro;
using namespace Vulkan;

namespace {

GpuPerformanceProfile Profile(VkDriverId driver, u32 vendor, std::string_view model,
                              GpuOptimizationMode mode, bool descriptor = true,
                              bool robustness2_feature = false,
                              std::set<std::string, std::less<>> extensions = {}) {
    return GpuPerformanceProfile::Create(
        driver, vendor, 0x720, model, extensions, true, true, descriptor, true, true,
        robustness2_feature, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT |
                                 VK_QUEUE_TRANSFER_BIT,
        true, mode);
}

RealFrameBudgetEvidence GoodEvidence() {
    return {
        .latest_ms = 27.0,
        .p95_ms = 28.0,
        .p99_ms = 31.0,
        .gpu_p95_ms = 22.0,
        .cpu_p95_ms = 8.0,
        .present_p95_ms = 1.0,
        .enough_real_samples = true,
        .gpu_valid = true,
        .cpu_valid = true,
        .thermal = 0,
    };
}

} // namespace

int main() {
    const auto g720 = Profile(VK_DRIVER_ID_ARM_PROPRIETARY, 0x13B5, "Mali-G720 MC7",
                              GpuOptimizationMode::Automatic);
    assert(g720.IsMali() && g720.IsMaliG720() && g720.IsActive());
    assert(g720.Architecture() == GpuArchitecture::MaliFifthGen);
    assert(g720.FastPaths().tile_attachment_layouts);
    assert(g720.FastPaths().tile_load_op_clear);
    assert(g720.FastPaths().precise_upload_barrier);
    assert(g720.FastPaths().precise_present_barrier);
    assert(g720.FastPaths().present_copy_for_identical_extent);
    assert(g720.FastPaths().compute_descriptor_reuse);
    assert(g720.EnabledFastPathMask() == 0x3f);

    // ARM's proprietary driver is primary evidence. A Mali name can supplement a noncanonical
    // vendor ID, but a product string alone never activates another vendor's driver.
    const auto supplemental = Profile(VK_DRIVER_ID_ARM_PROPRIETARY, 0xffff,
                                      "Mali G720-MC7", GpuOptimizationMode::Automatic);
    assert(supplemental.IsActive());
    const auto spoofed = Profile(VK_DRIVER_ID_NVIDIA_PROPRIETARY, 0x13B5,
                                 "Mali-G720 MC7", GpuOptimizationMode::Aggressive);
    assert(!spoofed.IsMali() && !spoofed.IsActive() && spoofed.EnabledFastPathMask() == 0);
    const auto name_only = Profile(VK_DRIVER_ID_NVIDIA_PROPRIETARY, 0xffff,
                                   "Mali-G720", GpuOptimizationMode::Automatic);
    assert(!name_only.IsActive());

    // Unknown Mali generations are reported but preserve generic rendering behavior.
    const auto unknown = Profile(VK_DRIVER_ID_ARM_PROPRIETARY, 0x13B5, "Mali GPU",
                                 GpuOptimizationMode::Automatic);
    assert(unknown.IsMali() && !unknown.IsMaliG720() && !unknown.IsActive());

    const auto off = Profile(VK_DRIVER_ID_ARM_PROPRIETARY, 0x13B5, "Mali-G720 MC7",
                             GpuOptimizationMode::Disabled);
    assert(!off.IsActive() && off.EnabledFastPathMask() == 0);
    const auto aggressive = Profile(VK_DRIVER_ID_ARM_PROPRIETARY, 0x13B5, "Mali-G720 MC7",
                                    GpuOptimizationMode::Aggressive);
    assert(aggressive.IsActive() && aggressive.EnabledFastPathMask() == g720.EnabledFastPathMask());

    const auto no_descriptor = Profile(VK_DRIVER_ID_ARM_PROPRIETARY, 0x13B5, "Mali-G720 MC7",
                                       GpuOptimizationMode::Automatic, false);
    assert(no_descriptor.IsActive() && !no_descriptor.FastPaths().compute_descriptor_reuse);
    assert(no_descriptor.EnabledFastPathMask() == 0x1f);

    // robustness2 activation requires both a real feature bit and extension advertisement.
    const auto no_robustness2 = Profile(
        VK_DRIVER_ID_ARM_PROPRIETARY, 0x13B5, "Mali-G720 MC7",
        GpuOptimizationMode::Automatic, true, true, {"VK_EXT_pipeline_robustness"});
    assert(no_robustness2.Capabilities().pipeline_robustness);
    assert(!no_robustness2.Capabilities().robustness2);
    const auto real_robustness2 = Profile(
        VK_DRIVER_ID_ARM_PROPRIETARY, 0x13B5, "Mali-G720 MC7",
        GpuOptimizationMode::Automatic, true, true,
        {"VK_EXT_pipeline_robustness", "VK_EXT_robustness2"});
    assert(real_robustness2.Capabilities().robustness2);

    RealFrameBudgetController budget;
    auto decision = budget.Update({});
    assert(decision.phase == BudgetPhase::Warming);
    assert(!decision.allow_frame_generation && !decision.allow_cache_maintenance);

    // Recovery requires sustained fresh evidence; no persisted history can skip warmup.
    for (int i = 0; i < 44; ++i) {
        decision = budget.Update(GoodEvidence());
        assert(decision.phase == BudgetPhase::Warming);
    }
    decision = budget.Update(GoodEvidence());
    assert(decision.phase == BudgetPhase::Headroom);
    assert(decision.allow_frame_generation && decision.allow_cache_maintenance &&
           decision.allow_noncritical_transfer && decision.allow_optional_renderer_work);

    auto pressure = GoodEvidence();
    pressure.latest_ms = 41.0;
    pressure.p95_ms = 36.0;
    pressure.p99_ms = 48.0;
    assert(budget.Update(pressure).phase == BudgetPhase::Headroom); // one-frame spike filtered
    decision = budget.Update(pressure);
    assert(decision.phase == BudgetPhase::Critical);
    assert(!decision.allow_frame_generation && !decision.allow_speculative_warmup &&
           !decision.allow_cache_maintenance && !decision.allow_noncritical_transfer &&
           !decision.allow_optional_renderer_work);

    auto thermal = GoodEvidence();
    thermal.thermal = 3;
    decision = budget.Update(thermal);
    assert(decision.phase == BudgetPhase::Thermal && !decision.allow_frame_generation);

    budget.Reset();
    decision = budget.Update(GoodEvidence());
    assert(decision.phase == BudgetPhase::Warming);

    std::cout << "Build21 GPU profile and real-frame budget policy tests passed\n";
    return 0;
}
