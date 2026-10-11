import pyjet


def _has_non_background_pixel(framebuffer: bytes, background: int = 0x0000) -> bool:
    for i in range(0, len(framebuffer), 2):
        if int.from_bytes(framebuffer[i:i + 2], "little") != background:
            return True
    return False


def test_scene_add_mesh_from_vertices_renders_triangle():
    scene = pyjet.Scene(320, 240)
    cam = pyjet.Camera()
    cam.setPosition(0, 0, -500)
    cam.setFOV(70)
    scene.setCamera(cam)

    material = pyjet.Material(0xF800)
    material.shadingMode = pyjet.ShadingMode.FLAT
    scene.addMesh([(0, 120, 0), (-120, -120, 0), (120, -120, 0)], material)
    scene.render()

    framebuffer = scene.get_framebuffer()
    assert _has_non_background_pixel(framebuffer)


def test_scene_add_mesh_object_with_indices():
    scene = pyjet.Scene(320, 240)
    cam = pyjet.Camera()
    cam.setPosition(0, 0, -500)
    cam.setFOV(70)
    scene.setCamera(cam)

    material = pyjet.Material(0x07E0)
    material.shadingMode = pyjet.ShadingMode.FLAT
    mesh = pyjet.Mesh(
        [(0, 120, 0), (-120, -120, 0), (120, -120, 0)],
        [(0, 1, 2)],
    )
    scene.addMesh(mesh, material)
    scene.render()

    framebuffer = scene.get_framebuffer()
    assert _has_non_background_pixel(framebuffer)
