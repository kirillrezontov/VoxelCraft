"""Run the mandatory window lifecycle case in an isolated KWin virtual compositor.

This is optional test orchestration, not a replacement rendering backend. The
application still selects hardware Vulkan and requires Khronos validation.
Requires installed kwin_wayland and dbus-run-session; never installs packages.
"""
import argparse
import os
import pathlib
import shlex
import shutil
import subprocess
import tempfile
import uuid

parser = argparse.ArgumentParser()
parser.add_argument("build", type=pathlib.Path)
args = parser.parse_args()
for program in ("kwin_wayland", "dbus-run-session", "ctest"):
    if not shutil.which(program):
        raise SystemExit(f"Missing prerequisite: {program}")
command = ["ctest", "--test-dir", str(args.build.resolve()), "-R",
           "^Client.Sessions.WindowLifecycle$", "-V", "--no-tests=error"]
with tempfile.TemporaryDirectory(prefix="vc-window-lifecycle-") as directory:
    root = pathlib.Path(directory)
    session = root / "session.sh"
    result = root / "result"
    session.write_text("#!/bin/sh\nexport SDL_VIDEODRIVER=wayland\n" +
                       shlex.join(command) + "\nstatus=$?\n" +
                       "printf '%s\\n' \"$status\" > " + shlex.quote(str(result)) +
                       "\nexit \"$status\"\n")
    session.chmod(0o700)
    env = os.environ.copy()
    for variable, subdir in (("XDG_CONFIG_HOME", "config"), ("XDG_DATA_HOME", "data"),
                             ("XDG_CACHE_HOME", "cache")):
        path = root / subdir
        path.mkdir()
        env[variable] = str(path)
    subprocess.run(["dbus-run-session", "--", "kwin_wayland", "--virtual",
                    "--width", "1280", "--height", "900", "--socket",
                    "vc-test-" + uuid.uuid4().hex, "--no-lockscreen",
                    "--no-global-shortcuts", "--no-kactivities", "--exit-with-session",
                    str(session)], env=env, timeout=90, check=False)
    if not result.exists():
        raise SystemExit("Compositor exited before the lifecycle test completed")
    raise SystemExit(int(result.read_text().strip()))
