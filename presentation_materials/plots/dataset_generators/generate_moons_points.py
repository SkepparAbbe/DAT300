import numpy as np
import matplotlib.pyplot as plt

N_POINTS_PER_MOON = 150
N_NOISE = 50
SEED = 42

# ============================================================
# Generate two interlocking moons
# ============================================================

rng = np.random.default_rng(SEED)

def make_moons(n, noise=0.08):
    """Generate two interlocking crescent-shaped clusters."""

    n1 = n // 2
    n2 = n - n1

    # First moon
    theta1 = np.linspace(0, np.pi, n1)

    moon1 = np.column_stack([
        np.cos(theta1),
        np.sin(theta1)
    ])

    # Second moon, shifted and flipped
    theta2 = np.linspace(0, np.pi, n2)

    moon2 = np.column_stack([
        1 - np.cos(theta2),
        -np.sin(theta2) - 0.15
    ])

    points = np.vstack([moon1, moon2])

    # Add random noise
    points += rng.normal(0, noise, points.shape)

    # Scale slightly
    points[:, 0] *= 1.5

    return points


moons = make_moons(
    N_POINTS_PER_MOON * 2,
    noise=0.08
)


# ============================================================
# Add scattered noise points
# ============================================================

noise_points = rng.uniform(
    low=[-2.5, -1.5],
    high=[3.5, 1.8],
    size=(N_NOISE, 2)
)

points = np.vstack([moons, noise_points])

np.savetxt(
    "../datasets/moons.csv",
    points,
    delimiter=",",
    header="x,y",
    comments=""
)

print("Dataset saved to moons.csv")