"""Plot speedup against threads from one or more CSV files made by frankenstein_timing.

Usage:  python3 plot_speedup.py speedup.png results_1copy.csv "1 copy (0.44 MB)" \
                                            results_100copies.csv "100 copies (44 MB)"
Arguments after the PNG name come in pairs: a CSV file, then the label for it.
"""
import csv
import sys

import matplotlib
matplotlib.use("Agg")                     # draw to a file, no window needed
import matplotlib.pyplot as plt

COLOURS = ["#2a78d6", "#eb6834", "#1baf7a", "#6250d6"]   # one colour per data set


def read_csv(path):
    threads, speedup = [], []
    with open(path) as f:
        for row in csv.DictReader(f):
            threads.append(int(row["threads"]))
            speedup.append(float(row["speedup"]))
    return threads, speedup


def main():
    if len(sys.argv) < 4 or len(sys.argv) % 2 != 0:
        sys.exit(__doc__)
    out_png = sys.argv[1]
    pairs = list(zip(sys.argv[2::2], sys.argv[3::2]))

    fig, ax = plt.subplots(figsize=(7, 4.5), dpi=150)
    max_t = 1
    ends = []                             # last point of each line, for its label
    for (path, label), colour in zip(pairs, COLOURS):
        t, s = read_csv(path)
        max_t = max(max_t, max(t))
        ax.plot(t, s, marker="o", markersize=6, linewidth=2, color=colour, label=label)
        ends.append((t[-1], s[-1]))

    ideal = sorted({2 ** k for k in range(max_t.bit_length())} | {max_t})   # 1, 2, 4, ..., max_t
    ax.plot(ideal, ideal, linestyle="--", linewidth=1.5, color="#8a8984",
            label="ideal (speedup = threads)")

    ax.set_xscale("log", base=2)
    ax.set_yscale("log", base=2)
    ax.set_xticks(ideal)
    ax.set_xticklabels([str(x) for x in ideal])
    ax.set_yticks(ideal)
    ax.set_yticklabels([str(x) for x in ideal])
    ax.set_xlabel("OpenMP threads")
    ax.set_ylabel("speedup  t(1) / t(threads)")
    ax.set_title("Encoding Frankenstein: speedup against threads")
    ax.grid(True, which="major", color="#e6e5e0", linewidth=0.8)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    ax.legend(frameon=False, loc="upper left")

    # Label the final speedup of each line, nudging labels apart so they never overlap.
    fig.canvas.draw()
    ys = sorted(ends, key=lambda p: p[1])
    gap = 9 * 1.4 * fig.dpi / 72          # one 9-point line of text, in pixels
    last_pix = None
    for x, y in ys:
        pix = ax.transData.transform((x, y))[1]
        shift = 0.0
        if last_pix is not None and pix - last_pix < gap:
            shift = gap - (pix - last_pix)
        last_pix = pix + shift
        ax.annotate(f"{y:.1f}x", (x, y), textcoords="offset pixels", xytext=(6 * fig.dpi / 72, shift),
                    va="center", fontsize=9, color="#52514e")
    fig.tight_layout()
    fig.savefig(out_png)
    print("saved", out_png)


if __name__ == "__main__":
    main()
