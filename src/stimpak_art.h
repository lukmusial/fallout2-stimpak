#ifndef FALLOUT_STIMPAK_ART_H_
#define FALLOUT_STIMPAK_ART_H_

namespace fallout {

// Names of the images Stimpak makes from the game's own art (see
// stimpak_art.cc). A file of the same name in the game data replaces one.
constexpr const char* kStimpakMainMenuArt = "mainmenu_sp.png"; // main menu, seven plates
constexpr const char* kStimpakHiresMenuPanelArt = "hr_menu_bg_sp.png"; // high-resolution menu panel, seven plates
constexpr const char* kStimpakHubPanelArt = "opbase_sp.png"; // options panel, nine slots (hub)

// Registers the generators with the art loader. Safe to call more than once.
void stimpakArtInit();

} // namespace fallout

#endif /* FALLOUT_STIMPAK_ART_H_ */
