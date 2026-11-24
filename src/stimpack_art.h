#ifndef FALLOUT_STIMPACK_ART_H_
#define FALLOUT_STIMPACK_ART_H_

namespace fallout {

// Names of the images Stimpack makes from the game's own art (see
// stimpack_art.cc). A file of the same name in the game data replaces one.
constexpr const char* kStimpackMainMenuArt = "mainmenu_sp.png"; // main menu, seven plates
constexpr const char* kStimpackHiresMenuPanelArt = "hr_menu_bg_sp.png"; // high-resolution menu panel, seven plates
constexpr const char* kStimpackHubPanelArt = "opbase_sp.png"; // options panel, eight slots (hub)

// Registers the generators with the art loader. Safe to call more than once.
void stimpackArtInit();

} // namespace fallout

#endif /* FALLOUT_STIMPACK_ART_H_ */
