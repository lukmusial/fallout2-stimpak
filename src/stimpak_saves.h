#ifndef FALLOUT_STIMPAK_SAVES_H_
#define FALLOUT_STIMPAK_SAVES_H_

#include "db.h"

namespace fallout {

// Saved games from the unmodded game (the 1.02 patch: 696 global variables,
// 26 party members) on a game whose lists are longer, such as the Fallout 2
// Restoration Project (791 and 29). Such a save holds the shorter lists and
// the engine would read it with the longer ones, which fails. While one is
// loaded, the load functions read the lists at the length the save has; the
// rest keep the values a new game starts with. The next save is written with
// the game's own lists, so it is a save of the modded game.

// Before the load functions run: looks at the save (SAVEGAME\SLOTxx\SAVE.DAT,
// already known to exist) and decides how to read it.
void stimpakSavesBeginLoad(const char* path);

// After the load functions (whether they succeeded or not).
void stimpakSavesEndLoad(bool loaded);

// The number of party member records the save holds (`current`: the game's).
int stimpakSavesPartyCount(int current);

// Reads one copy of the game's global variables (`keep`: into vars, else
// skipped), at the length the save holds.
int stimpakSavesReadGlobals(File* stream, int* vars, int length, bool keep);

// True if the last load read a save from before the mod.
bool stimpakSavesLastLoadWasLegacy();

} // namespace fallout

#endif /* FALLOUT_STIMPAK_SAVES_H_ */
