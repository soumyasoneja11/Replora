#!/usr/bin/env bash
# Regenerate results/benchmark_results.csv from replora_cli.
# Hits, misses, evictions, and rates are stable for a given engine.
# executionTimeMicros changes between runs. estimatedMemoryBytes follows sizeof
# and pointer width, so a 32-bit or wasm build will not match a 64-bit CSV.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${REPLORA_BUILD_DIR:-$root/build/release}"
cmake_args=(-S "$root/cpp" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release)
# Windows CMake otherwise picks NMake. This repo's native compiler is g++.
if [[ "${OS:-}" == "Windows_NT" ]] && command -v g++ >/dev/null 2>&1; then
  cmake_args+=(-G "MinGW Makefiles")
fi
cmake "${cmake_args[@]}"
cmake --build "$build_dir" --target replora_cli
cli="$build_dir/replora_cli"
if [[ -f "${cli}.exe" ]]; then
  cli="${cli}.exe"
fi

out_dir="$root/results"
mkdir -p "$out_dir"
tmp="$(mktemp)"
trap 'rm -f "$tmp"' EXIT

{
  echo "experimentId,workload,capacity,requests,seed,policy,hits,misses,evictions,hitRate,missRate,executionTimeMicros,estimatedMemoryBytes"
  while read -r experiment_id workload capacity; do
    [[ -z "$experiment_id" ]] && continue
    csv="$("$cli" \
      --capacity "$capacity" \
      --workload "$workload" \
      --requests 10000 \
      --seed 42 \
      --policy all \
      --format csv)"
    printf '%s\n' "$csv" | awk -F, -v id="$experiment_id" '
      NR > 1 && NF > 1 {
        print id "," $2 "," $3 "," $4 "," $5 "," $1 "," $6 "," $7 "," $8 "," $9 "," $10 "," $11 "," $12
      }
    '
  done <<'EOF'
looping-c4 looping 4
looping-c16 looping 16
looping-c64 looping 64
frequency-skewed-c4 frequency-skewed 4
frequency-skewed-c16 frequency-skewed 16
frequency-skewed-c64 frequency-skewed 64
EOF
} >"$tmp"

mv "$tmp" "$out_dir/benchmark_results.csv"
trap - EXIT
echo "wrote $out_dir/benchmark_results.csv"
