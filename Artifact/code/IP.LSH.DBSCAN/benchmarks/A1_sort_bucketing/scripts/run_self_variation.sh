#!/bin/bash
# Reference for the label Rand index: run each variant twice with identical inputs and compare
# the two runs (the multithreaded union-find makes the original non-deterministic).
set -euo pipefail
source "$(dirname "$0")/configs.sh"
APP="$BUILD/LSHDBSCAN_exec"
PY="${PY:-$HOME/.venvs/a1plot/bin/python}"
RAND="$(cd "$(dirname "$0")" && pwd)/rand_index.py"
K="${K:-36}"
work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT   # temporary: labels, symlinks
csv="$OUT/self_variation.csv"
echo "dataset,L,K,rand_baseline_run1_vs_run2,rand_a1_run1_vs_run2" > "$csv"
cd "$work"
ln -sf "$DATA/mnist/mnist_data.txt" mnist.txt
ln -sf "$DATA/household/clean_normalized_household.txt" household.txt
for which in household mnist; do
  case $which in
    mnist)     read -r label args <<< "$MNIST";     Ls=$MNIST_L ;;
    household) read -r label args <<< "$HOUSEHOLD"; Ls=$HOUSEHOLD_L ;;
  esac
  args=$(sed -E "s|-f [^ ]+|-f $label.txt|" <<< "$args")
  M=$(grep -oE -- '-M [0-9]+' <<< "$args" | cut -d' ' -f2)
  for L in $Ls; do
    for variant in baseline a1; do
      flag=""; [[ $variant == a1 ]] && flag="-s"
      for run in 1 2; do
        "$APP" $args $flag -L "$L" -t "$K" > /dev/null
        mv "$label.txt_${L}_${M}_${K}.idx_concurrentlshdbscan" "$variant.$run.idx"
      done
    done
    b=$("$PY" -I "$RAND" baseline.1.idx baseline.2.idx | cut -d, -f1)
    a=$("$PY" -I "$RAND" a1.1.idx a1.2.idx | cut -d, -f1)
    echo "$label,$L,$K,$b,$a" | tee -a "$csv"
    rm -f ./*.idx benchmark.txt
  done
done
