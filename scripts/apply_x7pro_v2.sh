#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

eden_dir="${1:?Pass the path to an Eden checkout.}"
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
v1_script="${script_dir}/apply_x7pro_v1.sh"
patch_file="${script_dir}/../patches/0002-x7pro-mali-g720-descriptor-budget.patch"

if [[ ! -d "$eden_dir/src/android" || ! -x "$eden_dir/.ci/android/build.sh" ]]; then
  echo "The target is not a complete Eden Android checkout: $eden_dir" >&2
  exit 1
fi

if [[ ! -x "$v1_script" || ! -f "$patch_file" ]]; then
  echo "The X7Pro V2 build files are incomplete." >&2
  exit 1
fi

bash "$v1_script" "$eden_dir"
git -C "$eden_dir" apply --check "$patch_file"
git -C "$eden_dir" apply "$patch_file"

printf '%s\n' \
  'Eden-X7Pro V2: enabling the presentation preset and the Mali-G720 descriptor-ring adjustment.' \
  'The renderer adjustment activates only on a proprietary ARM Mali-G720 driver with descriptor-buffer support.' \
  'It adds 8 MiB of bounded host-visible ring capacity; it does not bypass Eden ARM synchronization safeguards.'
