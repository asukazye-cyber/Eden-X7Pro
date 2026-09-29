# X7NX device-owned backend: implemented subset

This is an experimental Android ARM64 Release increment, not completion of the entire renderer
replacement roadmap. No phone performance or game-compatibility result is implied by compilation.

## Integrated changes

- Device-owned X7GpuBackend -> MaliGpuBackend -> MaliG720Backend, selected before resource caches.
  Android product/SoC detection, Vulkan GPU identity and proprietary ARM driver must agree.
  Other devices keep the existing renderer policies. Enabled synchronization2 is queried; Vulkan
  1.3 alone is no longer treated as proof that dynamic rendering was enabled.
- Persistent typed dummy images and samplers, separate sampled/writable backing, matching view
  dimensions, integer/depth formats, zero clear and resource-local transfer-to-shader barriers.
  Format support is checked. Unsupported combinations fail explicitly rather than binding an
  invalid view. No dependency on nullDescriptor for the selected G720 backend.
- Null buffers have shader/texel usage, nonzero 16 KiB ranges, initialization visibility, and a
  separate SSBO sink. Allocation is on first use, not per frame. Existing VMA owns allocations.
- Actual IR legalization runs before graphics and compute SPIR-V emission. Constant signed and
  unsigned bitfield extracts lower to equivalent shifts/masks; zero/full width are explicit and
  invalid/dynamic operands retain Eden translation. This is not a measured performance win or a
  claim that every Mali shader incompatibility is resolved. Cache ABI advances to avoid old blobs.
- Descriptor arena sizing is dispatched through the backend; existing frame-safe ring remains.
- Release X7NX heavy telemetry calls compile out. Android keeps thread affinity/cpuset control;
  role priorities remain, topology now also records available current frequency.

## Important incomplete semantics

Valid backing is NOT full Vulkan nullDescriptor emulation. Missing storage writes/atomics are not
yet discarded through shader guards, and subsequent reads of the same sink can observe writes.
Null image queries report dummy dimensions, not zero. Typed texel-buffer read/write separation and
all unsupported depth/multisample combinations need more work. Robust out-of-range behavior still
depends on the existing shader/buffer robustness path. These limitations preclude a blanket claim
that robustness2 has been replaced equivalently or that NieR device-lost is fixed.

## Preserved fallbacks and next work

Eden still owns render targets, alias tracking, framebuffer feedback/clone, render-pass scopes,
general resource transitions, allocation/suballocation, staging/readback, texture conversion/cache,
ASTC decoding, command pools/submission, and disk pipeline prewarming. Existing load/store and
sync2 conversion are reused; no unproved discard or global barrier removal was added. The previous
unused UMA policy scaffold and inert transient-attachment flags were removed; game profile metadata
does not constitute a learned prewarm database. Removed scaffolding is recoverable in Git history.

Tile-local fetch/input attachments cannot safely replace arbitrary guest texture feedback solely
because a Vulkan extension exists: coordinates, sample/format semantics, aliasing, ordering and
shader declarations must agree. No new programmable blending or driver-version quirk is claimed.
Unknown driver bugs are not assigned invented version ranges. NCE/HLE/audio/input are untouched;
FG remains off. Next: runtime null-access guards, then a subresource-aware render-target/feedback
manager with explicit proof of same-pixel access before choosing a tile-local path.

Validation includes source contracts, deterministic bitfield reference properties and Android
Release compilation. It does not replace Vulkan validation or physical POCO game testing.

`tests/x7nx_host/run.sh` compiles the actual IR pass/emitter/cleanup and evaluates 35,904 lowered
expressions, plus non-target, dynamic and invalid-range preservation cases. Only the Vulkan device
policy is replaced by a host-test seam; this is not a driver or SPIR-V execution test.
