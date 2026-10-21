# Python demos

**English** | [Русский](README_RU.md)

These scripts render with `pyjet3d` and write images in the dependency-free
binary PPM format.  They use only the package and the Python standard library.

## Requirements

```bash
python -m pip install pyjet3d
```

The demos can be run from the repository root or from this directory.  From
the repository root:

```bash
python examples/python/triangle.py
python examples/python/lit_cube.py --output lit_cube.ppm
python examples/python/lit_cube.py --smooth --output lit_cube_smooth.ppm
python examples/python/orbit.py --frames 24 --output-dir orbit_frames
```

## Demos

### `triangle.py`

The smallest complete render: one front-facing triangle, one material, one
frame and a PPM file.

```bash
python examples/python/triangle.py --width 320 --height 240 --output triangle.ppm
```

### `lit_cube.py`

Creates a depth-tested cube with a camera, a warm directional light, ambient
fill and a world-space point light.  The cube is rotated 35 degrees by default;
use `--angle` to change the pose.  By default normals are generated per face.
Pass `--smooth` to supply authored vertex normals; with PHONG shading the cube
looks rounded and demonstrates the optional `normals` mesh argument.

```bash
python examples/python/lit_cube.py --width 480 --height 320 --angle 35 --output lit_cube.ppm
python examples/python/lit_cube.py --smooth --output lit_cube_smooth.ppm
```

### `orbit.py`

Renders a short sequence of numbered PPM frames with the cube rotating around
the Y axis and the same three-light rig.  The scene is rebuilt for each frame
because `Scene.addMesh()` copies the mesh geometry.  Add `--smooth` for authored
vertex normals.

```bash
python examples/python/orbit.py --frames 24 --width 320 --height 240 --output-dir orbit_frames
python examples/python/orbit.py --smooth --frames 24 --output-dir orbit_smooth
```

Convert the sequence to a GIF or video with `ffmpeg`:

```bash
ffmpeg -framerate 24 -i orbit_frames/frame_%03d.ppm orbit.gif
```

## Viewing PPM files

Most image viewers do not open PPM files.  Convert them with Pillow:

```python
from PIL import Image

Image.open("lit_cube.ppm").save("lit_cube.png")
```

or with ImageMagick:

```bash
magick lit_cube.ppm lit_cube.png
```

## Files

- `demo_utils.py` — PPM output, RGB565 helper, camera setup, the shared
  directional/ambient/point-light rig and cube geometry (flat or smooth).
- `triangle.py` — minimal example.
- `lit_cube.py` — lighting and transforms, with optional authored normals.
- `orbit.py` — frame sequence loop with the same lighting.

The demos intentionally avoid a window and event loop.  A real application can
upload the framebuffer to a display, a texture, a video encoder or a web
endpoint after `scene.render()`.
