#!/usr/bin/env python3
"""Makes the README screenshots in docs/images/ from a real build.

Runs the game hidden through the test mode (as tests/run_tests.py does):
starts a new game, then walks through every Stimpak screen
(tests/scripts/screenshots.txt) and writes the screens at twice the game's
640x480 size.

  tools/screenshots.py --game-dir "/path/to/Fallout 2" [--build-dir build/debug]
"""

import argparse
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tests"))
import run_tests  # noqa: E402

IMAGES = ROOT / "docs" / "images"


def scale2x(src: Path, dst: Path):
    """Nearest-neighbour 2x of an 8-bit-palette PNG via the stdlib-free path:
    uses Pillow when available, else copies the file as it is."""
    try:
        from PIL import Image
    except ImportError:
        dst.write_bytes(src.read_bytes())
        return
    image = Image.open(src).convert("RGB")
    image.resize((image.width * 2, image.height * 2), Image.NEAREST).save(dst, optimize=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--game-dir", default=os.environ.get("FALLOUT2_GAME_DIR"))
    parser.add_argument("--build-dir", default=os.environ.get("STIMPAK_BUILD_DIR", str(ROOT / "build" / "debug")))
    args = parser.parse_args()
    if not args.game_dir:
        parser.error("--game-dir (or FALLOUT2_GAME_DIR) is required: your Fallout 2 folder")

    runner = run_tests.Runner(Path(args.build_dir).resolve(), Path(args.game_dir).expanduser().resolve())
    runner.prepare()
    runner.run("new_game", "new_game_save.txt")
    out = runner.run("screenshots", "screenshots.txt")

    IMAGES.mkdir(parents=True, exist_ok=True)
    for shot in sorted(out.glob("*.png")):
        scale2x(shot, IMAGES / shot.name)
        print(IMAGES / shot.name)


if __name__ == "__main__":
    main()
