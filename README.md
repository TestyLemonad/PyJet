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
- Full-resolution RGB565 output with a depth buffer, plus native RGB888 and
  RGBA8888 readback (no Python-level per-frame conversion loop).
- Multithreaded horizontal-tile rasterizer on desktop builds, with a persistent
  thread pool, parallel clearing and deterministic output. Object frustum
  culling happens before vertex transform/submission.
- SSE2/AVX2 span-fill, clear and native colour-conversion kernels on x86-64,
  with scalar fallbacks for other targets.
- Camera with position, rotation, `lookAt()`, FOV and near/far clipping.
- Flat, Gouraud and Phong shading, wireframe and unlit modes.
- Directional, ambient and world-space point lighting, with per-channel light
  colour and point-light range falloff.
- Triangle meshes built directly from Python lists, with automatic face normals
  or optional per-vertex normals for smooth shading.
- Per-mesh culling, blending and depth-buffer options.
- Framebuffer readback as `bytes`, pixel access and frame statistics.
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

Lighting is optional and uses the same scene:

```python
sun = pyjet3d.DirectionalLight(
    azimuth=225, elevation=45, r=255, g=241, b=214, intensity=255
)
scene.setDirectionalLight(sun)
scene.setAmbientLight(pyjet3d.AmbientLight(100, 104, 120))
scene.addPointLight(
    pyjet3d.PointLight(-220, -80, -280, 255, 190, 120, 150, 900)
)
```

Point lights fade linearly from `rangeMin` to `rangeMax` and illuminate the
built-in FLAT/GOURAUD/PHONG paths.  If a scene has no lights at all, surfaces
use their raw material colour at full brightness; add ambient light when you
want visible fill in shaded areas.

## Desktop performance

The Python build enables the desktop tile rasterizer.  `render()` splits the
framebuffer into horizontal tiles, clears each tile and rasterizes it on a
persistent pool while the calling thread participates.  Object culling and
vertex transforms run once on the calling thread before the pool starts, so
tile workers only read the immutable render queue.  Output is deterministic:
the serial and threaded paths produce identical pixels and triangle counts.

The default pool size is adaptive and capped at eight participants to avoid
oversubscribing memory bandwidth.  A scene can override it:

```python
scene.thread_count = 0       # auto (default)
scene.thread_count = 4       # use four participants
scene.thread_count = 1       # classic single-threaded renderer
scene.threaded_rendering = False
```

In auto mode, small scenes and low resolutions stay on the serial path; the
pool is used when the framebuffer is large enough or the scene has many
triangles.  Reading `scene.thread_count` returns the active participant count.
A `Scene` is not re-entrant: serialize rendering and framebuffer reads when
using multiple Python threads.  Clear and opaque span fills use SSE2 on every
x86-64 build and AVX2 at runtime on GCC/Clang builds that support it.
Framebuffer readback is available in three native formats:

```python
raw565 = scene.get_framebuffer()          # 2 bytes per pixel, RGB565
rgb888 = scene.get_framebuffer_rgb888()   # 3 bytes per pixel, R G B
rgba   = scene.get_framebuffer_rgba8888() # 4 bytes per pixel, R G B 0xFF
```

`framebuffer_to_rgb()` and `framebuffer_to_rgba()` convert an existing RGB565
byte string in native code, so no Python loop is needed when saving images.

## Python demos

The repository contains runnable, dependency-free demos:

| Demo | Description |
| --- | --- |
| [`examples/python/triangle.py`](examples/python/triangle.py) | Smallest possible render; writes `triangle.ppm`. |
| [`examples/python/lit_cube.py`](examples/python/lit_cube.py) | Depth-tested cube with directional, ambient and point lights; `--smooth` demonstrates authored vertex normals. |
| [`examples/python/orbit.py`](examples/python/orbit.py) | A rotating-cube frame sequence with the same lighting rig. |

```bash
python examples/python/triangle.py
python examples/python/lit_cube.py --output lit_cube.ppm
python examples/python/lit_cube.py --smooth --output lit_cube_smooth.ppm
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
- **Mesh** — vertices, triangle indices, optional per-vertex normals,
  transforms, culling, blending and depth flags.  Geometry is copied into the
  scene at `addMesh()` time.
- **Material** — RGB565 colour, alpha, diffuse/specular/emissive properties and
  all shading modes.
- **Lights** — directional, ambient and world-space point lights, all evaluated
  by the built-in lit shading modes.
- **Helpers** - native `framebuffer_to_rgb()` / `framebuffer_to_rgba()` and the
  small `Vector3`, `Color` value types.

Textures, custom shaders, sprites, particles, post-processing, picking and
native primitive/OBJ helpers exist in the C++ engine but are not part of the
current Python API.

## How it works

Jet is a fixed-function rasteriser.  It transforms vertices with integer
fixed-point maths, clips triangles, rasterises them with an optional depth
buffer, and shades them using material and light data.  The Python build is
configured for full-resolution desktop output with depth buffering, lighting
and the tile thread pool enabled; the embedded C++ build can be trimmed for
ESP32 and similar targets.  Object-level frustum culling runs before vertex
transforms, so invisible geometry never enters the render queue.

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
