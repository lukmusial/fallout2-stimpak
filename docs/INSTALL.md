# Installing Stimpak

Stimpak is a complete build of Fallout 2 Community Edition (CE) with the savegame
editor added. It installs exactly like CE and uses the same game folder, settings
and saved games. It runs on your own copy of Fallout 2 (GOG, Steam, Epic or
the original CD).

Download the package for your system from
[Releases](https://github.com/lukmusial/fallout2-stimpak/releases/latest).
Each package holds:

| File | What it is |
| --- | --- |
| the game (`fallout2-ce.exe`, `fallout2-ce` or `Fallout II Community Edition.app`) | CE with Stimpak |
| `ce.dat` (Windows, Linux) | CE's own data (inside the .app on macOS) |
| `EXAMPLE_fallout2.cfg` | CE's example settings, for a first install |
| `translations/english/stimpak.msg` | Stimpak's texts, for translators |
| `README.md`, `INSTALL.md`, `LICENSE.md` | documentation and license |

If you already play with CE, keep a copy of your CE executable: Stimpak replaces
it, and you can switch back at any time. Saved games work in both.

## Windows

1. Unzip `stimpak-<version>-windows-x64.zip`.
2. Copy `fallout2-ce.exe` and `ce.dat` into your Fallout 2 folder (the one with
   `master.dat` and `critter.dat`), replacing CE's if it is installed.
3. Start `fallout2-ce.exe`.

## macOS

1. Get the game data into a folder, for example `/Applications/Fallout2`, as
   described in [CE's macOS instructions](https://github.com/fallout2-ce/fallout2-ce#macos)
   (a Windows installation copied over, the GOG installer through `innoextract`,
   or the Mac CD's `GameData`).
2. Unzip `stimpak-<version>-macos.zip` and copy `Fallout II Community Edition.app`
   into that folder, next to `master.dat`.
3. The app carries its own (ad-hoc) signature rather than an Apple-notarised
   one, so on first start macOS asks before opening it:
   * macOS 15 (Sequoia) and later: open the app once, close the message, then
     go to **System Settings → Privacy & Security**, scroll to *Security* and
     choose **Open Anyway** next to "Fallout II Community Edition", and confirm.
   * macOS 14 and earlier: right-click (or Control-click) the app, choose
     **Open**, then **Open** again.

   Later starts work as usual. In Terminal,
   `xattr -dr com.apple.quarantine "Fallout II Community Edition.app"` does the
   same in one step.

## Linux

1. Get the game data into a folder, for example `~/Games/Fallout2` (a Windows
   installation copied over, or the GOG installer through `innoextract`).
2. Extract `stimpak-<version>-linux-x64.tar.gz` and copy `fallout2-ce` and
   `ce.dat` into that folder.
3. Run `./fallout2-ce` from that folder. SDL2 is built into the game; it
   needs a 64-bit x86 distribution with glibc 2.35 or later.

## Restoration Project

Stimpak works on a game with the
[Fallout 2 Restoration Project](https://github.com/BGforgeNet/Fallout2_Restoration_Project)
(RPU) installed; nothing to choose or set. Install RPU first as its
instructions say, then copy Stimpak into the same folder as above. CE takes the
place of RPU's `fallout2.exe` and sfall: it loads RPU's `mods/*.dat` in the
order of `mods/mods_order.txt` and reads `ddraw.ini`.

Saved games from before RPU was installed load as they are: Stimpak sees that
a save holds the unmodded game's lists and reads it with RPU's new global
variables and companions at their starting values. Saving such a game writes an
ordinary RPU save. On macOS, RPU's `rpu-install.command` moves `data/SAVEGAME`
to `backup/rpu/data/SAVEGAME`; copy the slot folders back to `data/SAVEGAME` to
see them in the game.

RPU recommends a new game: places you have already visited keep their state
from your save, so RPU's changes there show only in places you have not been to.

## Settings

Stimpak has no settings of its own. All of CE's settings in `fallout2.cfg` (and
`ddraw.ini`) apply as usual; `EXAMPLE_fallout2.cfg` is CE's documented example. If
the game folder has no `fallout2.cfg` yet, CE creates one.

## Uninstalling

Put your previous CE executable (or the original game's) back. Saved games made
with Stimpak are ordinary saved games and keep working.
