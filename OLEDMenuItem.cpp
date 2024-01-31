#include "OLEDMenuItem.h"
#include <string.h>
void OLEDMenuItem::calculate()
{
    if(!this->parent){
        return; // root 
    }
    if (this->str) {
        size_t strLength = strlen(this->str);
        this->imageWidth = pgm_read_byte(this->font) * strLength;
        this->imageHeight = pgm_read_byte(this->font + 1);
        // Let OLEDDisplay handle text alignment to achieve perfect alignments.
        // Of course if would be much more elegant if we could treat text as images (using imageWidth),
        // but alignments would be off if using non-monospaced fonts.
        // the calculated imageWidth will always be larger if using non-monospaced fonts, and weird things will happen,
        // so it's best to just use a monospaced font.)
        if (this->imageWidth <= OLED_MENU_WIDTH) {
            switch (this->alignment) {
                case OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_LEFT:
                    this->alignOffset = 0;
                    break;
                case OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER:
                    this->alignOffset = OLED_MENU_WIDTH / 2;
                    break;
                case OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_RIGHT:
                    this->alignOffset = OLED_MENU_WIDTH;
                    break;
                default:
                    break;
            }
        }
    } else if (this->imageWidth <= OLED_MENU_WIDTH) {
        switch (this->alignment) {
            case OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER:
            default:
                this->alignOffset = (OLED_MENU_WIDTH - this->imageWidth) / 2;
                break;
            case OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_RIGHT:
                this->alignOffset = OLED_MENU_WIDTH - this->imageWidth;
                break;
        }
    }

    if (this->imageWidth > OLED_MENU_WIDTH) {
        // Center alignment makes no sense when scrolling. Switch to left aligment.
        this->alignment = OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_LEFT;
    }

    // decide which page this item belongs to
    uint16_t lastLineAt = 0;
    for (int i = 0; i < parent->numSubItem - 1; ++i) {
        // find all items on the last page, see if it fits
        // put it on the next page if not.
        // last item in parent->numSubItem is always "this"
        OLEDMenuItem *item = parent->subItems[i];
        if (item->pageInParent != parent->maxPageIndex) {
            continue;
        }
        lastLineAt += item->imageHeight;
    }
    lastLineAt += imageHeight; // self

    if (lastLineAt <= OLED_MENU_USABLE_AREA_HEIGHT) {
        this->pageInParent = parent->maxPageIndex;
    } else {
        this->pageInParent = ++parent->maxPageIndex;
    }
}