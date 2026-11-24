#ifndef FALLOUT_DEV_AUTOMATION_H_
#define FALLOUT_DEV_AUTOMATION_H_

#include <string>
#include <vector>

#include "dinput.h"

namespace fallout {

// Scripted input driver for automated testing and screenshots.
//
// Enabled with `--dev-script=<file>`; output (screenshots, state dumps) goes to
// `--dev-out=<dir>` (default `devout`, relative to the working directory).
//
// Script format: one command per line, `#` starts a comment.
//   wait <ms>                 pause
//   until <state> [<ms>]      wait until the engine reports a state (mainmenu,
//                             game, stimpack, input = a text input dialog is open),
//                             fail after timeout (default 30000)
//   key <name|char|code> [n]  send a key (n times, one per input poll) (esc, enter, space, tab, backspace,
//                             up, down, left, right, home, end, pageup, pagedown, f1..f12)
//   text <string>             type the rest of the line
//   move <x> <y>              move the cursor (logical screen coordinates)
//   click <x> <y> [right]     move and click
//   shot <name>               write <out>/<name>.png
//   dump <name>               write <out>/<name>.json with player, party, quest and world state
//   log <text>                write a line to the debug log
//   quit [<code>]             exit the process

// Called from `main` before SDL starts. When a script is requested the game runs
// unattended: background app on macOS (never takes focus), hidden window, no audio
// device, and it counts as active without ever receiving focus.
void devAutomationEarlyInit(int argc, char** argv);
bool devAutomationWantsHiddenWindow();

void devAutomationParseArgs(int argc, char** argv);
bool devAutomationIsActive();

// `--dev-fast`: palette fades take no time (tests that measure fades leave it off).
bool devAutomationFastFades();

// Advances the script; called once per input poll.
void devAutomationTick();

// Replaces the hardware mouse state while a script is active.
bool devAutomationOverrideMouse(MouseData* mouseData);

// Extensions: other modules (for example a graphics mod's tests) add their own
// script commands. A handler is called once per input poll until it returns
// true; `first` is set on the first call for each script line.
struct DevAutomationCommand {
    std::string name;
    std::vector<std::string> args;
    std::string rest; // everything after the command name
    int line;
};
using DevAutomationCommandHandler = bool (*)(const DevAutomationCommand& command, bool first);
void devAutomationRegisterCommand(const char* name, DevAutomationCommandHandler handler);

// Called once per input poll while a script runs, before the current command.
void devAutomationRegisterTicker(void (*ticker)());

// Replaces how `shot` (and the failure screenshot) is written; nullptr restores
// the default (`screenshotSavePng`).
void devAutomationSetShotWriter(bool (*writer)(const char* path));

// `<out>/<name><extension>`, creating the output directory.
std::string devAutomationOutputPath(const std::string& name, const char* extension);

// Stops the run with exit code 2 and a failure screenshot.
[[noreturn]] void devAutomationFail(const DevAutomationCommand& command, const char* message);

// The synthetic cursor (logical screen coordinates).
void devAutomationSetMouse(int x, int y);
void devAutomationGetMouse(int* x, int* y);

// Engine state marks matched by `until`.
void devAutomationSetState(const char* state);
const char* devAutomationGetState();

} // namespace fallout

#endif /* FALLOUT_DEV_AUTOMATION_H_ */
