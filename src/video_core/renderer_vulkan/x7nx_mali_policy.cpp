// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "video_core/renderer_vulkan/x7nx_mali_policy.h"

#include "common/android/x7nx_device_profile.h"
#include "common/logging.h"
#include "video_core/vulkan_common/vulkan_device.h"

namespace Vulkan::X7NX {

MaliG7xxPolicy BuildMaliG7xxPolicy(const Device& device) {
    MaliG7xxPolicy policy{};
    policy.enabled = device.X7Backend().IsMaliG720();
    if (!policy.enabled) {
        return policy;
    }
    policy.use_synchronization2 = device.HasSynchronization2();
    policy.use_precise_barriers = policy.use_synchronization2;
    policy.use_dummy_descriptors = !device.X7Backend().UseNativeNullDescriptors(device.HasNullDescriptor());
    policy.prefer_transient_attachments = device.IsTiler();
    policy.reuse_attachment_resources = true;
    // Render-pass feedback has correctness-sensitive aliasing/lifetime requirements. Preserve
    // Eden's copy/clone fallback until replay validation proves the tile-local path for a driver.
    policy.framebuffer_feedback = FeedbackPath::CopyClone;
    LOG_INFO(Render_Vulkan,
             "[X7NX] Mali-G7xx policy: sync2={} dummy_descriptors={} transient_attachments={} feedback=copy-clone",
             policy.use_synchronization2, policy.use_dummy_descriptors,
             policy.prefer_transient_attachments);
    return policy;
}

const char* RendererPathName(const MaliG7xxPolicy& policy) {
    return policy.enabled ? "x7nx-mali-g7xx-copy-clone" : "eden-default";
}

} // namespace Vulkan::X7NX
