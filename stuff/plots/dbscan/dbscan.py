import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation, PillowWriter
from collections import deque

# ============================================================
# Configuration
# ============================================================

# For the "moons" dataset, these parameters work well:
# EPS = 0.35
# MIN_SAMPLES = 6

# For the "regions" dataset, these parameters work well:
EPS = 0.75
MIN_SAMPLES = 10

DATASET="regions"
DATASET_FILE = f'../datasets/{DATASET}.csv'
OUTPUT_FILE = f"dbscan_animation_{DATASET}.gif"

points = np.loadtxt(
    DATASET_FILE,
    delimiter=",",
    skiprows=1
)

# ============================================================
# DBSCAN implementation
# ============================================================

UNCLASSIFIED = -2
NOISE = -1

def region_query(points, point_index, eps):
    """Return indices of points within eps of point_index."""

    distances = np.linalg.norm(
        points - points[point_index],
        axis=1
    )

    return np.where(distances <= eps)[0].tolist()


def dbscan_with_animation(points, eps, min_samples):
    """
    Run DBSCAN while recording intermediate states
    for animation.
    """

    n = len(points)

    labels = np.full(n, UNCLASSIFIED)

    # Determine core points first
    neighborhoods = []

    for i in range(n):
        neighbors = region_query(points, i, eps)
        neighborhoods.append(neighbors)

    core = np.array([
        len(neighborhoods[i]) >= min_samples
        for i in range(n)
    ])

    steps = []

    # Initial state
    steps.append({
        "labels": labels.copy(),
        "current": None,
        "neighbors": [],
        "core": core.copy(),
        "cluster": 0
    })

    cluster_id = 0

    for start in range(n):

        if labels[start] != UNCLASSIFIED:
            continue

        # If this isn't a core point, mark it as noise for now.
        # DBSCAN may later convert it to a border point.
        if not core[start]:
            labels[start] = NOISE

            steps.append({
                "labels": labels.copy(),
                "current": start,
                "neighbors": neighborhoods[start],
                "core": core.copy(),
                "cluster": cluster_id
            })

            continue

        # Start a new cluster
        cluster_id += 1

        labels[start] = cluster_id

        queue = deque(neighborhoods[start])

        steps.append({
            "labels": labels.copy(),
            "current": start,
            "neighbors": neighborhoods[start],
            "core": core.copy(),
            "cluster": cluster_id
        })

        while queue:

            point = queue.popleft()

            # A point previously considered noise can become
            # a border point.
            if labels[point] == NOISE:
                labels[point] = cluster_id

            # Already assigned
            if labels[point] != UNCLASSIFIED:
                continue

            labels[point] = cluster_id

            neighbors = neighborhoods[point]

            steps.append({
                "labels": labels.copy(),
                "current": point,
                "neighbors": neighbors,
                "core": core.copy(),
                "cluster": cluster_id
            })

            # Only core points expand the cluster
            if core[point]:
                for neighbor in neighbors:

                    if labels[neighbor] in (UNCLASSIFIED, NOISE):
                        queue.append(neighbor)

    # Final state
    steps.append({
        "labels": labels.copy(),
        "current": None,
        "neighbors": [],
        "core": core.copy(),
        "cluster": cluster_id
    })

    return steps


steps = dbscan_with_animation(
    points,
    EPS,
    MIN_SAMPLES
)


# ============================================================
# Plot setup
# ============================================================

fig, ax = plt.subplots(figsize=(10, 7))


# ============================================================
# Animation
# ============================================================

def update(frame):

    ax.clear()

    state = steps[frame]

    labels = state["labels"]
    current = state["current"]
    neighbors = state["neighbors"]
    core = state["core"]

    # --------------------------------------------------------
    # Background
    # --------------------------------------------------------

    ax.set_xlim(points[:, 0].min() - 1, points[:, 0].max() + 1)
    ax.set_ylim(points[:, 1].min() - 1, points[:, 1].max() + 1)

    ax.set_aspect("equal")

    ax.grid(
        alpha=0.15,
        linestyle="--"
    )

    # --------------------------------------------------------
    # Draw clusters
    # --------------------------------------------------------

    unique_clusters = sorted(
        set(labels) - {UNCLASSIFIED, NOISE}
    )

    # colors = plt.cm.tab20(
    #     np.arange(max(len(unique_clusters), 1))
    # )

    colors = [
        "#741B47",  # dark fucsia
        "#C27BA0",  # light fucsia
        "#A64D79",  # medium fucsia
        "#EDA29B",  # light salmon
        "#E76F51",  # coral
    ]

    for index, cluster_id in enumerate(unique_clusters):

        mask = labels == cluster_id

        ax.scatter(
            points[mask, 0],
            points[mask, 1],
            s=45,
            color=colors[index],
            alpha=0.8,
            label=f"Cluster {cluster_id}"
        )

    # --------------------------------------------------------
    # Unclassified points
    # --------------------------------------------------------

    unclassified = labels == UNCLASSIFIED

    if np.any(unclassified):

        ax.scatter(
            points[unclassified, 0],
            points[unclassified, 1],
            s=45,
            color="lightgray",
            edgecolor="gray",
            label="Unvisited"
        )

    # --------------------------------------------------------
    # Noise
    # --------------------------------------------------------

    noise = labels == NOISE

    if np.any(noise):

        ax.scatter(
            points[noise, 0],
            points[noise, 1],
            s=55,
            color="black",
            marker="x",
            linewidth=1.5,
            label="Noise"
        )

    # --------------------------------------------------------
    # Core points
    # --------------------------------------------------------

    core_unclassified = (
        core &
        (labels == UNCLASSIFIED)
    )

    if np.any(core_unclassified):

        ax.scatter(
            points[core_unclassified, 0],
            points[core_unclassified, 1],
            s=100,
            facecolors="none",
            edgecolors="gray",
            linewidth=1.5
        )

    # --------------------------------------------------------
    # Current point
    # --------------------------------------------------------

    if current is not None:

        x, y = points[current]

        # Draw epsilon neighborhood
        circle = plt.Circle(
            (x, y),
            EPS,
            color=colors[3],
            alpha=0.15,
            linewidth=2,
            fill=True
        )

        ax.add_patch(circle)

        ax.scatter(
            x,
            y,
            s=180,
            facecolors="none",
            edgecolors=colors[3],
            linewidth=3,
            zorder=10
        )

        # Highlight neighbors
        if neighbors:

            neighbor_points = points[neighbors]

            ax.scatter(
                neighbor_points[:, 0],
                neighbor_points[:, 1],
                s=110,
                facecolors="none",
                edgecolors=colors[3],
                linewidth=1.5,
                zorder=9
            )

    # --------------------------------------------------------
    # Title / explanation
    # --------------------------------------------------------

    if current is None:

        title = (
            "DBSCAN — Final Clustering\n"
            f"ε = {EPS}   min_samples = {MIN_SAMPLES}"
        )

    else:

        n_neighbors = len(neighbors)

        if core[current]:

            status = "CORE POINT"

        else:

            status = "BORDER / NOISE POINT"

        title = (
            f"DBSCAN — inspecting point {current}\n"
            f"{n_neighbors} points within ε = {EPS}   |   {status}"
        )

    ax.set_title(
        title,
        fontsize=14,
        pad=15
    )

    ax.set_xlabel("X")
    ax.set_ylabel("Y")

    ax.legend(
        loc="upper right",
        framealpha=0.9
    )


# ============================================================
# Create animation
# ============================================================

animation = FuncAnimation(
    fig,
    update,
    frames=len(steps),
    interval=180,
    repeat=True
)


# ============================================================
# Save
# ============================================================

print("Creating animation...")

animation.save(
    OUTPUT_FILE,
    writer=PillowWriter(fps=5)
)

print(f"Saved to: {OUTPUT_FILE}")

plt.show()
