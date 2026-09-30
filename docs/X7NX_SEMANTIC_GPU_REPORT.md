# X7 Semantic GPU Recompiler — first executable slice (Phase 6)

## Scope and status

The first semantic compiler is integrated into the real Vulkan scheduler and CopyImage lowering.
It is deliberately bounded to a pending single-color clear followed by one image copy or an
observation boundary. It does NOT replace the Maxwell frontend or pretend to optimize whole frames.
No Wild Area/NieR result, FPS improvement or activation frequency on a physical POCO is claimed.
The preceding Phase5 native-fetch path remains experimental, opt-in, and independently guarded.

Actual flow:

    guest clear -> DeferColorClear -> X7RenderIR + VirtualFramebuffer pending content
    guest image copy -> texture-cache resource metadata -> RAW/WAR/WAW + alias/region proof
    -> cached MaliRenderProgram -> execute retained clear or discard dead clear
    -> original Vulkan copy/barriers -> existing scheduler submission

## What changes real execution

If a pending clear is fully overwritten before any read, the compiled program does not materialize
that clear. This removes the corresponding CLEAR render-pass begin/end and its attachment work.
The copy still executes exactly once with its original region and pre/post barriers. This is a
new dead-operation elimination, not just the existing repeated-clear coalescing. Other observations
flush the clear. CPU readback/submission/resource destruction keep Eden's existing flush boundaries.

## Requested architecture: implemented versus pending

| Item | Actual implementation / limitation |
|---|---|
| X7RenderIR | A bounded semantic block: Clear, CopyImage, Observe with read/write resources and dirty rectangles. Not a command log. Arbitrary Draw/Dispatch/Present graphs remain pending. |
| Resource graph | RAW/WAR/WAW edges for the live block; physical backing, guest/CPU ranges, format, aspect, mip/layer and producer dirty region. No global frame-wide lifetime graph yet. |
| VirtualFramebuffer | Pending logical clear CONTENT; its state and dirty region drive program execution and can prevent materialization. The VkImage backing is already allocated by Eden. No lazy VkImage allocation claim. |
| Lazy materialization | Clear commands remain deferred until an observer needs them; proven unobserved clears are discarded without recording their Vulkan render pass. |
| Dirty tracking | Exact rectangular clear region and copy coverage. Partial copies keep the clear. No dirty-tile bitset or automatic copy shrinking yet. |
| Aliasing | Same-backing/subresource identity plus conservative guest/CPU memory overlap checks. Unknown, sparse, remapped, converted and scaled cases retain the clear. No new aliased-memory allocation or copy-to-view substitution. |
| Graph fusion | Not general pass fusion. Phase4 still retains compatible CLEAR scopes; Phase6 removes an entirely dead clear scope. |
| MaliTileCompiler | No separate whole-graph tile compiler yet. The implemented lowering avoids one unnecessary attachment materialization/store sequence. |
| Barrier optimizer | No global barrier optimizer. Copy barriers remain intact. Native-fetch entry uses a resource/subresource-local image dependency, not a new global CPU wait. |
| MaliRenderProgram | Real executable two-operation plan; callbacks dispatch clear materialization/discard and the original Vulkan copy recording. |
| GPU JIT cache | Bounded 16-entry semantic plan/template cache, scheduler-owned. It caches emitted-plan decisions, not machine code or pipelines. |
| Cross-frame reuse | Equivalent structural/dependency signatures reuse the plan after rebinding/rechecking current resources. Addresses, VkImage handles, values, matrices and command buffers are not cached. |
| Game ID cache | Not needed for these game-independent two-operation plans; no persistent game-specific graph cache is implemented. |
| Pipeline prediction | Not implemented. Eden's existing pipeline cache/workers remain. |
| Descriptor virtualization | Eden's existing templates/cache/pools remain; Phase5 adds input attachment allocation to the retired descriptor pools. No new universal descriptor virtualizer. |
| Unified memory/transients | Uses existing Eden allocations/lifetimes. No new unified-memory manager or transient alias allocator is claimed. |
| Shader legalization | Phase3 IR legalizer remains in both shader paths; Phase5 native shader variant has exact coordinate/alias guards. Null-resource semantic limitations remain. |
| Command buffer reuse | No secondary-command-buffer replay or cross-frame command reuse. Programs record current handles through the existing scheduler. |
| NCE/HLE/timing/FG | No changes to NCE, HLE, kernel/services/loader/filesystem, input, audio, game speed or FG in this slice. |

## Eligibility and fallback

- AUTO: only selected POCO X7 Pro/Dimensity 8400/Mali-G720 backend; ON: development opt-in to the
  same guarded compiler; OFF: existing Eden path. Invalid values disable it. No extension forced.
- Pending color clear at slot 0, exactly one framebuffer image, one mip/layer, single sample,
  no depth/stencil or rescaling. Mixed clears remain on Eden.
- CopyImage with one region, same format, ordinary 2D single-sample color images, known valid guest
  and CPU memory ranges, no sparse/remapped/converted/rescaled/bad-overlap resources.
- Full dirty coverage, exact destination subresource, and no possible source read of the pending
  content are required to remove a clear. Other reads, partial copies and uncertain aliasing keep it.
- All unsupported operations terminate the represented block through existing observation/flush
  boundaries. No draws, resolves, stores required by a reader, or real copy barriers are removed.

## Validation

`tests/x7nx_semantic_gpu.cpp` runs the same compiler/cache used by the scheduler. It checks dependency
types, unknown/overlapping addresses, overflow, different formats/mips, observations, AUTO/ON/OFF,
virtual-content state and address-independent reuse. 10,000 generated clear/copy blocks compare
optimized results with an unoptimized pixel-array interpreter; copies must execute exactly once.
84 dead clears were eliminated in that deterministic synthetic suite. This is NOT a game statistic.
Python contracts check the live recording hooks and Android option resources. Existing Phase1–5
contracts and real IR tests are retained in CI. A Release Android build is still required; passing
host tests does not establish Vulkan-driver correctness, game performance or artifact identity.

## Files

Created: `src/video_core/renderer_vulkan/x7_render_ir.h`, `tests/x7nx_semantic_gpu.cpp`,
`tests/validate_x7nx_semantic_gpu.py`, this report.

Modified for Phase6: `vk_scheduler.h/.cpp`, `vk_texture_cache.cpp`, `src/video_core/CMakeLists.txt`,
`src/common/settings.h`, Android `IntSetting.kt`, `SettingsItem.kt`, `SettingsFragmentPresenter.kt`,
`values/arrays.xml`, `values/strings.xml`, `values-pt-rBR/strings.xml`, and `build-x7nx.yml`.
Phase5's separate commit documents its native-fetch changes in `X7NX_PHASE5_NATIVE_FETCH.md`.

## APK and next physical validation

Target delivery path (only valid after a successful build and verification):
`outputs/X7NX-Phase6-Semantic-Release.apk`. Do not substitute the Phase4 APK under that name.
Compare AUTO versus OFF in the same save, weather, settings and temperature. The log
`X7 semantic GPU compiler ACTIVE: dead clear materialization eliminated` confirms the optimization
was selected at least once, not a claimed FPS improvement. If there is no eligible block, the
compiler cannot improve that scene. Expansion needs real traces and a separate proof per operation.
