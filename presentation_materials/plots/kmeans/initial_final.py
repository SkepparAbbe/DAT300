import numpy as np
import matplotlib.pyplot as plt

# -----------------------------
# Configuration
# -----------------------------
N_POINTS = 1000
K = 3
SEED = 17
DATASET = "regions"
DATASET_FILE = f"../datasets/{DATASET}.csv"

INITIAL_OUTPUT_FILE = f"kmeans_initial_{DATASET}.png"
FINAL_OUTPUT_FILE = f"kmeans_final_{DATASET}.png"

points = np.loadtxt(
    DATASET_FILE,
    delimiter=",",
    skiprows=1
)

rng = np.random.default_rng(SEED)

# -----------------------------
# K-means implementation
# -----------------------------
def kmeans_steps(points, k, rng, max_iterations=20):
    # Randomly initialize centroids from existing points
    centroids = points[rng.choice(len(points), size=k, replace=False)]

    steps = []

    for iteration in range(max_iterations):
        # Assignment step
        distances = np.linalg.norm(
            points[:, None, :] - centroids[None, :, :],
            axis=2
        )
        labels = np.argmin(distances, axis=1)

        # Save state before updating centroids
        steps.append((
            points.copy(),
            labels.copy(),
            centroids.copy(),
            iteration
        ))

        # Update step
        new_centroids = np.zeros_like(centroids)

        for cluster in range(k):
            members = points[labels == cluster]

            if len(members) > 0:
                new_centroids[cluster] = members.mean(axis=0)
            else:
                # Reinitialize empty cluster
                new_centroids[cluster] = points[
                    rng.integers(len(points))
                ]

        # Check convergence
        if np.allclose(centroids, new_centroids):
            centroids = new_centroids

            # Save final state
            distances = np.linalg.norm(
                points[:, None, :] - centroids[None, :, :],
                axis=2
            )
            labels = np.argmin(distances, axis=1)

            steps.append((
                points.copy(),
                labels.copy(),
                centroids.copy(),
                iteration + 1
            ))

            break

        centroids = new_centroids

    return steps


steps = kmeans_steps(points, K, rng)


# -----------------------------
# Colors
# -----------------------------
colors = [
    "#741B47",  # dark fucsia
    "#C27BA0",  # light fucsia
    "#EDA29B",  # light salmon
    "#A64D79",  # medium fucsia
    "#E76F51",  # coral
]


# -----------------------------
# Plot initial state
# -----------------------------
def plot_initial(points, output_file):
    fig, ax = plt.subplots(figsize=(8, 6))

    # All points are gray
    ax.scatter(
        points[:, 0],
        points[:, 1],
        color="gray",
        s=45,
        alpha=0.7
    )

    ax.set_xlim(points[:, 0].min() - 1, points[:, 0].max() + 1)
    ax.set_ylim(points[:, 1].min() - 1, points[:, 1].max() + 1)

    ax.set_xlabel("X")
    ax.set_ylabel("Y")
    ax.set_title("Unclustered Data")

    ax.grid(alpha=0.2)

    plt.tight_layout()
    plt.savefig(output_file, dpi=150, bbox_inches="tight")
    plt.close(fig)

    print(f"Saved: {output_file}")


# -----------------------------
# Plot final state
# -----------------------------
def plot_final(step, output_file):
    pts, labels, centroids, iteration = step

    fig, ax = plt.subplots(figsize=(8, 6))

    # Plot each cluster
    # No centroids and no distance lines
    for cluster in range(K):
        cluster_points = pts[labels == cluster]

        ax.scatter(
            cluster_points[:, 0],
            cluster_points[:, 1],
            color=colors[cluster],
            s=45,
            alpha=0.7,
            label=f"Cluster {cluster + 1}"
        )

    ax.set_xlim(points[:, 0].min() - 1, points[:, 0].max() + 1)
    ax.set_ylim(points[:, 1].min() - 1, points[:, 1].max() + 1)

    ax.set_xlabel("X")
    ax.set_ylabel("Y")
    ax.set_title("Clustered Data")

    ax.grid(alpha=0.2)
    ax.legend(loc="upper right")

    plt.tight_layout()
    plt.savefig(output_file, dpi=150, bbox_inches="tight")
    plt.close(fig)

    print(f"Saved: {output_file}")


# -----------------------------
# Save images
# -----------------------------
plot_initial(points, INITIAL_OUTPUT_FILE)
plot_final(steps[-1], FINAL_OUTPUT_FILE)