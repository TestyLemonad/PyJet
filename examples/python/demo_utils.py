"""Shared helpers for the Python demos.

The demos intentionally depend only on the pyjet3d wheel and the Python
standard library.  They can be run from this directory or from the repository
root, for example:

    python examples/python/triangle.py
"""

from pathlib import Path

import pyjet3d


def rgb565(r, g, b):
    """Pack 8-bit RGB channels into the RGB565 format used by materials."""
    if not all(0 <= value <= 255 for value in (r, g, b)):
        raise ValueError("RGB channels must be in [0, 255]")
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def write_ppm(path, scene):
    """Write a rendered Scene as a binary PPM (RGB888) image."""
    output = Path(path)
    output.parent.mkdir(parents=True, exist_ok=True)

    rgb = pyjet3d.framebuffer_to_rgb(
        scene.get_framebuffer(), scene.width, scene.height
    )
    header = "P6\n{} {}\n255\n".format(scene.width, scene.height).encode("ascii")
    output.write_bytes(header + rgb)
    return output


def make_camera(scene, position=(0, 0, -700), target=(0, 0, 0), fov=55):
    """Create a camera suitable for the cube scenes."""
    camera = pyjet3d.Camera()
    camera.setPosition(*position)
    camera.setFOV(fov, scene.width)
    camera.nearPlane = 32
    camera.farPlane = 4096
    camera.lookAt(*target)
    scene.setCamera(camera)
    return camera


def cube_mesh(size=220):
    """Return a centred cube with outward-facing triangles.

    The winding is consistent, so the default CULL_BACKFACES mode works.
    """
    s = size / 2
    vertices = [
        (-s, -s, -s),
        (s, -s, -s),
        (s, s, -s),
        (-s, s, -s),
        (-s, -s, s),
        (s, -s, s),
        (s, s, s),
        (-s, s, s),
    ]
    faces = [
        (0, 3, 2), (0, 2, 1),  # -Z (towards the camera)
        (4, 5, 6), (4, 6, 7),  # +Z
        (0, 4, 7), (0, 7, 3),  # -X
        (1, 2, 6), (1, 6, 5),  # +X
        (0, 1, 5), (0, 5, 4),  # -Y
        (3, 7, 6), (3, 6, 2),  # +Y
    ]
    return pyjet3d.Mesh(vertices, faces)


def make_lit_scene(width=320, height=240, material_color=0xF800):
    """Create the standard camera, lights and red material used by demos."""
    scene = pyjet3d.Scene(width, height)
    scene.setBackcolor(rgb565(24, 32, 48))  # dark slate blue
    make_camera(scene)

    light = pyjet3d.DirectionalLight(
        azimuth=225,
        elevation=45,
        r=255,
        g=241,
        b=214,
        intensity=255,
    )
    scene.setDirectionalLight(light)
    scene.setAmbientLight(pyjet3d.AmbientLight(100, 104, 120))

    material = pyjet3d.Material(material_color)
    material.shadingMode = pyjet3d.ShadingMode.PHONG
    return scene, material
