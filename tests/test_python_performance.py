"""Native output and threaded-raster regression tests."""

import struct

import pytest

import pyjet3d


def make_scene(width=640, height=480, threads=1):
    scene = pyjet3d.Scene(width, height)
    camera = pyjet3d.Camera()
    camera.setPosition(0, 0, -500)
    camera.setFOV(70, width)
    camera.nearPlane = 32
    camera.farPlane = 4096
    camera.lookAt(0, 0, 0)
    scene.setCamera(camera)
    scene.setThreadCount(threads)
    return scene


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
        (0, 3, 2), (0, 2, 1),
        (4, 5, 6), (4, 6, 7),
        (0, 4, 7), (0, 7, 3),
        (1, 2, 6), (1, 6, 5),
        (0, 1, 5), (0, 5, 4),
        (3, 7, 6), (3, 6, 2),
    ]
    return pyjet3d.Mesh(vertices, faces)


def lit_cube_scene(threads):
    scene = make_scene(threads=threads)
    scene.setDirectionalLight(
        pyjet3d.DirectionalLight(225, 45, 255, 241, 214, 255)
    )
    scene.setAmbientLight(pyjet3d.AmbientLight(100, 104, 120))
    material = pyjet3d.Material(0xF800)
    material.shadingMode = pyjet3d.ShadingMode.GOURAUD
    scene.addMesh(cube_mesh(), material)
    return scene


def test_native_rgb_conversion_matches_historical_rounding():
    # Exercise every possible RGB565 value so the SIMD interleave/packing path
    # is checked against the exact integer expansion.
    colors = range(0x10000)
    raw = b"".join(struct.pack("<H", value) for value in colors)

    expected_rgb = bytearray()
    expected_rgba = bytearray()
    for value in colors:
        r5 = (value >> 11) & 0x1F
        g6 = (value >> 5) & 0x3F
        b5 = value & 0x1F
        r = (r5 * 255 + 15) // 31
        g = (g6 * 255 + 31) // 63
        b = (b5 * 255 + 15) // 31
        expected_rgb.extend((r, g, b))
        expected_rgba.extend((r, g, b, 0xFF))

    assert pyjet3d.framebuffer_to_rgb(raw, 0x10000, 1) == bytes(expected_rgb)
    assert pyjet3d.framebuffer_to_rgba(raw, 0x10000, 1) == bytes(expected_rgba)


def test_rgba_output_is_opaque_and_scene_helpers_agree():
    scene = make_scene()
    scene.addMesh(cube_mesh(), pyjet3d.Material(0x07E0))
    scene.render()

    packed = scene.get_framebuffer()
    rgb = scene.get_framebuffer_rgb888()
    rgba = scene.get_framebuffer_rgba8888()

    assert rgb == pyjet3d.framebuffer_to_rgb(packed, scene.width, scene.height)
    assert rgba == pyjet3d.framebuffer_to_rgba(packed, scene.width, scene.height)
    assert len(rgb) == scene.width * scene.height * 3
    assert len(rgba) == scene.width * scene.height * 4

    # Every tiny-endian RGBA word has an opaque alpha byte.
    assert all(rgba[index] == 0xFF for index in range(3, len(rgba), 4))
    # The 32-bit and 24-bit paths share the same RGB channel values.
    for index in range(0, scene.width * scene.height, 97):
        offset = index * 3
        word = rgba[index * 4:index * 4 + 4]
        assert word[0:3] == rgb[offset:offset + 3]


def test_threaded_tiles_match_serial_pixels_and_statistics():
    serial = lit_cube_scene(1)
    parallel = lit_cube_scene(4)

    serial.render()
    parallel.render()

    assert parallel.thread_count == 4
    assert parallel.last_frame_drawn_triangles == serial.last_frame_drawn_triangles
    assert parallel.last_frame_rasterized_triangles == serial.last_frame_rasterized_triangles
    assert parallel.get_framebuffer() == serial.get_framebuffer()
    assert parallel.get_framebuffer_rgb888() == serial.get_framebuffer_rgb888()


def test_threaded_rendering_can_be_disabled():
    scene = lit_cube_scene(4)
    serial = lit_cube_scene(1)
    scene.threaded_rendering = False
    scene.render()
    serial.render()

    assert scene.threaded_rendering is False
    assert scene.get_framebuffer() == serial.get_framebuffer()


def test_repeated_auto_threaded_scene_lifecycle():
    # Pool creation and teardown used to lose a completion wakeup after a few
    # frames. Keep a reference frame so both repeated batches and repeated
    # pool lifetimes are exercised.
    reference = None
    for _ in range(12):
        scene = make_scene(threads=0)
        scene.addMesh(cube_mesh(), pyjet3d.Material(0xF800))
        for _ in range(3):
            scene.render()

        assert scene.thread_count > 1
        frame = scene.get_framebuffer()
        if reference is None:
            reference = frame
        else:
            assert frame == reference


def water_scene(threads):
    scene = pyjet3d.Scene(640, 480)
    camera = pyjet3d.Camera()
    camera.setPosition(0, 120, -650)
    camera.setFOV(70, 640)
    camera.nearPlane = 32
    camera.farPlane = 4096
    camera.lookAt(0, -50, 0)
    scene.setCamera(camera)
    scene.setThreadCount(threads)

    cube = cube_mesh()
    cube.setPosition(0, 20, 0)
    cube.setRotation(15, 30, 0)
    material = pyjet3d.Material(0xF800)
    material.shadingMode = pyjet3d.ShadingMode.GOURAUD
    scene.addMesh(cube, material)

    water = pyjet3d.Mesh(
        [(-900, -120, -100), (900, -120, -100),
         (900, -120, 900), (-900, -120, 900)],
        [(0, 1, 2), (0, 2, 3)],
    )
    water.cullingMode = pyjet3d.CullingMode.NO_CULLING
    water_material = pyjet3d.Material(0x03E0)
    water_material.shadingMode = pyjet3d.ShadingMode.WATER_REFLECT
    water_material.alpha = 180
    water_material.specular = 32
    scene.addMesh(water, water_material)
    return scene


def test_threaded_water_reflect_matches_serial():
    serial = water_scene(1)
    threaded = water_scene(0)

    for _ in range(3):
        serial.render()
        threaded.render()

    assert threaded.last_frame_rasterized_triangles > 0
    assert threaded.last_frame_drawn_triangles == serial.last_frame_drawn_triangles
    assert threaded.last_frame_rasterized_triangles == serial.last_frame_rasterized_triangles
    assert threaded.get_framebuffer() == serial.get_framebuffer()


def test_frustum_cull_runs_before_object_transform_and_submission():
    scene = make_scene()
    # The camera is at z=-500 looking toward +z. This mesh is entirely behind
    # the near plane, so it must be rejected by the object cull before any
    # vertices enter the transform queue.
    scene.addMesh(
        [(-100, -100, -2000), (100, -100, -2000), (0, 100, -2000)],
        pyjet3d.Material(0xFFFF),
    )
    scene.render()

    assert scene.last_frame_drawn_objects == 0
    assert scene.last_frame_drawn_triangles == 0
    assert scene.last_frame_rasterized_triangles == 0


def test_native_conversion_rejects_bad_frame_size():
    with pytest.raises(ValueError):
        pyjet3d.framebuffer_to_rgb(b"\x00\x00", 2, 2)
    with pytest.raises(ValueError):
        pyjet3d.framebuffer_to_rgba(b"\x00\x00", 0, 1)
