# PyJet Python API

**English** | [Русский](Python_RU.md)

`pyjet3d` is a Python binding for [Jet](https://github.com/CubeCoders/Jet),
a dependency-free software rasteriser written in C++17.  It renders triangles
on the CPU into an RGB565 framebuffer, so it does not need OpenGL, Vulkan, a
GPU or a windowing library.

This guide covers the Python-facing API.  The native engine has many more
features; see [Engine documentation](#engine-documentation) at the end.

## Contents

- [Installation](#installation)
- [Quick start](#quick-start)
- [Coordinates and units](#coordinates-and-units)
- [Scene lifecycle](#scene-lifecycle)
- [Camera](#camera)
- [Meshes and winding](#meshes-and-winding)
- [Materials and shading](#materials-and-shading)
- [Lights](#lights)
- [Framebuffer and images](#framebuffer-and-images)
- [Animation](#animation)
- [API reference](#api-reference)
- [Limits and troubleshooting](#limits-and-troubleshooting)
- [Demos](#demos)
- [Engine documentation](#engine-documentation)

## Installation

```bash
pip install pyjet3d
```

Requirements:

- Python 3.9 or newer.
- A platform for which a wheel is published (Linux, Windows or macOS), or a
  C++17 compiler when building from source.
- No runtime dependencies beyond the Python standard library.

```python
import pyjet3d

print(pyjet3d.__version__)
print(pyjet3d.PIXEL_FORMAT)  # RGB565
```

> **Migrating from 0.2:** the import is now `pyjet3d`, not `pyjet`.  The
> package was renamed to avoid a collision with the unrelated `pyjet`
> distribution on PyPI.  Replace `import pyjet` with `import pyjet3d`.

## Quick start

```python
import pyjet3d

# 1. Create a full-resolution RGB565 scene.
scene = pyjet3d.Scene(320, 240)

# 2. Put the camera in front of the geometry and point it at the origin.
camera = pyjet3d.Camera()
camera.setPosition(0, 0, -500)
camera.setFOV(70, scene.width)  # FOV in degrees; width sets projection scale
camera.nearPlane = 32
camera.farPlane = 4096
scene.setCamera(camera)

# 3. Create a material.  Material colours use RGB565, not 8-bit RGB.
material = pyjet3d.Material(0xF800)  # bright red
material.shadingMode = pyjet3d.ShadingMode.FLAT

# 4. Add a triangle.  Counter-clockwise winding is front-facing.
scene.addMesh(
    [(0, 120, 0), (120, -120, 0), (-120, -120, 0)],
    material,
)

# 5. Render synchronously.
scene.render()

# 6. Read the framebuffer.  It contains width * height little-endian uint16
#    RGB565 values, exactly as returned by get_framebuffer().
framebuffer = scene.get_framebuffer()
assert len(framebuffer) == scene.width * scene.height * 2

# 7. Convert to RGB888 and save a dependency-free PPM image.
rgb = pyjet3d.framebuffer_to_rgb(framebuffer, scene.width, scene.height)
with open("triangle.ppm", "wb") as out:
    out.write(b"P6\n%d %d\n255\n" % (scene.width, scene.height))
    out.write(rgb)
```

The scene is rendered in software and the framebuffer is complete when
`render()` returns.  There is no swap chain or window: your application decides
where to display, encode or upload the pixels.

## Coordinates and units

- World-space units are arbitrary.  The examples use hundreds of units for a
  cube because the default camera clipping planes are in the same range.
- Mesh vertex coordinates are converted to integer world units when the mesh
  is created; non-integral values are rounded.
- The default camera is at `(0, 0, 0)` and looks along `+Z`.  A common setup is
  a camera at `(0, 0, -500)` looking at geometry around the origin.
- `+Y` points up and `+X` points right on screen.
- `x`, `y` and `z` rotations are Euler angles in degrees.
- `nearPlane` and `farPlane` are view-space clipping distances.  Geometry
  closer than `nearPlane` or farther than `farPlane` is clipped.
- The framebuffer row `y = 0` is the top row.  `get_pixel(x, y)` uses the same
  convention.

The default near/far values are `128` and `1024`.  Set them explicitly when
your scene uses a different scale; a cube of radius 120 easily fits, but a
large world may not.

## Scene lifecycle

A `Scene` owns its colour and depth buffers.  The Python build uses:

- a full-resolution RGB565 colour buffer (one `uint16_t` per pixel);
- a depth buffer (Z-buffering is enabled);
- the lighting path;
- one directional light slot and one ambient light slot;
- any number of world-space point lights evaluated by the built-in
  FLAT/GOURAUD/PHONG paths.

A typical frame is:

```python
scene.setBackcolor(0x0841)  # RGB565 clear colour
scene.render()

framebuffer = scene.get_framebuffer()
```

`setClearBuffer(False)` keeps the previous framebuffer contents between frames;
this can be useful for overlays or incremental drawing.  The default is `True`.

`Scene.render()` raises `RuntimeError` if no camera has been set.

## Camera

```python
camera = pyjet3d.Camera()
camera.setPosition(0, 0, -500)
camera.setRotation(0, 180, 0)     # degrees, X=pitch, Y=yaw, Z=roll
camera.lookAt(0, 0, 0)             # convenient alternative to manual rotation
camera.nearPlane = 32
camera.farPlane = 4096
camera.setFOV(60, scene.width)
scene.setCamera(camera)
```

- `setFOV(fov, screen_width=320)` sets the field of view in degrees and the
  width used for the projection scale.  The renderer uses `screen_width` for
  the horizontal projection; vertical coverage follows the framebuffer aspect
  ratio.  The valid range is `0 < fov < 180`.
- `Scene.setCamera()` and `Scene.render()` keep the projection aligned with the
  scene width.  You can call `setFOV(60)` and let the scene fill in the width.
- `position` and `rotation` are tuple properties as well as method-based
  setters:
  ```python
  camera.position = (0, 0, -600)
  camera.rotation = (0, 0, 0)
  print(camera.position, camera.rotation, camera.fov)
  ```
- `lookAt()` sets yaw and pitch to face a world-space point and resets roll.
- `Scene.setCamera()` stores a reference to the camera; keep the camera alive
  for the lifetime of the scene.  The binding also keeps it alive
  automatically while the scene exists.

For a camera looking along `+Z`, increasing yaw turns it toward `+X` (depending
on the sign convention of your scene), while pitch rotates it up or down.  If
an object is not visible, `lookAt()` plus a camera on the `-Z` side is the
quickest sanity check.

## Meshes and winding

A `Mesh` is a collection of vertices and triangles that is copied into the scene
when it is added:

```python
mesh = pyjet3d.Mesh(
    [
        (-100, -100, 0),
        (-100, 100, 0),
        (100, 100, 0),
        (100, -100, 0),
    ],
    [(0, 1, 2), (0, 2, 3)],
)

mesh.setPosition(0, 0, 0)
mesh.setRotation(0, 35, 0)   # degrees
mesh.setScale(1.0, 1.0, 1.0)  # absolute local-space scale
scene.addMesh(mesh, material)
```

Constructor rules:

- At least three vertices are required.
- If `indices` is omitted or empty, a three-vertex mesh becomes one triangle;
  a larger mesh is triangulated as a fan from vertex 0.  This is convenient for
  convex polygons, but explicit triangle indices are clearer for complex
  geometry.
- Indices are 16-bit; a single mesh supports at most 65,536 vertices.
- Out-of-range indices raise `ValueError`/`IndexError` instead of silently
  reading invalid geometry.
- `normals` is optional.  Supply exactly one normal per vertex to control
  lighting explicitly; omit it to compute one normal per face automatically.

### Normals and smooth shading

By default, each triangle gets its own face normal.  Indexed meshes may share
vertices, so the binding splits a shared vertex into one copy per distinct
face normal.  Coplanar triangles keep sharing vertices, while a cube corner
becomes three vertices with the correct normal for each face.  This is what
makes FLAT, GOURAUD and PHONG shading agree on low-poly models.  The split can
increase the internal vertex count; a mesh that expands beyond 65,536 vertices
raises `ValueError`.

For smooth GOURAUD/PHONG shading, pass authored normals:

```python
# Unit-length normals, one per vertex.  They do not need to be normalised by
# the caller; the binding normalises and converts them to fixed point.
smooth_quad = pyjet3d.Mesh(
    [(-80, -80, 0), (-80, 80, 0), (80, 80, 0), (80, -80, 0)],
    [(0, 1, 2), (0, 2, 3)],
    [(0, 0, -1), (0, 0, -1), (0, 0, -1), (0, 0, -1)],
)
```

Normals must be finite and non-zero.  A smooth normal may point in any
direction; it is not re-derived from the triangle winding.  FLAT shading still
uses the first vertex normal of each triangle, so use GOURAUD or PHONG when you
want a smooth field.  `setScale()` applies the inverse-transpose factor to
custom normals and requires all three scale components to be non-zero;
automatically generated normals are simply recomputed for the scaled
geometry.

Transforms:

- `setPosition(x, y, z)` sets the object position.
- `setRotation(x, y, z)` sets absolute Euler rotation in degrees.
- `setScale(x, y, z)` scales the **original local vertices** and recomputes
  automatic face normals (or re-applies the inverse-transpose transform to
  authored normals).  It is absolute, not cumulative: calling it twice with
  the same value produces the same geometry.
- `cullingMode` defaults to `CullingMode.CULL_BACKFACES`.
- `blendMode` selects the blend operation for the object.
- `ignoreZBuffer` draws without depth testing; `noWriteZBuffer` writes colour
  but not depth; `enabled=False` skips the object.

### Winding and culling

The front face is **counter-clockwise when viewed from the camera**.  The
minimal front-facing triangle is:

```python
scene.addMesh(
    [(0, 120, 0), (120, -120, 0), (-120, -120, 0)],
    material,
)
```

If a mesh is invisible, its winding may be reversed.  For imported or
double-sided geometry, use:

```python
mesh.cullingMode = pyjet3d.CullingMode.NO_CULLING
```

`CULL_FRONTFACES` is useful for showing the inside of a room or a hollow
object.  Disable culling for flat decals that need to be visible from both
sides.

`Scene.addMesh()` **copies** the mesh geometry into the scene.  Changing a
`Mesh` after adding it does not move or re-light the scene copy.  Create a new
mesh and rebuild the scene for animated object transforms (see
[Animation](#animation)).  A `Material` is referenced by the scene rather than
copied, so changes to that material are visible on subsequent renders.

## Materials and shading

`Material.color` and `Scene.setBackcolor()` use **RGB565** (`0xF800` is red),
while `Color`, used by lights, stores 8-bit components.

```python
# RGB565 from 8-bit channels, if you prefer to author colours that way.
def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

material = pyjet3d.Material(rgb565(220, 60, 40))
material.alpha = 255
material.diffuse = 255
material.specular = 0
material.emissive = False
material.shadingMode = pyjet3d.ShadingMode.FLAT
```

Alpha can also be supplied in the constructor:

```python
translucent = pyjet3d.Material(0xF800, 128)
```

Shading modes:

| Mode | Use |
| --- | --- |
| `ShadingMode.FLAT` | One colour per triangle.  Fast and good for low-poly work. |
| `ShadingMode.GOURAUD` | Lighting is evaluated at vertices and interpolated. |
| `ShadingMode.PHONG` | Per-pixel lighting; the smoothest result. |
| `ShadingMode.WIREFRAME` | Draw triangle edges only. |
| `ShadingMode.UNLIT` | Use the raw material colour, ignoring lights. |
| `ShadingMode.WATER_REFLECT` | The engine's animated water-reflection surface mode. |
| `ShadingMode.ADDITIVE` | Additive glow/neon style; lighting is bypassed. |

The Python build does not expose textures or custom shaders, so material
`color` is the visible surface colour unless a special shading mode is used.
`emissive=True` and `UNLIT` are the simplest ways to make a bright object that
does not react to lights.

`diffuse`, `specular`, `specularExponent`, `perspectiveCorrect`, `waterYBias`
and `waterReflectionMaxY` are exposed for engine-compatible material setup.
The most useful properties for ordinary Python scenes are `color`, `alpha`,
`emissive` and `shadingMode`.

## Lights

Lights are created as Python objects and passed to the scene.  The scene keeps
references to them.

```python
sun = pyjet3d.DirectionalLight(
    azimuth=225,
    elevation=45,
    r=255,
    g=241,
    b=214,
    intensity=255,
)
scene.setDirectionalLight(sun)

ambient = pyjet3d.AmbientLight(100, 104, 120)
scene.setAmbientLight(ambient)

# A warm point light in world space.  Full intensity inside rangeMin, a linear
# fade to zero at rangeMax.
lamp = pyjet3d.PointLight(
    x=-220, y=-80, z=-280,
    r=255, g=190, b=120,
    rangeMin=150, rangeMax=900,
)
scene.addPointLight(lamp)
```

- `DirectionalLight(azimuth=225, elevation=45, r=255, g=255, b=255,
  intensity=255)`: an infinite-distance light.  `direction` stores
  `(azimuth, elevation, 0)` in degrees.  If you assign `light.direction`
  directly, call `light.updateDirection(azimuth, elevation)` to recompute the
  derived vector.  `color` tints the directional contribution per channel and
  `intensity` scales it.
- `AmbientLight(r=32, g=32, b=32)`: a constant per-channel ambient tint.  It
  is added after the lit terms and does not cast shadows.
- `PointLight(x, y, z, r=255, g=255, b=255, rangeMin=0, rangeMax=255)`:
  a world-space diffuse light.  The contribution is full inside `rangeMin`,
  fades linearly to zero at `rangeMax`, and is absent outside `rangeMax`.
  Set `rangeMax <= rangeMin` (for example both to zero) to disable distance
  attenuation.  Set `light.enabled = False` to switch it off.

The built-in FLAT, GOURAUD and PHONG paths evaluate directional, ambient and
point lights.  Directional lighting also contributes a view-facing specular
term when `material.specular` is non-zero, and PHONG adds a per-pixel
`specularExponent` highlight.  Point lights are diffuse-only and do not add
specular highlights.  Point lights use a squared Lambert term.  They are
evaluated per pixel for GOURAUD/PHONG (including smooth custom normals) and
once per triangle for FLAT.  For a point-light-only scene, add a small ambient
term if you do not want surfaces outside the light range to be black.

When the scene has **no light sources at all**, surfaces use their raw material
colour at full brightness for backwards compatibility.  As soon as any
directional, ambient or enabled point light exists, surfaces outside its
influence become black unless ambient light fills them.  `UNLIT`,
`emissive=True`, `ADDITIVE` and custom shaders bypass the built-in lighting
path.

## Framebuffer and images

The framebuffer is RGB565, one little-endian `uint16_t` per pixel:

```python
raw = scene.get_framebuffer()
assert len(raw) == scene.width * scene.height * 2
pixel = scene.get_pixel(160, 120)  # integer RGB565 value
print("0x%04X" % pixel)
```

`pyjet3d.framebuffer_to_rgb(raw, width, height)` converts it to packed RGB888
bytes.  The length of `raw` must be exactly `width * height * 2`.

Write a PPM without third-party packages:

```python
def save_ppm(path, scene):
    rgb = pyjet3d.framebuffer_to_rgb(
        scene.get_framebuffer(), scene.width, scene.height
    )
    with open(path, "wb") as out:
        out.write(b"P6\n%d %d\n255\n" % (scene.width, scene.height))
        out.write(rgb)
```

Or hand the RGB bytes to Pillow:

```python
from PIL import Image

rgb = pyjet3d.framebuffer_to_rgb(
    scene.get_framebuffer(), scene.width, scene.height
)
Image.frombytes("RGB", (scene.width, scene.height), rgb).save("frame.png")
```

Native 24/32-bit readback avoids the intermediate Python loop entirely.  These
methods convert the current RGB565 buffer inside the extension:

```python
rgb = scene.get_framebuffer_rgb888()       # 3 bytes per pixel, R G B
rgba = scene.get_framebuffer_rgba8888()    # 4 bytes per pixel, R G B 0xFF
```

`framebuffer_to_rgba(raw, width, height)` performs the same native conversion
on an existing RGB565 `bytes` object.  Each pixel is returned in `R, G, B,
0xFF` byte order, ready for Pillow `RGBA` images or a true RGBA surface.

Useful scene properties and methods:

- `scene.width`, `scene.height`: output dimensions.
- `scene.pixel_format`: the string `"RGB565"`.
- `scene.frame_counter`: number of completed renders.
- `scene.last_frame_drawn_objects`: objects submitted in the last frame.
- `scene.last_frame_drawn_triangles`: triangles submitted in the last frame.
- `scene.last_frame_rasterized_triangles`: triangles that survived clipping
  and reached the rasteriser.
- `scene.thread_count`: write `0` for auto or `1` for serial; reading returns
  the active participant count (including the calling thread).
- `scene.threaded_rendering`: whether the internal tile pool is enabled.
- `scene.get_statistics()`: returns `(objects, triangles, vertices)` for the
  current scene contents.

## Desktop performance

The Python configuration enables the desktop tile rasterizer.  A render is
split into horizontal tiles; each tile clears its rows and rasterizes the
immutable render queue on a persistent pool.  Object-level frustum culling and
vertex transforms happen before the pool starts, so hidden objects never reach
the transform loop or the queue.  Tile boundaries never affect the result:
serial and threaded rendering produce identical framebuffers and triangle
counts.

The default participant count is adaptive and capped at eight to avoid
oversubscribing memory bandwidth.  Override it when a frame has a different
balance of clear and geometry work:

```python
scene.thread_count = 0       # auto (default)
scene.thread_count = 4       # four participants, including the caller
scene.thread_count = 1       # force the classic serial path
scene.threaded_rendering = False
```

In auto mode, small framebuffers and small triangle queues stay serial.  The
pool is created lazily, reused across frames, and the GIL is released while
`render()` runs, so other Python threads can make progress.  A `Scene` is not
re-entrant: serialize `render()` and framebuffer/readback calls when using
multiple Python threads.  x86-64 builds use SSE2 span fills/clears and
runtime-dispatched AVX2 on GCC/Clang; other architectures use portable scalar
fallbacks.

## Animation

Because `Scene.addMesh()` copies geometry, object transforms are baked at add
time.  The simplest animation loop rebuilds a small scene every frame:

```python
for frame in range(60):
    scene = pyjet3d.Scene(320, 240)

    camera = pyjet3d.Camera()
    camera.setPosition(0, 0, -700)
    camera.setFOV(55, scene.width)
    camera.nearPlane = 32
    camera.farPlane = 4096
    camera.lookAt(0, 0, 0)
    scene.setCamera(camera)

    light = pyjet3d.DirectionalLight(azimuth=225, elevation=45)
    scene.setDirectionalLight(light)
    scene.setAmbientLight(pyjet3d.AmbientLight(80, 84, 100))

    material = pyjet3d.Material(0xF800)
    material.shadingMode = pyjet3d.ShadingMode.PHONG
    mesh = make_cube()  # returns a pyjet3d.Mesh
    mesh.setRotation(0, frame * 6, 0)
    scene.addMesh(mesh, material)

    scene.render()
    save_ppm("frame_%03d.ppm" % frame, scene)
```

Rebuilding is cheap for the low-poly scenes this API targets.  If only the
camera moves, you can keep the scene and mutate the camera instead; the scene
keeps a reference to it.

`examples/python/orbit.py` contains a complete rotating-cube sequence.

## API reference

### `pyjet3d.Scene`

```python
Scene(width=320, height=240)
```

| Member | Description |
| --- | --- |
| `width`, `height` | Read-only output dimensions in pixels. |
| `pixel_format` | Read-only; always `"RGB565"`. |
| `setCamera(camera)` | Set the active camera.  The scene keeps a reference. |
| `setDirectionalLight(light)` | Set the single directional light (or replace it). |
| `setAmbientLight(light)` | Set the single ambient light. |
| `addPointLight(light)` | Add a world-space point light; the scene keeps a reference. |
| `setBackcolor(color)` | Set the RGB565 clear colour. |
| `setClearBuffer(clear)` | Enable/disable clearing the colour buffer each render. |
| `setThreadCount(count)` | Set raster participants: `0` auto, `1` serial, `>1` explicit. |
| `thread_count` | Read/write participant count (calling thread included). |
| `threaded_rendering` | Read/write tile-pool switch. |
| `addMesh(mesh, material)` | Copy a `Mesh` into the scene with a material. |
| `addMesh(vertices, material, indices=[], normals=[])` | Build and add a mesh from Python sequences; optional per-vertex normals enable smooth shading. |
| `render()` | Render one frame.  Raises if no camera is set. |
| `get_framebuffer()` | Return `width * height * 2` bytes of little-endian RGB565. |
| `get_framebuffer_rgb888()` | Return the current frame as packed RGB888 (3 bytes/pixel). |
| `get_framebuffer_rgba8888()` | Return the current frame as opaque RGBA8888 bytes (4 bytes/pixel). |
| `get_pixel(x, y)` | Return one RGB565 integer; raises `IndexError` out of range. |
| `get_statistics()` | Return `(objects, triangles, vertices)`. |
| `frame_counter`, `last_frame_*` | Read-only frame and submission counters. |

### `pyjet3d.Camera`

```python
Camera()
```

| Member | Description |
| --- | --- |
| `position` | Tuple `(x, y, z)` property. |
| `rotation` | Tuple `(x, y, z)` Euler angles in degrees. |
| `fov` | Read-only current FOV in degrees. |
| `nearPlane`, `farPlane` | Clipping distances; writable integers. |
| `setPosition(x, y, z)` | Set the camera position. |
| `setRotation(x, y, z)` | Set absolute Euler rotation in degrees. |
| `lookAt(x, y, z)` | Face a world-space target and reset roll. |
| `setFOV(fov, screen_width=320)` | Set FOV and projection width. |

### `pyjet3d.Mesh`

```python
Mesh(vertices, indices=[], normals=[])
```

| Member | Description |
| --- | --- |
| `setPosition(x, y, z)` | Set the object position. |
| `setRotation(x, y, z)` | Set absolute Euler rotation in degrees. |
| `setScale(x, y, z)` | Bake an absolute local scale into the vertices. |
| `normals` (constructor) | Optional per-vertex normals for smooth GOURAUD/PHONG lighting. |
| `cullingMode` | `CullingMode.CULL_BACKFACES` (default), `CULL_FRONTFACES` or `NO_CULLING`. |
| `blendMode` | One of the `BlendMode` values. |
| `ignoreZBuffer` | Draw without depth testing when `True`. |
| `noWriteZBuffer` | Do not update the depth buffer when `True`. |
| `enabled` | Skip this mesh when `False`. |

### `pyjet3d.Material`

```python
Material(color=0xFFFF)
Material(color, alpha)
```

| Member | Description |
| --- | --- |
| `color` | RGB565 base colour. |
| `alpha` | 0–255; 255 is opaque. |
| `diffuse`, `specular` | Lighting coefficients (0–255). |
| `emissive` | Bypass lighting and use the raw colour. |
| `specularExponent` | Per-pixel highlight control for `PHONG`. |
| `perspectiveCorrect` | Texture-coordinate option; only relevant to native textured surfaces. |
| `waterYBias`, `waterReflectionMaxY` | Options for `WATER_REFLECT`. |
| `shadingMode` | A `ShadingMode` value. |

### Lights and small value types

| Type | Description |
| --- | --- |
| `DirectionalLight(azimuth=225, elevation=45, r=255, g=255, b=255, intensity=255)` | Infinite light; its colour tints the directional diffuse/specular contribution.  `direction`, `color`, `intensity`, `updateDirection()`. |
| `AmbientLight(r=32, g=32, b=32)` | Constant ambient term.  `color`. |
| `PointLight(x, y, z, r=255, g=255, b=255, rangeMin=0, rangeMax=255)` | World-space diffuse point light with a linear range fade.  `position`, `color`, `rangeMin`, `rangeMax`, `enabled`. |
| `Color(r=255, g=255, b=255)` | 8-bit RGB value used by lights. |
| `Vector3(x=0, y=0, z=0)` | Integer world-space vector. |

### Enums

```python
pyjet3d.ShadingMode.FLAT
pyjet3d.ShadingMode.GOURAUD
pyjet3d.ShadingMode.PHONG
pyjet3d.ShadingMode.WIREFRAME
pyjet3d.ShadingMode.UNLIT
pyjet3d.ShadingMode.WATER_REFLECT
pyjet3d.ShadingMode.ADDITIVE

pyjet3d.CullingMode.CULL_BACKFACES
pyjet3d.CullingMode.CULL_FRONTFACES
pyjet3d.CullingMode.NO_CULLING

pyjet3d.BlendMode.BLEND_REPLACE
pyjet3d.BlendMode.BLEND_ADD
pyjet3d.BlendMode.BLEND_SUBTRACT
pyjet3d.BlendMode.BLEND_SCALE
pyjet3d.BlendMode.BLEND_AVERAGE
pyjet3d.BlendMode.BLEND_XOR
```

The enum values are also available directly from the module because they were
exported with `export_values()`; the namespaced form is clearer in application
code.

## Limits and troubleshooting

- **Nothing is visible:** check camera position and clipping planes, triangle
  winding, and whether the material is black.  Try
  `mesh.cullingMode = pyjet3d.CullingMode.NO_CULLING`.
- **The image is upside down or mirrored:** `get_pixel(x, 0)` is the top row;
  write PPM/Pillow data in row-major order as shown in the examples.
- **The mesh does not move:** `Scene.addMesh()` copies the mesh.  Re-add a new
  mesh or rebuild the scene after changing its transform.
- **The material changes do not show:** materials are referenced, but the scene
  still needs another `render()` after the change.
- **Everything is dark:** add `AmbientLight` and/or a directional light, and
  check that the light is not behind all visible faces.  For point lights,
  check `rangeMax` against the distance from the light to the surface and make
  sure the triangle normal faces the light.  `UNLIT`, `emissive=True` and
  `ADDITIVE` bypass lighting.
- **A cube has one dark face:** meshes are lit with face normals by default.
  Indexed geometry is split per face normal automatically, so FLAT and GOURAUD
  should agree on a planar face.  If you want a rounded, smooth surface, pass
  explicit per-vertex normals and use `GOURAUD` or `PHONG`.
- **A large scene is clipped:** increase `camera.farPlane`; decrease
  `nearPlane` for close objects.  Keep in mind that the renderer is a software
  rasteriser and very large framebuffers are expensive.
- **Textures or sprites are missing:** they are native C++ engine features not
  exposed by the current Python bindings.

## Demos

The repository includes dependency-free Python demos:

| Demo | What it shows |
| --- | --- |
| [`examples/python/triangle.py`](../examples/python/triangle.py) | Minimal render and PPM output. |
| [`examples/python/lit_cube.py`](../examples/python/lit_cube.py) | A depth-tested cube with directional, ambient and point lights; `--smooth` demonstrates authored normals. |
| [`examples/python/orbit.py`](../examples/python/orbit.py) | A short rotating-cube frame sequence with the same lighting rig. |

Run them from the repository root:

```bash
python examples/python/triangle.py
python examples/python/lit_cube.py --output lit_cube.ppm
python examples/python/lit_cube.py --smooth --output lit_cube_smooth.ppm
python examples/python/orbit.py --frames 24 --output-dir orbit_frames
```

The demos only require `pyjet3d` and the standard library.  See
[`examples/python/README.md`](../examples/python/README.md) for options and
[`README_RU.md`](../examples/python/README_RU.md) for the Russian version.

## Engine documentation

The Python package is a thin binding over the Jet engine.  The native
documentation remains useful for understanding the renderer and for extending
the bindings:

- [Official API reference](https://cubecoders.github.io/Jet/)
- [ESP32 performance notes](ESP32Performance.md)
- [Instancing](instancing.md)
- [Solid rectangles](SolidRectangles.md)
- [Tiled textures](TiledTextures.md)

Jet is developed by [CubeCoders](https://cubecoders.com).  It is distributed
under the MIT License; see [`LICENSE`](../LICENSE).
