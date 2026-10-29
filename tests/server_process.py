"""Exercise the real headless executable, snapshots and process-owned locks."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time


def require(value, message):
    if not value:
        raise RuntimeError(message)


def run(server, world, *extra, success=True):
    command = [server, "--world-dir", str(world), "--port", "0", "--radius", "1",
               "--no-mods", "--no-creatures", *extra]
    result = subprocess.run(command, capture_output=True, text=True, timeout=20)
    require((result.returncode == 0) == success, result.stdout + result.stderr)
    return result.stdout + result.stderr


def wait_for_text(process, log, expected):
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        text = log.read_text(errors="replace")
        if expected in text:
            return
        require(process.poll() is None, text)
        time.sleep(0.02)
    raise RuntimeError("Headless server did not report " + expected + ": " + log.read_text())


def main():
    server = str(Path(sys.argv[1]).resolve())
    root = Path(sys.argv[2])
    root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="server-process-", dir=root) as temporary:
        root = Path(temporary)
        world = root / "world"
        first = run(server, world, "--ticks", "90", "--survival")
        require("Server clean shutdown" in first and "saved world revision 1" in first, first)
        second = run(server, world, "--ticks", "30")
        require("loaded world revision 1" in second and "saved world revision 2" in second, second)
        snapshots = sorted(path.name for path in (world / "snapshots").iterdir())
        rejected = run(server, world, "--ticks", "1", "--seed", "999", success=False)
        require("differs from the saved world" in rejected, rejected)
        require(sorted(path.name for path in (world / "snapshots").iterdir()) == snapshots,
                "Rejected startup changed snapshot revisions")
        if os.name != "nt":
            log = root / "running.log"
            with log.open("w") as output:
                process = subprocess.Popen([server, "--world-dir", str(world), "--port", "0",
                                            "--radius", "1", "--no-mods", "--no-creatures"],
                                           stdout=output, stderr=subprocess.STDOUT)
                try:
                    wait_for_text(process, log, "Server world ready")
                    locked = run(server, world, "--ticks", "1", success=False)
                    require("locked" in locked.lower() or "another" in locked.lower(), locked)
                    # A failed shutdown must keep the live process/state available.
                    backup = world / "snapshots-backup"
                    (world / "snapshots").rename(backup)
                    (world / "snapshots").write_text("block snapshot I/O")
                    process.terminate()
                    wait_for_text(process, log, "Shutdown cancelled: save failed")
                    require(process.poll() is None, "Failed save terminated the live server")
                    (world / "snapshots").unlink()
                    backup.rename(world / "snapshots")
                    process.terminate()
                    require(process.wait(timeout=15) == 0, log.read_text())
                    require("saved world revision 3" in log.read_text(), log.read_text())
                finally:
                    if process.poll() is None:
                        process.kill()
                        process.wait()
            # A hard process exit must release the OS lock without a new commit.
            with log.open("w") as output:
                process = subprocess.Popen([server, "--world-dir", str(world), "--port", "0",
                                            "--radius", "1", "--no-mods", "--no-creatures"],
                                           stdout=output, stderr=subprocess.STDOUT)
                try:
                    wait_for_text(process, log, "Server world ready")
                    process.kill()
                    process.wait(timeout=5)
                finally:
                    if process.poll() is None:
                        process.kill()
                        process.wait()
            reopened = run(server, world, "--ticks", "30")
            require("loaded world revision 3" in reopened and "saved world revision 4" in reopened,
                    reopened)
        print("Real server persistence, failed-startup preservation and process lifecycle passed")


if __name__ == "__main__":
    main()
