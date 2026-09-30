from pathlib import Path
from xml.etree import ElementTree
root = Path(__file__).resolve().parents[1]
def read(path):
    return (root / path).read_text(encoding="utf-8")
scheduler = read("src/video_core/renderer_vulkan/vk_scheduler.h")
for item in ("RecordSemanticImageCopy", "program.Execute", "FlushDeferredClear", "DiscardSemanticClear",
             "Record(std::forward<T>(command))", "Materialization::PendingClear"):
    assert item in scheduler
impl = read("src/video_core/renderer_vulkan/vk_scheduler.cpp")
for item in ("RegisterSemanticClear(framebuffer, rt_slot)", "semantic_cache.Get(semantic_ir)",
             "semantic_framebuffer.dirty", "framebuffer->NumImages() != 1", "deferred_clear = {};"):
    assert item in impl
texture = read("src/video_core/renderer_vulkan/vk_texture_cache.cpp")
copy = texture.split("void TextureCacheRuntime::CopyImage(", 1)[1].split("void TextureCacheRuntime::CopyImageMSAA", 1)[0]
assert "scheduler.RecordSemanticImageCopy(source, destination, region, std::move(copy_command))" in copy
assert "cmdbuf.CopyImage(" in copy and "pre_barriers" in copy and "post_barriers" in copy
assert "vk_copies.size() == 1" in copy and "src.info.format == dst.info.format" in copy
for locale in ("values", "values-pt-rBR"):
    xml = ElementTree.parse(root / f"src/android/app/src/main/res/{locale}/strings.xml")
    assert xml.find("string[@name='x7nx_semantic_gpu_recompiler']") is not None
print("Semantic GPU integration and Android feature flag contracts: OK")
