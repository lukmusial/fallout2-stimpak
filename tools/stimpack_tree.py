#!/usr/bin/env python3
"""Builds the patched engine source tree that Stimpack is compiled from.

The engine checkout is never modified. Its `src/` is mirrored into
`<out>/src/`; files named by a patch in `patches/` get that patch applied, and
Stimpack's own files from `src/` are added. Files are written only when their
content changes, so incremental builds recompile only what changed.

Unpatched files start with a `#line` directive naming the original file, so
compiler messages and debuggers point at the engine checkout, not the copy.

  stimpack_tree.py --engine <engine root> --out <dir>
  stimpack_tree.py --engine <engine root> --out <dir> --list
  stimpack_tree.py --engine <engine root> --check

  --list   print the files of the tree (one per line) and exit
  --check  build the tree in a temporary directory only (do the patches apply?)

Patches are applied with `git apply` (exact) and, if that fails, with
`patch -F3` (tolerates nearby changes, as in a fork that edits the same files).
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

STIMPACK_DIR = Path(__file__).resolve().parent.parent
PATCH_DIR = STIMPACK_DIR / "patches"
OWN_SRC_DIR = STIMPACK_DIR / "src"
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hpp"}


def patch_targets():
    """Maps engine-relative paths (src/foo.cc) to their patch files."""
    targets = {}
    for patch in sorted(PATCH_DIR.rglob("*.patch")):
        relative = patch.relative_to(PATCH_DIR).as_posix()[: -len(".patch")]
        targets[relative] = patch
    return targets


def own_files():
    """Stimpack's own sources, keyed by their path in the tree (src/...)."""
    return {"src/" + p.relative_to(OWN_SRC_DIR).as_posix(): p
            for p in sorted(OWN_SRC_DIR.rglob("*")) if p.is_file()}


def apply_patch(original: Path, patch: Path, relative: str) -> bytes:
    """Returns the patched content of `original`; exits with the rejects on failure."""
    with tempfile.TemporaryDirectory(prefix="stimpack-") as tmp:
        work = Path(tmp) / relative
        work.parent.mkdir(parents=True)
        shutil.copyfile(original, work)

        exact = subprocess.run(
            ["git", "apply", "--whitespace=nowarn", "-p1", str(patch)],
            cwd=tmp, capture_output=True, text=True)
        if exact.returncode == 0:
            return work.read_bytes()

        shutil.copyfile(original, work)
        rejects = Path(tmp) / "rejects"
        fuzzy = subprocess.run(
            ["patch", "-p1", "-F3", "-N", "-s", "-r", str(rejects), "-i", str(patch)],
            cwd=tmp, capture_output=True, text=True)
        if fuzzy.returncode == 0:
            print(f"stimpack: {relative}: patch applied with fuzz", file=sys.stderr)
            return work.read_bytes()

        details = rejects.read_text(errors="replace") if rejects.exists() else fuzzy.stdout + fuzzy.stderr
        raise SystemExit(f"stimpack: cannot apply {patch.relative_to(STIMPACK_DIR)} to {original}\n{details}")


def with_line_directive(content: bytes, origin: Path) -> bytes:
    path = origin.as_posix().replace("\\", "\\\\").replace('"', '\\"')
    return f'#line 1 "{path}"\n'.encode() + content


def desired_tree(engine: Path):
    """Yields (relative path, content) for every file of the tree."""
    src = engine / "src"
    if not src.is_dir():
        raise SystemExit(f"stimpack: {src} not found (expected an engine checkout)")

    targets = patch_targets()
    own = own_files()

    for relative in targets:
        if not (engine / relative).is_file():
            raise SystemExit(f"stimpack: patch target {relative} not found in {engine}")

    for path in sorted(src.rglob("*")):
        if not path.is_file():
            continue
        relative = path.relative_to(engine).as_posix()
        if relative in own:
            raise SystemExit(f"stimpack: {relative} exists in the engine and in Stimpack's src/")
        if relative in targets:
            yield relative, apply_patch(path, targets[relative], relative)
        elif path.suffix in SOURCE_SUFFIXES:
            yield relative, with_line_directive(path.read_bytes(), path)
        else:
            yield relative, path.read_bytes()

    for relative, path in own.items():
        content = path.read_bytes()
        if path.suffix in SOURCE_SUFFIXES:
            content = with_line_directive(content, path)
        yield relative, content


def write_tree(engine: Path, out: Path):
    written = 0
    seen = set()
    for relative, content in desired_tree(engine):
        seen.add(relative)
        target = out / relative
        if target.is_file() and target.read_bytes() == content:
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        tmp = target.with_name(target.name + ".tmp")
        tmp.write_bytes(content)
        os.replace(tmp, target)
        written += 1

    removed = 0
    if (out / "src").is_dir():
        for path in sorted((out / "src").rglob("*"), reverse=True):
            relative = path.relative_to(out).as_posix()
            if path.is_file() and relative not in seen:
                path.unlink()
                removed += 1
            elif path.is_dir() and not any(path.iterdir()):
                path.rmdir()

    if written or removed:
        print(f"stimpack: {written} file(s) updated, {removed} removed in {out}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--engine", required=True, type=Path)
    parser.add_argument("--out", type=Path)
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    engine = args.engine.resolve()

    if args.check:
        with tempfile.TemporaryDirectory(prefix="stimpack-check-") as tmp:
            write_tree(engine, Path(tmp))
        print(f"stimpack: all {len(patch_targets())} patches apply to {engine}")
        return

    if args.out is None:
        parser.error("--out is required")
    out = args.out.resolve()

    if args.list:
        files = [p.relative_to(engine).as_posix() for p in (engine / "src").rglob("*") if p.is_file()]
        files += list(own_files())
        for relative in sorted(set(files)):
            print((out / relative).as_posix())
        return

    write_tree(engine, out)


if __name__ == "__main__":
    main()
