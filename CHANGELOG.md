# Changelog

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

### Added

- Full-resolution depth-buffered Python configuration with directional and
  ambient lighting.
- Camera rotation, look-at, near/far planes and position/rotation properties.
- `Vector3`, `Color`, `DirectionalLight`, `AmbientLight` and `PointLight`.
- Material alpha/diffuse/specular/emissive properties and all shading modes.
- `CullingMode` and `BlendMode`; per-mesh culling/blend/visibility flags.
- `Scene.get_pixel()`, dimensions, frame counters, statistics and backcolor.
- `pyjet3d.framebuffer_to_rgb()` helper.
- CI wheel tests via `cibuildwheel`'s `test-command`.

### Changed

- Primary import renamed from `pyjet` to `pyjet3d` to avoid a top-level package
  collision with the unrelated `pyjet` distribution on PyPI.
- Version bumped to 0.3.0.
