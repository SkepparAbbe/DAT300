#!/bin/bash
# End-to-end benchmark of the integrated A1: LSHDBSCAN_exec with (-s) and without sort bucketing.
# Per-phase times come from the app's own benchmark.txt; 1 warm-up + 5 measured runs per config.
# Labels of the first measured run are compared with rand_index.py (household also vs exact DBSCAN).
set -euo pipefail
source "$(dirname "$0")/configs.sh"
APP="$BUILD/LSHDBSCAN_exec"
PY="${PY:-$HOME/.venvs/a1plot/bin/python}"
RAND="$(cd "$(dirname "$0")" && pwd)/rand_index.py"
work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT; mkdir -p "$work/labels"   # temporary: labels, symlinks
csv="$OUT/app_runs.csv"; acc="$OUT/accuracy.csv"
[[ -s "$csv" ]] || echo "dataset,L,M,K,variant,rep,init_s,populate_s,core_s,merge_identify_s,merge_perform_s,relabel_s,peak_rss_kb,wall_s" > "$csv"
[[ -s "$acc" ]] || echo "dataset,L,M,K,rand_baseline_vs_a1,rand_baseline_vs_exact,rand_a1_vs_exact" > "$acc"
cd "$work"
# The app writes its labels next to the input file, so run on symlinks to keep data/ untouched.
ln -sf "$DATA/mnist/mnist_data.txt" mnist.txt
ln -sf "$DATA/household/clean_normalized_household.txt" household.txt
for which in "${@:-household mnist}"; do
  case $which in
    mnist)     read -r label args <<< "$MNIST";     Ls=$MNIST_L ;;
    household) read -r label args <<< "$HOUSEHOLD"; Ls=$HOUSEHOLD_L ;;
  esac
  args=$(sed -E "s|-f [^ ]+|-f $label.txt|" <<< "$args")
  M=$(grep -oE -- '-M [0-9]+' <<< "$args" | cut -d' ' -f2)
  for L in $Ls; do
    for K in $THREADS; do
      for variant in a1 baseline; do
        flag=""; [[ $variant == a1 ]] && flag="-s"
        for rep in 0 1 2 3 4 5; do
          rm -f benchmark.txt
          /usr/bin/time -v -o time.txt "$APP" $args $flag -L "$L" -t "$K" > app_stdout.txt
          phases=$(head -1 benchmark.txt | tr '\t' ',' | sed 's/,$//')
          rss=$(awk -F': ' '/Maximum resident/{print $2}' time.txt)
          wall=$(awk -F': ' '/Elapsed/{n=split($2,a,":"); s=0; for(i=1;i<=n;i++) s=s*60+a[i]; print s}' time.txt)
          echo "$label,$L,$M,$K,$variant,$rep,$phases,$rss,$wall" >> "$csv"
          labels="$label.txt_${L}_${M}_${K}.idx_concurrentlshdbscan"
          if [[ $rep == 1 ]]; then mv "$labels" "labels/$variant.idx"; else rm -f "$labels"; fi
        done
        echo "$label L=$L K=$K $variant: $(tail -1 "$csv")"
      done
      exact=""; [[ $label == household ]] && exact="$DATA/household/clean_normalized_household.idx_vanilla"
      echo "$label,$L,$M,$K,$("$PY" -I "$RAND" labels/baseline.idx labels/a1.idx $exact)" >> "$acc"
      rm -f labels/*.idx
    done
  done
done
