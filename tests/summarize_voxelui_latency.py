"""Summarize raw samples; retain distributions and counters for every CPU phase."""
import argparse
import gzip
import hashlib
import json
import math
import pathlib
import statistics

parser = argparse.ArgumentParser()
parser.add_argument("output", type=pathlib.Path)
parser.add_argument("--before", type=pathlib.Path, required=True)
parser.add_argument("--after", type=pathlib.Path, required=True)
parser.add_argument("--metadata", type=pathlib.Path, help="Explicit hardware/method metadata for these runs")
args = parser.parse_args()


def distribution(values):
    values = sorted(values)
    if not values:
        return None
    return dict(median=statistics.median(values), p95=values[math.ceil(len(values) * .95) - 1],
                p99=values[math.ceil(len(values) * .99) - 1], worst=values[-1])


report = {"schema": 1, "units": {"cpu": "milliseconds", "gpu": "milliseconds"},
          "quantiles": "median; p95/p99 nearest rank; worst maximum", "runs": {}}
if args.metadata:
    report["metadata"] = json.loads(args.metadata.read_text())
raw_directory = args.output.parent / "voxelui-latency-raw"
raw_directory.mkdir(parents=True, exist_ok=True)
for revision, directory in (("before", args.before), ("after", args.after)):
    report["runs"][revision] = modes = {}
    for path in sorted(directory.glob("*.jsonl")):
        raw = path.read_bytes()
        rows = [json.loads(line) for line in raw.splitlines()]
        archive = raw_directory / f"{revision}-{path.stem}.jsonl.gz"
        archive.write_bytes(gzip.compress(raw, mtime=0))
        groups = {}
        for row in rows:
            groups.setdefault(row["scenario"], []).append(row)
        modes[path.stem] = {"raw": str(archive.relative_to(args.output.parent)),
                            "raw_sha256": hashlib.sha256(raw).hexdigest(), "scenarios": {}}
        for scenario, samples in groups.items():
            timings = [row["timing"] for row in samples]
            modes[path.stem]["scenarios"][scenario] = {
                "samples": len(samples),
                "viewports": sorted({(row["width"], row["height"]) for row in samples if "width" in row}),
                "cpu_ms": {phase: distribution([t["cpu_ns"][phase] / 1e6 for t in timings])
                           for phase in timings[0]["cpu_ns"]},
                "calls_per_frame": {phase: distribution([t["calls"][phase] for t in timings])
                                    for phase in timings[0]["calls"]},
                "ui_gpu_ms_previous_submission": distribution([t["gpu_ms"] for t in timings if t["gpu_ms"] is not None]),
                "changed_frames": sum(t["upload_bytes"] > 0 for t in timings),
                "upload_bytes": distribution([t["upload_bytes"] for t in timings]),
                "queue_idle_calls": sum(t["queue_idle_calls"] for t in timings),
                "device_idle_calls": sum(t["device_idle_calls"] for t in timings),
            }
args.output.write_text(json.dumps(report, indent=2) + "\n")
