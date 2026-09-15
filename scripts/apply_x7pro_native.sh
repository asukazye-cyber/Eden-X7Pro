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

# Inject Mali native frame generation UI strings and arrays BEFORE applying patches
# This ensures the resources exist when patch 0005 tries to reference them
strings_xml="${eden_dir}/src/android/app/src/main/res/values/strings.xml"
arrays_xml="${eden_dir}/src/android/app/src/main/res/values/arrays.xml"

echo "[Mali-FG] Pre-injecting UI resources before patch application..."

# Inject Mali Frame Gen strings into strings.xml before </resources>
if ! grep -q "mali_native_frame_gen" "$strings_xml"; then
  sed -i '/<\/resources>/i\
    <string name="mali_native_frame_gen">Mali Native Frame Generation (Experimental)</string>\
    <string name="mali_native_frame_gen_description">Uses the POCO X7 Pro Vulkan compute path to estimate motion between two GPU-resident frames and generate one midpoint frame. It never requires Lossless.dll, Vulkan memory model, or VK_EXT_robustness2 support.</string>\
    <string name="mali_native_frame_gen_mode">Mode</string>\
    <string name="mali_native_frame_gen_mode_description">Performance uses a smaller motion search; Balanced is the POCO X7 Pro default; Quality spends more GPU time on motion estimation; Adaptive adjusts based on scene complexity.</string>\
    <string name="mali_native_frame_gen_mode_performance">Performance</string>\
    <string name="mali_native_frame_gen_mode_balanced">Balanced</string>\
    <string name="mali_native_frame_gen_mode_quality">Quality</string>\
    <string name="mali_native_frame_gen_mode_adaptive">Adaptive</string>\
    <string name="mali_native_frame_gen_target">Target output rate</string>\
    <string name="mali_native_frame_gen_target_description">The visual target for the experimental pacing policy. The emulator never changes the game'"'"'s actual emulation speed.</string>\
    <string name="mali_native_frame_gen_target_auto">Auto</string>\
    <string name="mali_native_frame_gen_target_40">40 FPS</string>\
    <string name="mali_native_frame_gen_target_60">60 FPS</string>\
    <string name="mali_native_frame_gen_debug_overlay">Mali FG debug overlay</string>\
    <string name="mali_native_frame_gen_debug_overlay_description">Shows profile state plus real/generated frame counters in the performance overlay and emits periodic [MALI-FG] log entries.</string>' "$strings_xml"
  echo "[Mali-FG] Injected strings into strings.xml"
fi

# Inject Mali Frame Gen arrays into arrays.xml before </resources>
if ! grep -q "maliNativeFrameGenModeNames" "$arrays_xml"; then
  sed -i '/<\/resources>/i\
\
    <string-array name="maliNativeFrameGenModeNames">\
        <item>@string/mali_native_frame_gen_mode_performance</item>\
        <item>@string/mali_native_frame_gen_mode_balanced</item>\
        <item>@string/mali_native_frame_gen_mode_quality</item>\
        <item>@string/mali_native_frame_gen_mode_adaptive</item>\
    </string-array>\
\
    <integer-array name="maliNativeFrameGenModeValues">\
        <item>0</item>\
        <item>1</item>\
        <item>2</item>\
        <item>3</item>\
    </integer-array>\
\
    <string-array name="maliNativeFrameGenTargetNames">\
        <item>@string/mali_native_frame_gen_target_auto</item>\
        <item>@string/mali_native_frame_gen_target_40</item>\
        <item>@string/mali_native_frame_gen_target_60</item>\
    </string-array>\
\
    <integer-array name="maliNativeFrameGenTargetValues">\
        <item>0</item>\
        <item>40</item>\
        <item>60</item>\
    </integer-array>' "$arrays_xml"
  echo "[Mali-FG] Injected arrays into arrays.xml"
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
# Eden's DivCeil concept requires an unsigned divisor; the source uses uint32_t extents.
sed -i 's/, 8)/, 8u)/g' "$target_source"
if ! grep -Fq '#include "video_core/renderer_vulkan/present/mali_native_frame_gen.h"' "$target_source"; then
  sed -i '/^#include <vector>$/a#include "video_core/renderer_vulkan/present/mali_native_frame_gen.h"' "$target_source"
fi
test -s "$target_source"
grep -Fq 'MaliNativeFrameGen::GenerateInto' "$target_source"

printf '%s\n' \
  'Eden Mali X7 Pro: native Vulkan compute frame generation enabled.' \
  'No Lossless.dll, Vulkan memory-model, or VK_EXT_robustness2 requirement is added.' \
  'The runtime profile remains gated by POCO X7 Pro / MT6899 / Mali-G720 detection.'
