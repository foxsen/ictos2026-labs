#!/usr/bin/env python3
"""Summarize raw benchmark CSV files without third-party dependencies."""

from __future__ import annotations

import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path
from statistics import fmean, median


def percentile(values: list[float], percent: float) -> float:
    ordered = sorted(values)
    if not ordered:
        return math.nan
    if len(ordered) == 1:
        return ordered[0]
    position = (len(ordered) - 1) * percent / 100.0
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    weight = position - lower
    return ordered[lower] * (1.0 - weight) + ordered[upper] * weight


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("inputs", nargs="+", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    groups: dict[tuple[str, str, str], list[float]] = defaultdict(list)
    for path in args.inputs:
        with path.open(newline="", encoding="utf-8") as source:
            for row in csv.DictReader(source):
                key = (row["platform"], row["benchmark"], row["message_bytes"])
                groups[key].append(float(row["ns_per_op"]))

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="", encoding="utf-8") as destination:
        writer = csv.writer(destination)
        writer.writerow(
            [
                "platform",
                "benchmark",
                "message_bytes",
                "samples",
                "min_batch_mean_ns_per_op",
                "median_batch_mean_ns_per_op",
                "mean_batch_mean_ns_per_op",
                "p95_batch_mean_ns_per_op",
                "p99_batch_mean_ns_per_op",
                "max_batch_mean_ns_per_op",
            ]
        )
        for (platform, benchmark, message_bytes), values in sorted(groups.items()):
            writer.writerow(
                [
                    platform,
                    benchmark,
                    message_bytes,
                    len(values),
                    f"{min(values):.3f}",
                    f"{median(values):.3f}",
                    f"{fmean(values):.3f}",
                    f"{percentile(values, 95):.3f}",
                    f"{percentile(values, 99):.3f}",
                    f"{max(values):.3f}",
                ]
            )


if __name__ == "__main__":
    main()
