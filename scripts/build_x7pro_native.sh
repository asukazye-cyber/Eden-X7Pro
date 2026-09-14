#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

eden_dir="${1:?Pass the path to an Eden checkout.}"
jobs="$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"

cd "${eden_dir}/src/android"
chmod +x ./gradlew

# This calls the product flavor added by the native patch, rather than Eden's normal optimized
# flavor. The resulting package id is verified separately by the workflow.
./gradlew copyMaliX7ProReleaseOutputs \
  -Dorg.gradle.caching=true \
  -Dorg.gradle.parallel=true \
  -Dorg.gradle.workers.max="${jobs}" \
  -PYUZU_ANDROID_ARGS="-DUSE_CCACHE=true" \
  --info
