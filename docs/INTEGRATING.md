# Adding Stimpack to another build of the game

Any project built on Fallout 2 Community Edition (a fork with its own changes,
a total conversion, a graphics mod) can include Stimpack without copying its
code. The project's own sources stay unpatched in its repository; the patches are
applied to a copy at build time.

## Including it

```sh
git submodule add <stimpack repository> external/stimpack
```

In the project's CMakeLists.txt, after the game target is defined:

```cmake
include(external/stimpack/cmake/Stimpack.cmake)
stimpack_attach(${EXECUTABLE_NAME})   # the fallout2-ce target
```

That is all. The target now compiles `<build>/stimpack-tree/src`: the project's
`src/` with Stimpack's patches applied and its sources added. Edits in the
project's `src/` are picked up on the next build. `FALLOUT2_STIMPACK` is defined
for the target, and `STIMPACK_VERSION` holds the version string.

### When a patch does not apply

The patches are applied exactly when possible, otherwise with a fuzz of three
lines, which absorbs most nearby changes a fork makes. If a fork changes the very
lines a patch touches, configuring fails and prints the rejected hunk. Then either
move the fork's change a few lines away from Stimpack's hook, or carry the hook in
the fork in a form the patch still finds. `python3 external/stimpack/tools/stimpack_tree.py
--engine . --check` tests this without building.

Stimpack's own files (`src/stimpack*.cc`, `src/dev_automation.*`) must not exist in
the project's `src/`.

## Hooks for other mods

All of these are optional and live in Stimpack's own headers, which the project's
code can include once Stimpack is attached.

### Drawing (`stimpack.h`)

A graphics mod that draws its own version of images (for example high-resolution
art over the game's 8-bit buffers) learns about every image Stimpack draws:

```cpp
StimpackRenderHooks hooks;
hooks.imageDrawn = [](int window, const FrmImage& image, int srcX, int srcY,
                      int width, int height, int destX, int destY) { /* ... */ };
hooks.buttonArt = [](int button, const FrmImage* normal, const FrmImage* pressed) { /* ... */ };
stimpackSetRenderHooks(hooks);
```

Stimpack's character editor mode uses the game's own character screen, so a mod
that already handles that screen needs nothing more there.

### Generated art (`art.h`, added by the patches)

```cpp
artRegisterGenerator("my_panel.png", [](int* width, int* height, std::vector<unsigned char>* pixels) {
    // fill a palette-indexed, single-frame image
    return true;
});
```

The game then loads `art/<type>/my_panel.png` from the generator whenever no file
of that name exists. Stimpack's menu art is made this way (`stimpack_art.h` lists
the names: `mainmenu_sp.png`, `hr_menu_bg_sp.png`, `opbase_sp.png`). A mod can ship
files of those names to replace them, and a mod that looks up its own versions of
images by name (for example high-resolution packs) can use the same names.

### Test harness (`dev_automation.h`)

Projects can add commands to the scripted test mode:

```cpp
devAutomationRegisterCommand("mycommand", [](const DevAutomationCommand& command, bool first) {
    // command.args: the words after the name; return false to be called again
    // on the next input poll (a command that waits), true when done.
    if (command.args.empty()) {
        devAutomationFail(command, "missing argument");
    }
    return true;
});
devAutomationRegisterTicker([] { /* every input poll while a script runs */ });
devAutomationSetShotWriter([](const char* path) { /* write `shot` images your way */ return true; });
```

`devAutomationOutputPath`, `devAutomationSetMouse` and `devAutomationGetMouse`
give access to the output folder and the scripted cursor.

### Engine state marks

`devAutomationSetState("...")` marks what the game is doing (`mainmenu`, `game`,
`stimpack`, `input`); scripts wait for them with `until`. A project can set its
own marks in its own screens.
