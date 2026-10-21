"""Pixel-level lighting regression tests.

These tests deliberately assert rendered output rather than just checking that
the API stores a light.  Lighting used to look unpredictable on indexed meshes
because shared cube vertices had their normals overwritten by the last triangle
that touched them; the tests below cover that case along with directional
colour, point lights and explicit per-vertex normals.
"""

import pytest

import pyjet3d


TRIANGLE = [(0, 120, 0), (120, -120, 0), (-120, -120, 0)]


def make_scene(width=320, height=240):
    scene = pyjet3d.Scene(width, height)
    camera = pyjet3d.Camera()
    camera.setPosition(0, 0, -500)
    camera.setFOV(70, width)
    camera.nearPlane = 32
    camera.farPlane = 4096
    camera.lookAt(0, 0, 0)
    scene.setCamera(camera)
    return scene


def material(color=0x2104, shading=pyjet3d.ShadingMode.FLAT):
    result = pyjet3d.Material(color)
    result.shadingMode = shading
    return result


def cube_mesh(size=260):
    s = size / 2
    vertices = [
        (-s, -s, -s),
        (s, -s, -s),
        (s, s, -s),
        (-s, s, -s),
        (-s, -s, s),
        (s, -s, s),
        (s, s, s),
        (-s, s, s),
    ]
    faces = [
        (0, 3, 2), (0, 2, 1),  # -Z, facing the camera
        (4, 5, 6), (4, 6, 7),  # +Z
        (0, 4, 7), (0, 7, 3),  # -X
        (1, 2, 6), (1, 6, 5),  # +X
        (0, 1, 5), (0, 5, 4),  # -Y
        (3, 7, 6), (3, 6, 2),  # +Y
    ]
    return pyjet3d.Mesh(vertices, faces)


def channels(color):
    return ((color >> 11) & 0x1F, (color >> 5) & 0x3F, color & 0x1F)


def test_shared_vertex_cube_has_correct_flat_face_lighting():
    scene = make_scene()
    scene.setDirectionalLight(
        pyjet3d.DirectionalLight(225, 45, 255, 241, 214, 255)
    )
    ambient = pyjet3d.AmbientLight(100, 104, 120)
    scene.setAmbientLight(ambient)
    scene.addMesh(cube_mesh(), material(0xF800))
    scene.render()

    lit_front = scene.get_pixel(160, 120)

    ambient_scene = make_scene()
    ambient_scene.setAmbientLight(ambient)
    ambient_scene.addMesh(cube_mesh(), material(0xF800))
    ambient_scene.render()
    ambient_front = ambient_scene.get_pixel(160, 120)

    # Before the shared-vertex split, FLAT picked whichever face normal was
    # stamped into v1 last and often rendered the front face as ambient-only.
    assert lit_front != ambient_front
    assert channels(lit_front)[0] > channels(ambient_front)[0]


def test_flat_gouraud_and_phong_agree_on_a_flat_cube_face():
    colors = []
    for shading in (
        pyjet3d.ShadingMode.FLAT,
        pyjet3d.ShadingMode.GOURAUD,
        pyjet3d.ShadingMode.PHONG,
    ):
        scene = make_scene()
        scene.setDirectionalLight(
            pyjet3d.DirectionalLight(225, 45, 255, 255, 255, 255)
        )
        scene.addMesh(cube_mesh(), material(0x8410, shading))
        scene.render()
        colors.append(scene.get_pixel(160, 120))

    assert colors[0] == colors[1]
    # PHONG performs per-pixel fixed-point interpolation; allow at most one
    # RGB565 step per channel of rounding difference.
    assert all(
        abs(a - b) <= 2
        for a, b in zip(channels(colors[0]), channels(colors[2]))
    )


def test_directional_light_colour_tints_diffuse_term():
    red_scene = make_scene()
    red_scene.setDirectionalLight(
        pyjet3d.DirectionalLight(270, 0, 255, 0, 0, 255)
    )
    red_scene.addMesh(TRIANGLE, material(0x8410))
    red_scene.render()
    red = channels(red_scene.get_pixel(160, 120))

    blue_scene = make_scene()
    blue_scene.setDirectionalLight(
        pyjet3d.DirectionalLight(270, 0, 0, 0, 255, 255)
    )
    blue_scene.addMesh(TRIANGLE, material(0x8410))
    blue_scene.render()
    blue = channels(blue_scene.get_pixel(160, 120))

    assert red[0] > red[2]
    assert blue[2] > blue[0]


def test_point_light_illuminates_the_facing_side_and_respects_range():
    def render(point_light):
        scene = make_scene()
        if point_light is not None:
            scene.addPointLight(point_light)
        scene.addMesh(TRIANGLE, material(0xFFFF))
        scene.render()
        return scene.get_pixel(160, 120)

    no_lights = render(None)
    lit = render(pyjet3d.PointLight(0, 0, -200, 255, 0, 0, 1000, 1000))
    behind = render(pyjet3d.PointLight(0, 0, 200, 255, 0, 0, 1000, 1000))
    out_of_range = render(
        pyjet3d.PointLight(0, 0, -200, 255, 0, 0, 0, 100)
    )

    # With no lights at all the legacy full-brightness fallback applies. Once
    # any light exists, surfaces outside its influence are black.
    assert no_lights == 0xFFFF
    assert lit != no_lights
    assert channels(lit)[0] > channels(lit)[1]
    assert channels(lit)[0] > channels(lit)[2]
    assert behind == 0
    assert out_of_range == 0


@pytest.mark.parametrize(
    "shading",
    [
        pyjet3d.ShadingMode.FLAT,
        pyjet3d.ShadingMode.GOURAUD,
        pyjet3d.ShadingMode.PHONG,
    ],
)
def test_point_light_works_in_all_lit_shading_modes(shading):
    scene = make_scene()
    scene.addPointLight(
        pyjet3d.PointLight(0, 0, -200, 255, 0, 0, 1000, 1000)
    )
    scene.addMesh(TRIANGLE, material(0xFFFF, shading))
    scene.render()

    pixel = scene.get_pixel(160, 120)
    assert channels(pixel)[0] > channels(pixel)[1]
    assert channels(pixel)[0] > channels(pixel)[2]


def test_explicit_vertex_normals_control_lighting():
    def render(normal):
        scene = make_scene()
        scene.setDirectionalLight(
            pyjet3d.DirectionalLight(225, 45, 255, 255, 255, 255)
        )
        mesh = pyjet3d.Mesh(
            TRIANGLE,
            [(0, 1, 2)],
            [normal, normal, normal],
        )
        scene.addMesh(mesh, material(0x8410))
        scene.render()
        return scene.get_pixel(160, 120)

    facing = render((0.0, 0.0, -1.0))
    away = render((0.0, 0.0, 1.0))

    assert facing != 0
    assert away == 0


def test_scene_add_mesh_accepts_per_vertex_normals():
    scene = make_scene()
    scene.setDirectionalLight(
        pyjet3d.DirectionalLight(225, 45, 255, 255, 255, 255)
    )
    scene.addMesh(
        TRIANGLE,
        material(0x8410),
        [(0, 1, 2)],
        [(0.0, 0.0, -1.0)] * 3,
    )
    scene.render()

    assert scene.get_pixel(160, 120) != 0


def test_directional_light_direction_controls_which_face_is_lit():
    def render(azimuth):
        scene = make_scene()
        scene.setDirectionalLight(
            pyjet3d.DirectionalLight(azimuth, 0, 255, 255, 255, 255)
        )
        scene.addMesh(TRIANGLE, material(0x8410))
        scene.render()
        return scene.get_pixel(160, 120)

    front_lit = render(270)
    back_lit = render(90)

    assert front_lit == 0x8410
    assert back_lit == 0


def test_disabled_point_light_returns_to_no_lights_fallback():
    scene = make_scene()
    light = pyjet3d.PointLight(0, 0, -200, 255, 0, 0, 1000, 1000)
    light.enabled = False
    scene.addPointLight(light)
    scene.addMesh(TRIANGLE, material(0xFFFF))
    scene.render()

    assert scene.get_pixel(160, 120) == 0xFFFF


def test_cube_vertices_are_split_per_face_normal():
    scene = make_scene()
    scene.addMesh(cube_mesh(), material(0xF800))
    objects, triangles, vertices = scene.get_statistics()

    assert objects == 1
    assert triangles == 12
    # Eight shared cube corners become 24 vertices: three face normals each.
    assert vertices == 24


def test_coplanar_shared_vertices_are_not_split():
    scene = make_scene()
    quad = pyjet3d.Mesh(
        [(-100, -100, 0), (-100, 100, 0), (100, 100, 0), (100, -100, 0)],
        [(0, 1, 2), (0, 2, 3)],
    )
    scene.addMesh(quad, material(0x8410))
    _, triangles, vertices = scene.get_statistics()

    assert triangles == 2
    assert vertices == 4


def test_custom_normals_reject_zero_scale():
    mesh = pyjet3d.Mesh(
        TRIANGLE,
        [(0, 1, 2)],
        [(0.0, 0.0, -1.0)] * 3,
    )
    with pytest.raises(ValueError):
        mesh.setScale(0.0, 1.0, 1.0)


def test_mesh_normals_must_match_vertex_count():
    try:
        pyjet3d.Mesh(TRIANGLE, [(0, 1, 2)], [(0.0, 0.0, -1.0)])
    except ValueError:
        pass
    else:  # pragma: no cover - defensive assertion
        raise AssertionError("mismatched normals should raise ValueError")
