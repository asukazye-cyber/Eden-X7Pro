#!/bin/sh
# Host test only. Requires GCC supporting C++23, Boost headers and fmt 12.1.0 headers.
# Optional arguments: fmt include directory, Boost include directory, output directory.
set -eu
test_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
x7nx_src="$test_root/../../src"
test_output=${3:-/tmp/x7nx-ir-test}
mkdir -p "$test_output"
g++ -std=c++23 -O1 -DFMT_HEADER_ONLY -ffunction-sections -fdata-sections \
  -I "$test_root/stubs" -I "$x7nx_src" -I "${1:-/usr/local/include}" -I "${2:-/usr/include}" \
  "$test_root/legalizer_test.cpp" "$x7nx_src/video_core/renderer_vulkan/x7nx_shader_legalizer.cpp" \
  "$x7nx_src/shader_recompiler/frontend/ir/basic_block.cpp" \
  "$x7nx_src/shader_recompiler/frontend/ir/ir_emitter.cpp" \
  "$x7nx_src/shader_recompiler/frontend/ir/microinstruction.cpp" \
  "$x7nx_src/shader_recompiler/frontend/ir/opcodes.cpp" \
  "$x7nx_src/shader_recompiler/frontend/ir/value.cpp" \
  "$x7nx_src/shader_recompiler/frontend/ir/type.cpp" \
  "$x7nx_src/shader_recompiler/ir_opt/identity_removal_pass.cpp" \
  "$x7nx_src/shader_recompiler/ir_opt/dead_code_elimination_pass.cpp" \
  -Wl,--gc-sections -o "$test_output/legalizer_test"
"$test_output/legalizer_test"

g++ -std=c++23 -O1 -DFMT_HEADER_ONLY -ffunction-sections -fdata-sections \
  -I "$x7nx_src" -I "${1:-/usr/local/include}" -I "${2:-/usr/include}" \
  "$test_root/framebuffer_fetch_test.cpp" \
  "$x7nx_src/shader_recompiler/frontend/ir/basic_block.cpp" \
  "$x7nx_src/shader_recompiler/frontend/ir/ir_emitter.cpp" \
  "$x7nx_src/shader_recompiler/frontend/ir/microinstruction.cpp" \
  "$x7nx_src/shader_recompiler/frontend/ir/opcodes.cpp" \
  "$x7nx_src/shader_recompiler/frontend/ir/value.cpp" \
  "$x7nx_src/shader_recompiler/frontend/ir/type.cpp" \
  -Wl,--gc-sections -o "$test_output/framebuffer_fetch_test"
"$test_output/framebuffer_fetch_test"
