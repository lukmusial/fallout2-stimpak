# Stimpak

Stimpak is a savegame editor that runs inside [Fallout 2 Community Edition](https://github.com/fallout2-ce/fallout2-ce).
It adds a **STIMPAK** button to the main menu. Pick a saved game, change your
character, your companions, the party, quests or where you are, then save the
result as a new saved game. The original save is never overwritten unless you
choose its slot yourself.

The editor uses the game's own screens and rules, so what you edit is what the game
will load: stats are capped where the game caps them, derived values (hit points,
carry weight, action points and so on) are recalculated by the game, and the save
is written by the game's own save code.

## What you can edit

| Screen | What it changes |
| --- | --- |
| **Character** | The seven SPECIAL stats (1 to 10), skills (no skill-point cost), tag skills, traits, perks (add or remove any perk and rank), level, experience, hit points, unspent skill points and money (bottle caps). |
| **Companions** | Each companion's stats, skills, hit points and perks. A companion can be levelled up on the spot. |
| **Roster** | Who is in the party: a companion can join or leave. *Experimental*, see [Limits](#limits). |
| **Quests** | Any quest can be set to not started, active or done. |
| **Location** | Towns can be marked unknown, known or visited on the world map, and the party can travel to any town at once. |

Then **PLAY** goes straight into the game with your changes, without saving
first; **SAVE AS** opens the game's save screen on the first empty slot; **EXIT**
returns to the main menu (if you leave with unsaved changes, Stimpak asks you to
confirm).

## Installing

You need your own copy of Fallout 2 (GOG, Steam or the original CD release).
Stimpak is a complete build of Fallout 2 Community Edition with the editor added:
it replaces the CE executable, or plays a vanilla installation.

1. Download the package for your system from the releases page:
   `stimpak-<version>-macos.zip`, `-windows-x64.zip` or `-linux-x64.tar.gz`.
2. Follow [docs/INSTALL.md](docs/INSTALL.md) (also inside the package). In short:
   put the executable next to your game's `master.dat` and `critter.dat`, the same
   way Fallout 2 Community Edition is installed.

Your saved games stay where they are. They are compatible both ways with Fallout 2
Community Edition: Stimpak only adds the editor and writes ordinary saves.

## Using it

1. Start the game and choose **STIMPAK** in the main menu (or press **S**).
2. Pick a saved game in the load screen. Games saved during combat cannot be edited.
3. The Stimpak hub opens over the loaded map. Choose a screen (each has a hotkey:
   **C**haracter, c**O**mpanions, **R**oster, **Q**uests, **L**ocation).
4. When you are done, choose **PLAY** to play on at once (save in the game as
   usual whenever you like), or **SAVE AS** to keep the result as a saved game,
   pick a slot and type a description, and load it later from **LOAD GAME**.

[docs/USAGE.md](docs/USAGE.md) describes every screen and key.

## Limits

* **Roster changes are experimental.** A companion who joins through Stimpak has
  not been through the game's own recruitment. Their dialogue may not know about
  it, and the original character elsewhere in the world is removed when that map
  is next loaded. Removing a companion takes them out of the game for that save.
* **Quests**: changing a quest's state changes the game variable behind it. The
  Pip-Boy and scripts that check it follow, but rewards, items and changes to maps
  that the quest would have made are not made.
* **Companion edits** last until the companion's next level-up, when the game
  applies its own values for that level. Edits made at their last level stay.
* **Travel** moves the party to the town's first entrance; the saved game starts
  there.
* Games saved during combat cannot be edited (saving them again would keep a
  half-restored combat).

Back up the saves you care about. Stimpak saves into a slot you choose, but a
saved game edited into a state the story does not expect can make quests
impossible to finish.

## Building from source

```sh
git clone --recurse-submodules <this repository> stimpak
cd stimpak
tools/build.sh Release          # or Debug
tools/package.sh                # the release archive for this system, in dist/
```

You need CMake 3.19 or newer, a C++17 compiler, Python 3.8 or newer and git. See
[docs/BUILDING.md](docs/BUILDING.md) for platforms, tests and how the build works.

## Adding Stimpak to another build of the game

Stimpak does not fork the engine. It is a set of its own source files plus small
patches to the engine's files (`patches/`), applied to a copy of the sources at
build time. Another project built on Fallout 2 Community Edition can include it as
a git submodule and add two lines to its CMakeLists.txt. Graphics mods can hook
into its screens, and test code can add commands to its test harness. See
[docs/INTEGRATING.md](docs/INTEGRATING.md).

## Translations

The English texts are built in. To translate Stimpak, copy
`data/text/english/game/stimpak.msg` to `data/text/<language>/game/stimpak.msg`
in your game folder (`<language>` is the game's language folder, for example
`german`) and translate the texts in the last pair of braces of each line. Lines
that are missing fall back to English.

## Testing

`tests/run_tests.py` drives a real build of the game through a scripted, hidden
test mode. It starts a new game, saves it, edits it with Stimpak in many ways and
checks the results in dumps of the game state. It needs your own Fallout 2 data
and ships no saved games or game art. See [docs/BUILDING.md](docs/BUILDING.md#tests).

## License

Stimpak is built on Fallout 2 Community Edition and is distributed under the same
[Sustainable Use License](LICENSE.md). Fallout 2 and its data belong to their
owners; no game data is included in this repository or its packages.
