// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "video_core/renderer_vulkan/x7nx_game_profile.h"

#include <fmt/format.h>

#include "common/android/x7nx_device_profile.h"
#include "video_core/vulkan_common/vulkan_device.h"

namespace Vulkan::X7NX {

GameProfile ProfileForTitle(u64 title_id) {
    switch (title_id) {
    case 0x0100ABF008968000: // Pokémon Sword
        return {.name = "pokemon-sword", .prewarm_pipeline_cache = true,
                .conservative_framebuffer_feedback = true, .conservative_depth_stencil = false};
    case 0x0100E0B012D02000: // NieR:Automata The End of YoRHa Edition
        return {.name = "nier-automata", .prewarm_pipeline_cache = true,
                .conservative_framebuffer_feedback = true, .conservative_depth_stencil = true};
    default:
        return {};
    }
}

std::string CacheNamespace(const Device& device) {
    const auto profile = Common::Android::X7NX::DeviceProfile::Instance().Snapshot();
    if (profile.kind != Common::Android::X7NX::DeviceKind::PocoX7ProDimensity8400 ||
        device.GetDriverID() != VK_DRIVER_ID_ARM_PROPRIETARY) {
        return "eden-compatible";
    }
    // IR legalization and descriptor semantics changed; old SPIR-V/pipeline blobs are not reused.
    return fmt::format("x7nx-abi3-vk{:08x}-drv{:08x}", device.ApiVersion(), device.GetDriverVersion());
}

} // namespace Vulkan::X7NX
