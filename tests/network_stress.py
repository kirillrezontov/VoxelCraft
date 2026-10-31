"""Bounded real-process malformed TCP stress; uses an ephemeral, temporary host."""
import argparse
import re
import socket
import struct
import subprocess
import tempfile
import time
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("server", type=Path)
    parser.add_argument("--iterations", type=int, default=100)
    options = parser.parse_args()
    if not 1 <= options.iterations <= 10000:
        parser.error("iterations must be 1..10000")
    with tempfile.TemporaryDirectory(prefix="voxelcraft-malformed-") as temporary:
        log_path = Path(temporary) / "server.log"
        with log_path.open("w") as output:
            host = subprocess.Popen([str(options.server.resolve()), "--no-save", "--port", "0",
                                     "--radius", "1", "--no-mods", "--no-creatures"],
                                    stdout=output, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 15
                port = None
                while time.monotonic() < deadline:
                    text = log_path.read_text(errors="replace")
                    match = re.search(r"Server listening on 127\.0\.0\.1:(\d+); content (\d+)", text)
                    if match and "Server world ready" in text:
                        port, fingerprint = int(match[1]), int(match[2])
                        break
                    assert host.poll() is None, text
                    time.sleep(0.01)
                assert port, "Server readiness timed out"
                malformed = [b"\0\0\0\0", struct.pack("<I", 4 * 1024 * 1024 + 1),
                             struct.pack("<I", 100) + b"truncated", struct.pack("<II", 4, 0xffffffff),
                             struct.pack("<II", 4, 10)]  # Save before Hello.
                for iteration in range(options.iterations):
                    with socket.create_connection(("127.0.0.1", port), timeout=3) as peer:
                        data = malformed[iteration % len(malformed)]
                        try:
                            # Deliberate fragmentation; no TCP read/message assumptions.
                            peer.sendall(data[:2])
                            peer.sendall(data[2:])
                        except (BrokenPipeError, ConnectionResetError):
                            pass
                    assert host.poll() is None, log_path.read_text(errors="replace")
                    time.sleep(0.02)
                time.sleep(0.2)
                # After malformed peers, prove a valid protocol-4 client can join.
                with socket.create_connection(("127.0.0.1", port), timeout=3) as peer:
                    hello = struct.pack("<I", 1) + b"VCNET" + struct.pack("<IQH", 4, fingerprint, 5) + b"Probe" + b"\0\0"
                    peer.sendall(struct.pack("<I", len(hello)) + hello)
                    def receive(count):
                        result = b""
                        while len(result) < count:
                            data = peer.recv(count - len(result))
                            assert data, "Host closed valid probe connection"
                            result += data
                        return result
                    size, = struct.unpack("<I", receive(4))
                    assert 4 <= size <= 4 * 1024 * 1024
                    message = receive(size)
                    assert struct.unpack_from("<I", message)[0] == 2, message[:80]
                    peer.sendall(struct.pack("<II", 4, 11))  # Goodbye.
                print(f"Malformed stress passed: {options.iterations} fragmented invalid sessions; valid join afterwards")
            finally:
                if host.poll() is None:
                    host.terminate()
                    try:
                        host.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        host.kill()
                        host.wait()


if __name__ == "__main__":
    main()
