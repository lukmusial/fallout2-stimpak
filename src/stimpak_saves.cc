#include "stimpak_saves.h"

#include <ctype.h>
#include <string.h>

#include <string>
#include <vector>

#include "debug.h"
#include "display_monitor.h"
#include "game.h"
#include "party_member.h"
#include "stimpak.h"

namespace fallout {

namespace {

    // The unmodded game: data\vault13.gam and data\party.txt of the 1.02 patch.
    constexpr int kVanillaGlobals = 696;
    constexpr int kVanillaParty = 26;

    // SAVE.DAT: the header, function #1 (one int), then the first copy of the
    // global variables, then the list of saved maps (a count, then names).
    constexpr int kHeaderSize = 30051;
    constexpr int kGlobalsOffset = kHeaderSize + 4;

    // GVAR_TORR_BRAHMIN_KILLED is 202 in 1.02; the Restoration Project keeps
    // it at 74 (1.02's unused GVAR_DUNTONS_ANGRY) and uses 202 and 29 (1.02's
    // unused GVAR_VIC_DEVICE) for new variables.
    constexpr int kTorrOld = 202;
    constexpr int kTorrNew = 74;
    constexpr int kReusedOld = 29;
    constexpr const char* kTorrName = "GVAR_TORR_BRAHMIN_KILLED";

    struct Layout {
        int globals = -1; // -1: the game's own lists
        int party = -1;
        bool moveTorr = false;
    };

    Layout gLayout;
    bool gLastLoadWasLegacy = false;

    int readBigEndian(const unsigned char* p)
    {
        return static_cast<int>((static_cast<unsigned>(p[0]) << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
    }

    // A saved map's name as the map list holds it: up to 8 letters, digits or
    // underscores, ".SAV" and a terminating zero.
    bool isMapName(const unsigned char* p, size_t available)
    {
        size_t length = 0;
        while (length < 8 && length < available && (isalnum(p[length]) || p[length] == '_')) {
            length++;
        }
        if (length == 0 || length + 5 > available) {
            return false;
        }
        return memcmp(p + length, ".SAV", 4) == 0 && p[length + 4] == '\0';
    }

    // How many global variables the save holds: the first copy is followed by
    // the map list.
    int globalsInSave(const std::vector<unsigned char>& data, int limit)
    {
        for (int count = 1; count <= limit; count++) {
            size_t end = kGlobalsOffset + static_cast<size_t>(count) * 4;
            if (end + 18 > data.size()) {
                break;
            }
            int maps = readBigEndian(&data[end]);
            if (maps > 0 && maps < 1000 && isMapName(&data[end + 4], data.size() - end - 4)) {
                return count;
            }
        }
        return -1;
    }

    // The names of the game's global variables, in order (data\vault13.gam).
    std::vector<std::string> globalNames()
    {
        std::vector<std::string> names;
        File* stream = fileOpen("data\\vault13.gam", "rt");
        if (stream == nullptr) {
            return names;
        }
        char line[512];
        while (fileReadString(line, sizeof(line), stream) != nullptr) {
            char* comment = strstr(line, "//");
            if (comment != nullptr) {
                *comment = '\0';
            }
            char* assign = strstr(line, ":=");
            if (assign == nullptr) {
                continue;
            }
            *assign = '\0';
            char* start = line;
            while (*start != '\0' && isspace(static_cast<unsigned char>(*start))) {
                start++;
            }
            char* end = start + strlen(start);
            while (end > start && isspace(static_cast<unsigned char>(end[-1]))) {
                end--;
            }
            names.emplace_back(start, end);
        }
        fileClose(stream);
        return names;
    }

} // namespace

void stimpakSavesBeginLoad(const char* path)
{
    gLayout = Layout();

    File* stream = fileOpen(path, "rb");
    if (stream == nullptr) {
        return;
    }
    std::vector<unsigned char> data(kGlobalsOffset + static_cast<size_t>(gGameGlobalVarsLength) * 4 + 32);
    size_t read = fileRead(data.data(), 1, data.size(), stream);
    fileClose(stream);
    data.resize(read);

    int globals = globalsInSave(data, gGameGlobalVarsLength);
    if (globals == -1 || globals == gGameGlobalVarsLength) {
        return;
    }
    if (globals != kVanillaGlobals || gPartyMemberDescriptionsLength < kVanillaParty) {
        debugPrint("STIMPAK: %s holds %d global variables, the game has %d; not a save this game can read\n",
            path, globals, gGameGlobalVarsLength);
        return;
    }

    gLayout.globals = globals;
    gLayout.party = kVanillaParty;
    std::vector<std::string> names = globalNames();
    gLayout.moveTorr = static_cast<int>(names.size()) == gGameGlobalVarsLength
        && names.size() > kTorrOld
        && names[kTorrNew] == kTorrName
        && names[kTorrOld] != kTorrName;
    debugPrint("STIMPAK: %s is a save of the unmodded game (%d global variables, %d party members); "
               "reading it into this game's %d and %d\n",
        path, globals, kVanillaParty, gGameGlobalVarsLength, gPartyMemberDescriptionsLength);
}

void stimpakSavesEndLoad(bool loaded)
{
    gLastLoadWasLegacy = loaded && gLayout.globals != -1;
    if (gLastLoadWasLegacy) {
        displayMonitorAddMessage(stimpakGetMessage(STIMPAK_MSG_LEGACY_SAVE,
            "Saved before the mods installed now; read with their additions."));
    }
    gLayout = Layout();
}

int stimpakSavesPartyCount(int current)
{
    return gLayout.party != -1 && gLayout.party < current ? gLayout.party : current;
}

int stimpakSavesReadGlobals(File* stream, int* vars, int length, bool keep)
{
    int count = gLayout.globals != -1 && gLayout.globals < length ? gLayout.globals : length;

    std::vector<int> buffer(count);
    if (fileReadInt32List(stream, buffer.data(), count) == -1) {
        return -1;
    }
    if (!keep) {
        return 0;
    }

    // vars holds the values a new game starts with (the load reset the game).
    int startTorrOld = vars[kTorrOld];
    int startReused = vars[kReusedOld];
    memcpy(vars, buffer.data(), sizeof(int) * count);
    if (gLayout.moveTorr) {
        vars[kTorrNew] = buffer[kTorrOld];
        vars[kTorrOld] = startTorrOld;
        vars[kReusedOld] = startReused;
    }
    return 0;
}

bool stimpakSavesLastLoadWasLegacy()
{
    return gLastLoadWasLegacy;
}

} // namespace fallout
