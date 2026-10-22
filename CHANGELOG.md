# Changelog

## 0.3.1

### Added

- Desktop tile rasterizer with a persistent `std::thread` pool. `Scene.render()`
  splits the framebuffer into horizontal tiles, clears and rasterizes them in
  parallel, and keeps object culling/vertex transforms ahead of the pool.
- Runtime `Scene.thread_count` / `Scene.threaded_rendering` controls. Auto mode
  is adaptive, capped at eight participants, and small frames stay serial.
- SIMD RGB565 span-fill and clear kernels: SSE2 on every x86-64 build, AVX2
  with runtime dispatch on GCC/Clang, and scalar fallbacks elsewhere.
- Native RGB888 and RGBA8888 framebuffer readback via
  `Scene.get_framebuffer_rgb888()` and `Scene.get_framebuffer_rgba8888()`.
- Native `framebuffer_to_rgb()` / `framebuffer_to_rgba()` module helpers.
- Python performance regression tests for native colour conversion, threaded
  pixel equivalence, thread controls, and culling before submission.
- `JET_THREADED_RASTER`, `JET_RASTER_TILE_HEIGHT` and
  `JET_MAX_RASTER_THREADS` configuration knobs.

### Changed

- `Scene.prepareFrame()` accepts an optional `clearNow` argument so the
  threaded renderer can fold clearing into each tile.
- `Scene.rasterizeBand()` accepts optional water-reflection pass flags and uses
  atomic byte markers for shared per-triangle statistics.
- The GIL is released while `Scene.render()` and native framebuffer conversion
  run.
- Python README and API guides document native 24/32-bit output, the tile pool
  and the early-culling order.

## 0.3.0

### Fixed

- Python framebuffer now uses full resolution instead of the embedded
  half-width/field-buffer layout.
- Framebuffer and depth buffer are zero-initialised and validated, so
  `get_framebuffer()` no longer exposes uninitialised heap memory.
- Corrected the README/example triangle winding and made the tests check real
  rendered pixels instead of arbitrary bytes in the buffer.
- `Scene.setCamera()` and `Scene.addMesh()` now keep Python camera/material
  objects alive while the scene references them.
- `Mesh.setScale()` now bakes an absolute scale into the vertices instead of
  setting an unused engine field.
- `Camera.setFOV()` now accepts the screen width; `Scene.setCamera()`
  automatically recalculates it for the scene dimensions.
- Added sdist header inclusion via `MANIFEST.in`.
- Removed generated `.so`, `.pyc` and `*.egg-info` files from the repository.
- Fixed indexed meshes corrupting shared-vertex normals: vertices are now split
  only when incident triangles have different face normals, so FLAT, GOURAUD
  and PHONG agree on cube faces and other low-poly geometry.
- Fixed directional-light colour being ignored by the diffuse term.  Light
  colour now tints FLAT, GOURAUD and PHONG diffuse lighting per channel.
- Fixed point lights being stored but never sampled.  The built-in lit paths
  now evaluate view-space point lights with squared-Lambert diffuse lighting,
  per-channel colour and linear `rangeMin`/`rangeMax` attenuation.
- Disabled the object-local directional precompute while point lights are
  active so view-space point lighting remains correct.

### Added

- Full-resolution depth-buffered Python configuration with directional,
  ambient and point lighting.
- Optional per-vertex normals for `Mesh` and `Scene.addMesh()`, enabling smooth
  GOURAUD/PHONG shading while default meshes retain automatic per-face normals.
- `JET_POINT_LIGHTS` feature flag: enabled for the Python build, off by default
  in the embedded/desktop config templates.
- Lighting regression tests covering shared cube vertices, custom normals,
  directional color, ambient-only scenes and point-light range falloff.
- Camera rotation, look-at, near/far planes and position/rotation properties.
- `Vector3`, `Color`, `DirectionalLight`, `AmbientLight` and `PointLight`.
- Material alpha/diffuse/specular/emissive properties and all shading modes.
- `CullingMode` and `BlendMode`; per-mesh culling/blend/visibility flags.
- `Scene.get_pixel()`, dimensions, frame counters, statistics and backcolor.
- `pyjet3d.framebuffer_to_rgb()` helper.
- CI wheel tests via `cibuildwheel`'s `test-command`.
- `version.txt` as the single release-version source.  A change to it on
  `main` builds wheels, creates a GitHub release with the wheels attached, and
  publishes to PyPI.
- Python-focused English and Russian documentation (`README.md`,
  `README_RU.md`, `docs/Python.md`, `docs/Python_RU.md`) and three runnable
  demos under `examples/python/`.

### Changed

- Primary import renamed from `pyjet` to `pyjet3d` to avoid a top-level package
  collision with the unrelated `pyjet` distribution on PyPI.
- Version bumped to 0.3.0.
