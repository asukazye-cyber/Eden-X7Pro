#pragma once
// Host unit-test seam only: the actual IR emitter, pass and cleanup are compiled unchanged.
namespace Vulkan {
struct TestBackend {
    bool enabled = true;
    bool LegalizeBitfields() const { return enabled; }
    bool UseNativeNullDescriptors(bool value) const { return !enabled && value; }
};
struct Device {
    TestBackend backend;
    const TestBackend& X7Backend() const { return backend; }
    bool HasNullDescriptor() const { return true; }
};
}
