#include "stimpak.h"

#include <ctype.h>
#include <string.h>

#include <algorithm>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "art.h"
#include "character_editor.h"
#include "color.h"
#include "combat.h"
#include "critter.h"
#include "dbox.h"
#include "debug.h"
#include "dev_automation.h"
#include "draw.h"
#include "game.h"
#include "game_mouse.h"
#include "game_sound.h"
#include "graph_lib.h"
#include "input.h"
#include "interface.h"
#include "kb.h"
#include "loadsave.h"
#include "map.h"
#include "memory.h"
#include "message.h"
#include "mouse.h"
#include "object.h"
#include "party_member.h"
#include "pipboy.h"
#include "platform_compat.h"
#include "proto.h"
#include "proto_instance.h"
#include "scripts.h"
#include "stimpak_art.h"
#include "svga.h"
#include "text_font.h"
#include "tile.h"
#include "window_manager.h"
#include "word_wrap.h"
#include "worldmap.h"

namespace fallout {

namespace {

    // Hub entries, top to bottom. Event codes are 600 + entry.
    enum HubEntry : int {
        HUB_ENTRY_CHARACTER,
        HUB_ENTRY_COMPANIONS,
        HUB_ENTRY_ROSTER,
        HUB_ENTRY_QUESTS,
        HUB_ENTRY_LOCATION,
        HUB_ENTRY_SAVE_AS,
        HUB_ENTRY_PLAY,
        HUB_ENTRY_EXIT,
        HUB_ENTRY_COUNT,
    };

    constexpr int kHubEventBase = 600;

    struct HubEntrySpec {
        int messageId;
        const char* fallback;
        int hotkeyLower;
        bool implemented;
    };

    constexpr HubEntrySpec kHubEntries[HUB_ENTRY_COUNT] = {
        { STIMPAK_MSG_CHARACTER, "CHARACTER", KEY_LOWERCASE_C, true },
        { STIMPAK_MSG_COMPANIONS, "COMPANIONS", KEY_LOWERCASE_O, true },
        { STIMPAK_MSG_ROSTER, "ROSTER", KEY_LOWERCASE_R, true },
        { STIMPAK_MSG_QUESTS, "QUESTS", KEY_LOWERCASE_Q, true },
        { STIMPAK_MSG_LOCATION, "LOCATION", KEY_LOWERCASE_L, true },
        { STIMPAK_MSG_SAVE_AS, "SAVE AS", KEY_LOWERCASE_S, true },
        { STIMPAK_MSG_PLAY, "PLAY", KEY_LOWERCASE_P, true },
        { STIMPAK_MSG_EXIT, "EXIT", KEY_LOWERCASE_E, true },
    };

    // Panel art: opbase_sp.png (options panel with nine slots, made from
    // opbase.frm by stimpak_art.cc). Slot 0 is the header, slots 1..8 the entries.
    constexpr int kPanelFirstSlotY = 17;
    constexpr int kPanelSlotPitch = 37;
    constexpr int kPanelSlotX = 13;
    constexpr int kHeaderFont = 101;
    constexpr int kButtonFont = 103;

    // English texts; text/<language>/game/stimpak.msg overrides them.
    const std::pair<int, const char*> kStimpakDefaultMessages[] = {
        { 100, "STIMPAK" },
        { 101, "CHARACTER" },
        { 102, "COMPANIONS" },
        { 103, "ROSTER" },
        { 104, "QUESTS" },
        { 105, "LOCATION" },
        { 106, "SAVE AS" },
        { 107, "EXIT" },
        { 108, "PLAY" },
        { 110, "This game was saved during combat and cannot be edited." },
        { 111, "Leave without saving your changes?" },
        { 120, "PERKS" },
        { 121, "LEVEL" },
        { 122, "EXPERIENCE" },
        { 123, "HIT POINTS (CUR/MAX)" },
        { 124, "SKILL POINTS" },
        { 125, "Caps:" },
        { 126, "CAPS" },
        { 130, "PERKS" },
        { 131, "RANK" },
        { 140, "COMPANION" },
        { 141, "No levels" },
        { 142, "Click: LEVEL UP" },
        { 143, "LEVEL UP?" },
        { 144, "Stats and skills become the next level's." },
        { 145, "There are no companions in this game." },
        { 146, "BACK" },
        { 147, "DONE" },
        { 150, "Companions have no traits or tag skills." },
        { 151, "Edited stats and skills are kept until" },
        { 152, "the companion's next level-up, which" },
        { 153, "applies the game's values for that level." },
        { 154, "Click the level line to level up now." },
        { 155, "" },
        { 160, "JOIN" },
        { 161, "IN PARTY" },
        { 162, "In the party.\\n- : leaves the party and this save.\\nTo bring the companion back, use + (join) again." },
        { 163, "Not in the party.\\n+ : joins now and appears next to you.\\nEXPERIMENTAL: the companion joins without the game's own story steps; their dialogue may not know about it, and the original character elsewhere is removed when that map is next loaded." },
        { 170, "STATE" },
        { 171, "-" },
        { 172, "ACTIVE" },
        { 173, "DONE" },
        { 174, "+/- : not started, ACTIVE, DONE. Changes the quest's variable: the Pip-Boy and scripts that check it follow. Rewards, items and changes to maps are not made." },
        { 180, "STATE" },
        { 181, "-" },
        { 182, "KNOWN" },
        { 183, "VISITED" },
        { 184, "Letters jump in the list.\\n+/- : unknown, known (on the world map), visited.\\nTRAVEL: the party moves to this town's entrance now; the saved game starts there. \"> \" marks the current town." },
        { 185, "TRAVEL" },
        { 186, "Move the party to this town now?" },
        { 187, "The party could not travel there." },
    };

    MessageList gStimpakMessageList;
    bool gStimpakMessageListLoaded = false;

    constexpr int kPanelMaxEntries = 8;

    // One plate of a panel: a button, or a dimmed plate when not enabled.
    struct PanelEntry {
        std::string label;
        bool enabled;
    };

    // Options-style panel: header slot with two text lines, then up to seven
    // plate buttons sending eventBase + index. Used by the hub and its pickers.
    struct Hub {
        int window = -1;
        unsigned char* buffer = nullptr;
        FrmImage background;
        FrmImage buttonUp;
        FrmImage buttonDown;
        // Normal and pressed images for each entry, with the label drawn in.
        unsigned char* images[kPanelMaxEntries][2] = {};
        int oldFont = 0;
        bool cursorWasHidden = false;
        bool mouseObjectsWereVisible = false;
    };

    void hubFree(Hub& hub)
    {
        if (hub.window != -1) {
            windowDestroy(hub.window);
            hub.window = -1;
        }
        for (auto& pair : hub.images) {
            for (unsigned char*& image : pair) {
                if (image != nullptr) {
                    internal_free(image);
                    image = nullptr;
                }
            }
        }
        hub.background.unlock();
        hub.buttonUp.unlock();
        hub.buttonDown.unlock();
        fontSetCurrent(hub.oldFont);

        if (hub.mouseObjectsWereVisible) {
            gameMouseObjectsShow();
        }
        if (hub.cursorWasHidden) {
            mouseHideCursor();
        }
    }

    // Header slot: two short lines (for the hub: character name and location).
    void hubDrawHeader(Hub& hub, const char* line1, const char* line2)
    {
        int width = hub.background.getWidth();
        fontSetCurrent(kHeaderFont);
        int lineHeight = fontGetLineHeight();
        int top = kPanelFirstSlotY + (kPanelSlotPitch - 3 - 2 * lineHeight) / 2;

        const char* lines[2] = { line1, line2 };
        for (int index = 0; index < 2; index++) {
            const char* text = lines[index] != nullptr ? lines[index] : "";
            int textWidth = fontGetStringWidth(text);
            int x = std::max(0, (width - textWidth) / 2);
            fontDrawText(hub.buffer + width * (top + index * lineHeight) + x, text, width - x, width, COLOR_GREEN);
        }
    }

    bool hubInit(Hub& hub, const char* header1, const char* header2, const std::vector<PanelEntry>& entries, int eventBase)
    {
        hub.oldFont = fontGetCurrent();

        stimpakArtInit();
        if (!hub.background.lock(InterfaceFrmId(kStimpakHubPanelArt))
            || !hub.buttonUp.lock(InterfaceFrameId::OptionsButtonOff)
            || !hub.buttonDown.lock(InterfaceFrameId::OptionsButtonOn)) {
            debugPrint("STIMPAK: missing panel art\n");
            return false;
        }

        int buttonWidth = hub.buttonUp.getWidth();
        int buttonHeight = hub.buttonUp.getHeight();
        size_t buttonSize = static_cast<size_t>(buttonWidth) * buttonHeight;

        // Panels with fewer entries drop the unused slots: the art's top part
        // down to the last used slot, then its bottom border.
        int entryCount = std::min(static_cast<int>(entries.size()), kPanelMaxEntries);
        int panelWidth = hub.background.getWidth();
        int artHeight = hub.background.getHeight();
        int keptTop = kPanelFirstSlotY + (entryCount + 1) * kPanelSlotPitch;
        int border = artHeight - (kPanelFirstSlotY + (kPanelMaxEntries + 1) * kPanelSlotPitch);
        int panelHeight = std::min(artHeight, keptTop + border);
        int x = (screenGetWidth() - panelWidth) / 2;
        int y = (screenGetHeight() - panelHeight) / 2;
        hub.window = windowCreate(x, y, panelWidth, panelHeight, static_cast<ColorWithFlags>(256), WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);
        if (hub.window == -1) {
            return false;
        }

        hub.buffer = windowGetBuffer(hub.window);
        memcpy(hub.buffer, hub.background.getData(), static_cast<size_t>(panelWidth) * (panelHeight - border));
        memcpy(hub.buffer + static_cast<size_t>(panelWidth) * (panelHeight - border),
            hub.background.getData() + static_cast<size_t>(panelWidth) * (artHeight - border),
            static_cast<size_t>(panelWidth) * border);
        stimpakNotifyImageDrawn(hub.window, hub.background, 0, 0, panelWidth, panelHeight - border, 0, 0);
        stimpakNotifyImageDrawn(hub.window, hub.background, 0, artHeight - border, panelWidth, border, 0, panelHeight - border);
        hubDrawHeader(hub, header1, header2);

        fontSetCurrent(kButtonFont);
        for (int entry = 0; entry < entryCount; entry++) {
            const char* label = entries[entry].label.c_str();
            int textX = std::max(0, (buttonWidth - fontGetStringWidth(label)) / 2);
            int textY = (buttonHeight - fontGetLineHeight()) / 2 + 1;
            int slotY = kPanelFirstSlotY + (entry + 1) * kPanelSlotPitch;

            if (!entries[entry].enabled) {
                // Not available yet: draw the plate with a dim label and no button.
                blitBufferToBufferTrans(hub.buttonUp.getData(), buttonWidth, buttonHeight, buttonWidth,
                    hub.buffer + panelWidth * slotY + kPanelSlotX, panelWidth);
                stimpakNotifyImageDrawn(hub.window, hub.buttonUp, 0, 0, hub.buttonUp.getWidth(), hub.buttonUp.getHeight(), kPanelSlotX, slotY);
                fontDrawText(hub.buffer + panelWidth * (slotY + textY) + kPanelSlotX + textX, label, buttonWidth, panelWidth, COLOR_DARK_GREY_2);
                continue;
            }

            for (int state = 0; state < 2; state++) {
                hub.images[entry][state] = static_cast<unsigned char*>(internal_malloc(buttonSize));
                if (hub.images[entry][state] == nullptr) {
                    return false;
                }
                memcpy(hub.images[entry][state], (state == 0 ? hub.buttonUp : hub.buttonDown).getData(), buttonSize);
                fontDrawText(hub.images[entry][state] + buttonWidth * (textY + state) + textX,
                    label,
                    buttonWidth,
                    buttonWidth,
                    state == 0 ? COLOR_DARK_YELLOW : COLOR_DARK_YELLOW_2);
            }

            int button = buttonCreate(hub.window,
                kPanelSlotX,
                slotY,
                buttonWidth,
                buttonHeight,
                -1,
                -1,
                -1,
                eventBase + entry,
                hub.images[entry][0],
                hub.images[entry][1],
                nullptr,
                BUTTON_FLAG_TRANSPARENT);
            if (button != -1) {
                buttonSetCallbacks(button, _gsound_lrg_butt_press, _gsound_lrg_butt_release);
                stimpakNotifyButtonArt(button, &hub.buttonUp, &hub.buttonDown);
            }
        }

        hub.mouseObjectsWereVisible = gameMouseObjectsIsVisible();
        if (hub.mouseObjectsWereVisible) {
            gameMouseObjectsHide();
        }
        gameMouseSetCursor(MOUSE_CURSOR_ARROW);
        hub.cursorWasHidden = cursorIsHidden();
        if (hub.cursorWasHidden) {
            mouseShowCursor();
        }

        windowRefresh(hub.window);
        return true;
    }

    // Shows the loaded map in greyscale behind the hub, without the interface bar.
    void shadeMapView()
    {
        // Stop object animations so nothing redraws in colour over the shading.
        isoDisable();
        interfaceBarHide();
        tileWindowRefresh();
        grayscalePaletteApply(windowGetBuffer(gIsoWindow), windowGetWidth(gIsoWindow), windowGetHeight(gIsoWindow), windowGetWidth(gIsoWindow));
        windowRefresh(gIsoWindow);
    }

    void showMessage(int messageId, const char* fallback)
    {
        const char* body[] = { stimpakGetMessage(messageId, fallback) };
        showDialogBox(nullptr, body, 1, 169, 116, COLOR_AMBER, nullptr, COLOR_AMBER, DIALOG_BOX_LARGE);
    }

    bool confirm(int messageId, const char* fallback)
    {
        const char* body[] = { stimpakGetMessage(messageId, fallback) };
        return showDialogBox(nullptr, body, 1, 169, 116, COLOR_AMBER, nullptr, COLOR_AMBER, DIALOG_BOX_LARGE | DIALOG_BOX_YES_NO) != 0;
    }

    // Returns the selected entry, or -1 when nothing was chosen this frame.
    int hubEntryForKey(int keyCode)
    {
        if (keyCode >= kHubEventBase && keyCode < kHubEventBase + HUB_ENTRY_COUNT) {
            return keyCode - kHubEventBase;
        }
        if (keyCode == KEY_ESCAPE) {
            return HUB_ENTRY_EXIT;
        }
        for (int entry = 0; entry < HUB_ENTRY_COUNT; entry++) {
            int lower = kHubEntries[entry].hotkeyLower;
            if (kHubEntries[entry].implemented && (keyCode == lower || keyCode == lower - ('a' - 'A'))) {
                return entry;
            }
        }
        return -1;
    }

    // ---------------------------------------------------------------------
    // List screen (ROSTER, QUESTS, LOCATION): perk-window layout with a list
    // of rows and a value column, +/- for the selected row, an info card and
    // an optional action button.

    struct ListRow {
        std::string name;
        std::string value;
        bool header; // group title, not selectable
    };

    struct ListScreen {
        const char* title;
        const char* valueLabel;
        const char* actionLabel; // nullptr: no action button
        std::function<std::vector<ListRow>()> rows;
        std::function<void(int row, std::string* title, std::string* text)> card;
        std::function<bool(int row, int delta)> change; // true when changed
        std::function<bool(int row)> action; // true: close the screen
    };

    constexpr int kListWidth = 573;
    constexpr int kListHeight = 230;
    constexpr int kListX = 45;
    constexpr int kListY = 43;
    constexpr int kListRowsWidth = 192;
    constexpr int kListRowsHeight = 129;
    constexpr int kListVisibleRows = 11;
    constexpr int kCardX = 280;
    constexpr int kCardWidth = 265;

    // Shortens `text` with "..." to fit `width` pixels in the current font.
    std::string fitText(const std::string& text, int width)
    {
        if (fontGetStringWidth(text.c_str()) <= width) {
            return text;
        }
        std::string shortened = text;
        while (!shortened.empty() && fontGetStringWidth((shortened + "...").c_str()) > width) {
            shortened.pop_back();
        }
        return shortened + "...";
    }

    // Returns true when anything was changed.
    bool runListScreen(const ListScreen& screen)
    {
        enum {
            CODE_DONE = 700,
            CODE_UP,
            CODE_DOWN,
            CODE_PLUS,
            CODE_MINUS,
            CODE_LIST,
            CODE_ACTION,
        };

        FrmImage background;
        FrmImage redUp;
        FrmImage redDown;
        FrmImage arrowUpOff;
        FrmImage arrowUpOn;
        FrmImage arrowDownOff;
        FrmImage arrowDownOn;
        FrmImage plusOff;
        FrmImage plusOn;
        FrmImage minusOff;
        FrmImage minusOn;
        if (!background.lock(InterfaceFrameId::PerkDialogBackground)
            || !redUp.lock(InterfaceFrameId::LittleRedButtonUp)
            || !redDown.lock(InterfaceFrameId::LittleRedButtonDown)
            || !arrowUpOff.lock(InterfaceFrameId::CharacterEditorUpArrowOff)
            || !arrowUpOn.lock(InterfaceFrameId::CharacterEditorUpArrowOn)
            || !arrowDownOff.lock(InterfaceFrameId::CharacterEditorDownArrowOff)
            || !arrowDownOn.lock(InterfaceFrameId::CharacterEditorDownArrowOn)
            || !plusOff.lock(InterfaceFrameId::CharacterEditorSpecialPositiveOff)
            || !plusOn.lock(InterfaceFrameId::CharacterEditorSpecialPositiveOn)
            || !minusOff.lock(InterfaceFrameId::CharacterEditorSpecialNegativeOff)
            || !minusOn.lock(InterfaceFrameId::CharacterEditorSpecialNegativeOn)) {
            debugPrint("STIMPAK: missing list screen art\n");
            return false;
        }

        int window = windowCreate((screenGetWidth() - kListWidth) / 2, (screenGetHeight() - kListHeight) / 2,
            kListWidth, kListHeight, static_cast<ColorWithFlags>(256), WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);
        if (window == -1) {
            return false;
        }
        unsigned char* buffer = windowGetBuffer(window);
        memcpy(buffer, background.getData(), kListWidth * kListHeight);
        stimpakNotifyImageDrawn(window, background, 0, 0, background.getWidth(), background.getHeight(), 0, 0);

        auto button = [&](int x, int y, FrmImage& up, FrmImage& down, int code) {
            int btn = buttonCreate(window, x, y, up.getWidth(), up.getHeight(), -1, -1, -1, code,
                up.getData(), down.getData(), nullptr, BUTTON_FLAG_TRANSPARENT);
            if (btn != -1) {
                buttonSetCallbacks(btn, _gsound_red_butt_press, nullptr);
                stimpakNotifyButtonArt(btn, &up, &down);
            }
        };
        button(48, 186, redUp, redDown, CODE_DONE);
        button(25, 46, arrowUpOff, arrowUpOn, CODE_UP);
        button(25, 47 + arrowUpOn.getHeight(), arrowDownOff, arrowDownOn, CODE_DOWN);
        int valueButtonY = 186 + redUp.getHeight() / 2 - plusOn.getHeight();
        button(153, valueButtonY, plusOff, plusOn, CODE_PLUS);
        button(153, valueButtonY + plusOn.getHeight() - 1, minusOff, minusOn, CODE_MINUS);
        buttonCreate(window, kListX, kListY, kListRowsWidth, kListRowsHeight, -1, -1, -1, CODE_LIST,
            nullptr, nullptr, nullptr, BUTTON_FLAG_TRANSPARENT);

        fontSetCurrent(103);
        fontDrawText(buffer + kListWidth * 16 + 49, screen.title, kListWidth, kListWidth, COLOR_DARK_YELLOW);
        fontDrawText(buffer + kListWidth * 186 + 69, stimpakGetMessage(STIMPAK_MSG_DONE, "DONE"), kListWidth, kListWidth, COLOR_DARK_YELLOW);
        fontDrawText(buffer + kListWidth * 186 + 177, screen.valueLabel, kListWidth, kListWidth, COLOR_DARK_YELLOW);

        // Optional action: a red button with its label at the foot of the card.
        constexpr int kActionX = 420;
        constexpr int kActionY = 186;
        if (screen.actionLabel != nullptr) {
            button(kActionX, kActionY, redUp, redDown, CODE_ACTION);
        }

        fontSetCurrent(101);
        int lineHeight = fontGetLineHeight() + 2;

        std::vector<ListRow> rows = screen.rows();
        int top = 0;
        int selected = 0;
        auto selectable = [&](int row) { return row >= 0 && row < static_cast<int>(rows.size()) && !rows[row].header; };
        // Moves the selection to the next selectable row in `direction`.
        auto step = [&](int from, int direction) {
            for (int row = from + direction; row >= 0 && row < static_cast<int>(rows.size()); row += direction) {
                if (selectable(row)) {
                    return row;
                }
            }
            return from;
        };
        if (!selectable(selected)) {
            selected = step(-1, 1);
        }

        auto redraw = [&]() {
            if (rows.empty()) {
                selected = -1;
                top = 0;
            } else if (selected < top) {
                top = selected;
                // Keep a group title visible above its first row.
                if (top > 0 && rows[top - 1].header) {
                    top--;
                }
            } else if (selected >= top + kListVisibleRows) {
                top = selected - kListVisibleRows + 1;
            }

            fontSetCurrent(101);
            blitBufferToBuffer(background.getData() + kListWidth * kListY + kListX, kListRowsWidth, kListRowsHeight, kListWidth,
                buffer + kListWidth * kListY + kListX, kListWidth);
            stimpakNotifyImageDrawn(window, background, kListX, kListY, kListRowsWidth, kListRowsHeight, kListX, kListY);
            int y = kListY;
            for (int row = top; row < std::min(static_cast<int>(rows.size()), top + kListVisibleRows); row++) {
                Color color = rows[row].header ? COLOR_DARK_YELLOW : (row == selected ? COLOR_LIGHT_YELLOW : COLOR_GREEN);
                int valueWidth = rows[row].value.empty() ? 0 : fontGetStringWidth(rows[row].value.c_str()) + 6;
                std::string name = fitText(rows[row].name, kListRowsWidth - valueWidth);
                fontDrawText(buffer + kListWidth * y + kListX, name.c_str(), kListRowsWidth, kListWidth, color);
                if (!rows[row].value.empty()) {
                    int width = fontGetStringWidth(rows[row].value.c_str());
                    fontDrawText(buffer + kListWidth * y + kListX + kListRowsWidth - width, rows[row].value.c_str(), kListWidth, kListWidth, color);
                }
                y += lineHeight;
            }

            // Card: parchment comes back from the background, then title and text.
            blitBufferToBuffer(background.getData() + kCardX, kListWidth - kCardX, kListHeight, kListWidth, buffer + kCardX, kListWidth);
            stimpakNotifyImageDrawn(window, background, kCardX, 0, kListWidth - kCardX, kListHeight, kCardX, 0);
            std::string title;
            std::string text;
            if (selectable(selected)) {
                screen.card(selected, &title, &text);
            }
            // Message files have no escapes: "\n" in a text means a new paragraph.
            for (size_t pos = text.find("\\n"); pos != std::string::npos; pos = text.find("\\n", pos)) {
                text.replace(pos, 2, "\n");
            }
            fontSetCurrent(102);
            int titleHeight = fontGetLineHeight();
            fontDrawText(buffer + kListWidth * 27 + kCardX, title.c_str(), kCardWidth, kListWidth, COLOR_BLACK);
            bufferDrawLine(buffer, kListWidth, kCardX, 27 + titleHeight, kCardX + kCardWidth, 27 + titleHeight, COLOR_BLACK);
            bufferDrawLine(buffer, kListWidth, kCardX, 28 + titleHeight, kCardX + kCardWidth, 28 + titleHeight, COLOR_BLACK);

            fontSetCurrent(101);
            int textY = 36 + titleHeight;
            // Paragraphs separated by '\n', each word-wrapped to the card.
            size_t start = 0;
            while (start <= text.size()) {
                size_t end = text.find('\n', start);
                std::string paragraph = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
                short beginnings[WORD_WRAP_MAX_COUNT];
                short count = 0;
                if (!paragraph.empty() && wordWrap(paragraph.c_str(), kCardWidth, beginnings, &count) == 0) {
                    for (int line = 0; line < count - 1; line++) {
                        std::string part = paragraph.substr(beginnings[line], beginnings[line + 1] - beginnings[line]);
                        fontDrawText(buffer + kListWidth * textY + kCardX, part.c_str(), kCardWidth, kListWidth, COLOR_BLACK);
                        textY += fontGetLineHeight() + 1;
                    }
                } else {
                    textY += fontGetLineHeight() + 1;
                }
                if (end == std::string::npos) {
                    break;
                }
                start = end + 1;
            }

            if (screen.actionLabel != nullptr) {
                fontSetCurrent(103);
                fontDrawText(buffer + kListWidth * (kActionY + 1) + kActionX + redUp.getWidth() + 4, screen.actionLabel, kListWidth, kListWidth, COLOR_BLACK);
            }

            windowRefresh(window);
        };

        redraw();

        bool changed = false;
        bool done = false;
        while (!done) {
            sharedFpsLimiter.mark();
            int keyCode = inputGetInput();
            switch (keyCode) {
            case CODE_DONE:
            case KEY_RETURN:
            case KEY_ESCAPE:
                done = true;
                break;
            case CODE_UP:
            case KEY_ARROW_UP:
                selected = step(selected, -1);
                redraw();
                break;
            case CODE_DOWN:
            case KEY_ARROW_DOWN:
                selected = step(selected, 1);
                redraw();
                break;
            case KEY_PAGE_UP:
                for (int i = 0; i < kListVisibleRows; i++) {
                    selected = step(selected, -1);
                }
                redraw();
                break;
            case KEY_PAGE_DOWN:
                for (int i = 0; i < kListVisibleRows; i++) {
                    selected = step(selected, 1);
                }
                redraw();
                break;
            case CODE_PLUS:
            case KEY_PLUS:
            case KEY_EQUAL:
            case KEY_ARROW_RIGHT:
            case CODE_MINUS:
            case KEY_MINUS:
            case KEY_ARROW_LEFT: {
                int delta = (keyCode == CODE_MINUS || keyCode == KEY_MINUS || keyCode == KEY_ARROW_LEFT) ? -1 : 1;
                if (selectable(selected) && screen.change(selected, delta)) {
                    changed = true;
                    rows = screen.rows();
                } else {
                    soundPlayFile("iisxxxx1");
                }
                redraw();
                break;
            }
            default:
                // A letter jumps to the next row starting with it.
                if (keyCode >= 'a' && keyCode <= 'z' || keyCode >= 'A' && keyCode <= 'Z') {
                    int count = static_cast<int>(rows.size());
                    for (int offset = 1; offset <= count; offset++) {
                        int row = (selected + offset) % count;
                        const std::string& name = rows[row].name;
                        size_t first = name.find_first_not_of("> ");
                        if (selectable(row) && first != std::string::npos && toupper(static_cast<unsigned char>(name[first])) == toupper(keyCode)) {
                            selected = row;
                            redraw();
                            break;
                        }
                    }
                }
                break;
            case CODE_LIST: {
                int mouseX;
                int mouseY;
                mouseGetPositionInWindow(window, &mouseX, &mouseY);
                int row = top + (mouseY - kListY) / lineHeight;
                if (selectable(row)) {
                    selected = row;
                    redraw();
                }
                break;
            }
            case CODE_ACTION:
                if (screen.actionLabel != nullptr && selectable(selected)) {
                    windowHide(window);
                    if (screen.action(selected)) {
                        changed = true;
                        done = true;
                    } else {
                        windowShow(window);
                        rows = screen.rows();
                        redraw();
                    }
                }
                break;
            }
            renderPresent();
            sharedFpsLimiter.throttle();
        }

        windowDestroy(window);
        return changed;
    }

    // ---------------------------------------------------------------------
    // ROSTER: every companion of party.txt; + joins, - leaves.

    Object* partyObjectForPid(const ProtoId& pid)
    {
        for (Object* member : get_all_party_members_objects(true)) {
            if (member != gDude && ProtoId(member) == pid) {
                return member;
            }
        }
        return nullptr;
    }

    // A free tile next to the player, or -1.
    int freeTileNearPlayer()
    {
        for (int distance = 1; distance <= 3; distance++) {
            for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
                int tile = tileGetTileInDirection(gDude->tile, static_cast<Rotation>(rotation), distance);
                if (tile != -1 && _obj_blocking_at(nullptr, tile, gDude->elevation) == nullptr) {
                    return tile;
                }
            }
        }
        return -1;
    }

    bool rosterJoin(const ProtoId& pid)
    {
        Proto* proto;
        if (protoGetProto(pid, &proto) == -1) {
            return false;
        }
        int tile = freeTileNearPlayer();
        if (tile == -1) {
            return false;
        }
        Object* object = nullptr;
        if (objectCreateWithFrmIdProtoId(&object, proto, pid) == -1 || object == nullptr) {
            return false;
        }
        Rect rect;
        objectSetLocation(object, tile, gDude->elevation, &rect);
        if (partyMemberAdd(object) == -1) {
            objectDestroy(object);
            return false;
        }
        return true;
    }

    bool rosterLeave(Object* member)
    {
        if (partyMemberRemove(member) == -1) {
            return false;
        }
        objectDestroy(member);
        return true;
    }

    bool runRoster()
    {
        std::vector<ProtoId> pids;
        for (int index = 1; index < gPartyMemberDescriptionsLength; index++) {
            const ProtoId& pid = gPartyMemberProtoIds[index];
            const char* name = protoGetName(pid);
            if (pid.objectType() == OBJ_TYPE_CRITTER && name != nullptr && *name != '\0') {
                pids.push_back(pid);
            }
        }
        std::sort(pids.begin(), pids.end(), [](const ProtoId& a, const ProtoId& b) {
            return compat_stricmp(protoGetName(a), protoGetName(b)) < 0;
        });

        ListScreen screen;
        screen.title = stimpakGetMessage(STIMPAK_MSG_ROSTER, "ROSTER");
        screen.valueLabel = stimpakGetMessage(STIMPAK_MSG_ROSTER_VALUE, "JOIN");
        screen.actionLabel = nullptr;
        screen.rows = [&]() {
            std::vector<ListRow> rows;
            for (const ProtoId& pid : pids) {
                rows.push_back({ protoGetName(pid), partyObjectForPid(pid) != nullptr ? stimpakGetMessage(STIMPAK_MSG_IN_PARTY, "IN PARTY") : "", false });
            }
            return rows;
        };
        screen.card = [&](int row, std::string* title, std::string* text) {
            *title = protoGetName(pids[row]);
            *text = stimpakGetMessage(partyObjectForPid(pids[row]) != nullptr ? STIMPAK_MSG_ROSTER_CARD_MEMBER : STIMPAK_MSG_ROSTER_CARD_OTHER, "");
        };
        screen.change = [&](int row, int delta) {
            Object* member = partyObjectForPid(pids[row]);
            if (delta > 0) {
                return member == nullptr && rosterJoin(pids[row]);
            }
            return member != nullptr && rosterLeave(member);
        };
        return runListScreen(screen);
    }

    // ---------------------------------------------------------------------
    // QUESTS: grouped by location; not started / ACTIVE / DONE.

    int questState(const QuestInfo& quest)
    {
        int value = gameGetGlobalVar(static_cast<GameGlobalVar>(quest.gvar));
        if (value >= quest.completedThreshold) {
            return 2;
        }
        return value >= quest.displayThreshold ? 1 : 0;
    }

    bool runQuests()
    {
        struct Entry {
            int quest; // -1 for a location title
            ListRow row;
        };
        auto build = [&]() {
            std::vector<Entry> entries;
            std::string location;
            for (int index = 0; index < questGetCount(); index++) {
                QuestInfo quest;
                if (!questGetInfo(index, &quest)) {
                    continue;
                }
                if (location != quest.location) {
                    location = quest.location;
                    entries.push_back({ -1, { location, "", true } });
                }
                static const int kStateMessages[] = { STIMPAK_MSG_QUEST_NOT_STARTED, STIMPAK_MSG_QUEST_ACTIVE, STIMPAK_MSG_QUEST_DONE };
                static const char* kStateFallbacks[] = { "-", "ACTIVE", "DONE" };
                int state = questState(quest);
                entries.push_back({ index, { quest.text, stimpakGetMessage(kStateMessages[state], kStateFallbacks[state]), false } });
            }
            return entries;
        };
        std::vector<Entry> entries = build();

        ListScreen screen;
        screen.title = stimpakGetMessage(STIMPAK_MSG_QUESTS, "QUESTS");
        screen.valueLabel = stimpakGetMessage(STIMPAK_MSG_QUEST_VALUE, "STATE");
        screen.actionLabel = nullptr;
        screen.rows = [&]() {
            entries = build();
            std::vector<ListRow> rows;
            for (const Entry& entry : entries) {
                rows.push_back(entry.row);
            }
            return rows;
        };
        screen.card = [&](int row, std::string* title, std::string* text) {
            QuestInfo quest;
            questGetInfo(entries[row].quest, &quest);
            *title = quest.location;
            *text = std::string(quest.text) + "\n\n" + stimpakGetMessage(STIMPAK_MSG_QUEST_CARD, "");
        };
        screen.change = [&](int row, int delta) {
            QuestInfo quest;
            if (!questGetInfo(entries[row].quest, &quest)) {
                return false;
            }
            int state = questState(quest) + delta;
            if (state < 0 || state > 2) {
                return false;
            }
            int values[] = { 0, quest.displayThreshold, quest.completedThreshold };
            gameSetGlobalVar(static_cast<GameGlobalVar>(quest.gvar), values[state]);
            return true;
        };
        return runListScreen(screen);
    }

    // ---------------------------------------------------------------------
    // LOCATION: towns (unknown / known / visited) and TRAVEL.

    bool runLocation(bool* travelled)
    {
        std::vector<City> areas;
        for (int index = 0; index < wmStimpakGetAreaCount(); index++) {
            City area = static_cast<City>(index);
            char name[48];
            wmGetAreaIdxName(area, name);
            if (wmStimpakAreaHasEntrance(area) && name[0] != '\0' && strcmp(name, "Error") != 0) {
                areas.push_back(area);
            }
        }
        auto areaName = [](City area) {
            char name[48];
            wmGetAreaIdxName(area, name);
            return std::string(name);
        };

        ListScreen screen;
        screen.title = stimpakGetMessage(STIMPAK_MSG_LOCATION, "LOCATION");
        screen.valueLabel = stimpakGetMessage(STIMPAK_MSG_LOCATION_VALUE, "STATE");
        screen.actionLabel = stimpakGetMessage(STIMPAK_MSG_TRAVEL, "TRAVEL");
        screen.rows = [&]() {
            static const int kStateMessages[] = { STIMPAK_MSG_AREA_UNKNOWN, STIMPAK_MSG_AREA_KNOWN, STIMPAK_MSG_AREA_VISITED };
            static const char* kStateFallbacks[] = { "-", "KNOWN", "VISITED" };
            City current = CITY_INVALID;
            wmGetPartyCurArea(&current);
            std::vector<ListRow> rows;
            for (City area : areas) {
                int state = wmStimpakGetAreaState(area);
                std::string name = areaName(area);
                if (area == current) {
                    name = "> " + name;
                }
                rows.push_back({ name, stimpakGetMessage(kStateMessages[state], kStateFallbacks[state]), false });
            }
            return rows;
        };
        screen.card = [&](int row, std::string* title, std::string* text) {
            *title = areaName(areas[row]);
            *text = stimpakGetMessage(STIMPAK_MSG_LOCATION_CARD, "");
        };
        screen.change = [&](int row, int delta) {
            int state = wmStimpakGetAreaState(areas[row]) + delta;
            if (state < 0 || state > 2) {
                return false;
            }
            wmStimpakSetAreaState(areas[row], state);
            return true;
        };
        screen.action = [&](int row) {
            if (!confirm(STIMPAK_MSG_TRAVEL_CONFIRM, "Move the party to this town now?")) {
                return false;
            }
            // The map change runs the new map's scripts; stop them again after.
            if (wmStimpakTravelToArea(areas[row]) != 0) {
                showMessage(STIMPAK_MSG_TRAVEL_FAILED, "The party could not travel there.");
                return false;
            }
            scriptsDisable();
            *travelled = true;
            return true;
        };
        return runListScreen(screen);
    }

    constexpr int kPickerEventBase = 640;

    // Companions in the party (alive, on the map), at most six so BACK fits.
    std::vector<Object*> stimpakCompanions()
    {
        std::vector<Object*> companions;
        for (Object* member : get_all_party_members_objects(false)) {
            if (member != gDude && ProtoId(member).objectType() == OBJ_TYPE_CRITTER && companions.size() < kPanelMaxEntries - 1) {
                companions.push_back(member);
            }
        }
        return companions;
    }

    // COMPANIONS: pick a party member, edit it, come back to the list.
    // Returns true when anything was changed.
    bool runCompanions()
    {
        std::vector<Object*> companions = stimpakCompanions();
        if (companions.empty()) {
            showMessage(STIMPAK_MSG_NO_COMPANIONS, "There are no companions in this game.");
            return false;
        }

        bool edited = false;
        for (;;) {
            std::vector<PanelEntry> entries;
            for (Object* companion : companions) {
                entries.push_back({ critterGetName(companion), true });
            }
            entries.push_back({ stimpakGetMessage(STIMPAK_MSG_BACK, "BACK"), true });
            int backIndex = static_cast<int>(entries.size()) - 1;

            Hub picker;
            if (!hubInit(picker, stimpakGetMessage(STIMPAK_MSG_COMPANIONS, "COMPANIONS"), critterGetName(gDude), entries, kPickerEventBase)) {
                hubFree(picker);
                return edited;
            }

            int chosen = -1;
            while (chosen == -1) {
                sharedFpsLimiter.mark();
                int keyCode = inputGetInput();
                if (keyCode >= kPickerEventBase && keyCode < kPickerEventBase + static_cast<int>(entries.size())) {
                    chosen = keyCode - kPickerEventBase;
                } else if (keyCode == KEY_ESCAPE) {
                    chosen = backIndex;
                } else if (keyCode >= '1' && keyCode < '1' + backIndex) {
                    chosen = keyCode - '1';
                }
                renderPresent();
                sharedFpsLimiter.throttle();
            }
            hubFree(picker);
            windowRefreshAll(&_scr_size);

            if (chosen == backIndex) {
                return edited;
            }
            if (characterEditorShowStimpakCompanion(companions[chosen]) == 0) {
                edited = true;
            }
            windowRefreshAll(&_scr_size);
        }
    }

} // namespace

static StimpakRenderHooks gStimpakRenderHooks;

void stimpakSetRenderHooks(const StimpakRenderHooks& hooks)
{
    gStimpakRenderHooks = hooks;
}

void stimpakNotifyImageDrawn(int window, const FrmImage& image, int srcX, int srcY, int width, int height, int destX, int destY)
{
    if (gStimpakRenderHooks.imageDrawn != nullptr) {
        gStimpakRenderHooks.imageDrawn(window, image, srcX, srcY, width, height, destX, destY);
    }
}

void stimpakNotifyButtonArt(int button, const FrmImage* normal, const FrmImage* pressed)
{
    if (gStimpakRenderHooks.buttonArt != nullptr) {
        gStimpakRenderHooks.buttonArt(button, normal, pressed);
    }
}

const char* stimpakGetMessage(int messageId, const char* fallback)
{
    if (!gStimpakMessageListLoaded) {
        gStimpakMessageListLoaded = true;
        messageListInit(&gStimpakMessageList);
        messageListLoad(&gStimpakMessageList, "game\\stimpak.msg");
    }

    MessageListItem item;
    item.num = messageId;
    if (messageListGetItem(&gStimpakMessageList, &item)) {
        return item.text;
    }

    for (const auto& [id, text] : kStimpakDefaultMessages) {
        if (id == messageId) {
            return text;
        }
    }

    return fallback;
}

#ifndef STIMPAK_VERSION
#define STIMPAK_VERSION "development"
#endif

StimpakResult stimpakRun()
{
    debugPrint("STIMPAK %s: editor opened\n", STIMPAK_VERSION);

    // A game saved mid-combat restores a half-finished combat state on load;
    // saving it again without running combat would persist that state.
    if (isInCombat()) {
        showMessage(STIMPAK_MSG_COMBAT_SAVE, "This game was saved during combat and cannot be edited.");
        return StimpakResult::Exit;
    }

    // The game loop never runs while editing: keep scripts from reacting.
    scriptsDisable();
    shadeMapView();

    std::vector<PanelEntry> entries;
    for (const HubEntrySpec& spec : kHubEntries) {
        entries.push_back({ stimpakGetMessage(spec.messageId, spec.fallback), spec.implemented });
    }

    Hub hub;
    if (!hubInit(hub, critterGetName(gDude), mapGetCityName(mapGetCurrentMap()), entries, kHubEventBase)) {
        hubFree(hub);
        return StimpakResult::Exit;
    }

    devAutomationSetState("stimpak");

    bool saved = false;
    bool play = false;
    bool edited = false;
    bool done = false;
    while (!done) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();
        switch (hubEntryForKey(keyCode)) {
        case HUB_ENTRY_CHARACTER:
            // Sub-screens open above the hub; hide it so it does not show around them.
            windowHide(hub.window);
            if (characterEditorShowStimpak() == 0) {
                edited = true;
            }
            windowShow(hub.window);
            windowRefreshAll(&_scr_size);
            break;
        case HUB_ENTRY_COMPANIONS:
            windowHide(hub.window);
            if (runCompanions()) {
                edited = true;
            }
            windowShow(hub.window);
            windowRefreshAll(&_scr_size);
            break;
        case HUB_ENTRY_ROSTER:
        case HUB_ENTRY_QUESTS:
        case HUB_ENTRY_LOCATION: {
            int entry = hubEntryForKey(keyCode);
            windowHide(hub.window);
            bool travelled = false;
            bool changed = entry == HUB_ENTRY_ROSTER
                ? runRoster()
                : (entry == HUB_ENTRY_QUESTS ? runQuests() : runLocation(&travelled));
            if (changed) {
                edited = true;
            }
            if (travelled) {
                // New map: show it shaded behind the hub, with its town name.
                shadeMapView();
                hubFree(hub);
                if (!hubInit(hub, critterGetName(gDude), mapGetCityName(mapGetCurrentMap()), entries, kHubEventBase)) {
                    done = true;
                }
            } else {
                windowShow(hub.window);
            }
            windowRefreshAll(&_scr_size);
            break;
        }
        case HUB_ENTRY_SAVE_AS:
            lsgRequestEmptySlotOnNextSave();
            windowHide(hub.window);
            // The save records whether the interface bar is hidden; the hub
            // hides it, so show it while saving or the game loads without it.
            interfaceBarShow();
            if (lsgSaveGame(LoadSaveMode::Normal) == 1) {
                saved = true;
                done = true;
            } else {
                shadeMapView();
                windowShow(hub.window);
                windowRefreshAll(&_scr_size);
            }
            break;
        case HUB_ENTRY_PLAY:
            // Straight into the game with the edits, saved or not.
            play = true;
            done = true;
            break;
        case HUB_ENTRY_EXIT:
            if (!edited || confirm(STIMPAK_MSG_DISCARD_CHANGES, "Leave without saving your changes?")) {
                done = true;
            }
            windowRefreshAll(&_scr_size);
            break;
        default:
            break;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    devAutomationSetState("");
    hubFree(hub);

    if (play) {
        // Undo the hub's shading of the map: animations, colours, the bar.
        isoEnable();
        interfaceBarShow();
        tileWindowRefresh();
        return StimpakResult::Play;
    }

    return saved ? StimpakResult::Saved : StimpakResult::Exit;
}

} // namespace fallout
