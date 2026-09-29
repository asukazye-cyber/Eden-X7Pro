# X7NX phase 1 architecture

X7NX is an Android ARM64 fork whose only shipping Android target is the POCO X7 Pro. It retains
Eden's mature guest CPU/NCE, HLE, loader, services, shader decoder, Vulkan scheduler and pipeline
cache as internal components. It does not change guest timing and it does not contain frame
generation in phase 1.

## Audited integration points

| Area | Eden integration point | Phase 1 action |
| --- | --- | --- |
| Android identity | `src/android/settings.gradle.kts`, `src/android/app/build.gradle.kts` | Own `X7NX` Gradle root, one ARM64-only `x7nx` target, and independent install package. |
| Device identification | `src/common/android/` | Add immutable profile snapshot populated from Android properties and Vulkan capabilities. |
| Host scheduling | `src/video_core/gpu_thread.cpp`, `vk_scheduler.cpp`, `vk_present_manager.cpp` | Apply role/deadline policy. Runtime topology is inspected; an all-big topology is never split into imaginary efficiency cores. |
| Vulkan boundary | `renderer_vulkan/x7nx_backend.*` | Fork-owned entry point initializes the device profile, Mali policy, UMA policy and frame telemetry before delegating to proven Eden renderer components. |
| Descriptor legalization | `vk_texture_cache.cpp`, `vk_buffer_cache.cpp` | Route image and buffer null-descriptor decisions through one X7NX legalizer; dummy descriptors stay the compatibility fallback. |
| UMA lifetime | `renderer_vulkan.cpp` | Record memory budget and explicitly classify capture readbacks as exceptional. Existing persistent frame/ring pools remain the allocator implementation. |
| Pipeline cache | `vk_pipeline_cache.cpp` | Preserve Eden's asynchronous per-title load/flush design; add cache/compile telemetry and a game-specific prewarm plan. |
| Game policy | `x7nx_game_profile.*` | Add documentation-only compatibility policies for Pokémon Sword and NieR:Automata. |

## Data flow

```text
Android properties + /sys CPU topology + Vulkan device capabilities
                            |
                            v
                 X7NX Backend / DeviceProfile
                    /        |        \
                   v         v         v
        role/deadline scheduler  Mali-G7xx policy  shader legalizer
                   |         |         |
                   +---------+---------+
                             v
        reused Vulkan scheduler / cache / resource pools / renderer
                             |
                             v
              [X7NX] telemetry + replay capture points
```

The policy is intentionally conservative: unknown SoC, unknown driver, unavailable
synchronization2, or an unsafe attachment/feedback condition selects Eden's normal copy/clone
path. No policy may make `VK_EXT_robustness2` or `nullDescriptor` mandatory.

The build rejects non-Android and non-ARM64 configurations when `ENABLE_X7NX` is selected. The
old multi-product Eden flavors are deliberately absent from this fork's Android build.

## Explicit non-goals

* No frame generation, frame duplication, speed-limit bypass, or change to Switch timing.
* No claim that a tile-local feedback path is safe until it is validated on a target driver.
* No GPU-to-CPU readback in a render hot path; capture remains an explicit exception.
