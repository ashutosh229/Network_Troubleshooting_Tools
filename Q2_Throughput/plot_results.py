import csv
import sys

import matplotlib.pyplot as plt


def load_rows(csv_path):
    rows = []
    with open(csv_path, newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            rows.append(
                (
                    int(row["second"]),
                    float(row["throughput_bps"]),
                    float(row["avg_delay_ms"]),
                )
            )
    return rows


def main():
    if len(sys.argv) != 2:
        print("Usage: python plot_results.py <throughput_results.csv>")
        sys.exit(1)

    rows = load_rows(sys.argv[1])
    if not rows:
        print("No rows found in CSV")
        sys.exit(1)

    seconds = [row[0] for row in rows]
    throughput = [row[1] for row in rows]
    delay = [row[2] for row in rows]

    fig, axes = plt.subplots(2, 1, figsize=(10, 7), sharex=True)

    axes[0].plot(seconds, throughput, marker="o", color="tab:blue")
    axes[0].set_ylabel("Throughput (bps)")
    axes[0].set_title("Observed Throughput vs Time")
    axes[0].grid(True, linestyle="--", alpha=0.4)

    axes[1].plot(seconds, delay, marker="s", color="tab:red")
    axes[1].set_xlabel("Time (s)")
    axes[1].set_ylabel("Average Delay (ms)")
    axes[1].set_title("Observed Average Delay vs Time")
    axes[1].grid(True, linestyle="--", alpha=0.4)

    fig.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()
