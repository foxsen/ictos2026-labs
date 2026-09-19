#!/usr/bin/env python3
"""Extract the final sel4bench JSON object and a compact IPC CSV table."""

from __future__ import annotations

import argparse
import csv
import json
import re
from pathlib import Path


ANSI_ESCAPE = re.compile(r"\x1b(?:\[[0-?]*[ -/]*[@-~]|.)")


def extract_json(log: str) -> list[dict]:
    clean = ANSI_ESCAPE.sub("", log).replace("\r", "")
    end = clean.rfind("END JSON OUTPUT")
    if end < 0:
        raise ValueError("END JSON OUTPUT marker not found")
    start_marker = clean.rfind("JSON OUTPUT", 0, end)
    if start_marker < 0:
        raise ValueError("JSON OUTPUT marker not found")
    start = clean.find("[", start_marker, end)
    if start < 0:
        raise ValueError("JSON array not found between markers")
    return json.loads(clean[start:end].strip())


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    parser.add_argument("--json", required=True, type=Path)
    parser.add_argument("--csv", required=True, type=Path)
    parser.add_argument("--profile", required=True, choices=("tcg", "kvm", "hardware"))
    parser.add_argument("--fastpath", required=True, choices=("on", "off"))
    args = parser.parse_args()

    parsed = extract_json(args.log.read_text(encoding="utf-8", errors="replace"))
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps(parsed, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    columns = [
        "profile",
        "fastpath",
        "benchmark",
        "function",
        "direction",
        "same_vspace",
        "ipc_length_words",
        "samples",
        "min_cycles",
        "median_cycles",
        "mean_cycles",
        "q1_cycles",
        "q3_cycles",
        "max_cycles",
    ]
    with args.csv.open("w", newline="", encoding="utf-8") as destination:
        writer = csv.DictWriter(destination, fieldnames=columns)
        writer.writeheader()
        for suite in parsed:
            for result in suite.get("Results", []):
                writer.writerow(
                    {
                        "profile": args.profile,
                        "fastpath": args.fastpath,
                        "benchmark": suite.get("Benchmark", ""),
                        "function": result.get("Function", ""),
                        "direction": result.get("Direction", ""),
                        "same_vspace": result.get("Same vspace?", ""),
                        "ipc_length_words": result.get("IPC length", ""),
                        "samples": result.get("Samples", ""),
                        "min_cycles": result.get("Min", ""),
                        "median_cycles": result.get("Median", ""),
                        "mean_cycles": result.get("Mean", ""),
                        "q1_cycles": result.get("1st quantile", ""),
                        "q3_cycles": result.get("3rd quantile", ""),
                        "max_cycles": result.get("Max", ""),
                    }
                )


if __name__ == "__main__":
    main()
