#!/usr/bin/env python3
"""Backend wiring checks and bitfield lowering reference properties (not device tests)."""
from pathlib import Path
import random

root = Path(__file__).resolve().parents[1]
def read(path):
    return (root / path).read_text(encoding="utf-8")

mask = 0xffffffff
rng = random.Random(720)
for offset in range(33):
    for width in range(33 - offset):
        for source in [0, mask, 0x80000000, 0x7fffffff] + [rng.getrandbits(32) for _ in range(100)]:
            expected = (source >> offset) & ((1 << width) - 1)
            lowered_unsigned = 0 if width == 0 else source if width == 32 else (source >> offset) & ((1 << width) - 1)
            expected_signed = expected - (1 << width) if width and expected & (1 << (width - 1)) else expected
            if width == 0:
                lowered_signed = 0
            elif width == 32:
                lowered_signed = source
            else:
                left = (source << (32 - offset - width)) & mask
                signed = left - (1 << 32) if left & (1 << 31) else left
                lowered_signed = signed >> (32 - width)
            assert lowered_unsigned == expected
            assert (lowered_signed & mask) == (expected_signed & mask)

pipeline = read("src/video_core/renderer_vulkan/vk_pipeline_cache.cpp")
assert pipeline.count("X7NX::LegalizeShader(device, program)") == 2
dummy = read("src/video_core/renderer_vulkan/x7nx_descriptors.cpp")
assert "VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL" in dummy
assert "VK_ACCESS_TRANSFER_WRITE_BIT" in dummy
assert "std::tuple{type, format, storage, multisample}" in dummy
assert "VK_IMAGE_VIEW_TYPE_CUBE_ARRAY" in dummy
assert "WaitIdle" not in dummy
assert "#ifdef NDEBUG" in read("src/common/android/x7nx_telemetry.h")
scheduler = read("src/common/android/x7nx_scheduler.cpp")
assert "SetCurrentThreadTo" not in scheduler
assert "scaling_cur_freq" in scheduler
renderer = read("src/video_core/renderer_vulkan/renderer_vulkan.cpp")
assert "IsReadbackAllowed" not in renderer
assert "ReadbackReason" not in renderer
print("X7NX backend source contracts and bitfield reference properties: OK")
