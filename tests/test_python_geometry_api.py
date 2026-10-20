import gc
import struct

import pytest

import pyjet3d

# Counter-clockwise when viewed from the camera at -Z: front-facing under the
# engine's default CULL_BACKFACES mode.
TRIANGLE = [(0, 120, 0), (120, -120, 0), (-120, -120, 0)]
BACKWARDS_TRIANGLE = [(0, 120, 0), (-120, -120, 0), (120, -120, 0)]


def make_scene(width=320, height=240):
    scene = pyjet3d.Scene(width, height)
    camera = pyjet3d.Camera()
    camera.setPosition(0, 0, -500)
    camera.setFOV(70)
    scene.setCamera(camera)
    return scene, camera


def make_material(color=0xF800):
    material = pyjet3d.Material(color)
    material.shadingMode = pyjet3d.ShadingMode.FLAT
    return material


def pixels(scene):
    framebuffer = scene.get_framebuffer()
    assert len(framebuffer) == scene.width * scene.height * 2
    return struct.unpack(
        "<{}H".format(scene.width * scene.height), framebuffer
    )


def count_color(scene, color):
    return sum(1 for value in pixels(scene) if value == color)


def test_scene_add_mesh_from_vertices_renders_triangle():
    scene, _ = make_scene()
    material = make_material()
    scene.addMesh(TRIANGLE, material)
    scene.render()

    assert count_color(scene, 0xF800) > 1000
    assert scene.get_pixel(0, 0) == 0


def test_scene_add_mesh_object_with_indices():
    scene, _ = make_scene()
    material = make_material(0x07E0)
    mesh = pyjet3d.Mesh(TRIANGLE, [(0, 1, 2)])
    scene.addMesh(mesh, material)
    scene.render()

    assert count_color(scene, 0x07E0) > 1000


def test_backface_winding_is_culled():
    scene, _ = make_scene()
    scene.addMesh(BACKWARDS_TRIANGLE, make_material())
    scene.render()

    assert count_color(scene, 0xF800) == 0


def test_no_culling_renders_both_windings():
    scene, _ = make_scene()
    mesh = pyjet3d.Mesh(BACKWARDS_TRIANGLE)
    mesh.cullingMode = pyjet3d.CullingMode.NO_CULLING
    scene.addMesh(mesh, make_material())
    scene.render()

    assert count_color(scene, 0xF800) > 1000


def test_scale_is_applied():
    base_scene, _ = make_scene()
    base_scene.addMesh(TRIANGLE, make_material())
    base_scene.render()
    base_area = count_color(base_scene, 0xF800)

    scaled_scene, _ = make_scene()
    mesh = pyjet3d.Mesh(TRIANGLE)
    mesh.setScale(2, 2, 2)
    scaled_scene.addMesh(mesh, make_material())
    scaled_scene.render()
    scaled_area = count_color(scaled_scene, 0xF800)

    assert base_area > 1000
    assert scaled_area > base_area * 2.0

    small_scene, _ = make_scene()
    small_mesh = pyjet3d.Mesh(TRIANGLE)
    small_mesh.setScale(0.5, 0.5, 0.5)
    small_scene.addMesh(small_mesh, make_material())
    small_scene.render()
    small_area = count_color(small_scene, 0xF800)

    assert small_area < base_area * 0.6


def test_fov_uses_scene_width():
    small_scene, _ = make_scene(320, 240)
    small_scene.addMesh(TRIANGLE, make_material())
    small_scene.render()
    small_area = count_color(small_scene, 0xF800)

    large_scene, large_camera = make_scene(640, 480)
    # Deliberately omit the width: render() must still use the scene width.
    large_camera.setFOV(70)
    large_scene.addMesh(TRIANGLE, make_material())
    large_scene.render()
    large_area = count_color(large_scene, 0xF800)

    assert small_area > 1000
    assert large_area > small_area * 2.5


def test_material_and_camera_stay_alive():
    def build_scene():
        scene = pyjet3d.Scene(320, 240)
        camera = pyjet3d.Camera()
        camera.setPosition(0, 0, -500)
        camera.setFOV(70)
        scene.setCamera(camera)

        material = make_material()
        scene.addMesh(TRIANGLE, material)
        return scene

    scene = build_scene()
    gc.collect()

    # Reuse freed heap blocks aggressively.  Without keep_alive the scene's
    # raw Material/Camera pointers would now be dangling.
    pressure = [pyjet3d.Material(0x001F) for _ in range(20000)]
    scene.render()

    assert count_color(scene, 0xF800) > 1000
    assert pressure


def test_framebuffer_starts_zero_initialized():
    scene, _ = make_scene()

    values = pixels(scene)
    assert len(values) == 320 * 240
    assert all(value == 0 for value in values)


def test_backcolor_and_full_framebuffer():
    scene, _ = make_scene()
    scene.setBackcolor(0x001F)
    scene.render()

    values = pixels(scene)
    assert len(values) == 320 * 240
    assert all(value == 0x001F for value in values)


def test_framebuffer_rgb_conversion():
    scene, _ = make_scene()
    scene.addMesh(TRIANGLE, make_material())
    scene.render()

    rgb = pyjet3d.framebuffer_to_rgb(
        scene.get_framebuffer(), scene.width, scene.height
    )
    assert len(rgb) == scene.width * scene.height * 3
    assert (255, 0, 0) in zip(rgb[0::3], rgb[1::3], rgb[2::3])


def test_get_pixel_bounds():
    scene, _ = make_scene()
    with pytest.raises(IndexError):
        scene.get_pixel(-1, 0)
    with pytest.raises(IndexError):
        scene.get_pixel(0, 240)


def test_scene_rejects_invalid_dimensions():
    with pytest.raises(ValueError):
        pyjet3d.Scene(0, 240)
    with pytest.raises(ValueError):
        pyjet3d.Scene(320, -1)


def test_lights_and_scene_statistics():
    scene, _ = make_scene()
    scene.setDirectionalLight(
        pyjet3d.DirectionalLight(azimuth=225, elevation=45)
    )
    scene.setAmbientLight(pyjet3d.AmbientLight(32, 32, 32))
    scene.addPointLight(pyjet3d.PointLight(0, 0, -400, 32, 32, 32, 0, 512))
    scene.addMesh(TRIANGLE, make_material())
    scene.render()

    objects, triangles, vertices = scene.get_statistics()
    assert objects == 1
    assert triangles == 1
    assert vertices == 3
    assert scene.last_frame_drawn_objects == 1
    assert scene.frame_counter == 1
