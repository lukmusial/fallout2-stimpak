# Changelog

## 1.1.0

One package for the unmodded game and the Fallout 2 Restoration Project (RPU).

* Saved games from before RPU (or another mod with more global variables and
  party members) was installed load on the modded game, from STIMPAK and from
  LOAD GAME. Stimpak sees that a save holds the 1.02 patch's 696 global
  variables and 26 party members, reads those, and leaves the mod's additions
  at their starting values (moving the one variable RPU moved). Saving writes
  an ordinary save of the modded game. A message in the game says when a save
  is read this way.
* Tested on RPU v2.4.34: all tests pass with RPU installed, and ten saves from
  before RPU load with the same player, party, quests and world as on the
  unmodded game.
* Tests: `--game-dir` may be a modded game folder; `test_old_saves` loads
  saves made without the mod; an unchanged save is compared with the game's
  own re-save (RPU's scripts change a variable on the first load).
* INSTALL.md: installing on RPU, and where RPU's installer moves your saves.
* Location: the TRAVEL button sits inside the card, right-aligned, instead of
  on the card's lower edge where its label was cut off.

## 1.0.2

* The high-resolution main menu (screens larger than 640x480 with the game's
  high-resolution art) shows the seventh button plate under EXIT again. The
  menu art generator loaded the original panel by name, which overwrote the
  engine's shared name buffer while the menu was still looking its panel up,
  so the menu received the six-plate original. The generator now loads by
  path, and the engine's named-art cache keeps its own copy of the name.
* Test: `test_menu_hires` checks the seventh plate at 2560x1200.

## 1.0.1

The game is the same as in 1.0.0; this release updates the instructions and
tests that come with it.

* INSTALL.md: first start of the macOS app on macOS 15 and later goes through
  **System Settings → Privacy & Security → Open Anyway** (right-click → Open no
  longer applies to apps that are not notarised); the older route and a
  Terminal one-liner are described too.
* README: screenshots of every screen, the Community Edition version Stimpak is
  built on, supported platforms and where to download the packages.
* The test suite runs on Windows (tested on Windows 11 ARM64 with the x64
  package) as well as macOS.

## 1.0.0

First public release.

* STIMPAK button in the main menu: open any saved game in the editor, then play
  the edited game at once (PLAY) or save the result to any slot (SAVE AS).
* Character: SPECIAL, skills, tag skills, traits, perks with ranks, level,
  experience, hit points, unspent skill points, bottle caps.
* Companions: stats, skills, hit points, perks; level up on the spot.
* Roster (experimental): companions join or leave the party.
* Quests: not started, active or done.
* Location: towns unknown, known or visited; travel to any town.
* Built-in English texts; translations through `stimpak.msg`.
* Menu art made at run time from the player's game files; no game art shipped.
* The interface bar is always shown after loading a game (early test builds
  could save it hidden).
* Saves from Fallout 2 Community Edition 1.3, whose sfall section is incomplete,
  load again.
* Builds against an unmodified Fallout 2 Community Edition checkout (patches
  applied at build time); hooks for graphics mods and test code.
* Automated tests that create their own saved games from a new game.
