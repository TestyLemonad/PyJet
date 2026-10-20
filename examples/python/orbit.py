#!/usr/bin/env python3
"""Render a short rotating-cube sequence as numbered PPM frames.

The scene is rebuilt for every frame because Scene.addMesh() copies geometry.
That is the simplest way to animate a mesh with this Python API.
"""

import argparse
from pathlib import Path

from demo_utils import cube_mesh, make_lit_scene, write_ppm


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--width", type=int, default=320)
    parser.add_argument("--height", type=int, default=240)
    parser.add_argument("--frames", type=int, default=12)
    parser.add_argument("--output-dir", default="orbit_frames")
    args = parser.parse_args()

    if args.frames < 1:
        parser.error("--frames must be at least 1")

    output_dir = Path(args.output_dir)
    for frame in range(args.frames):
        angle = 360.0 * frame / args.frames

        scene, material = make_lit_scene(args.width, args.height)
        mesh = cube_mesh(size=240)
        mesh.setRotation(0, angle, 0)
        scene.addMesh(mesh, material)
        scene.render()

        output = output_dir / "frame_{:03d}.ppm".format(frame)
        write_ppm(output, scene)
        print("frame {:03d}/{:03d}: {:>5.1f} deg -> {}".format(
            frame + 1, args.frames, angle % 360.0, output
        ))

    print("done: {} frames in {}".format(args.frames, output_dir))
    print("convert with: ffmpeg -i {}/frame_%03d.ppm animation.gif".format(output_dir))


if __name__ == "__main__":
    main()
