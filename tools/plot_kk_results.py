#!/usr/bin/env python3
import argparse
import csv
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt


DEFAULT_PLOT_STYLE = {
    "font_size": 20,
    "title_font_size": 15,
    "label_font_size": 20,
    "tick_font_size": 18,
    "legend_font_size": 14,
    "legend_columns": 2,
    "legend_location": "best",
}

DEFAULT_HIDDEN_METHODS = {
    "CM",
    "CM-CU",
    "KK-Sketch1",
    "Stingy",
    "o-Tailored",
    "Stable-Sketch",
    "LightGuardian",
    "SketchVisor",
    "Tailored",
}

DISPLAY_NAMES = {
    "KK-Sketch": "FusionSketch",
    "Residual Sketch": "FusionSketch",
}

PLOT_MARKERS = ["^", "s", "D", "v", "P", "X", "*", "<", ">", "h"]

METRICS = [
    ("AAE", "AAE of per-flow size estimation"),
    ("ARE", "ARE of all flows"),
    ("ARE_HH", "ARE of heavy-hitter flows"),
    ("HH_precision", "Precision of heavy-hitter detection"),
    ("HH_recall", "Recall of heavy-hitter detection"),
    ("F1", "F1 score of heavy hitter detection"),
    ("Accuracy", "Accuracy of heavy hitter detection"),
    ("F1_HC", "F1 score of heavy change detection"),
    ("WMRE", "WMRE of flow size distribution"),
    ("RE", "RE of entropy estimation"),
    ("insert_Mpps", "Insertion throughput (Mpps)"),
    ("query_Mqps", "Query throughput (Mqps)"),
]


def load_rows(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def plot_metric(rows, metric, title, out_dir, hidden_methods, style):
    by_method = defaultdict(list)
    for row in rows:
        if row["method"] in hidden_methods:
            continue
        if metric not in row:
            continue
        x_key = "target_memory_mb" if "target_memory_mb" in row else "memory_mb"
        by_method[row["method"]].append((float(row[x_key]), float(row[metric])))

    if not by_method:
        return False

    plt.figure(figsize=(7.2, 4.4))
    for index, (method, points) in enumerate(sorted(by_method.items())):
        points.sort()
        xs = [p[0] for p in points]
        ys = [p[1] for p in points]
        is_residual = method in {"KK-Sketch", "Residual Sketch"}
        marker = PLOT_MARKERS[index % len(PLOT_MARKERS)]
        linewidth = 2.4 if is_residual else 1.4
        label = DISPLAY_NAMES.get(method, method)
        plt.plot(xs, ys, marker=marker, linewidth=linewidth, label=label)

    plt.xlabel("Memory budget (MB)", fontsize=style["label_font_size"])
    plt.ylabel(metric, fontsize=style["label_font_size"])
    plt.xticks(fontsize=style["tick_font_size"])
    plt.yticks(fontsize=style["tick_font_size"])
    plt.grid(True, linestyle="--", linewidth=0.5, alpha=0.5)
    plt.legend(
        frameon=False,
        ncol=style["legend_columns"],
        loc=style["legend_location"],
        fontsize=style["legend_font_size"],
    )
    plt.tight_layout()
    plt.savefig(out_dir / f"{metric}.pdf")
    plt.close()
    return True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--csv", required=True, help="CSV written by KKSketch_Benchmark")
    parser.add_argument("--out-dir", default=None, help="Directory for PDF plots")
    parser.add_argument(
        "--hide-method",
        action="append",
        default=sorted(DEFAULT_HIDDEN_METHODS),
        help="Method to hide from plots only. May be repeated.",
    )
    parser.add_argument(
        "--metrics",
        default=None,
        help="Comma-separated metric names to plot (default: every available metric).",
    )
    parser.add_argument(
        "--font-size",
        type=float,
        default=DEFAULT_PLOT_STYLE["font_size"],
        help="Base font size for all plots.",
    )
    parser.add_argument(
        "--title-font-size",
        type=float,
        default=DEFAULT_PLOT_STYLE["title_font_size"],
        help="Plot title font size.",
    )
    parser.add_argument(
        "--label-font-size",
        type=float,
        default=DEFAULT_PLOT_STYLE["label_font_size"],
        help="X/Y axis label font size.",
    )
    parser.add_argument(
        "--tick-font-size",
        type=float,
        default=DEFAULT_PLOT_STYLE["tick_font_size"],
        help="Axis tick label font size.",
    )
    parser.add_argument(
        "--legend-font-size",
        type=float,
        default=DEFAULT_PLOT_STYLE["legend_font_size"],
        help="Legend text font size.",
    )
    parser.add_argument(
        "--legend-columns",
        type=int,
        default=DEFAULT_PLOT_STYLE["legend_columns"],
        help="Number of columns in the legend.",
    )
    parser.add_argument(
        "--legend-location",
        default=DEFAULT_PLOT_STYLE["legend_location"],
        help='Legend location, for example "best", "upper right", or "lower left".',
    )
    args = parser.parse_args()
    if args.legend_columns < 1:
        parser.error("--legend-columns must be at least 1")

    csv_path = Path(args.csv)
    out_dir = Path(args.out_dir) if args.out_dir else csv_path.parent / "plots"
    out_dir.mkdir(parents=True, exist_ok=True)

    style = {
        "font_size": args.font_size,
        "title_font_size": args.title_font_size,
        "label_font_size": args.label_font_size,
        "tick_font_size": args.tick_font_size,
        "legend_font_size": args.legend_font_size,
        "legend_columns": args.legend_columns,
        "legend_location": args.legend_location,
    }
    plt.rcParams.update({"font.size": style["font_size"]})

    rows = load_rows(csv_path)
    hidden_methods = set(args.hide_method or [])
    requested_metrics = None
    if args.metrics:
        requested_metrics = {metric.strip() for metric in args.metrics.split(",") if metric.strip()}
        known_metrics = {metric for metric, _ in METRICS}
        unknown = requested_metrics - known_metrics
        if unknown:
            parser.error("unknown metric(s): " + ", ".join(sorted(unknown)))
    written = 0
    for metric, title in METRICS:
        if requested_metrics is not None and metric not in requested_metrics:
            continue
        if plot_metric(rows, metric, title, out_dir, hidden_methods, style):
            written += 1
    print(f"Wrote {written} PDF plots to {out_dir}")


if __name__ == "__main__":
    main()
