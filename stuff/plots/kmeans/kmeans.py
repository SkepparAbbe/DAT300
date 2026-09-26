import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation, PillowWriter

# -----------------------------
# Configuration
# -----------------------------
N_POINTS = 1000
K = 3
SEED = 17
DATASET="regions"
DATASET_FILE = f'../datasets/{DATASET}.csv'
OUTPUT_FILE = f"kmeans_animation_{DATASET}.gif"

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
# Set up plot
# -----------------------------
fig, ax = plt.subplots(figsize=(8, 6))

# colors = plt.cm.tab10(np.arange(K))

colors = [
    "#741B47",  # dark fucsia
    "#C27BA0",  # light fucsia
    "#EDA29B",  # light salmon
    "#A64D79",  # medium fucsia
    "#E76F51",  # coral
]

ax.set_xlim(points[:, 0].min() - 1, points[:, 0].max() + 1)
ax.set_ylim(points[:, 1].min() - 1, points[:, 1].max() + 1)

ax.set_xlabel("X")
ax.set_ylabel("Y")
ax.set_title("K-means Clustering")

ax.grid(alpha=0.2)


# -----------------------------
# Animation update function
# -----------------------------
def update(frame):
    ax.clear()

    pts, labels, centroids, iteration = steps[frame]

    # Plot each cluster
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

    # Plot centroids
    ax.scatter(
        centroids[:, 0],
        centroids[:, 1],
        color="black",
        marker="X",
        s=220,
        linewidths=2,
        edgecolors="white",
        label="Centroids"
    )

    # Draw lines from points to their centroids
    for i, point in enumerate(pts):
        centroid = centroids[labels[i]]

        ax.plot(
            [point[0], centroid[0]],
            [point[1], centroid[1]],
            color=colors[labels[i]],
            alpha=0.08,
            linewidth=0.7
        )

    ax.set_xlim(points[:, 0].min() - 1, points[:, 0].max() + 1)
    ax.set_ylim(points[:, 1].min() - 1, points[:, 1].max() + 1)

    ax.set_xlabel("X")
    ax.set_ylabel("Y")
    ax.set_title(f"K-means Clustering — Iteration {iteration}")

    ax.grid(alpha=0.2)
    ax.legend(loc="upper right")


# -----------------------------
# Create animation
# -----------------------------
animation = FuncAnimation(
    fig,
    update,
    frames=len(steps),
    interval=1200,
    repeat=True
)


# -----------------------------
# Save GIF
# -----------------------------
animation.save(
    OUTPUT_FILE,
    writer=PillowWriter(fps=1)
)

print(f"Animation saved to: {OUTPUT_FILE}")

plt.show()
