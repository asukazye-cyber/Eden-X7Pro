#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

eden_dir="${1:?Pass the path to an Eden checkout.}"

if [[ ! -d "$eden_dir/src/android" || ! -x "$eden_dir/.ci/android/build.sh" ]]; then
  echo "The target is not a complete Eden Android checkout: $eden_dir" >&2
  exit 1
fi

# V0 intentionally carries no renderer or driver workarounds.  It establishes a
# reproducible, optimized Android baseline before X7 Pro-specific code changes
# are measured against Pokémon Sword.  Keep this hook so later versions can
# apply reviewed patches in one explicit place.
printf '%s\n' \
  'Eden-X7Pro V0: building upstream optimized Android baseline.' \
  'Target profile: POCO X7 Pro / Dimensity 8400-Ultra / Mali-G720 MC7.'
