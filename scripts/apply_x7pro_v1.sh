#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

eden_dir="${1:?Pass the path to an Eden checkout.}"
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
patch_file="${script_dir}/../patches/0001-x7pro-android-defaults.patch"

if [[ ! -d "$eden_dir/src/android" || ! -x "$eden_dir/.ci/android/build.sh" ]]; then
  echo "The target is not a complete Eden Android checkout: $eden_dir" >&2
  exit 1
fi

if [[ ! -f "$patch_file" ]]; then
  echo "The X7Pro V1 patch is missing: $patch_file" >&2
  exit 1
fi

# Keep the source adjustment explicit and fail rather than silently building a
# different upstream layout when the selected Eden revision changes.
git -C "$eden_dir" apply --check "$patch_file"
git -C "$eden_dir" apply "$patch_file"

printf '%s\n' \
  'Eden-X7Pro V1: applying the Dimensity 8400-Ultra / Mali-G720 baseline.' \
  'Defaults: 4 pipeline workers, asynchronous GPU emulation and asynchronous shaders.' \
  'All three values remain editable in Android settings.'
