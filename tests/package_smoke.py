"""Validate extracted CPack artifacts; never use real user configuration/worlds."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def run(executable, cwd, *args):
    result = subprocess.run([str(executable), *args], cwd=cwd, capture_output=True,
                            text=True, timeout=45)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return result.stdout + result.stderr


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("archives", nargs="+")
    parser.add_argument("--gpu", action="store_true")
    options = parser.parse_args()
    for archive_name in options.archives:
        archive = Path(archive_name).resolve()
        with tempfile.TemporaryDirectory(prefix="voxelcraft-package-") as temporary:
            root = Path(temporary)
            unpacked = root / "extracted"
            shutil.unpack_archive(archive, unpacked)
            files = [p for p in unpacked.rglob("*") if p.is_file()]
            assert not any(p.name == "CMakeCache.txt" or p.suffix in (".o", ".obj", ".a", ".lib") for p in files)
            assert any(p.name == "README.txt" for p in files)
            assert any(p.name == "stb_image.txt" for p in files)
            assert any(p.name == "stb_truetype.txt" for p in files)
            assert any(p.name == "DejaVu.txt" for p in files)
            assert any(p.name == "v0.8.2-commands.md" for p in files)
            for library in ("EnTT.txt", "GLM.txt", "Lua.html", "JSON.txt"):
                assert any(p.name == library for p in files), library
            binaries = [p for p in files if p.name in ("voxelcraft", "voxelcraft.exe", "voxelcraft-server", "voxelcraft-server.exe")]
            assert len(binaries) == 1, binaries
            executable = binaries[0]
            build_info = (executable.parent / "BUILD-INFO.txt").read_text()
            assert "0.8.2-rc1" in build_info and "network protocol: 4" in build_info
            if "Platform: Linux" in build_info:
                for runtime in ("libstdc++.so.6", "libgcc_s.so.1"):
                    assert (executable.parent / "lib" / runtime).exists(), runtime
                linked = subprocess.run(["ldd", str(executable)], capture_output=True, text=True, timeout=5)
                assert linked.returncode == 0, linked.stderr
                for runtime in ("libstdc++.so.6", "libgcc_s.so.1"):
                    assert any(runtime + " => " in line and str(executable.parent / "lib") in line
                               for line in linked.stdout.splitlines()), linked.stdout
            elif "Compiler: MSVC" in build_info:
                for runtime in ("msvcp140.dll", "vcruntime140.dll"):
                    assert any(p.name.lower() == runtime for p in files), runtime
            cwd = root / "unrelated-working-directory"
            cwd.mkdir()
            help_text = run(executable, cwd, "--help")
            assert "0.8.2" in help_text, help_text
            if "server" in executable.name:
                world = root / "world-тест-世界"
                output = run(executable, cwd, "--port", "0", "--radius", "1", "--ticks", "90",
                             "--world-dir", str(world), "--no-mods", "--no-creatures")
                assert "clean shutdown" in output, output
                assert (world / "snapshots/1/world.vcw").exists()
                output = run(executable, cwd, "--port", "0", "--ticks", "1", "--radius", "1",
                             "--world-dir", str(world), "--no-mods", "--no-creatures")
                assert "loaded world revision 1" in output, output
            else:
                assert (executable.parent / "assets/shaders/voxel.vert.spv").is_file()
                assert (executable.parent / "assets/shaders/voxel.frag.spv").is_file()
                assert any(p.name == "SDL.txt" for p in files)
                for font in ("DejaVuSans.ttf", "DejaVuSansMono.ttf", "LICENSE-DejaVu.txt"):
                    assert (executable.parent / "assets/fonts" / font).is_file(), font
                if options.gpu:
                    output = run(executable, cwd, "--smoke", "--no-settings", "--require-validation",
                                 "--screenshot", str(root / "package.ppm"))
                    assert "0 errors, 0 warnings (including shutdown)" in output, output
            print(archive.name + ": extracted package passed")


if __name__ == "__main__":
    main()
