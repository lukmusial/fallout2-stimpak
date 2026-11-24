#include "dev_automation.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "critter.h"
#include "debug.h"
#include "game.h"
#include "input.h"
#include "item.h"
#include "kb.h"
#include "map.h"
#include "mouse.h"
#include "object.h"
#include "party_member.h"
#include "perk.h"
#include "platform_compat.h"
#include "proto.h"
#include "scripts.h"
#include "skill.h"
#include "stat.h"
#include "svga.h"
#include "trait.h"
#include "win32.h"
#include "worldmap.h"

namespace fallout {

namespace {

    constexpr int kDefaultUntilTimeout = 30000;

    // A click first rests the cursor on the target (the window manager accepts a
    // press only on a button that was already hovered, and leaving the previous
    // button takes a poll of its own), then holds the button and releases it, so
    // that both the press and the release are seen by every screen.
    constexpr int kClickPollsSettle = 3;
    constexpr int kClickPollsDown = 2;
    constexpr int kClickPollsUp = 2;

    using Command = DevAutomationCommand;

    struct AutomationState {
        bool active = false;
        std::string outDir = "devout";
        std::vector<Command> commands;
        size_t next = 0;

        // Current command progress.
        bool waiting = false;
        unsigned int waitUntil = 0;
        unsigned int untilDeadline = 0;
        int clickPhase = 0; // 0 = idle, 1..n polls since start
        int keyRepeatDone = 0;
        // Set when a command queued a key: the rest waits for the next input
        // poll, so screens see one key per frame as with a real keyboard.
        bool inputSent = false;
        bool clickRight = false;

        // Synthetic mouse.
        int mouseX = 320;
        int mouseY = 240;
        bool buttonLeft = false;
        bool buttonRight = false;

        std::string engineState;

        // Extensions (see devAutomationRegisterCommand).
        std::vector<std::pair<std::string, DevAutomationCommandHandler>> extensionCommands;
        std::vector<void (*)()> tickers;
        bool (*shotWriter)(const char* path) = screenshotSavePng;
        bool commandStarted = false;
    };

    AutomationState gAutomation;

    std::string trim(const std::string& value)
    {
        size_t begin = value.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos) {
            return std::string();
        }
        size_t end = value.find_last_not_of(" \t\r\n");
        return value.substr(begin, end - begin + 1);
    }

    bool loadScript(const char* path)
    {
        std::ifstream stream(path);
        if (!stream.is_open()) {
            debugPrint("AUTOMATION: cannot open script %s\n", path);
            return false;
        }

        std::string raw;
        int lineNumber = 0;
        while (std::getline(stream, raw)) {
            lineNumber++;
            std::string line = trim(raw);
            if (line.empty() || line[0] == '#') {
                continue;
            }

            Command command;
            command.line = lineNumber;
            size_t space = line.find_first_of(" \t");
            command.name = line.substr(0, space);
            if (space != std::string::npos) {
                // `text` keeps inner spacing; everything else is split on whitespace.
                command.rest = line.substr(space + 1);
                std::istringstream words(command.rest);
                std::string word;
                while (words >> word) {
                    command.args.push_back(word);
                }
            }
            gAutomation.commands.push_back(command);
        }

        debugPrint("AUTOMATION: loaded %d commands from %s\n", static_cast<int>(gAutomation.commands.size()), path);
        return true;
    }

    int keyCodeFromName(const std::string& name)
    {
        struct NamedKey {
            const char* name;
            int code;
        };

        static const NamedKey kKeys[] = {
            { "esc", KEY_ESCAPE },
            { "enter", KEY_RETURN },
            { "space", KEY_SPACE },
            { "tab", KEY_TAB },
            { "backspace", KEY_BACKSPACE },
            { "delete", KEY_DELETE },
            { "up", KEY_ARROW_UP },
            { "down", KEY_ARROW_DOWN },
            { "left", KEY_ARROW_LEFT },
            { "right", KEY_ARROW_RIGHT },
            { "home", KEY_HOME },
            { "end", KEY_END },
            { "pageup", KEY_PAGE_UP },
            { "pagedown", KEY_PAGE_DOWN },
            { "f11", KEY_F11 },
            { "f12", KEY_F12 },
        };

        for (const NamedKey& key : kKeys) {
            if (compat_stricmp(name.c_str(), key.name) == 0) {
                return key.code;
            }
        }

        // f1..f10 are contiguous.
        if ((name[0] == 'f' || name[0] == 'F') && name.size() >= 2 && name.size() <= 3) {
            int index = atoi(name.c_str() + 1);
            if (index >= 1 && index <= 10) {
                return KEY_F1 + index - 1;
            }
        }

        if (name.size() == 1) {
            return static_cast<unsigned char>(name[0]);
        }

        char* end = nullptr;
        long code = strtol(name.c_str(), &end, 10);
        if (end != name.c_str() && *end == '\0') {
            return static_cast<int>(code);
        }

        return -1;
    }

    std::string outputPath(const std::string& name, const char* extension)
    {
        // compat_mkdir_recursive stops at the first directory that already exists,
        // so create each level here and ignore "already exists".
        std::string path = gAutomation.outDir;
        for (size_t pos = path.find('/', 1); pos != std::string::npos; pos = path.find('/', pos + 1)) {
            compat_mkdir(path.substr(0, pos).c_str());
        }
        compat_mkdir(path.c_str());
        return gAutomation.outDir + "/" + name + extension;
    }

    // Stops the run with exit code 2, leaving `failure.png` for diagnosis.
    [[noreturn]] void fail(const Command& command, const char* message)
    {
        debugPrint("AUTOMATION: line %d (%s): %s\n", command.line, command.name.c_str(), message);
        fprintf(stderr, "AUTOMATION: line %d (%s): %s\n", command.line, command.name.c_str(), message);
        gAutomation.shotWriter(outputPath("failure", ".png").c_str());
        exit(2);
    }

    // JSON helpers: output is small and only ever read by the test tools.

    void jsonString(std::ostream& out, const char* value)
    {
        out << '"';
        for (const char* p = value != nullptr ? value : ""; *p != '\0'; p++) {
            unsigned char ch = static_cast<unsigned char>(*p);
            if (ch == '"' || ch == '\\') {
                out << '\\' << *p;
            } else if (ch < 0x20) {
                char buffer[8];
                snprintf(buffer, sizeof(buffer), "\\u%04x", ch);
                out << buffer;
            } else {
                out << *p;
            }
        }
        out << '"';
    }

    template <typename Fn>
    void jsonIntArray(std::ostream& out, int count, Fn value)
    {
        out << '[';
        for (int index = 0; index < count; index++) {
            if (index != 0) {
                out << ',';
            }
            out << value(index);
        }
        out << ']';
    }

    void dumpCritter(std::ostream& out, Object* critter)
    {
        out << "\"pid\":" << critter->pid;
        out << ",\"name\":";
        jsonString(out, critterGetName(critter));
        out << ",\"tile\":" << critter->tile;
        out << ",\"hp\":" << critterGetHitPoints(critter);
        out << ",\"stats\":";
        jsonIntArray(out, STAT_COUNT, [&](int i) { return critterGetStat(critter, static_cast<Stat>(i)); });
        out << ",\"base_stats\":";
        jsonIntArray(out, SAVEABLE_STAT_COUNT, [&](int i) { return critterGetBaseStat(critter, static_cast<Stat>(i)); });
        out << ",\"bonus_stats\":";
        jsonIntArray(out, SAVEABLE_STAT_COUNT, [&](int i) { return critterGetBonusStat(critter, static_cast<Stat>(i)); });
        out << ",\"skills\":";
        jsonIntArray(out, SKILL_COUNT, [&](int i) { return skillGetValue(critter, static_cast<Skill>(i)); });
        out << ",\"perks\":[";
        bool first = true;
        for (int perk = 0; perk < PERK_COUNT; perk++) {
            int rank = perkGetRank(critter, static_cast<Perk>(perk));
            if (rank > 0) {
                if (!first) {
                    out << ',';
                }
                first = false;
                out << "{\"id\":" << perk << ",\"rank\":" << rank << '}';
            }
        }
        out << ']';
    }

    bool writeStateDump(const std::string& path)
    {
        std::ofstream out(path);
        if (!out.is_open()) {
            return false;
        }

        out << "{\n\"loaded\":" << (gGameLoaded ? "true" : "false");

        if (gDude != nullptr) {
            out << ",\n\"player\":{";
            dumpCritter(out, gDude);
            out << ",\"pc_stats\":";
            jsonIntArray(out, PC_STAT_COUNT, [](int i) { return pcGetStat(static_cast<PcStat>(i)); });

            Skill tagged[NUM_TAGGED_SKILLS];
            skillsGetTagged(tagged, NUM_TAGGED_SKILLS);
            out << ",\"tagged_skills\":";
            jsonIntArray(out, NUM_TAGGED_SKILLS, [&](int i) { return static_cast<int>(tagged[i]); });

            Trait trait1;
            Trait trait2;
            traitsGetSelected(&trait1, &trait2);
            out << ",\"traits\":[" << static_cast<int>(trait1) << ',' << static_cast<int>(trait2) << ']';
            out << ",\"caps\":" << itemGetTotalCaps(gDude) << '}';
        }

        out << ",\n\"party\":[";
        bool first = true;
        for (Object* member : get_all_party_members_objects(true)) {
            if (member == gDude) {
                continue;
            }
            if (!first) {
                out << ',';
            }
            first = false;
            out << "\n{";
            dumpCritter(out, member);
            int level = -1;
            int maxLevel = -1;
            partyMemberGetLevelInfo(member, &level, &maxLevel);
            out << ",\"level\":" << level << ",\"max_level\":" << maxLevel;
            out << '}';
        }
        out << ']';

        int worldX = 0;
        int worldY = 0;
        wmGetPartyWorldPos(&worldX, &worldY);
        City area = static_cast<City>(-1);
        wmGetPartyCurArea(&area);

        out << ",\n\"world\":{\"x\":" << worldX << ",\"y\":" << worldY
            << ",\"area\":" << static_cast<int>(area)
            << ",\"map\":" << static_cast<int>(mapGetCurrentMap())
            << ",\"map_name\":";
        jsonString(out, gMapHeader.name);
        out << ",\"elevation\":" << static_cast<int>(gElevation)
            << ",\"time\":" << gameTimeGetTime() << '}';

        out << ",\n\"gvars\":";
        jsonIntArray(out, gGameGlobalVars != nullptr ? gGameGlobalVarsLength : 0, [](int i) { return gGameGlobalVars[i]; });
        out << "\n}\n";

        return out.good();
    }

    // Returns true when the command has finished.
    bool runCommand(const Command& command)
    {
        const std::string& name = command.name;
        const std::vector<std::string>& args = command.args;
        unsigned int now = getTicks();

        if (name == "wait") {
            if (!gAutomation.waiting) {
                if (args.empty()) {
                    fail(command, "missing duration");
                }
                gAutomation.waiting = true;
                gAutomation.waitUntil = now + static_cast<unsigned int>(atoi(args[0].c_str()));
            }
            if (now < gAutomation.waitUntil) {
                return false;
            }
            gAutomation.waiting = false;
            return true;
        }

        if (name == "until") {
            if (args.empty()) {
                fail(command, "missing state");
            }
            if (!gAutomation.waiting) {
                int timeout = args.size() > 1 ? atoi(args[1].c_str()) : kDefaultUntilTimeout;
                gAutomation.waiting = true;
                gAutomation.untilDeadline = now + static_cast<unsigned int>(timeout);
            }
            if (gAutomation.engineState == args[0]) {
                gAutomation.waiting = false;
                return true;
            }
            if (now >= gAutomation.untilDeadline) {
                fail(command, ("timed out waiting for state '" + args[0] + "', current '" + gAutomation.engineState + "'").c_str());
            }
            return false;
        }

        if (name == "key") {
            if (args.empty()) {
                fail(command, "missing key");
            }
            int code = keyCodeFromName(args[0]);
            if (code < 0) {
                fail(command, "unknown key");
            }
            // Optional repeat count: one key per input poll, so every press is handled.
            int count = args.size() > 1 ? atoi(args[1].c_str()) : 1;
            enqueueInputEvent(code);
            gAutomation.inputSent = true;
            if (++gAutomation.keyRepeatDone < count) {
                return false;
            }
            gAutomation.keyRepeatDone = 0;
            return true;
        }

        if (name == "text") {
            // One character per input poll, like typing.
            if (gAutomation.keyRepeatDone < static_cast<int>(command.rest.size())) {
                enqueueInputEvent(static_cast<unsigned char>(command.rest[gAutomation.keyRepeatDone]));
                gAutomation.keyRepeatDone++;
                gAutomation.inputSent = true;
                return false;
            }
            gAutomation.keyRepeatDone = 0;
            return true;
        }

        if (name == "move" || name == "click") {
            if (args.size() < 2) {
                fail(command, "missing coordinates");
            }
            if (gAutomation.clickPhase == 0) {
                gAutomation.mouseX = atoi(args[0].c_str());
                gAutomation.mouseY = atoi(args[1].c_str());
                if (name == "move") {
                    return true;
                }
                gAutomation.clickRight = args.size() > 2 && args[2] == "right";
            }

            gAutomation.clickPhase++;
            bool down = gAutomation.clickPhase > kClickPollsSettle && gAutomation.clickPhase <= kClickPollsSettle + kClickPollsDown;
            gAutomation.buttonLeft = down && !gAutomation.clickRight;
            gAutomation.buttonRight = down && gAutomation.clickRight;
            if (gAutomation.clickPhase > kClickPollsSettle + kClickPollsDown + kClickPollsUp) {
                gAutomation.clickPhase = 0;
                return true;
            }
            return false;
        }

        if (name == "shot") {
            if (args.empty()) {
                fail(command, "missing name");
            }
            std::string path = outputPath(args[0], ".png");
            if (!gAutomation.shotWriter(path.c_str())) {
                fail(command, "screenshot failed");
            }
            debugPrint("AUTOMATION: wrote %s\n", path.c_str());
            return true;
        }

        if (name == "dump") {
            if (args.empty()) {
                fail(command, "missing name");
            }
            std::string path = outputPath(args[0], ".json");
            if (!writeStateDump(path)) {
                fail(command, "state dump failed");
            }
            debugPrint("AUTOMATION: wrote %s\n", path.c_str());
            return true;
        }

        if (name == "log") {
            debugPrint("AUTOMATION: %s\n", command.rest.c_str());
            return true;
        }

        if (name == "quit") {
            int code = args.empty() ? 0 : atoi(args[0].c_str());
            debugPrint("AUTOMATION: quit %d\n", code);
            exit(code);
        }

        for (const auto& [extensionName, handler] : gAutomation.extensionCommands) {
            if (extensionName == name) {
                bool first = !gAutomation.commandStarted;
                gAutomation.commandStarted = true;
                if (!handler(command, first)) {
                    return false;
                }
                gAutomation.commandStarted = false;
                return true;
            }
        }

        fail(command, "unknown command");
    }

} // namespace

static bool gAutomationUnattended = false;
static bool gAutomationFastFades = false;

void devAutomationEarlyInit(int argc, char** argv)
{
    for (int index = 1; index < argc; index++) {
        if (strncmp(argv[index], "--dev-script=", 13) == 0) {
            gAutomationUnattended = true;
            break;
        }
    }

    if (!gAutomationUnattended) {
        return;
    }

    SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP, "1");
    SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);

    // The window never receives focus, so do not wait for it.
    gProgramIsActive = true;
}

bool devAutomationWantsHiddenWindow()
{
    return gAutomationUnattended;
}

void devAutomationParseArgs(int argc, char** argv)
{
    const char* scriptPrefix = "--dev-script=";
    const char* outPrefix = "--dev-out=";

    const char* scriptPath = nullptr;
    for (int index = 1; index < argc; index++) {
        if (strncmp(argv[index], scriptPrefix, strlen(scriptPrefix)) == 0) {
            scriptPath = argv[index] + strlen(scriptPrefix);
        } else if (strcmp(argv[index], "--dev-fast") == 0) {
            gAutomationFastFades = true;
        } else if (strncmp(argv[index], outPrefix, strlen(outPrefix)) == 0) {
            gAutomation.outDir = argv[index] + strlen(outPrefix);
        }
    }

    if (scriptPath == nullptr) {
        return;
    }

    if (!loadScript(scriptPath)) {
        fprintf(stderr, "AUTOMATION: cannot open script %s\n", scriptPath);
        exit(2);
    }

    gAutomation.active = true;
    gAutomation.mouseX = screenGetWidth() / 2;
    gAutomation.mouseY = screenGetHeight() / 2;
}

bool devAutomationFastFades()
{
    return gAutomationFastFades;
}

bool devAutomationIsActive()
{
    return gAutomation.active;
}

void devAutomationTick()
{
    if (!gAutomation.active) {
        return;
    }

    for (void (*ticker)() : gAutomation.tickers) {
        ticker();
    }

    // Run every command that completes immediately, stop at the first one that waits.
    gAutomation.inputSent = false;
    while (gAutomation.next < gAutomation.commands.size()) {
        if (!runCommand(gAutomation.commands[gAutomation.next])) {
            return;
        }
        gAutomation.next++;
        if (gAutomation.inputSent) {
            return;
        }
    }
}

bool devAutomationOverrideMouse(MouseData* mouseData)
{
    if (!gAutomation.active) {
        return false;
    }

    if (mouseDeviceUsesRelativeMode()) {
        int x;
        int y;
        mouseGetPosition(&x, &y);
        mouseData->x = gAutomation.mouseX - x;
        mouseData->y = gAutomation.mouseY - y;
    } else {
        mouseData->x = gAutomation.mouseX;
        mouseData->y = gAutomation.mouseY;
    }
    mouseData->buttons[0] = gAutomation.buttonLeft ? 1 : 0;
    mouseData->buttons[1] = gAutomation.buttonRight ? 1 : 0;
    mouseData->wheelX = 0;
    mouseData->wheelY = 0;
    return true;
}

const char* devAutomationGetState()
{
    return gAutomation.engineState.c_str();
}

void devAutomationSetState(const char* state)
{
    if (gAutomation.engineState != state) {
        gAutomation.engineState = state;
        if (gAutomation.active) {
            debugPrint("AUTOMATION: state '%s'\n", state);
        }
    }
}

void devAutomationRegisterCommand(const char* name, DevAutomationCommandHandler handler)
{
    gAutomation.extensionCommands.emplace_back(name, handler);
}

void devAutomationRegisterTicker(void (*ticker)())
{
    gAutomation.tickers.push_back(ticker);
}

void devAutomationSetShotWriter(bool (*writer)(const char* path))
{
    gAutomation.shotWriter = writer != nullptr ? writer : screenshotSavePng;
}

std::string devAutomationOutputPath(const std::string& name, const char* extension)
{
    return outputPath(name, extension);
}

void devAutomationFail(const DevAutomationCommand& command, const char* message)
{
    fail(command, message);
}

void devAutomationSetMouse(int x, int y)
{
    gAutomation.mouseX = x;
    gAutomation.mouseY = y;
}

void devAutomationGetMouse(int* x, int* y)
{
    *x = gAutomation.mouseX;
    *y = gAutomation.mouseY;
}

} // namespace fallout
