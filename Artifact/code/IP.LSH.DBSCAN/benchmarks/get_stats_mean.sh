
#!/usr/bin/env bash
set -euo pipefail

DEFAULT_RUNS=30

PROFILE_SCRIPT="./profile_lshdbscan_flamegraph.sh"
ACCURACY_FILE="accuracy.txt"
BENCHMARK_FILE="benchmark.txt"
STAT_FILE="lshdbscan.perf.stat.txt"
CSV_FILE="run_metrics.csv"
RESULT_FILE="mean_results.txt"

total_runs="${1:-$DEFAULT_RUNS}"

# Ensure output files exist before measuring their initial line counts.
touch "$ACCURACY_FILE" "$BENCHMARK_FILE"

# Create a fresh CSV for this experiment.
# Results are appended after each successful run.
echo "run,elapsed_seconds,accuracy,hash_init_seconds,hash_population_seconds,core_buckets_seconds,merge_identification_seconds,tasks_merge_seconds,relabeling_seconds" > "$CSV_FILE"

echo "Running profiler $total_runs times..."

for ((i = 1; i <= total_runs; i++)); do
    echo "=== Run $i/$total_runs ==="

    # Remember where the existing appended output ends.
    accuracy_start=$(wc -l < "$ACCURACY_FILE")
    benchmark_start=$(wc -l < "$BENCHMARK_FILE")

    # Run the profiler.
    if ! bash "$PROFILE_SCRIPT"; then
        echo "Error: Profiling failed on run $i." >&2
        exit 1
    fi

    # Extract elapsed time from the stat file, which is overwritten.
    elapsed=$(awk '
        /seconds time elapsed/ { print $1; found = 1; exit }
        END { if (!found) exit 1 }
    ' "$STAT_FILE") || {
        echo "Error: Could not extract elapsed time on run $i." >&2
        exit 1
    }

    # Read only the accuracy appended during this run.
    accuracy=$(tail -n "+$((accuracy_start + 1))" "$ACCURACY_FILE" |
        awk 'NF { print $1; exit }')

    if [[ -z "$accuracy" ]]; then
        echo "Error: Could not extract accuracy on run $i." >&2
        exit 1
    fi

    # Read only the six benchmark values appended during this run.
    mapfile -t benchmark_values < <(
        tail -n "+$((benchmark_start + 1))" "$BENCHMARK_FILE" |
        awk '
            /Hash tables initialization time:/ {
                print $NF
            }
            /Hash tables population time:/ {
                print $NF
            }
            /Core buckets identification time:/ {
                print $NF
            }
            /Merge tasks identification time:/ {
                print $NF
            }
            /Tasks merge time:/ {
                print $NF
            }
            /Relabeling data time:/ {
                print $NF
            }
        '
    )

    if [[ ${#benchmark_values[@]} -ne 6 ]]; then
        echo "Error: Expected 6 benchmark values on run $i, got ${#benchmark_values[@]}." >&2
        exit 1
    fi

    # Append the complete row immediately after this run.
    printf '%s,%s,%s' "$i" "$elapsed" "$accuracy" >> "$CSV_FILE"
    printf ',%s' "${benchmark_values[@]}" >> "$CSV_FILE"
    printf '\n' >> "$CSV_FILE"

    echo "Elapsed: $elapsed s | Accuracy: $accuracy"
    echo "Metrics saved to $CSV_FILE"
done

# Calculate means from the accumulated CSV.
python3 - "$CSV_FILE" "$RESULT_FILE" "$total_runs" <<'PYTHON'
import csv
import sys
from statistics import mean

csv_file, result_file, expected_runs = sys.argv[1:]
expected_runs = int(expected_runs)

with open(csv_file, newline="") as f:
    reader = csv.DictReader(f)
    rows = list(reader)

if len(rows) != expected_runs:
    raise SystemExit(
        f"Expected {expected_runs} CSV rows, found {len(rows)}."
    )

metrics = [
    ("elapsed_seconds", "Mean elapsed time (seconds)"),
    ("accuracy", "Mean accuracy"),
    ("hash_init_seconds", "Mean Hash tables initialization time"),
    ("hash_population_seconds", "Mean Hash tables population time"),
    ("core_buckets_seconds", "Mean Core buckets identification time"),
    ("merge_identification_seconds", "Mean Merge tasks identification time"),
    ("tasks_merge_seconds", "Mean Tasks merge time"),
    ("relabeling_seconds", "Mean Relabeling data time"),
]

with open(result_file, "w") as f:
    f.write(f"Number of runs: {len(rows)}\n")

    for column, description in metrics:
        values = [float(row[column]) for row in rows]
        f.write(f"{description}: {mean(values):.12g}\n")

print(f"\nMean results saved to {result_file}")
PYTHON

cat "$RESULT_FILE"
