#!/usr/bin/env python3
"""Render a lit, depth-tested cube with directional, ambient and point light."""

import argparse

from demo_utils import cube_mesh, make_lit_scene, write_ppm


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--width", type=int, default=480)
    parser.add_argument("--height", type=int, default=320)
    parser.add_argument("--angle", type=float, default=35.0)
    parser.add_argument(
        "--smooth",
        action="store_true",
        help="use authored per-vertex normals for smooth GOURAUD/PHONG shading",
    )
    parser.add_argument("--output", default="lit_cube.ppm")
    args = parser.parse_args()

    scene, material = make_lit_scene(args.width, args.height)

    mesh = cube_mesh(size=240, smooth=args.smooth)
    mesh.setRotation(0, args.angle, 0)
    mesh.setScale(1.0, 1.0, 1.0)
    scene.addMesh(mesh, material)
    scene.render()

    output = write_ppm(args.output, scene)
    objects, triangles, _ = scene.get_statistics()
    print("wrote {} ({}x{})".format(output, scene.width, scene.height))
    print("objects={} triangles={} rasterized={} smooth={}".format(
        objects, triangles, scene.last_frame_rasterized_triangles,
        args.smooth,
    ))
    print("angle={:.1f} degrees; centre pixel=0x{:04X}".format(
        args.angle % 360.0,
        scene.get_pixel(scene.width // 2, scene.height // 2),
    ))


if __name__ == "__main__":
    main()
