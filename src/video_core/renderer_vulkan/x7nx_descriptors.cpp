// SPDX-License-Identifier: GPL-3.0-or-later
#include "video_core/renderer_vulkan/x7nx_descriptors.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/vulkan_common/vulkan_device.h"

namespace Vulkan::X7NX {
MaliDescriptorCompatibilityLayer::MaliDescriptorCompatibilityLayer(
    const Device& device_, MemoryAllocator& allocator_, Scheduler& scheduler_)
    : device{device_}, allocator{allocator_}, scheduler{scheduler_} {
    VkSamplerCreateInfo info{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST, .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .maxAnisotropy = 1.0f, .compareOp = VK_COMPARE_OP_NEVER};
    sampler = device.GetLogical().CreateSampler(info);
    info.compareEnable = VK_TRUE;
    shadow_sampler = device.GetLogical().CreateSampler(info);
}
VkImageView MaliDescriptorCompatibilityLayer::Sampled(
    Shader::TextureType type, bool integer, bool depth, bool multisample) {
    return Get(type, depth ? VK_FORMAT_D32_SFLOAT : integer ? VK_FORMAT_R32G32B32A32_UINT
        : VK_FORMAT_R8G8B8A8_UNORM, false, depth, multisample);
}
VkImageView MaliDescriptorCompatibilityLayer::Storage(Shader::TextureType type, VkFormat format) {
    return Get(type, format, true, false, false);
}
VkImageView MaliDescriptorCompatibilityLayer::Get(Shader::TextureType type, VkFormat format,
                                                   bool storage, bool depth, bool multisample) {
    const auto key = std::tuple{type, format, storage, multisample};
    if (const auto it = images.find(key); it != images.end()) return *it->second.view;
    using T = Shader::TextureType;
    VkImageType image_type = VK_IMAGE_TYPE_2D;
    VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
    u32 layers = 1;
    VkImageCreateFlags flags{};
    switch (type) {
    case T::Color1D: image_type = VK_IMAGE_TYPE_1D; view_type = VK_IMAGE_VIEW_TYPE_1D; break;
    case T::ColorArray1D: image_type = VK_IMAGE_TYPE_1D; view_type = VK_IMAGE_VIEW_TYPE_1D_ARRAY; break;
    case T::ColorArray2D: view_type = VK_IMAGE_VIEW_TYPE_2D_ARRAY; break;
    case T::Color3D: image_type = VK_IMAGE_TYPE_3D; view_type = VK_IMAGE_VIEW_TYPE_3D; break;
    case T::ColorCube: view_type = VK_IMAGE_VIEW_TYPE_CUBE; layers = 6; flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT; break;
    case T::ColorArrayCube: view_type = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY; layers = 6; flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT; break;
    case T::Color2D: case T::Color2DRect: break;
    case T::Buffer: throw vk::Exception(VK_ERROR_FORMAT_NOT_SUPPORTED); // Uses the texel-buffer owner instead.
    }
    const VkFormatFeatureFlags required = VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
        (storage ? VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT : VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
    if (!device.IsFormatSupported(format, required, FormatType::Optimal))
        throw vk::Exception(VK_ERROR_FORMAT_NOT_SUPPORTED);
    const VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        (storage ? VK_IMAGE_USAGE_STORAGE_BIT : VK_IMAGE_USAGE_SAMPLED_BIT);
    VkImageFormatProperties properties{};
    vk::Check(device.GetDispatchLoader().vkGetPhysicalDeviceImageFormatProperties(
        device.GetPhysical(), format, image_type, VK_IMAGE_TILING_OPTIMAL, usage, flags, &properties));
    if (properties.maxArrayLayers < layers)
        throw vk::Exception(VK_ERROR_FORMAT_NOT_SUPPORTED);
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    if (multisample) {
        const auto supported = properties.sampleCounts;
        if (supported & VK_SAMPLE_COUNT_2_BIT) samples = VK_SAMPLE_COUNT_2_BIT;
        else if (supported & VK_SAMPLE_COUNT_4_BIT) samples = VK_SAMPLE_COUNT_4_BIT;
        else throw vk::Exception(VK_ERROR_FORMAT_NOT_SUPPORTED);
    }
    Entry entry;
    entry.image = allocator.CreateImage({.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = flags, .imageType = image_type, .format = format, .extent = {1, 1, 1},
        .mipLevels = 1, .arrayLayers = layers, .samples = samples, .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED});
    const VkImageAspectFlags aspect = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    const VkImageSubresourceRange range{aspect, 0, 1, 0, layers};
    entry.view = device.GetLogical().CreateImageView({.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = *entry.image, .viewType = view_type, .format = format, .subresourceRange = range});
    // Establish ownership before recording commands that retain these Vulkan handles.
    auto& persistent = images.emplace(key, std::move(entry)).first->second;
    scheduler.RequestOutsideRenderPassOperationContext();
    scheduler.Record([image = *persistent.image, range, depth, storage, dispatch = &device.GetDispatchLoader()](vk::CommandBuffer cmd) {
        VkImageMemoryBarrier barrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image, .subresourceRange = range};
        cmd.PipelineBarrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, barrier);
        if (depth) {
            const VkClearDepthStencilValue zero{};
            dispatch->vkCmdClearDepthStencilImage(*cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &zero, 1, &range);
        } else cmd.ClearColorImage(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VkClearColorValue{}, range);
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | (storage ? VK_ACCESS_SHADER_WRITE_BIT : 0);
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        cmd.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, vk::PIPELINE_STAGE_GRAPHICS_COMPUTE, 0, barrier);
    });
    return *persistent.view;
}
} // namespace Vulkan::X7NX
