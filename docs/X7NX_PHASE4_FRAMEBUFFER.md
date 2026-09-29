# Phase 4: narrow, integrated framebuffer fast paths

Baseline for comparison: commit 5c5ad28 (Phase 3, workflow run 13). No physical-device FPS result
is claimed. These changes do not implement arbitrary framebuffer fetch, programmable blending or
a complete render-target replacement. They remove two specific sources of unnecessary pass breaks.

## Clear-to-draw scope retention

Eden defers full clears into a CLEAR-load render-pass variant. The first draw opens this variant;
the following draw requests the base LOAD variant. Previously the different VkRenderPass handle
ended the scope, stored attachments, inserted barriers, and began another scope on the same
framebuffer. The X7NX scheduler now keeps the already-open clear scope for later draws.

MaliRenderTargetManager is a real scheduler-owned lifetime tracker, reset on EndRenderPass. It
requires the identical framebuffer object, native framebuffer handle and render area. Eligibility
is restricted to Mali-G720, single sample, no resolve attachments and no discard policies. Only
the load operation differs; the existing STORE behavior is preserved. No layout is changed to
UNDEFINED, and no required pixels are discarded. Transfers, submissions, feedback barriers and
framebuffer switches still terminate the scope. Existing partial/scissored clear behavior remains.

This relies on Vulkan render-pass compatibility allowing different load/store operations:
https://docs.vulkan.org/spec/latest/chapters/renderpass.html#renderpass-compatibility

## Disjoint depth-feedback subresources

The existing feedback check ended a pass when another view referenced the active depth image.
MaliFramebufferFeedbackManager now checks mip/layer interval overlap before that specific barrier.
A proven-disjoint range does not require the pass break. Overlapping or unknown ranges still do.

Proof is intentionally restricted to ordinary 2D/2D-array views of the same non-null physical
VkImage, with equal format and sample count, and no 3D slice flags. Invalid ranges conservatively
overlap; interval arithmetic uses 64 bits. Combined depth/stencil aspects are treated together.
Cubemaps, format reinterpretation and different physical image handles stay conservative.

The old boolean result cache only keyed render-target/descriptor-table serials, not the current
shader's selected view list. The precise path therefore checks the current draw instead of caching
a disjoint result under that incomplete key. Other GPUs and the OpenGL path retain the old policy.
This can add CPU checks; the net performance change must be measured on the target device.

## What this does not do

- It does not enable a framebuffer-fetch extension or rewrite shaders to input attachments.
- It does not remove existing color-feedback exclusions or prove arbitrary same-draw feedback safe.
- It does not replace a copy/clone with an input attachment. The inspected depth-feedback fallback
  is a render-pass boundary, not a full-image copy; no copy reduction is claimed for this patch.
- It does not alter NCE, game accuracy, thermal controls, global barriers or FG.
- Phase 3 null-storage write/query limitations remain.

## Validation and next step

`tests/x7nx_render_targets.cpp` exercises eligibility exclusions, scope start/end/framebuffer/area
changes, invalid ranges, overflow edges, and 100,000 range pairs against enumerated overlap. CI
runs it before the Android build. Python checks cover integration points; neither is a GPU replay.

On the POCO, compare the baseline and this APK at identical resolution/settings in the same scene.
Check clear correctness, depth effects, transitions and frametime. Native tile-local feedback is a
separate next step requiring shader coordinate/ordering analysis, driver capability validation,
and a correctness-preserving fallback; it must not be enabled merely from the Mali name.
