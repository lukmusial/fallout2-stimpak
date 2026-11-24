#!/usr/bin/env python3
"""Maintainers: makes a development checkout to work on Stimpack.

  dev_checkout.py <dir>

Clones the engine (from the engine/ submodule) into <dir> at the pinned commit,
applies Stimpack's patches, copies its own sources, adds them to the build and
commits the result on a branch `stimpack-dev`. Work there as in any engine
checkout (build with CMake, run tests/run_tests.py --build-dir <dir>/build),
then bring the changes back with:

  tools/export_patches.py <dir>
"""

import argparse
import shutil
import subprocess
from pathlib import Path

STIMPACK_DIR = Path(__file__).resolve().parent.parent


def run(*args, cwd=None):
    subprocess.run(args, cwd=cwd, check=True)


def pinned_commit():
    for line in (STIMPACK_DIR / "engine-version.txt").read_text().splitlines():
        if line.startswith("commit="):
            return line.split("=", 1)[1].strip()
    raise SystemExit("engine-version.txt has no commit= line")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("dir", type=Path)
    args = parser.parse_args()
    target = args.dir.resolve()
    if target.exists():
        raise SystemExit(f"{target} already exists")

    engine = STIMPACK_DIR / "engine"
    if not (engine / "CMakeLists.txt").is_file():
        run("git", "submodule", "update", "--init", "engine", cwd=STIMPACK_DIR)
    run("git", "clone", "--quiet", str(engine), str(target))
    run("git", "checkout", "--quiet", "-b", "stimpack-dev", pinned_commit(), cwd=target)

    for patch in sorted((STIMPACK_DIR / "patches").rglob("*.patch")):
        run("git", "apply", str(patch), cwd=target)
    sources = []
    for path in sorted((STIMPACK_DIR / "src").rglob("*")):
        if path.is_file():
            relative = path.relative_to(STIMPACK_DIR)
            (target / relative).parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, target / relative)
            sources.append(relative.as_posix())
    if (STIMPACK_DIR / "data").is_dir():
        shutil.copytree(STIMPACK_DIR / "data", target / "data", dirs_exist_ok=True)

    with open(target / "CMakeLists.txt", "a") as cmake:
        cmake.write("\n# Stimpack development checkout (not exported).\n")
        cmake.write("target_sources(${EXECUTABLE_NAME} PRIVATE\n")
        for source in sources:
            cmake.write(f'    "{source}"\n')
        cmake.write(")\ntarget_compile_definitions(${EXECUTABLE_NAME} PRIVATE FALLOUT2_STIMPACK=1)\n")

    run("git", "add", "-A", cwd=target)
    run("git", "commit", "--quiet", "-m", "Stimpack (development checkout)", cwd=target)
    print(f"development checkout ready: {target} (branch stimpack-dev)")


if __name__ == "__main__":
    main()
