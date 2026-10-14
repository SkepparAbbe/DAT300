# Shared configs for the A1 scripts (sourced).
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"   # IP.LSH.DBSCAN/
DATA="$(cd "$ROOT/../../data" && pwd)"
BUILD="$ROOT/build"
OUT="$ROOT/benchmarks/A1_sort_bucketing"
MNIST="mnist -f $DATA/mnist/mnist_data.txt -a -e 0.2 -m 100 -M 9"
HOUSEHOLD="household -f $DATA/household/clean_normalized_household.txt -e 2000 -m 100 -M 5"
MNIST_L="58 116 230"
HOUSEHOLD_L="5 10 20"
THREADS="32 36 72"   # 36 physical cores; 72 = hyperthreads (the app's default)
