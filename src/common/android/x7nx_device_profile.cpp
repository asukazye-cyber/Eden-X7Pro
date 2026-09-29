// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/android/x7nx_device_profile.h"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <ranges>
#include <utility>

#include <sys/system_properties.h>

#include "common/logging.h"

namespace Common::Android::X7NX {
namespace {

struct State {
    std::mutex mutex;
    std::once_flag identity_once;
    DeviceProfileSnapshot snapshot{};
};

State& GetState() {
    static State state;
    return state;
}

std::string Property(const char* name) {
    char value[PROP_VALUE_MAX]{};
    return __system_property_get(name, value) > 0 ? value : "";
}

std::string Lower(std::string value) {
    std::ranges::transform(value, value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool Contains(const std::string& value, std::string_view needle) {
    return value.find(needle) != std::string::npos;
}

void RefreshKind(DeviceProfileSnapshot& snapshot) {
    snapshot.kind = snapshot.poco_x7_pro && snapshot.dimensity_8400 && snapshot.mali_g720
                        ? DeviceKind::PocoX7ProDimensity8400
                        : DeviceKind::SafeFallback;
}

} // namespace

DeviceProfile& DeviceProfile::Instance() {
    static DeviceProfile profile;
    return profile;
}

void DeviceProfile::DetectAndroidIdentity() {
    State& state = GetState();
    std::call_once(state.identity_once, [&] {
        const std::string soc = Lower(Property("ro.soc.model") + " " +
                                      Property("ro.board.platform") + " " +
                                      Property("ro.hardware") + " " +
                                      Property("ro.mediatek.platform"));
        const std::string product = Lower(Property("ro.product.brand") + " " +
                                          Property("ro.product.model") + " " +
                                          Property("ro.product.device"));
        std::scoped_lock lock{state.mutex};
        state.snapshot.dimensity_8400 = Contains(soc, "mt6899") ||
                                         Contains(soc, "dimensity 8400") ||
                                         Contains(soc, "dimensity8400");
        state.snapshot.poco_x7_pro = Contains(product, "poco x7 pro") ||
                                    Contains(product, "2412dpc0a") || Contains(product, "rodin");
        // This is a model hint, not an affinity mask. The runtime scheduler still probes /sys.
        state.snapshot.all_big_core_hint = state.snapshot.dimensity_8400;
        RefreshKind(state.snapshot);
        LOG_INFO(Common, "[X7NX] Android identity: soc='{}' product='{}' target={}", soc, product,
                 state.snapshot.kind == DeviceKind::PocoX7ProDimensity8400);
    });
}

void DeviceProfile::UpdateVulkanCapabilities(VulkanCapabilities capabilities) {
    DetectAndroidIdentity();
    State& state = GetState();
    const std::string identity = Lower(capabilities.model_name + " " + capabilities.driver_name);
    std::scoped_lock lock{state.mutex};
    state.snapshot.mali_g720 = Contains(identity, "mali-g720") || Contains(identity, "malig720") ||
                              Contains(identity, "g720 mc") || Contains(identity, "g720mc");
    state.snapshot.mali_g7xx = state.snapshot.mali_g720 || Contains(identity, "mali-g7");
    state.snapshot.vulkan = std::move(capabilities);
    RefreshKind(state.snapshot);
    LOG_INFO(Common, "[X7NX] Vulkan profile: model='{}' driver='{}' target={} sync2={} nullDescriptor={}",
             state.snapshot.vulkan.model_name, state.snapshot.vulkan.driver_name,
             state.snapshot.kind == DeviceKind::PocoX7ProDimensity8400,
             state.snapshot.vulkan.synchronization2, state.snapshot.vulkan.null_descriptor);
}

DeviceProfileSnapshot DeviceProfile::Snapshot() const {
    State& state = GetState();
    std::scoped_lock lock{state.mutex};
    return state.snapshot;
}

bool DeviceProfile::IsPocoX7ProDimensity8400() const {
    return Snapshot().kind == DeviceKind::PocoX7ProDimensity8400;
}

} // namespace Common::Android::X7NX
