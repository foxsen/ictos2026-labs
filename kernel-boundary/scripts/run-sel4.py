#!/usr/bin/env python3
"""Run sel4bench until its JSON marker appears, then stop QEMU cleanly enough for CI."""

from __future__ import annotations

import argparse
import os
import selectors
import signal
import subprocess
import sys
import time
from pathlib import Path


MARKER = b"END JSON OUTPUT"


def stop_process_group(process: subprocess.Popen[bytes]) -> None:
    if process.poll() is not None:
        return
    os.killpg(process.pid, signal.SIGTERM)
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait(timeout=5)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--profile", required=True, choices=("tcg", "kvm"))
    parser.add_argument("--log", required=True, type=Path)
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()

    simulate = args.build_dir.resolve() / "simulate"
    if not simulate.is_file():
        raise SystemExit(f"simulate helper not found: {simulate}")

    command = [str(simulate)]
    if args.profile == "tcg":
        command.extend(["-c", "max", "-o", ""])
    else:
        if not Path("/dev/kvm").exists():
            raise SystemExit("KVM profile requested, but /dev/kvm is unavailable")
        command.extend(["-c", "host", "-o", "", "--extra-qemu-args=-enable-kvm"])

    args.log.parent.mkdir(parents=True, exist_ok=True)
    process = subprocess.Popen(
        command,
        cwd=args.build_dir,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        start_new_session=True,
    )
    assert process.stdout is not None
    selector = selectors.DefaultSelector()
    selector.register(process.stdout, selectors.EVENT_READ)
    deadline = time.monotonic() + args.timeout
    collected = bytearray()
    found = False

    try:
        with args.log.open("wb") as log:
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    break
                events = selector.select(timeout=0.5)
                for key, _ in events:
                    chunk = os.read(key.fileobj.fileno(), 4096)
                    if not chunk:
                        continue
                    log.write(chunk)
                    log.flush()
                    sys.stdout.buffer.write(chunk)
                    sys.stdout.buffer.flush()
                    collected.extend(chunk)
                    if MARKER in collected:
                        found = True
                        break
                if found:
                    break
    finally:
        stop_process_group(process)

    if not found:
        raise SystemExit(f"sel4bench JSON marker not found within {args.timeout}s; see {args.log}")


if __name__ == "__main__":
    main()
