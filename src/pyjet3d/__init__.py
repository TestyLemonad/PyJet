"""Python bindings for the Jet 3D software rasteriser."""

import struct
from pathlib import Path

from ._pyjet import (
    AmbientLight,
    BlendMode,
    Camera,
    Color,
    CullingMode,
    DirectionalLight,
    Material,
    Mesh,
    PointLight,
    Scene,
    ShadingMode,
    Vector3,
    PIXEL_FORMAT,
)

def _read_version():
    """Use version.txt in a source tree, installed metadata otherwise."""
    here = Path(__file__).resolve()
    for parent in list(here.parents)[:3]:
        candidate = parent / "version.txt"
        if candidate.is_file():
            raw = candidate.read_text(encoding="utf-8").strip()
            if raw.startswith("v"):
                raw = raw[1:]
            if raw:
                return raw
    try:
        from importlib.metadata import PackageNotFoundError, version as dist_version

        return dist_version("pyjet3d")
    except PackageNotFoundError:
        return "0.3.0"


__version__ = _read_version()

__all__ = [
    "AmbientLight",
    "BlendMode",
    "Camera",
    "Color",
    "CullingMode",
    "DirectionalLight",
    "Material",
    "Mesh",
    "PointLight",
    "Scene",
    "ShadingMode",
    "Vector3",
    "framebuffer_to_rgb",
    "PIXEL_FORMAT",
]


def framebuffer_to_rgb(framebuffer, width, height):
    """Convert an RGB565 framebuffer to packed RGB888 bytes.

    ``framebuffer`` must contain exactly ``width * height`` little-endian
    uint16 RGB565 values, as returned by :meth:`Scene.get_framebuffer`.
    The result is a ``bytes`` object with three bytes per pixel, suitable for
    writing a PPM file or constructing a Pillow image.
    """
    if width <= 0 or height <= 0:
        raise ValueError("width and height must be positive")
    pixels = width * height
    if len(framebuffer) != pixels * 2:
        raise ValueError(
            "framebuffer size does not match width * height * 2 bytes"
        )

    values = struct.unpack("<{}H".format(pixels), framebuffer)
    rgb = bytearray(pixels * 3)
    out = 0
    for value in values:
        r5 = (value >> 11) & 0x1F
        g6 = (value >> 5) & 0x3F
        b5 = value & 0x1F
        rgb[out] = (r5 * 255 + 15) // 31
        rgb[out + 1] = (g6 * 255 + 31) // 63
        rgb[out + 2] = (b5 * 255 + 15) // 31
        out += 3
    return bytes(rgb)
