from pathlib import Path

import pyjet3d


def test_primary_package_and_version():
    assert pyjet3d.Scene is not None
    assert pyjet3d.Camera is not None
    assert pyjet3d.Mesh is not None
    assert pyjet3d.PIXEL_FORMAT == "RGB565"

    version_file = Path(__file__).resolve().parents[1] / "version.txt"
    expected = version_file.read_text(encoding="utf-8").strip()
    assert expected.startswith("v")
    assert pyjet3d.__version__ == expected[1:]


def test_culling_modes_are_exposed():
    assert pyjet3d.CullingMode.CULL_BACKFACES is not None
    assert pyjet3d.CullingMode.NO_CULLING is not None


def test_light_types_are_exposed():
    assert pyjet3d.DirectionalLight is not None
    assert pyjet3d.AmbientLight is not None
    assert pyjet3d.PointLight is not None
