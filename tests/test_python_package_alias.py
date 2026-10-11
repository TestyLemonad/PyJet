import pyjet
import pyjet3d


def test_pyjet3d_alias_matches_pyjet_api():
    assert pyjet.Scene is pyjet3d.Scene
    assert pyjet.Camera is pyjet3d.Camera
    assert pyjet.Material is pyjet3d.Material
    assert pyjet.ShadingMode is pyjet3d.ShadingMode
