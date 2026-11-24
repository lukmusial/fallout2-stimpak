#!/usr/bin/env python3
"""Maintainers: exports Stimpack from a development checkout of the engine.

A development checkout is the engine at the pinned commit (see engine-version.txt) with
Stimpack applied and edited in place (tools/dev_checkout.py makes one). This
script turns it back into this repository's form:

  - files the checkout adds under src/  -> src/ here (Stimpack's own sources)
  - engine files it changes under src/  -> patches/src/<file>.patch
  - data/                               -> data/ here

  export_patches.py <checkout> [--base <commit>]

Only committed and working-tree changes under src/ and data/ are exported;
CMakeLists.txt changes are not (cmake/Stimpack.cmake adds the sources).
"""

import argparse
import shutil
import subprocess
from pathlib import Path

STIMPACK_DIR = Path(__file__).resolve().parent.parent


def git(checkout, *args):
    return subprocess.run(["git", "-C", str(checkout), *args], check=True, capture_output=True, text=True).stdout


def pinned_commit():
    for line in (STIMPACK_DIR / "engine-version.txt").read_text().splitlines():
        if line.startswith("commit="):
            return line.split("=", 1)[1].strip()
    raise SystemExit("engine-version.txt has no commit= line")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("checkout", type=Path)
    parser.add_argument("--base")
    args = parser.parse_args()
    checkout = args.checkout.resolve()
    base = args.base or pinned_commit()

    status = git(checkout, "diff", "--name-status", "--no-renames", base, "--", "src", "data")
    added, modified = [], []
    for line in status.splitlines():
        kind, path = line.split("\t", 1)
        if kind == "A":
            added.append(path)
        elif kind == "M":
            modified.append(path)
        else:
            raise SystemExit(f"unsupported change {kind} {path}: Stimpack only adds and patches files")

    for directory in ("src", "patches", "data"):
        shutil.rmtree(STIMPACK_DIR / directory, ignore_errors=True)

    for path in added:
        target = STIMPACK_DIR / path
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(checkout / path, target)

    for path in modified:
        if not path.startswith("src/"):
            raise SystemExit(f"{path}: only engine sources under src/ can be patched")
        diff = git(checkout, "diff", "--no-color", "--no-ext-diff", "-U3", base, "--", path)
        target = STIMPACK_DIR / "patches" / (path + ".patch")
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(f"Stimpack patch for {path} (engine commit {base[:12]}).\n\n" + diff)

    print(f"exported {len(added)} own file(s) and {len(modified)} patch(es) from {checkout}")


if __name__ == "__main__":
    main()
