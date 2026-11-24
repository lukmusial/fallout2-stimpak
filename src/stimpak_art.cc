#include "stimpak_art.h"

#include <string.h>

#include <vector>

#include "art.h"

namespace fallout {

// Stimpak's menu art is made at run time from the game's own images, so no
// game art is shipped with the mod. Pixel indices are copied, never
// re-quantised, so the results use the game palette exactly.

namespace {

    // mainmenu.frm (640x480): the button plates are 35 rows tall on a 41-row
    // pitch starting at y=15, spanning roughly x=16..210. Copying the 41-row band
    // that ends with the last plate's shadow one pitch further down adds a plate.
    constexpr int kVanillaPitch = 41;
    constexpr int kVanillaLastBandTop = 214;
    constexpr int kVanillaLastBandBottom = 257;
    constexpr int kVanillaLeft = 14;
    constexpr int kVanillaRight = 212;

    // HR_MENU_BG.FRM (191x242, high-resolution menu): opaque plates separated
    // by transparent rows; the last plate spans rows 205..241. A new plate
    // follows after the same 5-row gap.
    constexpr int kHiresLastPlateTop = 205;
    constexpr int kHiresLastPlateBottom = 242;
    constexpr int kHiresGap = 5;

    // opbase.frm (164x217, options panel): five 34-row slots on a 37-row pitch
    // starting at y=17. The Stimpak hub needs nine (a header and eight buttons).
    constexpr int kOpbaseFirstSlot = 17;
    constexpr int kOpbasePitch = 37;
    constexpr int kOpbaseSlots = 5;
    constexpr int kHubSlots = 9;

    bool copyImage(const FrmImage& image, int* width, int* height, std::vector<unsigned char>* pixels)
    {
        if (image.getData() == nullptr) {
            return false;
        }

        *width = image.getWidth();
        *height = image.getHeight();
        pixels->assign(image.getData(), image.getData() + static_cast<size_t>(*width) * static_cast<size_t>(*height));
        return true;
    }

    bool makeMainMenu(int* width, int* height, std::vector<unsigned char>* pixels)
    {
        FrmImage image;
        if (!image.lock(InterfaceFrmId(InterfaceFrameId::MainMenuBackgroundImage)) || !copyImage(image, width, height, pixels)) {
            return false;
        }

        if (*height < kVanillaLastBandBottom + kVanillaPitch || *width < kVanillaRight) {
            return false;
        }

        // The band and its copy overlap (rows 255..256), so copy from the original.
        const std::vector<unsigned char> original(*pixels);
        const size_t stride = static_cast<size_t>(*width);
        for (int y = kVanillaLastBandTop; y < kVanillaLastBandBottom; y++) {
            memcpy(pixels->data() + (y + kVanillaPitch) * stride + kVanillaLeft,
                original.data() + y * stride + kVanillaLeft,
                kVanillaRight - kVanillaLeft);
        }
        return true;
    }

    bool makeHiresMenuPanel(int* width, int* height, std::vector<unsigned char>* pixels)
    {
        FrmImage image;
        // A full path, not InterfaceFrmId("..."): locking by name formats the path
        // into the engine's shared name buffer, which the caller of this
        // generator may still be using.
        if (!image.lock("art\\intrface\\HR_MENU_BG.FRM") || !copyImage(image, width, height, pixels)) {
            return false;
        }

        if (*height < kHiresLastPlateBottom) {
            return false;
        }

        const size_t stride = static_cast<size_t>(*width);
        std::vector<unsigned char> plate(pixels->begin() + kHiresLastPlateTop * stride, pixels->begin() + kHiresLastPlateBottom * stride);
        pixels->resize(pixels->size() + kHiresGap * stride, 0);
        pixels->insert(pixels->end(), plate.begin(), plate.end());
        *height += kHiresGap + (kHiresLastPlateBottom - kHiresLastPlateTop);
        return true;
    }

    bool makeHubPanel(int* width, int* height, std::vector<unsigned char>* pixels)
    {
        FrmImage image;
        if (!image.lock(InterfaceFrmId(InterfaceFrameId::OptionsBase)) || !copyImage(image, width, height, pixels)) {
            return false;
        }

        const int end = kOpbaseFirstSlot + kOpbaseSlots * kOpbasePitch;
        if (*height < end) {
            return false;
        }

        const size_t stride = static_cast<size_t>(*width);
        std::vector<unsigned char> band(pixels->begin() + (end - kOpbasePitch) * stride, pixels->begin() + end * stride);
        std::vector<unsigned char> tail(pixels->begin() + end * stride, pixels->end());
        pixels->resize(end * stride);
        for (int extra = 0; extra < kHubSlots - kOpbaseSlots; extra++) {
            pixels->insert(pixels->end(), band.begin(), band.end());
        }
        pixels->insert(pixels->end(), tail.begin(), tail.end());
        *height += (kHubSlots - kOpbaseSlots) * kOpbasePitch;
        return true;
    }

} // namespace

void stimpakArtInit()
{
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;

    artRegisterGenerator(kStimpakMainMenuArt, makeMainMenu);
    artRegisterGenerator(kStimpakHiresMenuPanelArt, makeHiresMenuPanel);
    artRegisterGenerator(kStimpakHubPanelArt, makeHubPanel);
}

} // namespace fallout
