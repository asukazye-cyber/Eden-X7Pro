// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <string>

#include "common/common_types.h"

namespace Common::Android::X7NX {

struct TelemetrySnapshot {
    f64 cpu_frame_p50_ms{};
    f64 cpu_frame_p95_ms{};
    f64 cpu_frame_p99_ms{};
    f64 gpu_frame_p50_ms{};
    f64 gpu_frame_p95_ms{};
    f64 gpu_frame_p99_ms{};
    u64 pipeline_cache_hits{};
    u64 pipeline_cache_misses{};
    u64 shader_compile_stalls{};
    u64 prewarm_requests{};
    u64 prewarm_pipelines{};
    u64 barrier_count{};
    u64 device_lost_count{};
    u64 device_memory_bytes{};
    std::string renderer_path{"eden-default"};
};

class Telemetry final {
public:
#ifdef NDEBUG
    // Instrumentation is deliberately compiled out of performance builds. Inline calls disappear
    // without locks, sampling, sorting or GPU timing claims; debug builds retain the collector.
    static Telemetry& Instance() { static Telemetry telemetry; return telemetry; }
    void RecordCpuFrame(f64) {}
    void RecordGpuFrame(f64) {}
    void RecordPipelineCacheHit() {}
    void RecordPipelineCacheMiss() {}
    void RecordShaderCompileStall(f64) {}
    void RecordPrewarm(size_t) {}
    void RecordBarriers(u32) {}
    void RecordDeviceLost() {}
    void SetDeviceMemoryBytes(u64) {}
    void SetRendererPath(const std::string&) {}
    [[nodiscard]] TelemetrySnapshot Snapshot() const { return {}; }
    void LogSummaryIfDue() {}
#else
    static Telemetry& Instance();

    void RecordCpuFrame(f64 milliseconds);
    void RecordGpuFrame(f64 milliseconds);
    void RecordPipelineCacheHit();
    void RecordPipelineCacheMiss();
    void RecordShaderCompileStall(f64 milliseconds);
    void RecordPrewarm(size_t pipelines);
    void RecordBarriers(u32 count);
    void RecordDeviceLost();
    void SetDeviceMemoryBytes(u64 bytes);
    void SetRendererPath(std::string path);
    [[nodiscard]] TelemetrySnapshot Snapshot() const;
    void LogSummaryIfDue();
#endif

private:
    Telemetry() = default;
};

} // namespace Common::Android::X7NX
