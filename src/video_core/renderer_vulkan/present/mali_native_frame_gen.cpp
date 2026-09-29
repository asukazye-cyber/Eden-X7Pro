// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <array>
#include <string>
#include <vector>
#include "video_core/renderer_vulkan/present/mali_native_frame_gen.h"

#include "common/android/poco_x7pro_profile.h"
#include "common/div_ceil.h"
#include "common/logging.h"
#include "common/settings.h"
#include "video_core/host_shaders/mali_x7pro_interpolate_comp_spv.h"
#include "video_core/host_shaders/mali_x7pro_motion_comp_spv.h"
#include "video_core/renderer_vulkan/present/util.h"
#include "video_core/renderer_vulkan/vk_present_manager.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/vulkan_common/vulkan_device.h"

namespace Vulkan {
namespace {

constexpr size_t MAX_IN_FLIGHT_SETS = 7;
constexpr VkFormat INPUT_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat MOTION_FORMAT = VK_FORMAT_R16G16_SFLOAT;
constexpr VkFormat CONFIDENCE_FORMAT = VK_FORMAT_R8_UNORM;

VkImageMemoryBarrier MakeBarrier(VkImage image, VkAccessFlags src_access, VkAccessFlags dst_access,
                                 VkImageLayout old_layout, VkImageLayout new_layout = VK_IMAGE_LAYOUT_GENERAL) {
    return VkImageMemoryBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = src_access,
        .dstAccessMask = dst_access,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
}

VkImageCopy MakeCopy(VkExtent2D extent) {
    return VkImageCopy{
        .srcSubresource{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0,
                        .baseArrayLayer = 0, .layerCount = 1},
        .srcOffset{},
        .dstSubresource{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0,
                        .baseArrayLayer = 0, .layerCount = 1},
        .dstOffset{},
        .extent{.width = extent.width, .height = extent.height, .depth = 1},
    };
}

VkDescriptorImageInfo StorageImage(VkImageView view) {
    return VkDescriptorImageInfo{.sampler = VK_NULL_HANDLE,
                                 .imageView = view,
                                 .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
}

const char* ModeName(u32 mode) {
    switch (mode) {
    case 0:
        return "Performance";
    case 1:
        return "Balanced";
    case 2:
        return "Quality";
    case 3:
        return "Adaptive";
    default:
        return "Balanced";
    }
}

} // Anonymous namespace

MaliNativeFrameGen::MaliNativeFrameGen(MemoryAllocator& memory_allocator_, Scheduler& scheduler_)
    : memory_allocator{memory_allocator_}, scheduler{scheduler_} {}

MaliNativeFrameGen::~MaliNativeFrameGen() = default;

size_t MaliNativeFrameGen::WantedGenerations(size_t capacity) {
    wanted_generations = !unavailable && Settings::values.mali_native_frame_gen.GetValue() &&
                                  history_count > 0 && capacity > 0
                              ? 1
                              : 0;
    return wanted_generations;
}

size_t MaliNativeFrameGen::GeneratedFrameCount() const {
    return generated_frames;
}

void MaliNativeFrameGen::Disable(std::string_view reason) {
    unavailable = true;
    generated_frames = 0;
    wanted_generations = 0;
    Common::Android::UpdatePocoX7ProFrameGenStatus("disabled:" + std::string{reason}, frame_count,
                                                    generated_frame_total);
    LOG_WARNING(Render_Vulkan, "[MALI-FG] DISABLED: reason={}", reason);
}

MaliNativeFrameGen::QualityConfig MaliNativeFrameGen::SelectQuality() const {
    switch (Settings::values.mali_native_frame_gen_mode.GetValue()) {
    case 0: // Performance
        return {.motion_scale = 4, .search_radius = 1, .confidence_bias = 0.20f,
                .minimum_confidence = 0.30f, .edge_threshold = 0.28f};
    case 2: // Quality
        return {.motion_scale = 2, .search_radius = 3, .confidence_bias = 0.15f,
                .minimum_confidence = 0.38f, .edge_threshold = 0.20f};
    case 3: // Adaptive begins conservatively; the capability gate keeps the normal renderer safe.
    case 1: // Balanced
    default:
        return {.motion_scale = 4, .search_radius = 2, .confidence_bias = 0.18f,
                .minimum_confidence = 0.34f, .edge_threshold = 0.23f};
    }
}

bool MaliNativeFrameGen::EnsureReady(const Device& device, Frame* frame, VkFormat frame_format) {
    if (unavailable || !Settings::values.mali_native_frame_gen.GetValue()) {
        return false;
    }
    if (!Common::Android::IsPocoX7ProProfileActive()) {
        Disable("poco-x7-pro-mali-g720-profile-not-active");
        return false;
    }
    if (device.HasBrokenCompute()) {
        Disable("compute-blacklisted-by-driver");
        return false;
    }
    if (frame_format != INPUT_FORMAT) {
        Disable("presentation-format-is-not-rgba8-unorm");
        return false;
    }
    if (!frame->storage_view) {
        Disable("intermediate-presentation-image-has-no-storage-view");
        return false;
    }
    if (!device.IsFormatSupported(INPUT_FORMAT, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT,
                                  FormatType::Optimal) ||
        !device.IsFormatSupported(MOTION_FORMAT, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT,
                                  FormatType::Optimal) ||
        !device.IsFormatSupported(CONFIDENCE_FORMAT, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT,
                                  FormatType::Optimal)) {
        Disable("required-storage-image-format-unavailable");
        return false;
    }

    const VkExtent2D frame_extent{.width = frame->width, .height = frame->height};
    const QualityConfig quality = SelectQuality();
    if (extent.width != frame_extent.width || extent.height != frame_extent.height ||
        format != frame_format || configured_motion_scale != quality.motion_scale) {
        try {
            Rebuild(device, frame_extent);
        } catch (const vk::Exception& exception) {
            Disable(exception.what());
            return false;
        }
    }
    return true;
}

void MaliNativeFrameGen::Reset() {
    history = {};
    motion = {};
    confidence = {};
    descriptor_sets = {};
    descriptor_pool = {};
    motion_pipeline = {};
    interpolation_pipeline = {};
    motion_shader = {};
    interpolation_shader = {};
    motion_pipeline_layout = {};
    interpolation_pipeline_layout = {};
    motion_set_layout = {};
    interpolation_set_layout = {};
    extent = {};
    motion_extent = {};
    format = VK_FORMAT_UNDEFINED;
    current_history = 0;
    history_count = 0;
    configured_motion_scale = 0;
    frame_count = 0;
    generated_frame_total = 0;
    generated_frames = 0;
}

void MaliNativeFrameGen::Rebuild(const Device& device, VkExtent2D new_extent) {
    // Reconfiguration happens only on a size/format change, never on the per-frame path.
    scheduler.Finish();
    Reset();

    extent = new_extent;
    format = INPUT_FORMAT;
    const QualityConfig quality = SelectQuality();
    configured_motion_scale = quality.motion_scale;
    motion_extent = {.width = Common::DivCeil(extent.width, quality.motion_scale),
                     .height = Common::DivCeil(extent.height, quality.motion_scale)};

    for (auto& image : history) {
        image.image = CreateWrappedImage(memory_allocator, extent, INPUT_FORMAT);
        image.view = CreateWrappedImageView(device, image.image, INPUT_FORMAT);
        image.extent = extent;
    }
    motion.image = CreateWrappedImage(memory_allocator, motion_extent, MOTION_FORMAT);
    motion.view = CreateWrappedImageView(device, motion.image, MOTION_FORMAT);
    motion.extent = motion_extent;
    confidence.image = CreateWrappedImage(memory_allocator, motion_extent, CONFIDENCE_FORMAT);
    confidence.view = CreateWrappedImageView(device, confidence.image, CONFIDENCE_FORMAT);
    confidence.extent = motion_extent;

    descriptor_pool = CreateWrappedDescriptorPool(
        device, MAX_IN_FLIGHT_SETS * 9, MAX_IN_FLIGHT_SETS * 2,
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE});
    motion_set_layout = CreateWrappedDescriptorSetLayout(
        device, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                 VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
        VK_SHADER_STAGE_COMPUTE_BIT);
    interpolation_set_layout = CreateWrappedDescriptorSetLayout(
        device, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                 VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                 VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
        VK_SHADER_STAGE_COMPUTE_BIT);

    const VkPushConstantRange motion_range{.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                            .offset = 0,
                                            .size = sizeof(MotionConstants)};
    const VkPushConstantRange interpolation_range{.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                                                   .offset = 0,
                                                   .size = sizeof(InterpolationConstants)};
    motion_pipeline_layout = device.GetLogical().CreatePipelineLayout({
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = motion_set_layout.address(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &motion_range,
    });
    interpolation_pipeline_layout = device.GetLogical().CreatePipelineLayout({
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = interpolation_set_layout.address(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &interpolation_range,
    });

    motion_shader = CreateWrappedShaderModule(device, MALI_X7PRO_MOTION_COMP_SPV);
    interpolation_shader = CreateWrappedShaderModule(device, MALI_X7PRO_INTERPOLATE_COMP_SPV);
    motion_pipeline = CreateWrappedComputePipeline(device, motion_pipeline_layout, *motion_shader);
    interpolation_pipeline =
        CreateWrappedComputePipeline(device, interpolation_pipeline_layout, *interpolation_shader);

    std::vector<VkDescriptorSetLayout> layouts;
    layouts.reserve(MAX_IN_FLIGHT_SETS * 2);
    for (size_t i = 0; i < MAX_IN_FLIGHT_SETS; ++i) {
        layouts.push_back(*motion_set_layout);
    }
    for (size_t i = 0; i < MAX_IN_FLIGHT_SETS; ++i) {
        layouts.push_back(*interpolation_set_layout);
    }
    descriptor_sets = CreateWrappedDescriptorSets(descriptor_pool, layouts);
    LOG_INFO(Render_Vulkan,
             "[MALI-FG] Compute pipeline: READY extent={}x{} motion={}x{} mode={}", extent.width,
             extent.height, motion_extent.width, motion_extent.height,
             ModeName(Settings::values.mali_native_frame_gen_mode.GetValue()));
    Common::Android::UpdatePocoX7ProFrameGenStatus("compute-ready", 0, 0);
}

void MaliNativeFrameGen::CopyCurrentFrame(Frame* frame) {
    const u32 next_history = 1 - current_history;
    Image& destination = history[next_history];
    const VkImage source = *frame->image;
    const VkExtent2D copy_extent{.width = frame->width, .height = frame->height};

    scheduler.RequestOutsideRenderPassOperationContext();
    scheduler.Record([source, destination_image = *destination.image, copy_extent,
                      source_layout = VK_IMAGE_LAYOUT_GENERAL,
                      destination_layout = destination.layout](vk::CommandBuffer cmdbuf) {
        const std::array before{
            MakeBarrier(source, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                        VK_ACCESS_TRANSFER_READ_BIT, source_layout,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
            MakeBarrier(destination_image,
                        destination_layout == VK_IMAGE_LAYOUT_UNDEFINED
                            ? 0
                            : VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                        VK_ACCESS_TRANSFER_WRITE_BIT, destination_layout,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
        };
        cmdbuf.PipelineBarrier(vk::PIPELINE_STAGE_GRAPHICS_COMPUTE_TRANSFER,
                               VK_PIPELINE_STAGE_TRANSFER_BIT, 0, {}, {}, before);
        cmdbuf.CopyImage(source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destination_image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, MakeCopy(copy_extent));
        const std::array after{
            MakeBarrier(source, VK_ACCESS_TRANSFER_READ_BIT,
                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
            MakeBarrier(destination_image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
        };
        cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT,
                               vk::PIPELINE_STAGE_GRAPHICS_COMPUTE, 0, {}, {}, after);
    });
    destination.layout = VK_IMAGE_LAYOUT_GENERAL;
    current_history = next_history;
    history_count = std::min<u32>(history_count + 1, 2);
}

void MaliNativeFrameGen::UpdateMotionDescriptors(const Device& device, VkDescriptorSet set) {
    const u32 previous = 1 - current_history;
    const std::array infos{StorageImage(*history[previous].view), StorageImage(*history[current_history].view),
                           StorageImage(*motion.view), StorageImage(*confidence.view)};
    std::array<VkWriteDescriptorSet, 4> writes{};
    for (u32 binding = 0; binding < writes.size(); ++binding) {
        writes[binding] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                           .pNext = nullptr,
                           .dstSet = set,
                           .dstBinding = binding,
                           .dstArrayElement = 0,
                           .descriptorCount = 1,
                           .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                           .pImageInfo = &infos[binding],
                           .pBufferInfo = nullptr,
                           .pTexelBufferView = nullptr};
    }
    device.GetLogical().UpdateDescriptorSets(writes, {});
}

void MaliNativeFrameGen::UpdateInterpolationDescriptors(const Device& device, VkDescriptorSet set,
                                                         VkImageView target) {
    const u32 previous = 1 - current_history;
    const std::array infos{StorageImage(*history[previous].view), StorageImage(*history[current_history].view),
                           StorageImage(*motion.view), StorageImage(*confidence.view),
                           StorageImage(target)};
    std::array<VkWriteDescriptorSet, 5> writes{};
    for (u32 binding = 0; binding < writes.size(); ++binding) {
        writes[binding] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                           .pNext = nullptr,
                           .dstSet = set,
                           .dstBinding = binding,
                           .dstArrayElement = 0,
                           .descriptorCount = 1,
                           .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                           .pImageInfo = &infos[binding],
                           .pBufferInfo = nullptr,
                           .pTexelBufferView = nullptr};
    }
    device.GetLogical().UpdateDescriptorSets(writes, {});
}

void MaliNativeFrameGen::Process(const Device& device, Frame* frame, VkFormat frame_format,
                                 VkExtent2D) {
    generated_frames = 0;
    if (!Settings::values.mali_native_frame_gen.GetValue()) {
        Common::Android::UpdatePocoX7ProFrameGenStatus("off", frame_count, generated_frame_total);
        return;
    }
    if (!Common::Android::IsPocoX7ProProfileActive()) {
        Common::Android::UpdatePocoX7ProFrameGenStatus("hardware-profile-inactive", frame_count,
                                                        generated_frame_total);
        return;
    }
    if (!EnsureReady(device, frame, frame_format)) {
        return;
    }

    CopyCurrentFrame(frame);
    ++frame_count;
    if (history_count < 2 || wanted_generations == 0) {
        Common::Android::UpdatePocoX7ProFrameGenStatus("warming-up", frame_count,
                                                        generated_frame_total);
        return;
    }
    generated_frames = 1;
    ++generated_frame_total;
}

void MaliNativeFrameGen::GenerateInto(const Device& device, Frame* destination, size_t generation) {
    if (generated_frames == 0 || generation != 0 || !destination->storage_view) {
        return;
    }
    if (destination->index >= MAX_IN_FLIGHT_SETS) {
        Disable("generated-frame-index-out-of-range");
        return;
    }
    const size_t set_slot = static_cast<size_t>(frame_count % MAX_IN_FLIGHT_SETS);
    const VkDescriptorSet motion_set = descriptor_sets[set_slot];
    const VkDescriptorSet interpolation_set = descriptor_sets[MAX_IN_FLIGHT_SETS + destination->index];
    UpdateMotionDescriptors(device, motion_set);
    UpdateInterpolationDescriptors(device, interpolation_set, *destination->storage_view);

    const QualityConfig quality = SelectQuality();
    const MotionConstants motion_constants{.frame_width = extent.width,
                                            .frame_height = extent.height,
                                            .motion_width = motion_extent.width,
                                            .motion_height = motion_extent.height,
                                            .motion_scale = quality.motion_scale,
                                            .search_radius = quality.search_radius,
                                            .confidence_bias = quality.confidence_bias,
                                            .reserved = 0.0f};
    const InterpolationConstants interpolation_constants{
        .frame_width = extent.width,
        .frame_height = extent.height,
        .motion_width = motion_extent.width,
        .motion_height = motion_extent.height,
        .motion_scale = quality.motion_scale,
        .reserved = 0,
        .interpolation_time = 0.5f,
        .minimum_confidence = quality.minimum_confidence,
        .edge_threshold = quality.edge_threshold,
        .reserved_float = 0.0f,
    };
    const VkImage target = *destination->image;
    scheduler.RequestOutsideRenderPassOperationContext();
    scheduler.Record([this, motion_set, interpolation_set, target, motion_constants,
                      interpolation_constants](vk::CommandBuffer cmdbuf) {
        const std::array before_motion{
            MakeBarrier(*motion.image,
                        motion.layout == VK_IMAGE_LAYOUT_UNDEFINED
                            ? 0
                            : VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                        VK_ACCESS_SHADER_WRITE_BIT, motion.layout),
            MakeBarrier(*confidence.image,
                        confidence.layout == VK_IMAGE_LAYOUT_UNDEFINED
                            ? 0
                            : VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                        VK_ACCESS_SHADER_WRITE_BIT, confidence.layout),
        };
        cmdbuf.PipelineBarrier(vk::PIPELINE_STAGE_GRAPHICS_COMPUTE,
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, {}, {}, before_motion);
        cmdbuf.BindPipeline(VK_PIPELINE_BIND_POINT_COMPUTE, *motion_pipeline);
        cmdbuf.BindDescriptorSets(VK_PIPELINE_BIND_POINT_COMPUTE, *motion_pipeline_layout, 0,
                                  motion_set, {});
        cmdbuf.PushConstants(*motion_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                             sizeof(motion_constants), &motion_constants);
        cmdbuf.Dispatch(Common::DivCeil(motion_extent.width, 8u),
                       Common::DivCeil(motion_extent.height, 8u), 1);

        const std::array between{
            MakeBarrier(*motion.image, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_GENERAL),
            MakeBarrier(*confidence.image, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_GENERAL),
            MakeBarrier(target, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT,
                        VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL),
        };
        cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                                   VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                   VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, {}, {}, between);
        cmdbuf.BindPipeline(VK_PIPELINE_BIND_POINT_COMPUTE, *interpolation_pipeline);
        cmdbuf.BindDescriptorSets(VK_PIPELINE_BIND_POINT_COMPUTE, *interpolation_pipeline_layout,
                                  0, interpolation_set, {});
        cmdbuf.PushConstants(*interpolation_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                             sizeof(interpolation_constants), &interpolation_constants);
        cmdbuf.Dispatch(Common::DivCeil(extent.width, 8u), Common::DivCeil(extent.height, 8u), 1);

        const auto after = MakeBarrier(target, VK_ACCESS_SHADER_WRITE_BIT,
                                       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT,
                                       VK_IMAGE_LAYOUT_GENERAL);
        cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                   VK_PIPELINE_STAGE_TRANSFER_BIT,
                               0, {}, {}, after);
    });
    motion.layout = VK_IMAGE_LAYOUT_GENERAL;
    confidence.layout = VK_IMAGE_LAYOUT_GENERAL;
    if (!reported_active || (Settings::values.mali_native_frame_gen_debug_overlay.GetValue() &&
                             frame_count % 120 == 0)) {
        LOG_INFO(Render_Vulkan,
                 "[MALI-FG] ACTIVE: real_frames={} generated_frames={} output_multiplier=2 mode={}",
                 frame_count, generated_frame_total,
                 ModeName(Settings::values.mali_native_frame_gen_mode.GetValue()));
        reported_active = true;
    }
    Common::Android::UpdatePocoX7ProFrameGenStatus("active", frame_count, generated_frame_total);
}

} // namespace Vulkan
