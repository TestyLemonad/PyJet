<p align="center">
  <img src="Logo.png" alt="PyJet logo" width="512px">
</p>

# PyJet

**English** | [Русский](README_RU.md)

[![PyPI](https://img.shields.io/pypi/v/pyjet3d.svg)](https://pypi.org/project/pyjet3d/)
[![Python](https://img.shields.io/badge/python-3.9%2B-blue.svg)](https://pypi.org/project/pyjet3d/)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

`pyjet3d` is a small, dependency-free 3D software rasteriser for Python.  It
draws low-poly geometry on the CPU into an RGB565 framebuffer, so you can
render a scene without a GPU, OpenGL, a game engine or a windowing library.

It is a Python binding for [CubeCoders/Jet](https://github.com/CubeCoders/Jet),
a fixed-function C++17 rasteriser designed for constrained devices and useful
on ordinary desktops too.

## Features

- Pure CPU rendering; no runtime dependencies beyond the standard library.
- Full-resolution RGB565 output with a depth buffer.
- Camera with position, rotation, `lookAt()`, FOV and near/far clipping.
- Flat, Gouraud and Phong shading, wireframe and unlit modes.
- Directional and ambient lighting; point-light objects for custom/native shading.
- Triangle meshes built directly from Python lists.
- Per-mesh culling, blending and depth-buffer options.
- Framebuffer readback as `bytes`, plus RGB888 conversion.
- Wheels for Linux, Windows and macOS via `cibuildwheel`.

## Install

```bash
python -m pip install pyjet3d
```

Python 3.9 or newer is required.  No extra packages are needed at runtime.

> **0.3.0 migration:** the import was renamed from `pyjet` to `pyjet3d` to
> avoid a collision with the unrelated `pyjet` project on PyPI.  Change
> `import pyjet` to `import pyjet3d`.

## Quick start

```python
import pyjet3d

scene = pyjet3d.Scene(320, 240)

camera = pyjet3d.Camera()
camera.setPosition(0, 0, -500)
camera.setFOV(70, scene.width)   # FOV in degrees; width sets projection scale
camera.nearPlane = 32
camera.farPlane = 4096
scene.setCamera(camera)

material = pyjet3d.Material(0xF800)  # RGB565 bright red
material.shadingMode = pyjet3d.ShadingMode.FLAT

# Counter-clockwise winding: front-facing with the default CULL_BACKFACES.
scene.addMesh(
    [(0, 120, 0), (120, -120, 0), (-120, -120, 0)],
    material,
)
scene.render()

# The framebuffer contains width * height little-endian RGB565 values.
framebuffer = scene.get_framebuffer()
assert len(framebuffer) == scene.width * scene.height * 2

# Convert to RGB888 and save a dependency-free PPM image.
rgb = pyjet3d.framebuffer_to_rgb(framebuffer, scene.width, scene.height)
with open("triangle.ppm", "wb") as out:
    out.write(b"P6\n%d %d\n255\n" % (scene.width, scene.height))
    out.write(rgb)
```

That is the whole loop: build a scene, add geometry, render, and use the pixels.
There is no hidden window or event loop.  For Pillow output:

```python
from PIL import Image

rgb = pyjet3d.framebuffer_to_rgb(
    scene.get_framebuffer(), scene.width, scene.height
)
Image.frombytes("RGB", (scene.width, scene.height), rgb).save("triangle.png")
```

## Python demos

The repository contains runnable, dependency-free demos:

| Demo | Description |
| --- | --- |
| [`examples/python/triangle.py`](examples/python/triangle.py) | Smallest possible render; writes `triangle.ppm`. |
| [`examples/python/lit_cube.py`](examples/python/lit_cube.py) | Depth-tested cube with camera, ambient light and directional light. |
| [`examples/python/orbit.py`](examples/python/orbit.py) | A rotating-cube frame sequence for animation experiments. |

```bash
python examples/python/triangle.py
python examples/python/lit_cube.py --output lit_cube.ppm
python examples/python/orbit.py --frames 24 --output-dir orbit_frames
```

See [`examples/python/README.md`](examples/python/README.md) for options and
[`README_RU.md`](examples/python/README_RU.md) for the Russian version.

## Documentation

- [Python API guide](docs/Python.md) — complete English documentation.
- [Руководство по Python API](docs/Python_RU.md) — полная русская версия.
- [Changelog](CHANGELOG.md)
- [Engine API reference](https://cubecoders.github.io/Jet/)
- Native engine notes: [ESP32 performance](docs/ESP32Performance.md),
  [instancing](docs/instancing.md), [solid rectangles](docs/SolidRectangles.md)
  and [tiled textures](docs/TiledTextures.md).

## What is exposed in Python

`pyjet3d` is intentionally a focused binding:

- **Scene** — dimensions, camera, lights, mesh submission, backcolor, clearing,
  framebuffer readback, pixel access and frame statistics.
- **Camera** — position, Euler rotation, `lookAt()`, FOV and clipping planes.
- **Mesh** — vertices, triangle indices, transforms, culling, blending and
  depth flags.  Geometry is copied into the scene at `addMesh()` time.
- **Material** — RGB565 colour, alpha, diffuse/specular/emissive properties and
  all shading modes.
- **Lights** — directional and ambient lighting; point-light objects for
  custom/native shading.
- **Helpers** — `framebuffer_to_rgb()` and the small `Vector3`, `Color` value
  types.

Textures, custom shaders, sprites, particles, post-processing, picking and
native primitive/OBJ helpers exist in the C++ engine but are not part of the
current Python API.

## How it works

Jet is a fixed-function rasteriser.  It transforms vertices with integer
fixed-point maths, clips triangles, rasterises them with an optional depth
buffer, and shades them using material and light data.  The Python build is
configured for full-resolution desktop output with depth buffering and
lighting enabled; the embedded C++ build can be trimmed for ESP32 and similar
targets.

The engine supports much more than the Python wrapper exposes, including
textures, instancing, sprites, particles, screen-space effects and custom
shaders.  See the [native documentation](https://cubecoders.github.io/Jet/)
and the [JetExamples](https://github.com/CubeCoders/JetExamples) repository for
those features.

## Development

Run the Python tests from the repository root:

```bash
python -m pytest
```

Build a wheel locally:

```bash
python -m pip install build
python -m build --wheel
```

The release version lives in [`version.txt`](version.txt) as a single line such
as `v0.3.0`.  A push that changes this file on `main` starts the release
workflow: it builds wheels on Linux, Windows and macOS, creates or updates the
matching GitHub release with the wheels attached, and publishes them to PyPI.
See [`.github/workflows/wheels.yml`](.github/workflows/wheels.yml) for details.

## Community and license

Jet is a [CubeCoders](https://cubecoders.com) project.  Join the
[Discord server](https://discord.gg/FSdJYDTEYt) to share projects and ask for
help.

PyJet is distributed under the MIT License.  See [`LICENSE`](LICENSE) for the
full text.  You can use it in open-source and closed-source projects, including
commercial products, provided the copyright and permission notices are kept.

Contributions are welcome; please read [`CONTRIBUTING.md`](CONTRIBUTING.md)
first.
