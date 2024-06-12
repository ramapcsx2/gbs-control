/*
   See https://github.com/PSHarold/OLED-SSD1306-Menu
   for original code, license and documentation
*/
#include "OLEDMenuManager.h"
#include "OLEDMenuFonts.h"

/**
 * @brief Construct a new OLEDMenuManager::OLEDMenuManager object
 *
 * @param display
 */
OLEDMenuManager::OLEDMenuManager(SSD1306Wire * _display)
{
    display = _display;
    // allocate memory for menu items list
    allItems = new OLEDMenuItem * [OLED_MENU_MAX_ITEMS_NUM];
    uint8_t i = 0;
    while(i < OLED_MENU_MAX_ITEMS_NUM) {
        allItems[i] = nullptr;
        i++;
    }
}

/**
 * @brief initializer does what constructor can NOT do
 *
 */
void OLEDMenuManager::init() {
    rootItem = registerItem(nullptr, 0, nullptr);
    pushItem(rootItem);
    // randomSeed(millis()); // does this work?
}

/**
 * @brief
 *
 * @return OLEDMenuItem*
 */
OLEDMenuItem *OLEDMenuManager::allocItem()
{
    int i = 0;
    OLEDMenuItem * newItem = nullptr;
    auto lambdaAllocItem = [&newItem]() {
        newItem = new OLEDMenuItem();
        newItem->used = true;
    };
    while (i < OLED_MENU_MAX_ITEMS_NUM) {
        if(this->allItems[i] == nullptr) {
            lambdaAllocItem();
            break;
        }
        else if (!this->allItems[i]->used)
        {
            delete this->allItems[i];
            lambdaAllocItem();
            break;
        }
        // if (!this->allItems[i].used) {
        //     memset(&this->allItems[i], 0, sizeof(OLEDMenuItem));
        //     newItem = &this->allItems[i];
        //     newItem->used = true;
        //     break;
        // }
        i++;
    }
    if (newItem == nullptr) {
        // char msg[40];
        _DBGF(PSTR("reached max. menu items: %d\n"), OLED_MENU_MAX_ITEMS_NUM);
        // sprintf(msg, PSTR("Maximum number of items reached: %d"), OLED_MENU_MAX_ITEMS_NUM);
        // panicAndDisable(msg);
    }
    return newItem;
}

/**
 * @brief
 *
 * @param parent
 * @param tag
 * @param imageWidth
 * @param imageHeight
 * @param xbmImage
 * @param handler
 * @param alignment
 * @return OLEDMenuItem*
 */
OLEDMenuItem *OLEDMenuManager::registerItem(
    OLEDMenuItem *parent,
    uint16_t tag,
    uint16_t imageWidth,
    uint16_t imageHeight,
    const uint8_t *xbmImage,
    MenuItemHandler handler,
    OLEDDISPLAY_TEXT_ALIGNMENT alignment)
{
    OLEDMenuItem *newItem = allocItem();
    constexpr uint8_t maxMenuItemHeight = OLED_MENU_USABLE_AREA_HEIGHT / (OLED_MENU_ITEMS_PER_SCREEN - 1);
    if(newItem == nullptr)
        goto register_image_item_end;

    if (alignment == OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER_BOTH) {
        alignment = OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_CENTER;
    }
    newItem->imageWidth = imageWidth;

    // fixing the height of image items (at the first row is always a menu control item)
    newItem->imageHeight = (imageHeight > maxMenuItemHeight) ? maxMenuItemHeight : imageHeight;

    newItem->xbmImage = xbmImage;

    newItem->tag = tag;
    newItem->handler = handler;
    newItem->alignment = alignment;
    if (parent) {
        parent->addSubItem(newItem);
        if (parent == rootItem) {
            cursor = 0;
            itemUnderCursor = rootItem->subItems[0];
        }
    }

register_image_item_end:
    return newItem;
}

/**
 * @brief
 *
 * @param parent
 * @param tag
 * @param string
 * @param handler
 * @param font
 * @param alignment
 * @return OLEDMenuItem*
 */
OLEDMenuItem *OLEDMenuManager::registerItem(
    OLEDMenuItem *parent,
    uint16_t tag,
    const char *string,
    MenuItemHandler handler,
    const uint8_t *font,
    OLEDDISPLAY_TEXT_ALIGNMENT alignment)
{
    OLEDMenuItem *newItem = allocItem();
    if(newItem == nullptr)
        goto register_text_item_end;

    newItem->str = string;
    if (font == nullptr) {
        font = DejaVu_Sans_Mono_12;
    }
    newItem->font = font;
    newItem->tag = tag;
    newItem->handler = handler;
    newItem->alignment = alignment;
    if (parent) {
        if (parent->numSubItem == OLED_MENU_MAX_SUBITEMS_NUM) {
            _DBGF(PSTR("Maximum number of sub items reached: %d\n"), OLED_MENU_MAX_SUBITEMS_NUM);
            delete newItem;
            newItem = nullptr;
            goto register_text_item_end;
            // char msg[50];
            // sprintf(msg, PSTR("Maximum number of sub items reached: %d"), OLED_MENU_MAX_SUBITEMS_NUM);
            // panicAndDisable(msg);
        }
        parent->addSubItem(newItem);
        if (parent == rootItem) {
            cursor = 0;
            itemUnderCursor = rootItem->subItems[0];
        }
    }
register_text_item_end:
    return newItem;
}

/**
 * @brief
 *
 * @param item
 * @param yOffset
 * @param negative
 */
void OLEDMenuManager::drawOneItem(OLEDMenuItem *item, uint16_t yOffset, bool negative)
{
    int16_t curScrollOffset = 0;
    if (negative) {
        this->display->setColor(OLEDDISPLAY_COLOR::WHITE); // display in negative, so fill white first, then draw image in black
        this->display->fillRect(0, yOffset, OLED_MENU_WIDTH, item->imageHeight);
        this->display->setColor(OLEDDISPLAY_COLOR::BLACK);
    } else {
        this->display->setColor(OLEDDISPLAY_COLOR::WHITE);
    }

    uint16_t wrappingOffset = OLED_MENU_WIDTH;
    if (item->imageWidth < OLED_MENU_WIDTH) {
        // item width <= screen width, no scrolling
        curScrollOffset = item->alignOffset;
    } else if (item->imageWidth > OLED_MENU_WIDTH && (item == itemUnderCursor || item->alwaysScrolls)) {
#if OLED_MENU_RESET_ALWAYS_SCROLL_ON_SELECTION
        // scrolling items will have lead in frames (they stay still for some frames before scrolling)
        if (leadInFramesLeft == 0) {
            curScrollOffset = item->scrollOffset--;
        } else {
            --leadInFramesLeft;
        }
#else
        if (item == itemUnderCursor && !item->alwaysScrolls) {
            // non always-scrolling items will have lead in frames (they stay still for some frames before scrolling)
            if (leadInFramesLeft == 0) {
                curScrollOffset = item->scrollOffset--;
            } else {
                --leadInFramesLeft;
            }
        } else {
            curScrollOffset = item->scrollOffset--;
        }
#endif
        int16_t left = item->imageWidth + item->scrollOffset + 1;
        wrappingOffset = left + OLED_MENU_WRAPPING_SPACE;
        if (left <= 0) {
            curScrollOffset = item->scrollOffset = wrappingOffset;
            wrappingOffset = OLED_MENU_WIDTH + 1;
        }
    } else {
        // already zeroed, but assign again for easy understanding
        curScrollOffset = 0;
    }
    if (item->str) {
        // let OLEDDisplay handle text alignment
        this->display->setTextAlignment(item->alignment);
        this->display->setFont(item->font);
        this->display->drawString(curScrollOffset, yOffset, item->str);
        if (wrappingOffset < OLED_MENU_WIDTH) {
            this->display->drawString(wrappingOffset, yOffset, item->str);
        }
    } else {
        this->display->drawXbm(curScrollOffset, yOffset, item->imageWidth, item->imageHeight, item->xbmImage);
        if (wrappingOffset < OLED_MENU_WIDTH) {
            this->display->drawXbm(wrappingOffset, yOffset, item->imageWidth, item->imageHeight, item->xbmImage);
        }
    }
}

/**
 * @brief
 *
 * @param parent
 */
void OLEDMenuManager::drawSubItems(OLEDMenuItem *parent)
{
    uint8_t i = 0;
    display->clear();
    drawStatusBar(itemUnderCursor == nullptr);
    uint16_t yOffset = OLED_MENU_STATUS_BAR_HEIGHT;
    uint8_t targetPage = itemUnderCursor == nullptr ? 0 : itemUnderCursor->pageInParent;
    while(i < parent->numSubItem) {
        OLEDMenuItem *subItem = parent->subItems[i];
#if OLED_MENU_OVER_DRAW
        if (subItem->pageInParent >= targetPage) {
#else
        if (subItem->pageInParent == targetPage) {
#endif
            bool negative = subItem == itemUnderCursor;
            drawOneItem(subItem, yOffset, negative);
            yOffset += subItem->imageHeight;
            if (itemUnderCursor == parent->subItems[i]) {
                cursor = i;
            }
            if (subItem->pageInParent > targetPage) {
                // over-draw one item that belongs to the next page in case it is big and leaves a huge blank on current page
                break;
            }
        }
        i++;
    }
    display->display();
}

/**
 * @brief
 *
 * @param preserveCursor
 */
void OLEDMenuManager::goBack(bool preserveCursor)
{
    // go back one page
    if (peakItem() != rootItem) {
        // cannot pop root
        if (state == OLEDMenuState::FREEZING) {
            unfreeze();
        }
        popItem(preserveCursor);
    }
    resetScroll();
    state = OLEDMenuState::GOING_BACK;
}

/**
 * @brief
 *
 * @param preserveCursor
 */
void OLEDMenuManager::goMain(bool preserveCursor)
{
    while (peakItem() != rootItem) {
        goBack(preserveCursor);
    }
}

/**
 * @brief
 *
 */
void OLEDMenuManager::nextItem()
{
    resetScroll();
    OLEDMenuItem *parentItem = peakItem();
    if (!parentItem->numSubItem) {
        // shouldn't happen
        return;
    }
    if (itemUnderCursor == nullptr) {
        // status bar is selected
        itemUnderCursor = parentItem->subItems[cursor = 0];
    } else if (cursor == parentItem->numSubItem - 1) {
        if (parentItem == rootItem) {
            // cannot select status bar on main menu, wrap around
            itemUnderCursor = parentItem->subItems[cursor = 0];
        } else {
            // select status bar next
            itemUnderCursor = nullptr;
        }
    } else {
        itemUnderCursor = parentItem->subItems[++cursor];
    }
}

/**
 * @brief
 *
 */
void OLEDMenuManager::prevItem()
{
    resetScroll();
    OLEDMenuItem *parentItem = peakItem();
    if (!parentItem->numSubItem) {
        // shouldn't happen
        return;
    }
    if (itemUnderCursor == nullptr) {
        // status bar is selected
        cursor = parentItem->numSubItem - 1;
        itemUnderCursor = parentItem->subItems[cursor];
    } else if (cursor == 0) {
        if (parentItem == rootItem) {
            // cannot select status bar on main menu, wrap around
            cursor = parentItem->numSubItem - 1;
            itemUnderCursor = parentItem->subItems[cursor];
        } else {
            // select status bar next
            itemUnderCursor = nullptr;
        }
    } else {
        itemUnderCursor = parentItem->subItems[--cursor];
    }
}

/**
 * @brief
 *
 */
void OLEDMenuManager::resetScroll()
{
    leadInFramesLeft = OLED_MENU_SCROLL_LEAD_IN_FRAMES;
    if (itemUnderCursor && itemUnderCursor->parent) {
        OLEDMenuItem *parent = itemUnderCursor->parent;
        for (int i = 0; i < itemUnderCursor->parent->numSubItem; ++i) {
            if (!OLED_MENU_RESET_ALWAYS_SCROLL_ON_SELECTION && parent->subItems[i]->alwaysScrolls) {
                continue;
            }
            parent->subItems[i]->scrollOffset = 0;
        }
    }
    lastUpdateTime = 0;
}

/**
 * @brief
 *
 * @param item
 * @param btn
 * @param isFirstTime
 */
void OLEDMenuManager::enterItem(OLEDMenuItem *item, OLEDMenuNav btn, bool isFirstTime)
{
    bool willEnter = true;
    if (this->state == OLEDMenuState::IDLE) {
        if(!pushItem(item)) return;
        this->state = OLEDMenuState::ITEM_HANDLING;
    }
    if (item->handler) {
        // Notify the handler that we're about to enter the sub menu and draw sub items of this item, or we're now freezing.
        // You can dynamically edit the sub items, or clear all the items and register new ones.
        // Handlers can return "false" to stop the manager from entering the sub menu if some conditions are not met.
        // If there are no sub items, then the return value does not matter.
        // In addition, a handler can call freeze() to block the manager and print custom stuff. If so,
        // the manager will continously call the handler until unfreeze() is called.
        // And until then, the handler is responsible for handling inputs.
        //
        willEnter = item->handler(this, item, btn, isFirstTime);
        if (this->state == OLEDMenuState::FREEZING) {
            return;
        }
        if (this->state == OLEDMenuState::GOING_BACK) {
            state = OLEDMenuState::IDLE;
            return;
        }
    }
    this->state = OLEDMenuState::IDLE;
    if (!willEnter || !item->numSubItem) {
        popItem();
        return;
    }
    resetScroll();
    cursor = 0;
    this->itemUnderCursor = item->subItems[0];
}

/**
 * @brief
 *
 * @param negative
 */
void OLEDMenuManager::drawStatusBar(bool negative)
{
    if (negative) {
        this->display->setColor(OLEDDISPLAY_COLOR::WHITE); // display in negative, so fill white first, then draw in black
        this->display->fillRect(0, 0, OLED_MENU_WIDTH, OLED_MENU_STATUS_BAR_HEIGHT);
        this->display->setColor(OLEDDISPLAY_COLOR::BLACK);
    } else {
        this->display->setColor(OLEDDISPLAY_COLOR::WHITE);
    }
    if (peakItem() != rootItem) {
        // not on main menu, draw back button
        this->display->drawXbm(0, 0, IMAGE_ITEM(OM_STATUS_BAR_BACK));
    } else {
        // main menu, draw some custom info
        this->display->drawXbm(0, 0, IMAGE_ITEM(OM_STATUS_CUSTOM));
    }
    static uint8_t totalItems = 0;
    uint8_t curIndex = 1;
    this->display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_RIGHT);
    if (itemUnderCursor) {
        curIndex = cursor + 1;
        // itemUnderCursor must have a parent
        totalItems = itemUnderCursor->parent->numSubItem;
        // TODO Adjust to OLED_MENU_STATUS_BAR_HEIGHT
        this->display->setFont(DejaVu_Sans_Mono_10);
        this->display->drawStringf(OLED_MENU_WIDTH, 1, statusBarBuffer, "%d/%d", curIndex, totalItems);
    } else {
        this->display->drawString(OLED_MENU_WIDTH, 0, "     ");
    }
    this->display->setTextAlignment(OLEDDISPLAY_TEXT_ALIGNMENT::TEXT_ALIGN_LEFT);
}

/**
 * @brief
 *
 * @param preserveCursor
 * @return OLEDMenuItem*
 */
OLEDMenuItem *OLEDMenuManager::popItem(bool preserveCursor)
{
    if (itemSP == 1) {
        return rootItem; // cannot pop root
    }
    OLEDMenuItem *top = itemStack[--itemSP];
    if (preserveCursor) {
        for (int i = 0; i < top->parent->numSubItem; ++i) {
            // May need a better way. Maybe store indexes in OLEDMenuItems? But is it worth it?
            if (top == top->parent->subItems[i]) {
                cursor = i;
                itemUnderCursor = top;
                return top;
            }
        }
        // panicAndDisable("invalid item: item cannot be found in its parent's subItems");
        _DBGN(F("invalid item: item cannot be found in its parent's subItems"));
        return nullptr;
    }
    cursor = 0;
    itemUnderCursor = top->parent->subItems[0];
    return top;
}

/**
 * @brief
 *
 * @param nav
 */
void OLEDMenuManager::tick(OLEDMenuNav nav)
{
    if (this->disabled) {
        return;
    }
    if (state == OLEDMenuState::FREEZING) {
        // handlers do not get affected by OLED_MENU_REFRESH_INTERVAL_IN_MS and will get called with every tick
        enterItem(peakItem(), nav, false);
        return;
    }

    unsigned long curMili = millis();
    if (nav == OLEDMenuNav::IDLE) {
        if (curMili - lastKeyPressedTime > OLED_MENU_SCREEN_SAVER_KICK_IN_SECONDS * 1000) {
            if (curMili - screenSaverLastUpdateTime > OLED_MENU_SCREEN_SAVER_REFRESH_INTERVAL_IN_MS) {
                drawScreenSaver();
                screenSaverLastUpdateTime = curMili;
            }
            return;
        } else if (curMili - this->lastUpdateTime < OLED_MENU_REFRESH_INTERVAL_IN_MS) {
            return;
        }
    } else {
        lastKeyPressedTime = curMili;
    }
    switch (nav) {
        case OLEDMenuNav::UP:
            prevItem();
            break;
        case OLEDMenuNav::DOWN:
            nextItem();
            break;
        case OLEDMenuNav::ENTER:
            if (itemUnderCursor) {
                enterItem(itemUnderCursor, OLEDMenuNav::IDLE, true);
            } else {
                // status bar is selected
                goBack();
                this->state = OLEDMenuState::IDLE;
            }
            break;
        default:
            break;
    }
    if (this->state != OLEDMenuState::FREEZING) {
        drawSubItems(peakItem());
        this->lastUpdateTime = curMili;
    }
}