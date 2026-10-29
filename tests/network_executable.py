"""Run the actual server and two actual Vulkan clients on the test display."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time


def require(value, message):
    if not value:
        raise RuntimeError(message)


def main():
    server, client = (str(Path(value).resolve()) for value in sys.argv[1:3])
    root, mods = (Path(value).resolve() for value in sys.argv[3:5])
    root.mkdir(parents=True, exist_ok=True)
    processes = []
    with tempfile.TemporaryDirectory(prefix="network-process-", dir=root) as directory:
        work = Path(directory)
        with (work / "server.log").open("w") as server_log:
            host = subprocess.Popen([server, "--no-save", "--port", "0", "--radius", "1",
                                     "--mods-dir", str(mods), "--ticks", "1800", "--no-creatures"],
                                    stdout=server_log, stderr=subprocess.STDOUT)
            processes.append(host)
            try:
                deadline = time.monotonic() + 15
                address = None
                while time.monotonic() < deadline:
                    log = (work / "server.log").read_text(errors="replace")
                    match = re.search(r"Server listening on 127\.0\.0\.1:(\d+)", log)
                    if match:
                        address = "127.0.0.1:" + match.group(1)
                        break
                    require(host.poll() is None, log)
                    time.sleep(0.02)
                require(address, "Server did not publish its ephemeral port")
                logs = []
                for name in ("NetAlice", "NetBob"):
                    output = (work / (name + ".log")).open("w")
                    logs.append((name, output))
                    process = subprocess.Popen([client, "--connect", address, "--name", name,
                                                "--mods-dir", str(mods), "--no-settings", "--frames", "120",
                                                "--require-validation", "--screenshot", str(work / (name + ".ppm"))],
                                               stdout=output, stderr=subprocess.STDOUT)
                    processes.append(process)
                try:
                    for process in processes[1:]:
                        require(process.wait(timeout=30) == 0, "Connected client failed")
                finally:
                    for _, output in logs:
                        output.close()
                    for name, _ in logs:
                        log = (work / (name + ".log")).read_text(errors="replace")
                        print(name + ":\n" + log)
                for name, _ in logs:
                    log = (work / (name + ".log")).read_text(errors="replace")
                    require("Connected as " + name in log, log)
                    require("2 players" in log, "Executable client never observed both peers: " + log)
                    require("0 errors, 0 warnings (including shutdown)" in log, log)
                    require((work / (name + ".ppm")).read_bytes().startswith(b"P6"), "Missing Vulkan capture")
                wrong = subprocess.run([client, "--connect", address, "--name", "Mismatch", "--no-mods",
                                        "--no-settings", "--frames", "1", "--require-validation"],
                                       capture_output=True, text=True, timeout=15)
                require(wrong.returncode != 0 and "Content mismatch" in wrong.stderr,
                        wrong.stdout + wrong.stderr)
                print("Two real Vulkan executables and mismatched-content rejection passed")
            finally:
                for process in reversed(processes):
                    if process.poll() is None:
                        process.terminate()
                        try:
                            process.wait(timeout=5)
                        except subprocess.TimeoutExpired:
                            process.kill()
                            process.wait()


if __name__ == "__main__":
    main()
