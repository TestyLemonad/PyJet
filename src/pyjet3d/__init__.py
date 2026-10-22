"""Python bindings for the Jet 3D software rasteriser."""

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
    framebuffer_to_rgb,
    framebuffer_to_rgba,
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
    "framebuffer_to_rgba",
    "PIXEL_FORMAT",
]
