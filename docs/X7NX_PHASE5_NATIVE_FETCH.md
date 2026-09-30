# Phase 5: experimental native framebuffer fetch

This is an integrated, deliberately narrow input-attachment implementation, not a claim that
all guest framebuffer feedback has been converted. Baseline: Phase4 commit 4cab557.
No game FPS, Mali-driver correctness, activation frequency or NieR improvement is established.

## Implemented path

1. Query and enable the EXT rasterization-order attachment-access feature through the existing
   Vulkan feature chain. Require the selected X7NX Mali-G720 backend and actual color-read feature.
2. Analyze the real fragment IR. Accept only one non-array, non-integer, non-depth 2D texture,
   with fetch coordinates that are conversions of FragCoord.xy, LOD zero, no offset/MSAA/sparse
   result. Recognize a bounded unscaled form of the existing rescaling pass. Reject other reads.
3. Retain the original SPIR-V and compile a second fragment module. The variant declares a
   SubpassData image at descriptor set 1/binding 0, input attachment index 0, and emits OpImageRead.
4. Allocate input-attachment descriptors with the existing tick-retired descriptor pool. Candidate
   pipelines use descriptor sets rather than mixing descriptor buffers with a conventional set.
5. Create a separate compatible native framebuffer/render pass with color attachment 0 as input,
   GENERAL layout, LOAD/STORE, and rasterization-order subpass/pipeline flags. Regular passes are
   unchanged; Phase4 CLEAR-variant reuse never applies to this distinct input pass.
6. Select the variant per draw only after proving the sampled native view is the actual attachment
   view with identity swizzle. Invalidate pipeline binding on normal/native transitions. Flush
   pending clears and make earlier writes visible to input reads at entry. Consecutive qualifying
   native draws can remain in the ordered input pass.

## Runtime restrictions (all required)

- Option enabled, matching device/backend, extension and enabled color-access feature.
- RGBA8 UNORM attachment at slot 0 only, no depth/stencil attachment, one layer/mip, no MSAA/resolve.
- Exactly the same native view as the sampled image, identity swizzle, ordinary 2D view.
- 1x resolution and no active rescaling; the 1.25x setting does NOT use this first implementation.
- Direct non-indexed draw of one triangle and one instance, no geometry/tessellation shaders or
  transform feedback. This excludes within-draw inter-primitive overlap: ordinary sampled-image
  feedback must not silently acquire programmable-blend semantics.
- No other texture/image binding that might alias the render target.

Other cases retain Eden's existing path. Compilation/allocation failure of the optional native
pipeline keeps the original pipeline. The native pass may add a pass boundary, a descriptor
allocation and pipeline work: it can be slower. It does not remove an existing full-image copy
in this branch and should not be described as a universal bandwidth/FPS improvement.

## User control and evidence

Graphics > Advanced contains **Mali native framebuffer fetch (experimental)**, default OFF.
Restart the game after changing it. The Portuguese (Brazil) label is provided too.
Startup log distinguishes `supported` and `enabled`; an `X7NX native framebuffer fetch ACTIVE`
line is emitted once per pipeline only after the draw guards select the real native path.
Absence of ACTIVE is not evidence of a broken toggle; the game may not issue eligible draws.
Shader cache namespace is x7nx-abi5. Older APKs are not overwritten.

Host tests exercise the real IR proof (three accepted forms and rejected offsets/LOD/types/stages/
extra reads), plus the existing 35,904 bitfield cases. Python checks verify integration/UI guards.
These are not GPU validation, GPU replay, game testing or a benchmark.

Before relaxing restrictions: capture eligible draws on the actual phone; compare rendered images
and frame times with the option off/on, check Vulkan validation when available, and retain a
driver/version-specific rollback. Scaling, depth, arrays, MSAA, filtered/normalized coordinates,
multi-primitive feedback and broader shader matching each need their own correctness proof.

Reference: Khronos VK_EXT_rasterization_order_attachment_access specification and sample:
https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_rasterization_order_attachment_access.html
https://docs.vulkan.org/samples/latest/samples/extensions/rasterization_order_attachment_access/README.html
