#!/usr/bin/env python3
"""Minimal PyJet demo: render one triangle and save it as a PPM image."""

import argparse

import pyjet3d

from demo_utils import write_ppm


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--width", type=int, default=320)
    parser.add_argument("--height", type=int, default=240)
    parser.add_argument("--output", default="triangle.ppm")
    args = parser.parse_args()

    scene = pyjet3d.Scene(args.width, args.height)

    camera = pyjet3d.Camera()
    camera.setPosition(0, 0, -500)
    camera.setFOV(70)
    scene.setCamera(camera)

    material = pyjet3d.Material(0xF800)  # RGB565 bright red
    material.shadingMode = pyjet3d.ShadingMode.FLAT

    # Counter-clockwise winding: front-facing with the default CULL_BACKFACES.
    scene.addMesh(
        [(0, 120, 0), (120, -120, 0), (-120, -120, 0)],
        material,
    )
    scene.render()

    output = write_ppm(args.output, scene)
    print("wrote {} ({}x{})".format(output, scene.width, scene.height))
    print("pixels:", scene.width * scene.height)
    print("centre pixel: 0x{:04X}".format(scene.get_pixel(scene.width // 2, scene.height // 2)))


if __name__ == "__main__":
    main()
