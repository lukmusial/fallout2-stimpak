# Using Stimpak

## Opening a saved game

Choose **STIMPAK** in the main menu (hotkey **S**). The game's load screen opens:
pick a saved game as you would to play it. The game is loaded, but nothing moves:
game time is stopped and no scripts run while you edit.

A game saved during combat cannot be edited (Stimpak says so and returns to the
main menu): saving it again without finishing the fight would keep a
half-restored combat in the save.

## The hub

The hub lists the editor's screens. Click one or press its hotkey:

| Entry | Key | |
| --- | --- | --- |
| CHARACTER | C | your character |
| COMPANIONS | O | the companions in the party |
| ROSTER | R | who is in the party |
| QUESTS | Q | quest states |
| LOCATION | L | towns on the world map, travel |
| SAVE AS | S | save the edited game |
| PLAY | P | play the edited game now, without saving first |
| EXIT | E / Esc | back to the main menu |

Edits live in the loaded game. **PLAY** closes the editor and starts the game
loop on the edited game, exactly as if it had just been loaded: nothing is
saved until you save in the game. **SAVE AS** opens the game's save screen on
the first empty slot; choose any slot, type a description and press DONE, and
Stimpak returns to the main menu. **EXIT** after unsaved changes asks *Leave
without saving your changes?*

## Character

The game's character screen, with everything editable:

* **SPECIAL**: the + and - buttons next to each stat, from 1 to 10. Values from
  traits and perks are added by the game as usual, and the total is capped at 10.
* **Skills**: select a skill, then use the slider's + and - buttons. Skill points
  are not spent; a skill can go up to the game's maximum (300%). Tagged skills
  rise faster, as in the game.
* **Tag skills** and **traits**: click to toggle, as in character creation.
* **Perks**: the PERKS button (or **P**) opens a list of every perk with its rank.
  Up and down choose a perk, + and - change its rank, and the card on the right
  describes it. DONE (Enter) keeps the changes.
* **Typed values**: click a line to type a new value.
  * **Level** (level box, bottom left) sets the level and the experience that
    goes with it, up to the game's highest level.
  * **Exp** sets the experience; the level follows.
  * **Caps** (third line of the level box) sets the bottle caps you carry.
  * **Hit Points** (first line of the box at the top) takes the current hit
    points, or *current/maximum* (for example `70/90`) to change the maximum too.
  * **Skill points** (the counter under the skills) sets the unspent skill points.

DONE (Enter) keeps the edits; CANCEL (Esc) puts everything back as it was when the
screen opened. Level-up screens, the perk pop-up and the full heal that the game's
own level-up would do do not happen.

## Companions

The list shows the companions in the party. Choosing one opens the same character
screen for them, without the parts only the player has (traits, tag skills, karma,
kills).

* Stats, skills, hit points and perks are edited as for the player.
* The level line shows the companion's level. Clicking it (and confirming) levels
  the companion up now: the game replaces their stats and skills with those of the
  next level, exactly as a level-up in play does. Companions without levels say
  *No levels*.
* The game replaces a companion's stats and skills on each of their level-ups.
  Edits therefore last until the next level-up; edits made at their last level
  stay for good. Level them up first, then edit.

## Roster

Every companion the game knows, with **IN PARTY** next to those in the party.
Letters jump in the list. **+** (or the JOIN button) makes the selected companion
join the party now: they appear next to you. **-** makes them leave the party and
this saved game.

This screen is **experimental**. A companion who joins here has not been through
the game's own recruitment, so their dialogue may not know about it, and the
original character elsewhere in the world is removed when that map is next
loaded. Test the save before playing on with it.

## Quests

All quests, grouped by town, with their state: **-** (not started), **ACTIVE** or
**DONE**. **+** and **-** step through the states.

A quest's state is a game variable. Changing it changes what the Pip-Boy shows
and what scripts that check it see, but the game does not hand out rewards, items
or map changes that finishing the quest would have made.

## Location

All towns of the world map with their state: **-** (unknown), **KNOWN** (on the
map) or **VISITED**. Letters jump in the list; **+** and **-** step through the
states. "> " marks the town you are in.

**TRAVEL** moves the party to the selected town's first entrance now (after a
confirmation). The game loads that map, and the saved game will start there.

## Keys in lists

Up and down move the selection, Page Up and Page Down move a page, letters jump to
the next entry starting with that letter, + and - change the value, Enter or DONE
closes the screen.
