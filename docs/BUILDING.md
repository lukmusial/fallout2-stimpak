# Building and testing Stimpak

## Requirements

* CMake 3.19 or newer, a C++17 compiler (Clang, GCC or Visual Studio 2022)
* Python 3.8 or newer (applies the patches at build time; no packages needed)
* git, and `patch` (included with macOS, Linux and Git for Windows)
* Linux: `libsdl2-dev` and `zlib1g-dev`; macOS and Windows build their own SDL2
* Ninja is used when installed (faster); otherwise CMake's default generator

## Building

```sh
git clone --recurse-submodules <this repository> stimpak
cd stimpak
tools/build.sh Release            # build/release
tools/build.sh Debug              # build/debug
tools/package.sh                  # release archive for this system, in dist/
```

On Windows, run the scripts from Git Bash.

`tools/build.sh` is a thin wrapper around:

```sh
cmake -S engine -B build/release -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PROJECT_fallout2-ce_INCLUDE=$PWD/cmake/StimpakProjectInclude.cmake
cmake --build build/release --target fallout2-ce
```

## How the build works

The repository has three parts:

| Path | |
| --- | --- |
| `engine/` | Fallout 2 Community Edition as a git submodule, at the commit in `engine-version.txt`; the build reads it as it is. |
| `src/` | Stimpak's own sources (the editor, its menu art, the test harness). |
| `patches/src/<file>.patch` | One patch per engine file Stimpak changes: small hooks (the menu button, the editor mode of the character screen, accessors for quests, towns and companions, the test harness hooks). |

`cmake/Stimpak.cmake` (`stimpak_attach(<target>)`) runs `tools/stimpak_tree.py`
when the build is configured and again before every build. The tool mirrors the
engine's `src/` into `<build>/stimpak-tree/src`, applies the patches there, adds
Stimpak's own sources, and writes only files whose content changed. The game
target is then compiled from that tree. Unpatched files carry a `#line` directive,
so compiler messages and debuggers point at the original files.

`python3 tools/stimpak_tree.py --engine engine --check` reports whether the
patches apply, without building.

Stimpak makes the art it needs (the main menu with a seventh button and its
options-style panel) at run time from the player's own game files
(`src/stimpak_art.cc`), so the repository and the packages hold only Stimpak's code.

## Tests

The tests drive a real build through the game's scripted test mode and need a
copy of Fallout 2's data, which they only read:

```sh
tools/build.sh Debug
tests/run_tests.py --game-dir "/path/to/Fallout 2"      # or set FALLOUT2_GAME_DIR
tests/run_tests.py --list                               # what is tested
tests/run_tests.py --game-dir ... -k companion          # some of them
```

The game runs hidden and silent (it does not take keyboard focus), one run per
script. The run directory is `<build>/test-run`; it links the game data, keeps
its own `data/` for saves and settings, and holds the screenshots of every run in
`devout/<test>/` for review.

The tests make their own saved games: `test_new_game` starts a new game with the first
premade character and saves it in slot 1; the other tests open that save (or
saves made by earlier tests) in Stimpak, edit it through the user interface,
save it, load the result and compare dumps of the game state. The checks are
relative (for example "Strength went up by 3, capped at 10; nothing but stats,
skills, perks, level and caps changed"), so they do not depend on the premade
character, the language of the game data or its art. Game scripts change a few
global variables every time a game loads; `test_save_unchanged` measures which
and the comparisons leave them out.

The tests assume the game's English names where a script jumps to a list entry
by its first letter (Sulik in the roster, Klamath in the town list).

### The test harness

`--dev-script=<file>` makes the game follow a script; `--dev-out=<dir>` sets
where screenshots and dumps go, `--dev-fast` makes palette fades instant and
`--dev-load-game=<slot>` loads a saved game at start. The commands (`wait`,
`until <state>`, `key`, `text`, `move`, `click`, `shot`, `dump`, `log`, `quit`)
are described in `src/dev_automation.h`, and the scripts in `tests/scripts/` are
examples. Other code can add commands (see INTEGRATING.md).

## Working on Stimpak

The patches are not edited by hand. Make a development checkout, work in it like
in any engine checkout, and export the result:

```sh
tools/dev_checkout.py ../stimpak-dev     # engine + patches + sources, committed
cmake -S ../stimpak-dev -B ../stimpak-dev/build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build ../stimpak-dev/build --target fallout2-ce
tests/run_tests.py --build-dir ../stimpak-dev/build --game-dir ...
tools/export_patches.py ../stimpak-dev   # back into src/, patches/ and data/
```

`export_patches.py` copies new files under `src/` into this repository's `src/`
and writes one patch per changed engine file. Changes to the checkout's
CMakeLists.txt are not exported (Stimpak.cmake adds the sources).

### Moving to a newer engine

1. Update the submodule (`git -C engine checkout <commit>`) and `engine-version.txt`.
2. `python3 tools/stimpak_tree.py --engine engine --check`. If a patch no longer
   applies, make a development checkout at the old commit, rebase its
   `stimpak-dev` branch onto the new commit, resolve the conflicts, and export
   with `--base <new commit>`.
3. Build and run the tests.
