#!/usr/bin/env python3
"""Builds the patched engine source tree that Stimpak is compiled from.

The engine checkout is never modified. Its `src/` is mirrored into
`<out>/src/`; files named by a patch in `patches/` get that patch applied, and
Stimpak's own files from `src/` are added. Files are written only when their
content changes, so incremental builds recompile only what changed.

Unpatched files start with a `#line` directive naming the original file, so
compiler messages and debuggers point at the engine checkout, not the copy.

  stimpak_tree.py --engine <engine root> --out <dir>
  stimpak_tree.py --engine <engine root> --out <dir> --list
  stimpak_tree.py --engine <engine root> --check

  --list   print the files of the tree (one per line) and exit
  --check  build the tree in a temporary directory only (do the patches apply?)

Patches are applied with `git apply` (exact) and, if that fails, with
`patch -F3` (tolerates nearby changes, as in a fork that edits the same files).

A fork whose own changes are interwoven with a patch can carry that file with
the patch already merged. It lists the file in `stimpak-merged.txt` at its
root, with the hash of the patch it merged (`--patch-hash <file>` prints it):

  # file                    Stimpak patch hash
  src/character_editor.cc   1f2e3d4c5b6a7980

The file is then used as it is. When a new Stimpak version changes that
patch, the hash no longer matches and the build stops until the fork has
merged the new patch and updated the hash.
"""

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

STIMPAK_DIR = Path(__file__).resolve().parent.parent
PATCH_DIR = STIMPAK_DIR / "patches"
OWN_SRC_DIR = STIMPAK_DIR / "src"
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hpp"}


def patch_targets():
    """Maps engine-relative paths (src/foo.cc) to their patch files."""
    targets = {}
    for patch in sorted(PATCH_DIR.rglob("*.patch")):
        relative = patch.relative_to(PATCH_DIR).as_posix()[: -len(".patch")]
        targets[relative] = patch
    return targets


MERGED_LIST = "stimpak-merged.txt"


def patch_hash(patch: Path) -> str:
    return hashlib.sha256(patch.read_bytes().replace(b"\r\n", b"\n")).hexdigest()[:16]


def merged_files(engine: Path, targets):
    """Files the engine checkout carries with Stimpak's patch merged in."""
    listing = engine / MERGED_LIST
    merged = {}
    if not listing.is_file():
        return merged
    for number, line in enumerate(listing.read_text().splitlines(), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        parts = line.split()
        if len(parts) != 2:
            raise SystemExit(f"{listing}:{number}: expected '<file> <patch hash>'")
        relative, recorded = parts
        if relative not in targets:
            raise SystemExit(f"{listing}:{number}: Stimpak has no patch for {relative}")
        current = patch_hash(targets[relative])
        if recorded != current:
            raise SystemExit(
                f"stimpak: {targets[relative].relative_to(STIMPAK_DIR)} changed since {relative} merged it "
                f"(merged {recorded}, now {current}).\n"
                f"Merge the new patch into {engine / relative} and set its hash in {listing} to {current}.")
        merged[relative] = current
    return merged


def own_files():
    """Stimpak's own sources, keyed by their path in the tree (src/...)."""
    return {"src/" + p.relative_to(OWN_SRC_DIR).as_posix(): p
            for p in sorted(OWN_SRC_DIR.rglob("*")) if p.is_file()}


def apply_patch(original: Path, patch: Path, relative: str) -> bytes:
    """Returns the patched content of `original`; exits with the rejects on failure."""
    with tempfile.TemporaryDirectory(prefix="stimpak-") as tmp:
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
            print(f"stimpak: {relative}: patch applied with fuzz", file=sys.stderr)
            return work.read_bytes()

        details = rejects.read_text(errors="replace") if rejects.exists() else fuzzy.stdout + fuzzy.stderr
        raise SystemExit(f"stimpak: cannot apply {patch.relative_to(STIMPAK_DIR)} to {original}\n{details}")


def with_line_directive(content: bytes, origin: Path) -> bytes:
    path = origin.as_posix().replace("\\", "\\\\").replace('"', '\\"')
    return f'#line 1 "{path}"\n'.encode() + content


def desired_tree(engine: Path):
    """Yields (relative path, content) for every file of the tree."""
    src = engine / "src"
    if not src.is_dir():
        raise SystemExit(f"stimpak: {src} not found (expected an engine checkout)")

    targets = patch_targets()
    own = own_files()
    merged = merged_files(engine, targets)

    for relative in targets:
        if not (engine / relative).is_file():
            raise SystemExit(f"stimpak: patch target {relative} not found in {engine}")

    for path in sorted(src.rglob("*")):
        if not path.is_file():
            continue
        relative = path.relative_to(engine).as_posix()
        if relative in own:
            raise SystemExit(f"stimpak: {relative} exists in the engine and in Stimpak's src/")
        if relative in targets and relative not in merged:
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
        print(f"stimpak: {written} file(s) updated, {removed} removed in {out}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--engine", required=True, type=Path)
    parser.add_argument("--out", type=Path)
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--patch-hash", metavar="FILE", help="print the hash of Stimpak's patch for FILE (src/...)")
    args = parser.parse_args()
    engine = args.engine.resolve()

    if args.patch_hash:
        targets = patch_targets()
        if args.patch_hash not in targets:
            raise SystemExit(f"Stimpak has no patch for {args.patch_hash}")
        print(patch_hash(targets[args.patch_hash]))
        return

    if args.check:
        with tempfile.TemporaryDirectory(prefix="stimpak-check-") as tmp:
            write_tree(engine, Path(tmp))
        merged = merged_files(engine, patch_targets())
        note = f" ({len(merged)} merged in the checkout)" if merged else ""
        print(f"stimpak: all {len(patch_targets())} patches apply to {engine}{note}")
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
