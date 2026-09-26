#!/bin/bash

set -euo pipefail

#var definition
script_dir="$(cd "$(dirname "$0")" && pwd)"
binary="$script_dir/../build/LSHDBSCAN_exec"
flamegraph_dir="$script_dir/FlameGraph"
perf_data="${1:-lshdbscan.perf.data}"
svg_output="${2:-lshdbscan-flamegraph.svg}"

shift $(( $# > 0 ? 1 : 0 )) || true
shift $(( $# > 0 ? 1 : 0 )) || true

#arguments
default_args=(
  -f ../../../data/household/clean_normalized_household.txt
  -m 100
  -e 2000
  -M 5
  -L 20
)

if [[ ! -x "$binary" ]]; then
  echo "Missing executable: $binary" >&2
  echo "Build it with: cd $script_dir/../build && make LSHDBSCAN_exec" >&2
  exit 1
fi

if [[ ! -d "$flamegraph_dir" ]]; then
  echo "Missing FlameGraph directory: $flamegraph_dir" >&2
  echo "Clone it with: git clone --depth 1 https://github.com/brendangregg/FlameGraph.git $flamegraph_dir" >&2
  exit 1
fi

if [[ $# -gt 0 ]]; then
  run_args=("$@")
else
  run_args=("${default_args[@]}")
fi

cd "$script_dir"

perf record -F 99 --call-graph dwarf,16384 -o "$perf_data" -- "$binary" "${run_args[@]}"
perf script -i "$perf_data" | "$flamegraph_dir/stackcollapse-perf.pl" | "$flamegraph_dir/flamegraph.pl" --title "LSHDBSCAN_exec Flame Graph" > "$svg_output"

echo "perf data: $script_dir/$perf_data"
echo "flame graph: $script_dir/$svg_output"