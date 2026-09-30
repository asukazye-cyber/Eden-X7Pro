from pathlib import Path
from xml.etree import ElementTree

root = Path(__file__).resolve().parents[1]
def source(name):
    return (root / name).read_text(encoding="utf-8")

device = source("src/video_core/vulkan_common/vulkan_device.h")
assert "RasterizationOrderAttachmentAccess" in device
assert "features.rasterization_order_attachment_access.rasterizationOrderColorAttachmentAccess" in device
assert "Settings::values.x7nx_native_framebuffer_fetch.GetValue()" in device
emitter = source("src/shader_recompiler/backend/spirv/spirv_emit_context.cpp")
assert "spv::Dim::SubpassData" in emitter
assert "spv::Decoration::InputAttachmentIndex" in emitter
pipeline = source("src/video_core/renderer_vulkan/vk_graphics_pipeline.cpp")
for guard in ("fetch_single_triangle", "!texture_cache.IsRescaling()", "view.HasIdentitySwizzle()",
              "view.RenderTarget() == framebuffer->FetchColorView()", "views.size() == 1",
              "!spv_modules[1] && !spv_modules[2] && !spv_modules[3]",
              "native_fetch ? *fetch_pipeline : *pipeline", "VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT",
              "VK_PIPELINE_COLOR_BLEND_STATE_CREATE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_BIT_EXT"):
    assert guard in pipeline, guard
rasterizer = source("src/video_core/renderer_vulkan/vk_rasterizer.cpp")
assert "!is_indexed && instance_count == 1" in rasterizer
assert "state.vertex_buffer.count == 3" in rasterizer
assert "}, single_triangle);" in rasterizer
scheduler = source("src/video_core/renderer_vulkan/vk_scheduler.cpp")
assert "state.native_fetch == native_fetch" in scheduler
assert "VK_ACCESS_INPUT_ATTACHMENT_READ_BIT" in scheduler
assert "&& !override_handle" in scheduler  # no Phase4 CLEAR reuse for native input passes
passes = source("src/video_core/renderer_vulkan/vk_render_pass_cache.cpp")
assert "VK_SUBPASS_DESCRIPTION_RASTERIZATION_ORDER_ATTACHMENT_COLOR_ACCESS_BIT_EXT" in passes
assert "key.native_color_fetch ? references2.data() : nullptr" in passes
assert "VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT" in source("src/video_core/renderer_vulkan/vk_texture_cache.cpp")
settings = source("src/common/settings.h")
assert 'x7nx_native_framebuffer_fetch{linkage, false' in settings
for locale in ("values", "values-pt-rBR"):
    xml = ElementTree.parse(root / f"src/android/app/src/main/res/{locale}/strings.xml")
    assert xml.find("string[@name='x7nx_native_framebuffer_fetch']") is not None
print("Native framebuffer fetch integration and Android option contracts: OK")
