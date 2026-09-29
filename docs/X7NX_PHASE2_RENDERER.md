# X7NX phase 2: renderer validation work

Phase 2 starts by making two correctness-sensitive paths concrete rather than enabling an
unproven tile renderer.

1. Descriptor legalization now covers both image and buffer null descriptors. If a target driver
does not provide a safe native null descriptor, both paths bind the established dummy resources.
2. Persistent pipeline caches are namespaced by the active Vulkan API/driver revision on the
target Mali driver. Existing cache loading and prewarm continue on the worker queue, so the render
thread does not synchronously compile the disk cache.

The telemetry line now reports `prewarm=requests/pipelines` in addition to cache hit/miss and
compile stalls. A different driver revision intentionally starts with a cold X7NX cache; this is a
correctness and reproducibility boundary, not a performance regression claim.

Tile-local framebuffer feedback, attachment aliasing and reduced barriers remain gated on replay
captures with visual validation for Pokémon Sword and NieR:Automata. Until then, `copy-clone` is
the selected renderer path.
