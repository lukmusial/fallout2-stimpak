# Changelog

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
