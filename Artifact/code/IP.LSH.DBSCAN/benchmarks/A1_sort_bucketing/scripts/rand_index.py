"""Rand index between label files (one integer label per line; noise -1 is one group).
usage: rand_index.py baseline a1 [exact]  ->  prints RI(baseline,a1)[,RI(baseline,exact),RI(a1,exact)]"""
import sys

import numpy as np


def rand_index(a, b):
    pairs = lambda n: n * (n - 1) / 2
    _, both = np.unique(np.stack([a, b]), axis=1, return_counts=True)
    _, only_a = np.unique(a, return_counts=True)
    _, only_b = np.unique(b, return_counts=True)
    total = pairs(len(a))
    both, only_a, only_b = (pairs(c.astype(np.float64)).sum() for c in (both, only_a, only_b))
    return (total + 2 * both - only_a - only_b) / total


files = [np.loadtxt(path, dtype=np.int64) for path in sys.argv[1:]]
out = [rand_index(files[0], files[1])]
if len(files) == 3:
    out += [rand_index(files[0], files[2]), rand_index(files[1], files[2])]
print(",".join(f"{value:.6f}" for value in out) + ("" if len(files) == 3 else ",,"))
