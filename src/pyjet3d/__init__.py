"""Python bindings for the Jet 3D software rasteriser."""

import struct

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

__version__ = "0.3.0"

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
