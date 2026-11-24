#ifndef FALLOUT_STIMPAK_H_
#define FALLOUT_STIMPAK_H_

namespace fallout {

class FrmImage;

// Stimpak: savegame editor opened from the main menu.

// Message ids in `text/<language>/game/stimpak.msg`.
enum StimpakMessage : int {
    STIMPAK_MSG_MENU_BUTTON = 100,
    STIMPAK_MSG_CHARACTER = 101,
    STIMPAK_MSG_COMPANIONS = 102,
    STIMPAK_MSG_ROSTER = 103,
    STIMPAK_MSG_QUESTS = 104,
    STIMPAK_MSG_LOCATION = 105,
    STIMPAK_MSG_SAVE_AS = 106,
    STIMPAK_MSG_EXIT = 107,
    STIMPAK_MSG_PLAY = 108,
    STIMPAK_MSG_COMBAT_SAVE = 110,
    STIMPAK_MSG_DISCARD_CHANGES = 111,
    STIMPAK_MSG_PERKS_BUTTON = 120,
    STIMPAK_MSG_EDIT_LEVEL = 121,
    STIMPAK_MSG_EDIT_EXPERIENCE = 122,
    STIMPAK_MSG_EDIT_HIT_POINTS = 123,
    STIMPAK_MSG_EDIT_SKILL_POINTS = 124,
    STIMPAK_MSG_CAPS = 125,
    STIMPAK_MSG_EDIT_CAPS = 126,
    STIMPAK_MSG_PERK_EDITOR_TITLE = 130,
    STIMPAK_MSG_PERK_EDITOR_RANK = 131,
    STIMPAK_MSG_COMPANION_HEADER = 140,
    STIMPAK_MSG_COMPANION_NO_LEVELS = 141,
    STIMPAK_MSG_COMPANION_LEVEL_UP_HINT = 142,
    STIMPAK_MSG_COMPANION_LEVEL_UP = 143,
    STIMPAK_MSG_COMPANION_LEVEL_UP_CONFIRM = 144,
    STIMPAK_MSG_NO_COMPANIONS = 145,
    STIMPAK_MSG_BACK = 146,
    STIMPAK_MSG_DONE = 147,
    STIMPAK_MSG_COMPANION_INFO_FIRST = 150,
    STIMPAK_MSG_COMPANION_INFO_LAST = 155,
    STIMPAK_MSG_ROSTER_VALUE = 160,
    STIMPAK_MSG_IN_PARTY = 161,
    STIMPAK_MSG_ROSTER_CARD_MEMBER = 162,
    STIMPAK_MSG_ROSTER_CARD_OTHER = 163,
    STIMPAK_MSG_QUEST_VALUE = 170,
    STIMPAK_MSG_QUEST_NOT_STARTED = 171,
    STIMPAK_MSG_QUEST_ACTIVE = 172,
    STIMPAK_MSG_QUEST_DONE = 173,
    STIMPAK_MSG_QUEST_CARD = 174,
    STIMPAK_MSG_LOCATION_VALUE = 180,
    STIMPAK_MSG_AREA_UNKNOWN = 181,
    STIMPAK_MSG_AREA_KNOWN = 182,
    STIMPAK_MSG_AREA_VISITED = 183,
    STIMPAK_MSG_LOCATION_CARD = 184,
    STIMPAK_MSG_TRAVEL = 185,
    STIMPAK_MSG_TRAVEL_CONFIRM = 186,
    STIMPAK_MSG_TRAVEL_FAILED = 187,
};

// Returns the text of a message: from `text/<language>/game/stimpak.msg` when the
// game data has one (translations), else the built-in English text, else `fallback`.
const char* stimpakGetMessage(int messageId, const char* fallback);

// Drawing hooks, for graphics mods that draw their own version of the images
// Stimpak puts on screen (for example high-resolution art). Every member is
// optional.
struct StimpakRenderHooks {
    // `image` (its sub-rectangle at srcX, srcY of width x height) was just
    // drawn into `window` at destX, destY.
    void (*imageDrawn)(int window, const FrmImage& image, int srcX, int srcY, int width, int height, int destX, int destY) = nullptr;
    // A button was just made from these images (labels are drawn into copies).
    void (*buttonArt)(int button, const FrmImage* normal, const FrmImage* pressed) = nullptr;
};
void stimpakSetRenderHooks(const StimpakRenderHooks& hooks);

// Used by Stimpak's screens (and its character editor mode) to call the hooks.
void stimpakNotifyImageDrawn(int window, const FrmImage& image, int srcX, int srcY, int width, int height, int destX, int destY);
void stimpakNotifyButtonArt(int button, const FrmImage* normal, const FrmImage* pressed);

// How the editor was left.
enum class StimpakResult {
    Exit, // back to the main menu, nothing saved
    Saved, // saved with SAVE AS, back to the main menu
    Play, // PLAY: the edited game goes on in the game loop (saved or not)
};

// Runs the editor on the game that was just loaded from the main menu (map live,
// game loop not running). On Exit and Saved the caller tears the game down as
// after leaving a game; on Play it runs the game loop on the edited game.
StimpakResult stimpakRun();

} // namespace fallout

#endif /* FALLOUT_STIMPAK_H_ */
