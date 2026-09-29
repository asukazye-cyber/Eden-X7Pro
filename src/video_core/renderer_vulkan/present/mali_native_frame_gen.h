// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <string_view>

#include "common/common_types.h"
#include "video_core/vulkan_common/vulkan_memory_allocator.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

class Device;
class Scheduler;
struct Frame;

// A small, self-contained two-pass Vulkan implementation: block matching produces a real motion
// field and confidence image, then a second compute pass motion-warps the two GPU-resident frames
// into the intermediate presentation frame. It never consumes the LSFG DLL pipeline.
class MaliNativeFrameGen {
public:
    explicit MaliNativeFrameGen(MemoryAllocator& memory_allocator, Scheduler& scheduler);
    ~MaliNativeFrameGen();

    void Process(const Device& device, Frame* frame, VkFormat format, VkExtent2D guest_extent);
    [[nodiscard]] size_t WantedGenerations(size_t capacity);
    [[nodiscard]] size_t GeneratedFrameCount() const;
    void GenerateInto(const Device& device, Frame* destination, size_t generation);

private:
    struct Image {
        vk::Image image;
        vk::ImageView view;
        VkExtent2D extent{};
        VkImageLayout layout{VK_IMAGE_LAYOUT_UNDEFINED};
    };

    struct MotionConstants {
        u32 frame_width;
        u32 frame_height;
        u32 motion_width;
        u32 motion_height;
        u32 motion_scale;
        u32 search_radius;
        f32 confidence_bias;
        f32 reserved;
    };
    static_assert(sizeof(MotionConstants) == 32);

    struct InterpolationConstants {
        u32 frame_width;
        u32 frame_height;
        u32 motion_width;
        u32 motion_height;
        u32 motion_scale;
        u32 reserved;
        f32 interpolation_time;
        f32 minimum_confidence;
        f32 edge_threshold;
        f32 reserved_float;
    };
    static_assert(sizeof(InterpolationConstants) == 40);

    struct QualityConfig {
        u32 motion_scale;
        u32 search_radius;
        f32 confidence_bias;
        f32 minimum_confidence;
        f32 edge_threshold;
    };

    [[nodiscard]] bool EnsureReady(const Device& device, Frame* frame, VkFormat format);
    void Rebuild(const Device& device, VkExtent2D extent);
    void Reset();
    void CopyCurrentFrame(Frame* frame);
    void UpdateMotionDescriptors(const Device& device, VkDescriptorSet set);
    void UpdateInterpolationDescriptors(const Device& device, VkDescriptorSet set,
                                        VkImageView target);
    [[nodiscard]] QualityConfig SelectQuality() const;
    void Disable(std::string_view reason);

    MemoryAllocator& memory_allocator;
    Scheduler& scheduler;

    std::array<Image, 2> history;
    Image motion;
    Image confidence;
    VkExtent2D extent{};
    VkExtent2D motion_extent{};
    VkFormat format{VK_FORMAT_UNDEFINED};
    u32 current_history{};
    u32 history_count{};
    u32 configured_motion_scale{};
    u64 frame_count{};
    u64 generated_frame_total{};
    size_t wanted_generations{};
    size_t generated_frames{};
    bool unavailable{};
    bool reported_active{};

    vk::DescriptorPool descriptor_pool;
    vk::DescriptorSetLayout motion_set_layout;
    vk::DescriptorSetLayout interpolation_set_layout;
    vk::PipelineLayout motion_pipeline_layout;
    vk::PipelineLayout interpolation_pipeline_layout;
    vk::ShaderModule motion_shader;
    vk::ShaderModule interpolation_shader;
    vk::Pipeline motion_pipeline;
    vk::Pipeline interpolation_pipeline;
    vk::DescriptorSets descriptor_sets;
};

} // namespace Vulkan
