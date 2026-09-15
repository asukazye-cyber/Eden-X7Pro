#!/usr/bin/env python3
"""Structural guardrails complement the compiled policy tests and real Android build."""
from pathlib import Path
import sys

root = Path(sys.argv[1])
def read(name):
    return (root / 'src' / name).read_text(encoding='utf-8')

fg = read('video_core/renderer_vulkan/present/mali_native_frame_gen.cpp')
process = fg[fg.index('void MaliNativeFrameGen::Process'):fg.index('void MaliNativeFrameGen::Skip')]
assert process.index('wanted_generations == 0') < process.index('EnsureReady')
assert 'frame_count % MAX_IN_FLIGHT_SETS' not in fg
assert 'motion_old_layout' in fg and 'confidence_old_layout' in fg
profile = read('common/android/poco_x7pro_profile.cpp')
assert 'ShouldGeneratePocoX7ProFrame' not in profile
assert 'Percentile(state' not in profile
assert 'mt6899 || state.poco_x7_pro' in profile
assert 'malig720' in profile and 'g720mc' in profile
timing = read('video_core/renderer_vulkan/x7pro_gpu_timing.h')
assert 'VK_QUERY_RESULT_WITH_AVAILABILITY_BIT' in timing
assert 'VK_QUERY_RESULT_WAIT_BIT' not in timing
assert 'WaitIdle' not in timing and '.Wait(' not in timing
assert 'timestampValidBits' in timing and 'TimestampDelta' in timing
present = read('video_core/renderer_vulkan/vk_present_manager.cpp')
reserve = present[present.index('Frame* PresentManager::TryGetSyntheticFrame'):present.index('void PresentManager::ReleaseUnusedSyntheticFrame')]
assert '.Wait(' not in reserve and 'try_to_lock' in reserve
assert 'synthetic_in_flight || free_queue.size() < 2' in reserve
assert 'DrainSyntheticFrame(frame)' in present
assert 'if (accepted) Common::Android::X7Pro::Presented' in present
renderer = read('video_core/renderer_vulkan/renderer_vulkan.cpp')
assert renderer.index('T::Admit') < renderer.index('frame_gen.Process')
assert 'scheduler.Flush(*frame->render_ready, {}, true)' in renderer
assert 'SetResolution' not in renderer
telemetry = read('common/android/x7pro_telemetry.cpp')
assert 'CLOCK_THREAD_CPUTIME_ID' in telemetry
assert 'SlowestOnePercentMean' in telemetry
assert 'AThermal_registerThermalStatusListener' in telemetry
assert 'NOT physical scanout' in telemetry
print('Build19 structural guardrails passed')
