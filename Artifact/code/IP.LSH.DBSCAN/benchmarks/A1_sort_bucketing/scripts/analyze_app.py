"""Aggregate the end-to-end runs of LSHDBSCAN_exec with (-s, A1) and without sort bucketing.

usage: python analyze_app.py <dir containing app_runs.csv and accuracy.csv>
"""
import csv
import statistics
import sys
from collections import defaultdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

out = Path(sys.argv[1])
COLORS = {"baseline": "#2a78d6", "a1": "#eb6834"}  # categorical slots 1, 2
NAMES = {"baseline": "hash maps (original)", "a1": "sort bucketing (-s, A1)"}
INK, MUTED, GRID = "#0b0b0b", "#52514e", "#e4e3df"
PHASES = {"phase1": ("populate_s",), "core": ("core_s",),
          "total": ("populate_s", "core_s", "merge_identify_s", "merge_perform_s", "relabel_s")}

runs = defaultdict(list)
for row in csv.DictReader(open(out / "app_runs.csv")):
    runs[(row["dataset"], int(row["L"]), int(row["K"]), row["variant"])].append(row)
accuracy = {}
for row in csv.DictReader(open(out / "accuracy.csv")):
    accuracy[(row["dataset"], int(row["L"]), int(row["K"]))] = row

summary = {}
for key, rows in runs.items():
    measured = [r for r in rows if int(r["rep"]) > 0]
    if len(measured) < 5:
        print("incomplete:", key, len(measured))
        continue
    s = {name: [sum(float(r[c]) for c in cols) for r in measured] for name, cols in PHASES.items()}
    summary[key] = {name: (statistics.median(v), min(v)) for name, v in s.items()}
    summary[key]["peak_mb"] = statistics.median(int(r["peak_rss_kb"]) for r in measured) / 1024

with open(out / "app_summary.csv", "w", newline="") as fh:
    w = csv.writer(fh)
    w.writerow(["dataset", "L", "K", "variant", "phase1_med_s", "phase1_min_s", "core_med_s", "core_min_s",
                "total_med_s", "total_min_s", "peak_rss_mb"])
    for (d, L, K, v), s in sorted(summary.items()):
        w.writerow([d, L, K, v, *s["phase1"], *s["core"], *s["total"], round(s["peak_mb"], 1)])

datasets = sorted({k[0] for k in summary})
md = []
for d in datasets:
    Ls = sorted({k[1] for k in summary if k[0] == d})
    Ks = sorted({k[2] for k in summary if k[0] == d})
    exact = d == "household"
    md.append(f"### {d}\n")
    md.append("Times from the app's `benchmark.txt`, in seconds as `median (min)` over 5 measured runs; "
              "x = original / A1 on medians. Total = all clustering phases (excludes reading data and writing labels). "
              "Rand index from the first measured run" + (" (exact = vanilla DBSCAN labels)." if exact else ".") + "\n")
    head = ("| L | K | Phase I orig | Phase I A1 | x | core buckets orig | core buckets A1 | x | total orig | total A1 | x "
            "| peak RSS orig / A1 (MB) | Rand orig vs A1 |" + (" Rand vs exact orig / A1 |" if exact else ""))
    md.append(head)
    md.append("|---" * (head.count("|") - 1) + "|")
    for L in Ls:
        for K in Ks:
            b, a = summary.get((d, L, K, "baseline")), summary.get((d, L, K, "a1"))
            if not (b and a):
                continue
            cell = lambda s, m: f"{s[m][0]:.4f} ({s[m][1]:.4f})"
            sp = lambda m: f"**{b[m][0] / a[m][0]:.2f}**"
            acc = accuracy.get((d, L, K), {})
            line = (f"| {L} | {K} | {cell(b, 'phase1')} | {cell(a, 'phase1')} | {sp('phase1')} | {cell(b, 'core')} | "
                    f"{cell(a, 'core')} | {sp('core')} | {cell(b, 'total')} | {cell(a, 'total')} | {sp('total')} | "
                    f"{b['peak_mb']:.0f} / {a['peak_mb']:.0f} | {acc.get('rand_baseline_vs_a1', '')} |")
            if exact:
                line += f" {acc.get('rand_baseline_vs_exact', '')} / {acc.get('rand_a1_vs_exact', '')} |"
            md.append(line)
    md.append("")
(out / "app_results_tables.md").write_text("\n".join(md) + "\n")


def style(ax):
    ax.grid(True, color=GRID, linewidth=0.8)
    ax.set_axisbelow(True)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(MUTED)
    ax.tick_params(colors=MUTED, labelsize=8)


plt.rcParams.update({"font.size": 9, "text.color": INK, "axes.labelcolor": MUTED})
rows = [("phase1", "Phase I (populate)"), ("core", "Core-bucket identification"), ("total", "All clustering phases")]
for d in datasets:
    Ls = sorted({k[1] for k in summary if k[0] == d})
    fig, axes = plt.subplots(len(rows), len(Ls), figsize=(3.6 * len(Ls), 2.8 * len(rows)), squeeze=False)
    for c, L in enumerate(Ls):
        for r, (metric, title) in enumerate(rows):
            ax = axes[r][c]
            style(ax)
            for v in ("baseline", "a1"):
                pts = sorted((k[2], s[metric][0]) for k, s in summary.items() if k[:2] == (d, L) and k[3] == v)
                if not pts:
                    continue
                ax.plot(*zip(*pts), color=COLORS[v], linewidth=2, marker="o", markersize=4, label=NAMES[v])
                ax.annotate("A1" if v == "a1" else "orig", pts[-1], xytext=(4, 0),
                            textcoords="offset points", color=MUTED, fontsize=7, va="center")
            Ks = sorted({k[2] for k in summary if k[:2] == (d, L)})
            ax.set_xscale("log", base=2)
            ax.set_ylim(bottom=0)
            ax.set_xticks(Ks)
            ax.set_xticklabels([str(K) for K in Ks])
            ax.minorticks_off()
            ax.set_title(f"{title}, L={L}", fontsize=9, color=INK, loc="left")
            if r == len(rows) - 1:
                ax.set_xlabel("threads K (36 physical cores)")
            if c == 0:
                ax.set_ylabel("median time (s)")
    handles, labels = axes[0][0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="upper right", ncol=2, frameon=False, fontsize=8)
    fig.suptitle(f"{d}, LSHDBSCAN_exec: time vs threads", y=1.02, fontsize=11, color=INK, x=0.01, ha="left")
    fig.tight_layout(rect=(0, 0, 1, 0.97))
    fig.savefig(out / f"app_scaling_{d}.png", dpi=150, bbox_inches="tight", facecolor="white")
    plt.close(fig)

fig, axes = plt.subplots(1, len(datasets), figsize=(4.2 * len(datasets), 3.2), squeeze=False)
for ax, d in zip(axes[0], datasets):
    style(ax)
    for v in ("baseline", "a1"):
        pts = sorted((k[1], s["peak_mb"] / 1024) for k, s in summary.items() if k[0] == d and k[2] == 36 and k[3] == v)
        if not pts:
            continue
        ax.plot(*zip(*pts), color=COLORS[v], linewidth=2, marker="o", markersize=5, label=NAMES[v])
        ax.annotate(f"{pts[-1][1]:.2f} GB", pts[-1], xytext=(5, 0), textcoords="offset points",
                    color=MUTED, fontsize=7, va="center")
    ax.set_xticks(sorted({k[1] for k in summary if k[0] == d}))
    ax.set_ylim(bottom=0)
    ax.set_xlabel("hash tables L")
    ax.set_ylabel("peak RSS (GB), K=36")
    ax.set_title(d, fontsize=10, color=INK, loc="left")
handles, labels = axes[0][0].get_legend_handles_labels()
fig.legend(handles, labels, loc="upper center", ncol=2, frameon=False, fontsize=8)
fig.tight_layout(rect=(0, 0, 1, 0.9))
fig.savefig(out / "app_memory_rss_vs_L.png", dpi=150, bbox_inches="tight", facecolor="white")
print("wrote", out / "app_summary.csv", out / "app_results_tables.md", "and plots")

# Before/after for A1 if the previous version's summary is kept (keys table by table vs grouped).
v1_path = out / "v1_table_by_table" / "app_summary.csv"
if v1_path.exists():
    v1 = {(r["dataset"], int(r["L"]), int(r["K"])): r for r in csv.DictReader(open(v1_path)) if r["variant"] == "a1"}
    lines = ["| dataset | L | K | Phase I orig | Phase I A1 v1 (table by table) | Phase I A1 v2 (grouped keys) "
             "| total orig | total A1 v1 | total A1 v2 | peak RSS A1 v1 / v2 (MB) |", "|---" * 10 + "|"]
    for (d, L, K, v), s in sorted(summary.items()):
        if v != "a1" or (d, L, K) not in v1:
            continue
        if (d, L, K, "baseline") not in summary:
            continue
        b, old = summary[(d, L, K, "baseline")], v1[(d, L, K)]
        lines.append(f"| {d} | {L} | {K} | {b['phase1'][0]:.3f} | {float(old['phase1_med_s']):.3f} | **{s['phase1'][0]:.3f}** "
                     f"| {b['total'][0]:.3f} | {float(old['total_med_s']):.3f} | **{s['total'][0]:.3f}** "
                     f"| {float(old['peak_rss_mb']):.0f} / {s['peak_mb']:.0f} |")
    (out / "v1_vs_v2.md").write_text("\n".join(lines) + "\n")
    print("wrote", out / "v1_vs_v2.md")
