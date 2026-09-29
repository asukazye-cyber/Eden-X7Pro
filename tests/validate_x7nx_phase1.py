#!/usr/bin/env python3
"""Small source-level contract checks for the isolated X7NX Phase 1 flavor."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(name: str) -> str:
    return (ROOT / name).read_text(encoding="utf-8")


def main() -> None:
    gradle = read("src/android/app/build.gradle.kts")
    settings = read("src/android/settings.gradle.kts")
    cmake = read("CMakeLists.txt")
    docs = read("docs/X7NX_PHASE1_ARCHITECTURE.md")
    profile = read("src/common/android/x7nx_device_profile.cpp")
    scheduler = read("src/common/android/x7nx_scheduler.cpp")
    policy = read("src/video_core/renderer_vulkan/x7nx_mali_policy.cpp")
    buffer_cache = read("src/video_core/renderer_vulkan/vk_buffer_cache.cpp")
    pipeline_cache = read("src/video_core/renderer_vulkan/vk_pipeline_cache.cpp")

    assert 'applicationId = "org.x7nx.emulator"' in gradle
    assert 'rootProject.name = "X7NX"' in settings
    assert 'include("X7NX")' in settings
    assert 'create("mainline")' not in gradle
    assert 'create("maliX7Pro")' not in gradle
    assert '"-DENABLE_X7NX=ON"' in gradle
    assert '"-DENABLE_MALI_X7PRO_FG=OFF"' in gradle
    assert "ENABLE_X7NX" in cmake
    assert "X7NX is intentionally limited to Android ARM64" in cmake
    assert "VK_EXT_robustness2" in docs
    assert "cpu_capacity" in scheduler and "cpuinfo_max_freq" in scheduler
    assert "l2_cache_bytes" in scheduler and "all_big_core" in scheduler
    assert "CopyClone" in policy and "HasSynchronization2" in policy
    assert "null_descriptor" in profile
    assert (ROOT / "src/video_core/renderer_vulkan/x7nx_backend.cpp").exists()
    assert "device.GetDriverName()" in read("src/video_core/renderer_vulkan/x7nx_gpu_backend.cpp")
    assert "AllowsNullDescriptor" in buffer_cache
    assert "X7NX::CacheNamespace(device)" in pipeline_cache
    assert "RecordPrewarm(state.total)" in pipeline_cache

    for path in ("profiles/x7nx/pokemon-sword.json", "profiles/x7nx/nier-automata.json"):
        data = json.loads(read(path))
        assert data["policies"]["frame_generation"] == "disabled"
        assert data["policies"]["framebuffer_feedback"] == "copy-clone"
    print("X7NX Phase 1 source contract: OK")


if __name__ == "__main__":
    main()
