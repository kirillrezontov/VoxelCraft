"""Exercise cache path handling from both supported launch directories."""
import pathlib
import subprocess
import sys
import tempfile

module = pathlib.Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="vc-voxelui-discovery-") as directory:
    root = pathlib.Path(directory)
    host = root / "VoxelCraft"
    source = root / "VoxelUI"
    host.mkdir()
    (source / "include/voxelui").mkdir(parents=True)
    (source / "CMakeLists.txt").write_text(
        'set(VoxelUI_VERSION 0.2.1)\n'
        'add_library(voxelui INTERFACE)\n'
        'add_library(software INTERFACE)\n'
        'add_library(VoxelUI::voxelui ALIAS voxelui)\n'
        'add_library(VoxelUI::software ALIAS software)\n'
    )
    for header in ("software.hpp", "runtime.hpp"):
        (source / "include/voxelui" / header).touch()
    (host / "CMakeLists.txt").write_text(
        'cmake_minimum_required(VERSION 3.25)\n'
        'project(Discovery NONE)\n'
        f'include("{module.as_posix()}")\n'
    )

    def configure(name, cwd, value, succeeds=True, missing=None):
        result = subprocess.run(
            ["cmake", "-S", str(host), "-B", str(root / name),
             "-DVC_VOXELUI_NATIVE=OFF", f"-DVC_VOXELUI_SOURCE_DIR={value}"],
            cwd=cwd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        )
        assert (result.returncode == 0) == succeeds, result.stdout
        if succeeds:
            assert str(source) in result.stdout, result.stdout
        else:
            for text in (f"Supplied: '{value}'", "Resolved:", missing,
                         "CMAKE_PREFIX_PATH", "VC_VOXELUI_SOURCE_DIR:STRING=../VoxelUI"):
                assert text in result.stdout, result.stdout

    configure("parent", root, "../VoxelUI")
    configure("source", host, "../VoxelUI")
    configure("absolute", root, str(source))
    configure("invalid", root, "../absent", False, "absent/CMakeLists.txt")
    for header in ("software.hpp", "runtime.hpp"):
        path = source / "include/voxelui" / header
        path.unlink()
        configure("missing-" + header, root, "../VoxelUI", False, header)
        path.touch()
print("VoxelUI sibling discovery and incomplete-checkout diagnostics passed")
