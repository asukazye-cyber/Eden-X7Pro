// SPDX-License-Identifier: GPL-3.0-or-later
#include <cassert>
#include <cmath>
#include <iostream>
#include "common/android/x7pro_governor.h"
using namespace Common::Android::X7Pro;
int main() {
    Samples<300> samples;
    assert(samples.count == 0 && samples.Percentile(.99) == 0);
    for (int i = 1; i <= 300; ++i) samples.Add(i);
    assert(samples.Percentile(.95) == 285);
    assert(samples.Percentile(.99) == 297);
    assert(samples.SlowestOnePercentMean() == 299); // NOT P99 (297).
    samples.Add(1000);
    assert(samples.count == 300 && samples.total_count == 301);
    assert(samples.Percentile(.99) == 298);
    samples.Add(NAN); samples.Add(-1);
    assert(samples.total_count == 301);
    Samples<10> zero; zero.Add(0); assert(zero.count == 1 && zero.Average() == 0);
    assert(TimestampDelta(250, 4, 8) == 10);
    assert(TimestampDelta(~uint64_t{0} - 2, 3, 64) == 6);
    assert(TimestampDelta(1, 2, 0) == 0);

    Evidence e{.real_ms = 33.36, .real_p95 = 33.5, .real_p99 = 34,
        .cpu_core_ms = 12, .gpu_ms = 10, .fg_ms = 2, .present_ms = 1,
        .gpu_valid = true, .cpu_valid = true, .fg_valid = true, .queue_ready = true};
    Governor g;
    for (int i = 0; i < 29; ++i) assert(!g.Decide(e).admit);
    assert(g.Decide(e).admit && g.phase == Phase::Probing);
    for (int i = 0; i < 70; ++i) assert(g.Decide(e).admit);
    assert(g.phase == Phase::Active); // 33.36 ms jitter does not cause the old hard-gate storm.
    e.gpu_ms = 30;
    auto d = g.Decide(e); assert(!d.admit && d.reason == Drop::Budget);
    e.gpu_ms = 10;
    for (int i = 0; i < 15; ++i) assert(!g.Decide(e).admit);
    for (int i = 0; i < 29; ++i) assert(!g.Decide(e).admit);
    assert(g.Decide(e).admit);
    e.thermal = 3; d = g.Decide(e);
    assert(!d.admit && d.reason == Drop::Thermal && g.phase == Phase::Disabled);
    e.thermal = 0; e.automatic = false;
    assert(g.Decide(e).admit);
    e.gpu_valid = false; d = g.Decide(e);
    assert(!d.admit && d.reason == Drop::Budget); // Manual never invents GPU headroom.
    e.gpu_valid = true; e.queue_ready = false; d = g.Decide(e);
    assert(!d.admit && d.reason == Drop::Queue);
    e.queue_ready = true; e.real_ms = 48; d = g.Decide(e);
    assert(!d.admit && d.reason == Drop::RealDeadline);
    e.real_ms = 33.36; e.real_p95 = 70; e.real_p99 = 83;
    assert(!g.Decide(e).admit && g.margin == 9);
    std::cout << "Build19 policy/statistics/timestamp wrap tests passed\n";
}
