#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

eden_dir="${1:?Pass the path to an Eden checkout.}"
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd -- "${script_dir}/.." && pwd)"

if [[ ! -d "$eden_dir/src/android" || ! -f "$eden_dir/CMakeLists.txt" ]]; then
  echo "The target is not a complete Eden Android checkout: $eden_dir" >&2
  exit 1
fi

# Apply the native patches to the pinned upstream revision first.  V1 changes the Android
# asynchronous-GPU default next to the native frame-generation settings, so applying it first
# would make git correctly reject the native hunk as ambiguous.
for patch_file in \
  "${repo_dir}/patches/0003-x7pro-native-profile-and-shaders.patch" \
  "${repo_dir}/patches/0004-x7pro-native-vulkan-integration.patch" \
  "${repo_dir}/patches/0005-x7pro-android-package-and-ui.patch"; do
  [[ -f "$patch_file" ]] || { echo "Missing patch: $patch_file" >&2; exit 1; }
  git -C "$eden_dir" apply --check "$patch_file"
  git -C "$eden_dir" apply "$patch_file"
done

# The reviewed V1/V2 defaults are independent once the native changes are in place.
bash "${script_dir}/apply_x7pro_v2.sh" "$eden_dir"

# Build #16 is based on the fully composed V1/V2 tree because V1 changes the neighboring Android
# renderer defaults. The relaxed whitespace mode only normalizes the legacy CRLF patch assets.
build16_patch="${repo_dir}/patches/0007-build16-profile-governor-and-ui.patch"
[[ -f "$build16_patch" ]] || { echo "Missing patch: $build16_patch" >&2; exit 1; }
git -C "$eden_dir" apply --check --ignore-space-change "$build16_patch"
git -C "$eden_dir" apply --ignore-space-change "$build16_patch"

profile_gate_patch="${repo_dir}/patches/0008-build16-gate-native-fg-by-profile.patch"
[[ -f "$profile_gate_patch" ]] || { echo "Missing patch: $profile_gate_patch" >&2; exit 1; }
git -C "$eden_dir" apply --check --ignore-space-change "$profile_gate_patch"
git -C "$eden_dir" apply --ignore-space-change "$profile_gate_patch"

# The compute implementation is kept as a compressed, source-only asset to avoid carrying an
# additional copy of the whole upstream tree. It is decompressed deterministically into the file
# referenced by 0003 before CMake configures the Android build.
source_asset="${repo_dir}/overrides/mali_native_frame_gen.cpp.gz.b64"
target_source="${eden_dir}/src/video_core/renderer_vulkan/present/mali_native_frame_gen.cpp"
[[ -f "$source_asset" ]] || { echo "Missing native frame-generation source asset." >&2; exit 1; }
base64 --decode "$source_asset" | gzip --decompress > "$target_source"
if ! grep -Fq '#include "video_core/renderer_vulkan/present/mali_native_frame_gen.h"' "$target_source"; then
  sed -i '/^#include <vector>$/a#include "video_core/renderer_vulkan/present/mali_native_frame_gen.h"' "$target_source"
fi
# Eden's DivCeil concept requires an unsigned divisor; the source uses uint32_t extents.
sed -i 's/, 8)/, 8u)/g' "$target_source"
test -s "$target_source"
grep -Fq 'MaliNativeFrameGen::GenerateInto' "$target_source"

# Build19 is deliberately applied AFTER the decoded Build18 source asset.
build19_patch="${repo_dir}/patches/0009-build19-fg-scheduling-telemetry.patch"
git -C "$eden_dir" apply --check "$build19_patch"
git -C "$eden_dir" apply "$build19_patch"

# Build21 is based on the complete Build20 composition above. It centralizes architecture policy
# and adds only capability-gated, Vulkan-valid renderer fast paths with generic fallbacks.
build21_patch="${repo_dir}/patches/0010-build21-mali-g720-renderer-budget.patch"
[[ -f "$build21_patch" ]] || { echo "Missing patch: $build21_patch" >&2; exit 1; }
git -C "$eden_dir" apply --check "$build21_patch"
git -C "$eden_dir" apply "$build21_patch"

printf '%s\n' \
  'Eden Mali X7 Pro Build21: capability-gated Mali-G720 renderer and real-frame budget enabled.' \
  'No Lossless.dll, Vulkan memory-model, or VK_EXT_robustness2 requirement is added.' \
  'Renderer fast paths use Vulkan identity/capabilities and retain independently gated fallbacks.'
