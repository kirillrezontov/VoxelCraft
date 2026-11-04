"""Expose an explicitly supplied, already installed Khronos layer locally.

No downloads or system changes. The library must be trusted by the caller.
Read its exported metadata rather than inventing a layer/extension version.
"""
import argparse
import ctypes
import json
import pathlib
import shlex

parser = argparse.ArgumentParser()
parser.add_argument("library", type=pathlib.Path)
parser.add_argument("directory", type=pathlib.Path)
args = parser.parse_args()
library = args.library.resolve(strict=True)
layer = ctypes.CDLL(str(library))


class Properties(ctypes.Structure):
    _fields_ = [("name", ctypes.c_char * 256), ("version", ctypes.c_uint32),
                ("implementation", ctypes.c_uint32), ("description", ctypes.c_char * 256)]


class Extension(ctypes.Structure):
    _fields_ = [("name", ctypes.c_char * 256), ("version", ctypes.c_uint32)]


properties = Properties()
count = ctypes.c_uint32(1)
enumerate_layers = layer.vkEnumerateInstanceLayerProperties
enumerate_layers.argtypes = [ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(Properties)]
enumerate_layers.restype = ctypes.c_int32
if enumerate_layers(ctypes.byref(count), ctypes.byref(properties)) != 0 or count.value != 1:
    raise SystemExit("Could not enumerate one layer")
if properties.name != b"VK_LAYER_KHRONOS_validation":
    raise SystemExit("Explicit library is not the Khronos validation layer")
enumerate_extensions = layer.vkEnumerateInstanceExtensionProperties
enumerate_extensions.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(Extension)]
enumerate_extensions.restype = ctypes.c_int32
count = ctypes.c_uint32()
if enumerate_extensions(properties.name, ctypes.byref(count), None) != 0 or count.value > 256:
    raise SystemExit("Could not enumerate validation extensions")
extensions = (Extension * count.value)()
if enumerate_extensions(properties.name, ctypes.byref(count), extensions) != 0:
    raise SystemExit("Could not read validation extensions")
version = properties.version
manifest = {"file_format_version": "1.2.0", "layer": {
    "name": properties.name.decode(), "type": "GLOBAL", "library_path": str(library),
    "api_version": f"{version >> 22}.{(version >> 12) & 1023}.{version & 4095}",
    "implementation_version": str(properties.implementation), "description": properties.description.decode(),
    "instance_extensions": [{"name": extension.name.decode(), "spec_version": str(extension.version)}
                            for extension in extensions],
}}
args.directory.mkdir(parents=True, exist_ok=True)
path = args.directory / "VkLayer_khronos_validation.json"
path.write_text(json.dumps(manifest, indent=2) + "\n")
print(f"Created {path}; API {manifest['layer']['api_version']}")
print(f"export VK_LAYER_PATH={shlex.quote(str(args.directory.resolve()))}")
