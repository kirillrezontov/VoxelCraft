"""Run VSync modes serially and retain each process's diagnostic log."""
import argparse
import pathlib
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("binary", type=pathlib.Path)
parser.add_argument("directory", type=pathlib.Path)
parser.add_argument("--frames", type=int, default=240)
args = parser.parse_args()
args.directory.mkdir(parents=True, exist_ok=True)
for mode in ("off", "on"):
    command = [str(args.binary.resolve()), str((args.directory / f"{mode}.jsonl").resolve()), mode, str(args.frames)]
    with (args.directory / f"{mode}.log").open("w") as log:
        child = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        try:
            for line in child.stdout:
                log.write(line)
                if "complete" in line or "Validation enabled" in line:
                    print(mode + ": " + line.rstrip(), flush=True)
            if child.wait() != 0:
                raise SystemExit(f"Benchmark failed: {command}; see {log.name}")
        finally:
            if child.poll() is None:
                child.terminate()
                child.wait()
