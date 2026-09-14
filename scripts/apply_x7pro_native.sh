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

# Apply the native patches to the pinned upstream revision first. V1 changes the Android
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

# The compute implementation is kept as a compressed, source-only asset to avoid carrying an
# additional copy of the whole upstream tree. It is decompressed deterministically into the file
# referenced by 0003 before CMake configures the Android build.
source_asset="${repo_dir}/overrides/mali_native_frame_gen.cpp.gz.b64"
target_source="${eden_dir}/src/video_core/renderer_vulkan/present/mali_native_frame_gen.cpp"
[[ -f "$source_asset" ]] || { echo "Missing native frame-generation source asset." >&2; exit 1; }
base64 --decode "$source_asset" | gzip --decompress > "$target_source"
test -s "$target_source"
grep -Fq 'MaliNativeFrameGen::GenerateInto' "$target_source"

printf '%s\n' \
  'Eden Mali X7 Pro: native Vulkan compute frame generation enabled.' \
  'No Lossless.dll, Vulkan memory-model, or VK_EXT_robustness2 requirement is added.' \
  'The runtime profile remains gated by POCO X7 Pro / MT6899 / Mali-G720 detection.'
