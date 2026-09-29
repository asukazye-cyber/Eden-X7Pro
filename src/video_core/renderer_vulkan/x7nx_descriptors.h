// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <map>
#include <tuple>
#include "shader_recompiler/shader_info.h"
#include "video_core/vulkan_common/vulkan_memory_allocator.h"

namespace Vulkan {
class Device;
class Scheduler;
namespace X7NX {
// Persistent, typed null-image backing. Entries are allocated only on first use, never per frame.
// Sampled and writable images do NOT alias: a guest invalid store cannot poison a null texture.
class MaliDescriptorCompatibilityLayer {
public:
    MaliDescriptorCompatibilityLayer(const Device&, MemoryAllocator&, Scheduler&);
    VkImageView Sampled(Shader::TextureType type, bool integer, bool depth, bool multisample);
    VkImageView Storage(Shader::TextureType type, VkFormat format);
    VkSampler Sampler(bool depth) const { return depth ? *shadow_sampler : *sampler; }
private:
    struct Entry { vk::Image image; vk::ImageView view; };
    VkImageView Get(Shader::TextureType type, VkFormat format, bool storage, bool depth, bool multisample);
    const Device& device;
    MemoryAllocator& allocator;
    Scheduler& scheduler;
    vk::Sampler sampler, shadow_sampler;
    std::map<std::tuple<Shader::TextureType, VkFormat, bool, bool>, Entry> images;
};
} // namespace X7NX
} // namespace Vulkan
