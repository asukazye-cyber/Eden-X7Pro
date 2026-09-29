// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/android/x7nx_telemetry.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <mutex>

#include "common/logging.h"

namespace Common::Android::X7NX {
namespace {
constexpr size_t SAMPLE_COUNT = 240;

struct State {
    std::mutex mutex;
    std::array<f64, SAMPLE_COUNT> cpu{};
    std::array<f64, SAMPLE_COUNT> gpu{};
    size_t cpu_size{};
    size_t gpu_size{};
    size_t cpu_next{};
    size_t gpu_next{};
    TelemetrySnapshot counters{};
    std::chrono::steady_clock::time_point last_log{};
};

State& GetState() {
    static State state;
    return state;
}

f64 Percentile(const std::array<f64, SAMPLE_COUNT>& source, size_t count, f64 p) {
    if (count == 0) {
        return 0.0;
    }
    std::array<f64, SAMPLE_COUNT> sorted{};
    std::copy_n(source.begin(), count, sorted.begin());
    std::sort(sorted.begin(), sorted.begin() + count);
    return sorted[std::min(count - 1, static_cast<size_t>((count - 1) * p))];
}

void AddSample(std::array<f64, SAMPLE_COUNT>& samples, size_t& size, size_t& next, f64 value) {
    if (value <= 0.0) {
        return;
    }
    samples[next] = value;
    next = (next + 1) % samples.size();
    size = std::min(size + 1, samples.size());
}
} // namespace

Telemetry& Telemetry::Instance() {
    static Telemetry telemetry;
    return telemetry;
}

void Telemetry::RecordCpuFrame(f64 milliseconds) { auto& s = GetState(); std::scoped_lock l{s.mutex}; AddSample(s.cpu, s.cpu_size, s.cpu_next, milliseconds); }
void Telemetry::RecordGpuFrame(f64 milliseconds) { auto& s = GetState(); std::scoped_lock l{s.mutex}; AddSample(s.gpu, s.gpu_size, s.gpu_next, milliseconds); }
void Telemetry::RecordPipelineCacheHit() { auto& s = GetState(); std::scoped_lock l{s.mutex}; ++s.counters.pipeline_cache_hits; }
void Telemetry::RecordPipelineCacheMiss() { auto& s = GetState(); std::scoped_lock l{s.mutex}; ++s.counters.pipeline_cache_misses; }
void Telemetry::RecordShaderCompileStall(f64) { auto& s = GetState(); std::scoped_lock l{s.mutex}; ++s.counters.shader_compile_stalls; }
void Telemetry::RecordPrewarm(size_t pipelines) { auto& s = GetState(); std::scoped_lock l{s.mutex}; ++s.counters.prewarm_requests; s.counters.prewarm_pipelines += pipelines; }
void Telemetry::RecordBarriers(u32 count) { auto& s = GetState(); std::scoped_lock l{s.mutex}; s.counters.barrier_count += count; }
void Telemetry::RecordDeviceLost() { auto& s = GetState(); std::scoped_lock l{s.mutex}; ++s.counters.device_lost_count; }
void Telemetry::SetDeviceMemoryBytes(u64 bytes) { auto& s = GetState(); std::scoped_lock l{s.mutex}; s.counters.device_memory_bytes = bytes; }
void Telemetry::SetRendererPath(std::string path) { auto& s = GetState(); std::scoped_lock l{s.mutex}; s.counters.renderer_path = std::move(path); }

TelemetrySnapshot Telemetry::Snapshot() const {
    auto& s = GetState();
    std::scoped_lock l{s.mutex};
    auto result = s.counters;
    result.cpu_frame_p50_ms = Percentile(s.cpu, s.cpu_size, .50);
    result.cpu_frame_p95_ms = Percentile(s.cpu, s.cpu_size, .95);
    result.cpu_frame_p99_ms = Percentile(s.cpu, s.cpu_size, .99);
    result.gpu_frame_p50_ms = Percentile(s.gpu, s.gpu_size, .50);
    result.gpu_frame_p95_ms = Percentile(s.gpu, s.gpu_size, .95);
    result.gpu_frame_p99_ms = Percentile(s.gpu, s.gpu_size, .99);
    return result;
}

void Telemetry::LogSummaryIfDue() {
    auto& s = GetState();
    const auto now = std::chrono::steady_clock::now();
    { std::scoped_lock l{s.mutex}; if (s.last_log.time_since_epoch().count() && now - s.last_log < std::chrono::seconds{10}) return; s.last_log = now; }
    const auto snapshot = Snapshot();
    LOG_INFO(Common, "[X7NX] path={} cpu_ms(p50/p95/p99)={:.2f}/{:.2f}/{:.2f} gpu_ms(p50/p95/p99)={:.2f}/{:.2f}/{:.2f} cache={}/{} prewarm={}/{} stalls={} barriers={} memory={} device_lost={}", snapshot.renderer_path, snapshot.cpu_frame_p50_ms, snapshot.cpu_frame_p95_ms, snapshot.cpu_frame_p99_ms, snapshot.gpu_frame_p50_ms, snapshot.gpu_frame_p95_ms, snapshot.gpu_frame_p99_ms, snapshot.pipeline_cache_hits, snapshot.pipeline_cache_misses, snapshot.prewarm_requests, snapshot.prewarm_pipelines, snapshot.shader_compile_stalls, snapshot.barrier_count, snapshot.device_memory_bytes, snapshot.device_lost_count);
}

} // namespace Common::Android::X7NX
