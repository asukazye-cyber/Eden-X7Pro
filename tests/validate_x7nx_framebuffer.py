from pathlib import Path

root = Path(__file__).resolve().parents[1]
def read(path):
    return (root / path).read_text(encoding="utf-8")

scheduler = read("src/video_core/renderer_vulkan/vk_scheduler.cpp")
assert "MaliRenderTargetManager::Eligible" in scheduler
assert "x7_render_targets.Begin(" in scheduler
assert "x7_render_targets.CanContinue(" in scheduler
assert "x7_render_targets.End();" in scheduler
assert "state.framebuffer == framebuffer_handle" in scheduler
assert "base.resolve_color || base.resolve_depth_stencil" in scheduler
assert "framebuffer->DiscardsMsaaDepthStencil()" in scheduler
texture = read("src/video_core/renderer_vulkan/vk_texture_cache.cpp")
assert "sampled.ImageHandle() == attachment.ImageHandle()" in texture
assert "sampled.format == attachment.format" in texture
assert "sampled.Samples() == attachment.Samples()" in texture
assert "ImageViewFlagBits::Slice" in texture
assert "Manager::MayOverlap" in texture
cache = read("src/video_core/texture_cache/texture_cache.h")
assert "if (!precise_feedback && render_targets_serial" in cache
assert "runtime.FeedbackLoopMayOverlap(" in cache
print("X7NX framebuffer integration contracts: OK")
