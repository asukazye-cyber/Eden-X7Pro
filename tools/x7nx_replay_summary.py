#!/usr/bin/env python3
"""Summarize an X7NX JSON replay capture without changing the capture."""

import json
import statistics
import sys
from pathlib import Path


def percentile(values: list[float], fraction: float) -> float:
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int((len(ordered) - 1) * fraction))]


def main(path: Path) -> int:
    data = json.loads(path.read_text(encoding="utf-8"))
    samples = data["samples"]
    if not samples:
        raise ValueError("capture has no samples")
    cpu = [sample["cpu_frame_ms"] for sample in samples]
    gpu = [sample["gpu_frame_ms"] for sample in samples if "gpu_frame_ms" in sample]
    print(f"[X7NX] title={data['title_id']} path={data['renderer_path']} samples={len(samples)}")
    print("[X7NX] cpu_ms p50/p95/p99=" + "/".join(f"{percentile(cpu, p):.2f}" for p in (.50, .95, .99)))
    if gpu:
        print("[X7NX] gpu_ms p50/p95/p99=" + "/".join(f"{percentile(gpu, p):.2f}" for p in (.50, .95, .99)))
    print(f"[X7NX] cache hit/miss={sum(s['cache_hits'] for s in samples)}/{sum(s['cache_misses'] for s in samples)} barriers={sum(s['barriers'] for s in samples)}")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("usage: x7nx_replay_summary.py CAPTURE.json", file=sys.stderr)
        raise SystemExit(2)
    raise SystemExit(main(Path(sys.argv[1])))
