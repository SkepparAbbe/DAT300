import numpy as np
import matplotlib.pyplot as plt

N_POINTS = 350
K = 3
SEED = 42

# -----------------------------
# Generate sample data
# -----------------------------
rng = np.random.default_rng(SEED)

true_centers = np.array([
    [-3, -2],
    [0, 3],
    [3, -1],
])

points = np.vstack([
    rng.normal(center, 0.8, size=(N_POINTS // K, 2))
    for center in true_centers
])

np.savetxt(
    "../datasets/regions.csv",
    points,
    delimiter=",",
    header="x,y",
    comments=""
)

print("Dataset saved to regions.csv")