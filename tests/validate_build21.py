#!/usr/bin/env python3
"""Build 21 structural/correctness guardrails. This does not claim hardware performance."""
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

root = Path(sys.argv[1] if len(sys.argv) > 1 else "eden")
errors: list[str] = []

def text(path: str) -> str:
    p = root / path
    if not p.is_file():
        errors.append(f"missing {path}")
        return ""
    return p.read_text(encoding="utf-8")

def require(path: str, *needles: str) -> str:
    data = text(path)
    for needle in needles:
        if needle not in data:
            errors.append(f"{path}: missing {needle!r}")
    return data

profile_h = require(
    "src/video_core/vulkan_common/vulkan_gpu_profile.h",
    "GpuArchitecture", "GpuCapabilities", "GpuWorkarounds", "GpuFastPaths",
    "GpuOptimizationMode", "preserve_same_layout_dependencies")
profile_cpp = require(
    "src/video_core/vulkan_common/vulkan_gpu_profile.cpp",
    "VK_DRIVER_ID_ARM_PROPRIETARY", "ARM_VENDOR_ID", "profile.mali_g720",
    "requested_mode != GpuOptimizationMode::Disabled", "EnabledFastPathMask",
    '"VK_EXT_robustness2"')
if "POCO" in profile_h + profile_cpp or "Dimensity" in profile_h + profile_cpp:
    errors.append("central Vulkan policy must not depend on handset/SoC branding")

require("src/video_core/vulkan_common/vulkan_device.h", "gpu_performance_profile",
        "GetGpuPerformanceProfile", "GetPipelineCacheUUID", "GetVendorID", "GetDeviceID")
require("src/video_core/vulkan_common/vulkan_device.cpp", "GpuPerformanceProfile::Create",
        "ConfigureRendererDiagnostics", "supported_extensions",
        "features.robustness2.nullDescriptor")

descriptor = require("src/video_core/renderer_vulkan/vk_descriptor_buffer.cpp",
                     "GetGpuPerformanceProfile", "use_g720_budget")
if "Mali-G720" in descriptor:
    errors.append("descriptor allocator contains a local product-name policy check")

require("src/video_core/renderer_vulkan/vk_render_pass_cache.cpp", "AttachmentLayout",
        "VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL",
        "VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL", "VK_DEPENDENCY_BY_REGION_BIT")
require("src/video_core/renderer_vulkan/present/window_adapt_pass.cpp",
        "tile_load_op_clear", "TileLoadClear")
require("src/video_core/renderer_vulkan/present/util.cpp", "VK_ATTACHMENT_LOAD_OP_CLEAR",
        "clearValueCount = clear_value != nullptr ? 1U : 0U")

require("src/video_core/renderer_vulkan/vk_sync_optimizer.h", "UploadToMain",
        "VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT", "VK_ACCESS_TRANSFER_WRITE_BIT",
        "original conservative dependency")
require("src/video_core/renderer_vulkan/vk_scheduler.cpp", "Sync::UploadToMain",
        "precise_upload_barrier")

present = require("src/video_core/renderer_vulkan/vk_present_manager.cpp",
                  "present_copy_for_identical_extent", "exact_extent", "use_copy",
                  "present_src_stages", "present_dst_stages", "PresentCopy", "PresentBlit")
if present.find("if (use_copy)") > present.find("cmdbuf.BlitImage"):
    errors.append("copy selection must precede the fallback blit")

compute = require("src/video_core/renderer_vulkan/vk_compute_pipeline.cpp",
                  "CurrentGeneration", "last_descriptor_buffer_generation",
                  "last_descriptor_payload", "TouchFrame", "DescriptorReuse")
require("src/video_core/renderer_vulkan/present/mali_native_frame_gen.cpp",
        "motion_descriptor_history", "interpolation_descriptor_history",
        "destination view is stable")

cache = require("src/video_core/renderer_vulkan/vk_pipeline_cache.cpp",
                "CACHE_VERSION = 19", "VulkanCacheIdentity", "GetPipelineCacheUUID",
                "VULKAN_CACHE_MAX_BYTES", "renderer_config", "AllowWork")
if "CACHE_VERSION = 18" in cache:
    errors.append("old un-hardened driver cache version remains")

budget = require("src/common/android/x7pro_governor.h", "RealFrameBudgetController",
                 "TargetMs", "BudgetPhase::Critical", "allow_frame_generation",
                 "allow_cache_maintenance", "recovery_streak >= 45")
telemetry = require("src/common/android/x7pro_telemetry.cpp", "s.budget.Update",
                    "if (!budget.allow_frame_generation)", "dynamic-resolution=OFF",
                    "SaveTitleBudgetHistory", "max_titles=32")
if telemetry.find("if (!budget.allow_frame_generation)") > telemetry.find("s.governor.Decide(e)"):
    errors.append("real-frame budget must reject FG before the legacy FG governor")
require("src/common/android/x7pro_budget_store.cpp", "MaxRecords = 32", "Version = 1",
        "MaxFileBytes", "std::isfinite", "ResetTitleBudgetHistory")

require("src/video_core/renderer_vulkan/renderer_vulkan.h", "applet_download_buffer")
require("src/video_core/renderer_vulkan/renderer_vulkan.cpp", "if (!applet_download_buffer)",
        "scheduler.Finish()", "AppletDownloadReuse", "AppletReadback")
require("src/video_core/gpu_logging/gpu_flight_recorder.h", "Capacity = 256",
        "std::atomic", "DumpOnce")
require("src/video_core/gpu_logging/gpu_flight_recorder.cpp",
        '"vulkan_flight_recorder.gpu-dump"', "test_and_set", "catch (...)")
require("src/video_core/gpu_logging/gpu_renderer_diagnostics.cpp",
        "Pipeline map (session)", "Descriptor buffer (session)", "Present (session)")

require("src/common/settings.h", '"mali_renderer_optimization"')
require("src/android/app/src/main/java/org/yuzu/yuzu_emu/features/settings/model/IntSetting.kt",
        "MALI_RENDERER_OPTIMIZATION")
require("src/android/app/src/main/java/org/yuzu/yuzu_emu/features/settings/ui/SettingsFragmentPresenter.kt",
        "maliRendererOptimizationNames", "MALI_RENDERER_OPTIMIZATION")
require("src/android/app/src/main/res/values/strings.xml", "mali_renderer_optimization_description")

for xml in ["src/android/app/src/main/res/values/arrays.xml",
            "src/android/app/src/main/res/values/strings.xml"]:
    try:
        ET.parse(root / xml)
    except Exception as exc:
        errors.append(f"{xml}: XML parse failed: {exc}")

# No new wait-idle call is permitted in the hot files touched by Build 21.
for path in [
    "src/video_core/renderer_vulkan/vk_scheduler.cpp",
    "src/video_core/renderer_vulkan/vk_present_manager.cpp",
    "src/video_core/renderer_vulkan/vk_compute_pipeline.cpp",
    "src/video_core/renderer_vulkan/present/window_adapt_pass.cpp",
]:
    data = text(path)
    if "WaitIdle(" in data or "vkDeviceWaitIdle" in data or "vkQueueWaitIdle" in data:
        errors.append(f"{path}: routine idle wait introduced")

if errors:
    print("Build21 validation FAILED:")
    for error in errors:
        print(f" - {error}")
    raise SystemExit(1)
print("Build21 structural/correctness validation passed")
