#!/bin/bash
# Integrated A1 (ConcurrentLSHDBSCAN -s) vs hash maps on the real datasets: identical bucket
# partitions and candidate sets for every table, plus Rand index between the final labels.
set -euo pipefail
source "$(dirname "$0")/configs.sh"
TEST="$BUILD/sort_bucketing_test"
log="$OUT/correctness.txt"; : > "$log"
status=0
for spec in "MNIST:$MNIST_L" "HOUSEHOLD:$HOUSEHOLD_L"; do
  name=${spec%%:*}; read -r label args <<< "${!name}"
  for L in ${spec#*:}; do
    for K in 1 36 72; do
      out=$("$TEST" $args -L "$L" -t "$K" 2>&1) || status=1
      echo "$label: $(grep -E 'tables identical|FAILED' <<< "$out" | sed 's|^.*/||') $(tail -1 <<< "$out")" | tee -a "$log"
    done
  done
done
exit $status
