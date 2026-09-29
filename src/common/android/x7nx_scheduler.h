// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <vector>

#include "common/common_types.h"

namespace Common::Android::X7NX {

enum class ThreadRole : u8 {
    EmulationCritical,
    GpuSubmission,
    RenderWorker,
    ShaderCompiler,
    Audio,
    IO,
    Background,
};

struct CpuCoreInfo {
    s32 cpu{};
    s64 capacity{};
    s64 max_frequency_khz{};
    s64 current_frequency_khz{};
    u64 midr{};
    s64 l2_cache_bytes{};
};

struct CpuTopology {
    std::vector<CpuCoreInfo> allowed_cores;
    bool all_big_core{};
    bool topology_complete{};
};

// Frame deadlines influence priority, never guest timing. Android retains affinity/cpuset control;
// different frequency bins of an all-A725 SoC must not be treated as efficiency cores.
class ThreadPolicy final {
public:
    static CpuTopology ProbeTopology();
    static void ApplyCurrentThread(ThreadRole role,
                                   std::chrono::nanoseconds frame_deadline = {});
};

} // namespace Common::Android::X7NX
