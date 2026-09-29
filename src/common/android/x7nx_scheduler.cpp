// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/android/x7nx_scheduler.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <ranges>
#include <string>

#include <sched.h>
#include <unistd.h>

#include "common/android/x7nx_device_profile.h"
#include "common/logging.h"
#include "common/thread.h"

namespace Common::Android::X7NX {
namespace {

s64 ReadDecimal(const std::string& path) {
    s64 value{};
    std::ifstream file{path};
    return file >> value ? value : 0;
}

u64 ReadHex(const std::string& path) {
    u64 value{};
    std::ifstream file{path};
    return file >> std::hex >> value ? value : 0;
}

s64 ReadCacheSize(s32 cpu) {
    // Android kernels expose different cache index layouts. Keep the largest private cache value;
    // it is diagnostic/ranking metadata, never a hard-coded cluster assignment.
    s64 largest{};
    for (s32 index = 0; index < 8; ++index) {
        const std::string base = "/sys/devices/system/cpu/cpu" + std::to_string(cpu) +
                                 "/cache/index" + std::to_string(index) + "/";
        std::ifstream level{base + "level"};
        std::ifstream type{base + "type"};
        std::string type_name;
        s32 cache_level{};
        if (!(level >> cache_level) || !(type >> type_name) || cache_level != 2) {
            continue;
        }
        std::ifstream size{base + "size"};
        std::string value;
        if (!(size >> value) || value.empty()) {
            continue;
        }
        const char suffix = value.back();
        value.pop_back();
        const s64 kib = std::strtoll(value.c_str(), nullptr, 10);
        if (kib > 0) {
            largest = std::max(largest, suffix == 'M' ? kib * 1024 * 1024 : kib * 1024);
        }
    }
    return largest;
}

const char* RoleName(ThreadRole role) {
    switch (role) {
    case ThreadRole::EmulationCritical: return "emulation";
    case ThreadRole::GpuSubmission: return "gpu-submit";
    case ThreadRole::RenderWorker: return "render-worker";
    case ThreadRole::ShaderCompiler: return "shader-compiler";
    case ThreadRole::Audio: return "audio";
    case ThreadRole::IO: return "io";
    case ThreadRole::Background: return "background";
    }
    return "unknown";
}

ThreadPriority PriorityFor(ThreadRole role, std::chrono::nanoseconds deadline) {
    if (role == ThreadRole::EmulationCritical || role == ThreadRole::GpuSubmission) {
        return deadline != std::chrono::nanoseconds{} && deadline <= std::chrono::milliseconds{4}
                   ? ThreadPriority::Critical
                   : ThreadPriority::VeryHigh;
    }
    if (role == ThreadRole::RenderWorker || role == ThreadRole::Audio) {
        return ThreadPriority::High;
    }
    if (role == ThreadRole::Background) {
        return ThreadPriority::Low;
    }
    return ThreadPriority::Normal;
}

} // namespace

CpuTopology ThreadPolicy::ProbeTopology() {
    CpuTopology topology;
    cpu_set_t allowed{};
    if (sched_getaffinity(getpid(), sizeof(allowed), &allowed) != 0) {
        return topology;
    }
    const long total = sysconf(_SC_NPROCESSORS_CONF);
    if (total <= 0) {
        return topology;
    }
    const s32 cpu_count = static_cast<s32>(std::min<long>(total, CPU_SETSIZE));
    for (s32 cpu = 0; cpu < cpu_count; ++cpu) {
        if (!CPU_ISSET(cpu, &allowed)) {
            continue;
        }
        const std::string base = "/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/";
        CpuCoreInfo info{.cpu = cpu,
                         .capacity = ReadDecimal(base + "cpu_capacity"),
                         .max_frequency_khz = ReadDecimal(base + "cpufreq/cpuinfo_max_freq"),
                         .midr = ReadHex(base + "regs/identification/midr_el1"),
                         .l2_cache_bytes = ReadCacheSize(cpu)};
        topology.allowed_cores.push_back(info);
    }
    topology.topology_complete = !topology.allowed_cores.empty() &&
                                 std::ranges::all_of(topology.allowed_cores, [](const auto& core) {
                                     return core.capacity > 0 || core.max_frequency_khz > 0;
                                 });
    const u64 first_midr = topology.allowed_cores.empty() ? 0 : topology.allowed_cores.front().midr;
    topology.all_big_core = first_midr != 0 && std::ranges::all_of(
        topology.allowed_cores, [first_midr](const auto& core) { return core.midr == first_midr; });
    return topology;
}

void ThreadPolicy::ApplyCurrentThread(ThreadRole role, std::chrono::nanoseconds frame_deadline) {
    if (!DeviceProfile::Instance().IsPocoX7ProDimensity8400()) {
        return;
    }
    const CpuTopology topology = ProbeTopology();
    SetCurrentThreadPriority(PriorityFor(role, frame_deadline));
    if (topology.all_big_core || !topology.topology_complete) {
        SetCurrentThreadToAllCores();
        LOG_DEBUG(Common, "[X7NX] role={} uses all allowed cores (all-big={} complete={})",
                  RoleName(role), topology.all_big_core, topology.topology_complete);
        return;
    }

    // A non-target/heterogeneous firmware can still have a meaningful capacity split. Leave
    // background work to Eden's conservative policy; critical roles receive the measured fast set.
    if (role == ThreadRole::ShaderCompiler || role == ThreadRole::IO || role == ThreadRole::Background) {
        SetCurrentThreadToBackgroundWork();
    } else {
        SetCurrentThreadToPerformanceCores();
    }
    LOG_DEBUG(Common, "[X7NX] role={} uses measured heterogeneous topology", RoleName(role));
}

} // namespace Common::Android::X7NX
