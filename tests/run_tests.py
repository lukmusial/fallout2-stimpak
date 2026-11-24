#!/usr/bin/env python3
"""Stimpack's automated tests.

The tests drive a real build of the game through its scripted test mode
(`--dev-script`, see src/dev_automation.h): the game runs hidden and silent,
follows a script of key presses and clicks, writes screenshots and JSON dumps
of the game state, and exits. Nothing is compared against stored images: the
checks are made on state dumps, so the tests do not depend on a particular
game language, resolution or art.

No saved games are shipped. The first step starts a new game with the first
premade character and saves it; every test works from that save (and from
the saves earlier tests make).

You need your own copy of Fallout 2 (master.dat, critter.dat, patch000.dat).
The game folder is only read; the tests run in <build>/test-run.

  tests/run_tests.py --game-dir "~/Games/Fallout 2" [--build-dir build/debug] [-k name] [--list]

Environment: FALLOUT2_GAME_DIR (instead of --game-dir), STIMPACK_BUILD_DIR.
Screenshots of every run are kept in <build>/test-run/devout/<test>/ for review.
"""

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCRIPTS = ROOT / "tests" / "scripts"
GAME_TIMEOUT = 180
GAME_FILES = ("master.dat", "critter.dat", "patch000.dat")
GAME_DIRS = ("sound",)

# State dump layout (src/dev_automation.cc).
ST = 0
SKILL_SMALL_GUNS = 0
SKILL_MELEE_WEAPONS = 3
PC_LEVEL = 1
STAT_MAX_HP = 7


class TestFailure(Exception):
    pass


class Runner:
    def __init__(self, build_dir: Path, game_dir: Path):
        self.build_dir = build_dir
        self.game_dir = game_dir
        self.run_dir = build_dir / "test-run"
        self.exe = find_executable(build_dir)
        self.start = None
        self.lists = None
        # Global variables that game scripts change on every load (for example
        # GVAR_DUDE_STOMACH); measured by test_save_unchanged.
        self.volatile_gvars = set()

    def prepare(self):
        """Run directory: links to the game data and a fresh data/ folder."""
        missing = [name for name in GAME_FILES if not (self.game_dir / name).is_file()]
        if missing:
            raise SystemExit(f"{self.game_dir}: {', '.join(missing)} not found (use --game-dir)")

        if self.run_dir.exists():
            shutil.rmtree(self.run_dir)
        self.run_dir.mkdir(parents=True)
        for name in GAME_FILES + GAME_DIRS:
            if (self.game_dir / name).exists():
                (self.run_dir / name).symlink_to(self.game_dir / name)
        engine_files = self.engine_dir() / "files"
        if (engine_files / "f2_res.dat").is_file():
            (self.run_dir / "f2_res.dat").symlink_to(engine_files / "f2_res.dat")
        (self.run_dir / "data").mkdir()
        self.write_config(engine_files / "fallout2.cfg")

        if self.app_bundle():
            # The macOS build looks for the game data next to the .app.
            (self.run_dir / self.app_bundle().name).symlink_to(self.app_bundle())

    def app_bundle(self):
        return self.exe.parents[2] if platform.system() == "Darwin" and self.exe.parents[2].suffix == ".app" else None

    def engine_dir(self):
        cache = self.build_dir / "CMakeCache.txt"
        if cache.is_file():
            for line in cache.read_text(errors="replace").splitlines():
                if line.startswith("CMAKE_HOME_DIRECTORY:"):
                    return Path(line.split("=", 1)[1])
        return ROOT / "engine"

    def write_config(self, template: Path):
        # The engine's example configuration, pinned to what the scripts expect:
        # 640x480 logical screen in a window, no opening movies.
        pinned = {
            "resolution_x": "1280", "resolution_y": "960", "scale": "2", "windowed": "1",
            "skip_opening_movies": "2", "show_script_messages": "0", "show_tile_num": "0",
            "output_map_data_info": "0",
        }
        lines = []
        for line in template.read_text(errors="replace").splitlines():
            key = line.split("=", 1)[0].strip()
            lines.append(f"{key}={pinned[key]}" if key in pinned and "=" in line else line)
        (self.run_dir / "fallout2.cfg").write_text("\n".join(lines) + "\n")

    def run(self, name, script, extra_args=()):
        out_dir = self.run_dir / "devout" / name
        if out_dir.exists():
            shutil.rmtree(out_dir)
        out_dir.mkdir(parents=True)
        app = self.app_bundle()
        exe = self.run_dir / app.name / self.exe.relative_to(app) if app else self.exe
        args = [str(exe), f"--dev-script={SCRIPTS / script}", f"--dev-out={out_dir}", "--dev-fast", *extra_args]
        try:
            result = subprocess.run(args, cwd=self.run_dir, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                                    timeout=GAME_TIMEOUT, text=True, errors="replace")
        except subprocess.TimeoutExpired:
            raise TestFailure(f"{script}: the game did not finish within {GAME_TIMEOUT}s")
        if result.returncode != 0:
            lines = [l for l in result.stderr.splitlines() if "AUTOMATION" in l] or result.stderr.splitlines()
            raise TestFailure(f"{script}: exit code {result.returncode}: {' / '.join(lines[-2:])}")
        return out_dir

    def state_of_slot(self, slot, name):
        out = self.run(name, "load_slot.txt", [f"--dev-load-game={slot}"])
        return json.loads((out / "state.json").read_text())

    def slot_dir(self, slot):
        return self.run_dir / "data" / "SAVEGAME" / f"SLOT{slot:02d}"

    def clear_slots_after(self, slot):
        for path in (self.run_dir / "data" / "SAVEGAME").glob("SLOT*"):
            if int(path.name[4:]) > slot:
                shutil.rmtree(path)


def find_executable(build_dir: Path) -> Path:
    candidates = [*build_dir.glob("*.app/Contents/MacOS/*"), build_dir / "fallout2-ce",
                  *build_dir.glob("*/fallout2-ce.exe"), build_dir / "fallout2-ce.exe"]
    candidates = [c for c in candidates if c.is_file()]
    if not candidates:
        raise SystemExit(f"no game executable in {build_dir} (build first: tools/build.sh Debug)")
    return candidates[0]


def stable(state, volatile_gvars=()):
    """The parts of a state dump that a save and reload must keep exactly."""
    def critter(c):
        return {k: c[k] for k in ("pid", "name", "base_stats", "bonus_stats", "skills", "perks")}

    player = critter(state["player"])
    for key in ("pc_stats", "tagged_skills", "traits", "caps"):
        player[key] = state["player"][key]
    world = {k: state["world"][k] for k in ("area", "map", "map_name", "elevation", "x", "y")}
    return {
        "player": player,
        "party": sorted((critter(m) for m in state["party"]), key=lambda c: c["pid"]),
        "world": world,
        "gvars": [None if i in volatile_gvars else v for i, v in enumerate(state["gvars"])],
    }


def expect(condition, message):
    if not condition:
        raise TestFailure(message)


def differences(a, b, prefix=""):
    """Paths (player.skills[3], ...) where two dumps differ."""
    if isinstance(a, dict) and isinstance(b, dict):
        out = []
        for key in sorted(set(a) | set(b)):
            out += differences(a.get(key), b.get(key), f"{prefix}.{key}" if prefix else key)
        return out
    if isinstance(a, list) and isinstance(b, list) and len(a) == len(b):
        out = []
        for index, (x, y) in enumerate(zip(a, b)):
            out += differences(x, y, f"{prefix}[{index}]")
        return out
    return [] if a == b else [prefix]


# Tests. Each returns a short note. They run in this order; later tests use the
# saves earlier ones make (slot 1: the new game; slot 2: made by `lists`).

def test_new_game(runner):
    """Fixture: a new game with the first premade character, saved in slot 1."""
    runner.run("new_game", "new_game_save.txt")
    expect(runner.slot_dir(1).is_dir(), "slot 1 was not saved")
    runner.start = runner.state_of_slot(1, "new_game_state")
    expect(runner.start["loaded"], "slot 1 does not load")
    return f"{runner.start['player']['name']} in {runner.start['world']['map_name']}"


def test_menu(runner):
    """The main menu with the Stimpack button (a screenshot for review)."""
    out = runner.run("menu", "menu.txt")
    expect((out / "menu.png").is_file(), "no screenshot")
    return "devout/menu/menu.png"


def test_save_unchanged(runner):
    """Open slot 1 in Stimpack and save it unchanged: the new save equals slot 1."""
    runner.clear_slots_after(1)
    runner.run("save_unchanged", "save_unchanged.txt")
    expect(runner.slot_dir(2).is_dir(), "Save As did not write slot 2")
    saved = runner.state_of_slot(2, "save_unchanged_state")

    # Loading a game runs the map's scripts, and some of them change a global
    # variable every time. Measure which by saving the new slot once more.
    runner.run("save_unchanged_again", "save_unchanged_slot2.txt")
    again = runner.state_of_slot(3, "save_unchanged_again_state")
    runner.volatile_gvars = {i for i, (a, b) in enumerate(zip(saved["gvars"], again["gvars"])) if a != b}

    diff = differences(stable(runner.start, runner.volatile_gvars), stable(saved, runner.volatile_gvars))
    expect(not diff, f"the unchanged save differs: {diff[:8]}")
    note = f", {len(runner.volatile_gvars)} global(s) change on every load" if runner.volatile_gvars else ""
    return "slot 2 == slot 1" + note


def test_cancel(runner):
    """Edits followed by CANCEL leave the game as it was."""
    runner.clear_slots_after(1)
    runner.run("cancel", "cancel.txt")
    saved = runner.state_of_slot(2, "cancel_state")
    diff = differences(stable(runner.start, runner.volatile_gvars), stable(saved, runner.volatile_gvars))
    expect(not diff, f"cancelled edits were saved: {diff[:8]}")
    return "no changes"


def test_edit_player(runner):
    """Strength, a skill, a perk, level and caps change; nothing else does."""
    runner.clear_slots_after(1)
    runner.run("edit_player", "edit_player.txt")
    before = runner.start["player"]
    after = runner.state_of_slot(2, "edit_player_state")["player"]

    # The editor shows (and caps at 10) the total, traits included.
    expect(after["stats"][ST] == min(10, before["stats"][ST] + 3),
           f"ST {before['stats'][ST]} -> {after['stats'][ST]}")
    expect(after["skills"][SKILL_SMALL_GUNS] > before["skills"][SKILL_SMALL_GUNS],
           f"Small Guns {before['skills'][SKILL_SMALL_GUNS]} -> {after['skills'][SKILL_SMALL_GUNS]}")
    expect(len(after["perks"]) == len(before["perks"]) + 1, f"perks {before['perks']} -> {after['perks']}")
    expect(after["pc_stats"][PC_LEVEL] == 12, f"level {after['pc_stats'][PC_LEVEL]}")
    expect(after["caps"] == 5000, f"caps {after['caps']}")

    # Perks may change bonus stats (and derived ones).
    allowed = {"base_stats", "bonus_stats", "skills", "perks", "pc_stats", "caps", "stats", "hp"}
    changed = {d.split("[")[0].split(".")[0] for d in differences(before, after)}
    expect(changed <= allowed, f"unexpected changes: {sorted(changed - allowed)}")
    return (f"ST {after['stats'][ST]}, Small Guns {after['skills'][SKILL_SMALL_GUNS]}%, "
            f"{len(after['perks'])} perk(s), level 12, 5000 caps")


def test_lists(runner):
    """Roster (Sulik joins), quests (one becomes active), location (travel)."""
    runner.clear_slots_after(1)
    runner.run("lists", "lists.txt")
    after = runner.state_of_slot(2, "lists_state")
    runner.lists = after

    expect(len(after["party"]) == len(runner.start["party"]) + 1,
           f"party size {len(runner.start['party'])} -> {len(after['party'])}")
    changed_gvars = [i for i, (a, b) in enumerate(zip(runner.start["gvars"], after["gvars"])) if a != b]
    expect(changed_gvars, "no quest variable changed")
    expect(after["world"]["map_name"] != runner.start["world"]["map_name"],
           f"still in {after['world']['map_name']}")
    return (f"party {[m['name'] for m in after['party']]}, {len(changed_gvars)} global(s) changed, "
            f"now in {after['world']['map_name']}")


def test_edit_companion(runner):
    """A companion levels up and gets edited stats, skills and hit points."""
    if runner.lists is None or not runner.lists["party"]:
        raise TestFailure("needs the save made by the lists test")
    runner.clear_slots_after(2)
    runner.run("edit_companion", "edit_companion.txt")
    after = runner.state_of_slot(3, "edit_companion_state")
    before_member = runner.lists["party"][0]
    member = next((m for m in after["party"] if m["pid"] == before_member["pid"]), None)
    expect(member is not None, "the companion left the party")

    if before_member["max_level"] > before_member["level"]:
        expect(member["level"] == before_member["level"] + 1,
               f"level {before_member['level']} -> {member['level']}")
    expect(member["hp"] == 70, f"hit points {member['hp']}")
    expect(member["stats"][STAT_MAX_HP] == 90, f"max hit points {member['stats'][STAT_MAX_HP]}")

    diff = differences(stable(runner.lists)["player"], stable(after)["player"])
    expect(not diff, f"editing a companion changed the player: {diff[:8]}")
    return f"{member['name']}: level {before_member['level']} -> {member['level']}, HP 70/90"


def test_perk_scan(runner):
    """Every perk's card can be shown in the perk editor."""
    runner.run("perk_scan", "perk_scan.txt")
    return "all perks shown"


def test_game_screens(runner):
    """The game's own character screen and Pip-Boy still open in a game."""
    runner.run("game_screens", "game_screens.txt", ["--dev-load-game=1"])
    return "character screen, Pip-Boy"


def test_creation_screen(runner):
    """The game's own character creation screen still opens."""
    runner.run("creation_screen", "creation_screen.txt")
    return "creation screen"


TESTS = [
    test_new_game,
    test_menu,
    test_save_unchanged,
    test_cancel,
    test_edit_player,
    test_lists,
    test_edit_companion,
    test_perk_scan,
    test_game_screens,
    test_creation_screen,
]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--game-dir", default=os.environ.get("FALLOUT2_GAME_DIR"))
    parser.add_argument("--build-dir", default=os.environ.get("STIMPACK_BUILD_DIR", str(ROOT / "build" / "debug")))
    parser.add_argument("-k", dest="filter", help="run only tests whose name contains this (plus what they need)")
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args()

    if args.list:
        for test in TESTS:
            print(f"{test.__name__}: {test.__doc__.strip().splitlines()[0]}")
        return 0
    if not args.game_dir:
        parser.error("--game-dir (or FALLOUT2_GAME_DIR) is required: your Fallout 2 folder")

    runner = Runner(Path(args.build_dir).resolve(), Path(args.game_dir).expanduser().resolve())
    runner.prepare()

    selected = [t for t in TESTS if t is test_new_game or not args.filter or args.filter in t.__name__]
    if test_edit_companion in selected and test_lists not in selected:
        selected.insert(selected.index(test_edit_companion), test_lists)

    failures = 0
    for test in selected:
        started = time.time()
        try:
            note = test(runner)
            print(f"PASS {test.__name__:22} {time.time() - started:5.1f}s  {note}", flush=True)
        except TestFailure as error:
            failures += 1
            print(f"FAIL {test.__name__:22} {time.time() - started:5.1f}s  {error}", flush=True)
            if test is test_new_game:
                break
    print(f"{len(selected) - failures}/{len(selected)} passed; screenshots in {runner.run_dir / 'devout'}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
