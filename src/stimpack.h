#ifndef FALLOUT_STIMPACK_H_
#define FALLOUT_STIMPACK_H_

namespace fallout {

class FrmImage;

// Stimpack: savegame editor opened from the main menu.

// Message ids in `text/<language>/game/stimpack.msg`.
enum StimpackMessage : int {
    STIMPACK_MSG_MENU_BUTTON = 100,
    STIMPACK_MSG_CHARACTER = 101,
    STIMPACK_MSG_COMPANIONS = 102,
    STIMPACK_MSG_ROSTER = 103,
    STIMPACK_MSG_QUESTS = 104,
    STIMPACK_MSG_LOCATION = 105,
    STIMPACK_MSG_SAVE_AS = 106,
    STIMPACK_MSG_EXIT = 107,
    STIMPACK_MSG_COMBAT_SAVE = 110,
    STIMPACK_MSG_DISCARD_CHANGES = 111,
    STIMPACK_MSG_PERKS_BUTTON = 120,
    STIMPACK_MSG_EDIT_LEVEL = 121,
    STIMPACK_MSG_EDIT_EXPERIENCE = 122,
    STIMPACK_MSG_EDIT_HIT_POINTS = 123,
    STIMPACK_MSG_EDIT_SKILL_POINTS = 124,
    STIMPACK_MSG_CAPS = 125,
    STIMPACK_MSG_EDIT_CAPS = 126,
    STIMPACK_MSG_PERK_EDITOR_TITLE = 130,
    STIMPACK_MSG_PERK_EDITOR_RANK = 131,
    STIMPACK_MSG_COMPANION_HEADER = 140,
    STIMPACK_MSG_COMPANION_NO_LEVELS = 141,
    STIMPACK_MSG_COMPANION_LEVEL_UP_HINT = 142,
    STIMPACK_MSG_COMPANION_LEVEL_UP = 143,
    STIMPACK_MSG_COMPANION_LEVEL_UP_CONFIRM = 144,
    STIMPACK_MSG_NO_COMPANIONS = 145,
    STIMPACK_MSG_BACK = 146,
    STIMPACK_MSG_DONE = 147,
    STIMPACK_MSG_COMPANION_INFO_FIRST = 150,
    STIMPACK_MSG_COMPANION_INFO_LAST = 155,
    STIMPACK_MSG_ROSTER_VALUE = 160,
    STIMPACK_MSG_IN_PARTY = 161,
    STIMPACK_MSG_ROSTER_CARD_MEMBER = 162,
    STIMPACK_MSG_ROSTER_CARD_OTHER = 163,
    STIMPACK_MSG_QUEST_VALUE = 170,
    STIMPACK_MSG_QUEST_NOT_STARTED = 171,
    STIMPACK_MSG_QUEST_ACTIVE = 172,
    STIMPACK_MSG_QUEST_DONE = 173,
    STIMPACK_MSG_QUEST_CARD = 174,
    STIMPACK_MSG_LOCATION_VALUE = 180,
    STIMPACK_MSG_AREA_UNKNOWN = 181,
    STIMPACK_MSG_AREA_KNOWN = 182,
    STIMPACK_MSG_AREA_VISITED = 183,
    STIMPACK_MSG_LOCATION_CARD = 184,
    STIMPACK_MSG_TRAVEL = 185,
    STIMPACK_MSG_TRAVEL_CONFIRM = 186,
    STIMPACK_MSG_TRAVEL_FAILED = 187,
};

// Returns the text of a message: from `text/<language>/game/stimpack.msg` when the
// game data has one (translations), else the built-in English text, else `fallback`.
const char* stimpackGetMessage(int messageId, const char* fallback);

// Drawing hooks, for graphics mods that draw their own version of the images
// Stimpack puts on screen (for example high-resolution art). Every member is
// optional.
struct StimpackRenderHooks {
    // `image` (its sub-rectangle at srcX, srcY of width x height) was just
    // drawn into `window` at destX, destY.
    void (*imageDrawn)(int window, const FrmImage& image, int srcX, int srcY, int width, int height, int destX, int destY) = nullptr;
    // A button was just made from these images (labels are drawn into copies).
    void (*buttonArt)(int button, const FrmImage* normal, const FrmImage* pressed) = nullptr;
};
void stimpackSetRenderHooks(const StimpackRenderHooks& hooks);

// Used by Stimpack's screens (and its character editor mode) to call the hooks.
void stimpackNotifyImageDrawn(int window, const FrmImage& image, int srcX, int srcY, int width, int height, int destX, int destY);
void stimpackNotifyButtonArt(int button, const FrmImage* normal, const FrmImage* pressed);

// Runs the editor on the game that was just loaded from the main menu (map live,
// game loop not running). Returns true when the game was saved. The caller tears
// the game down afterwards exactly as after leaving a game.
bool stimpackRun();

} // namespace fallout

#endif /* FALLOUT_STIMPACK_H_ */
