#ifndef TV5725_H_
#define TV5725_H_

#include "tw.h"

namespace detail {
struct TVAttrs {
  // Segment register at 0xf0, no bit offset, 8 bits, initial value assumed invalid
  static const uint8_t SegByteOffset = 0xf0;
  static const uint8_t SegBitOffset = 0;
  static const uint8_t SegBitWidth = 8;
  static const uint8_t SegInitial = 0xff;
};
}

template<uint8_t Addr>
class TV5725 : public tw::SegmentedSlave<Addr, detail::TVAttrs> {
private:
   // Stupid template boilerplate
   typedef tw::SegmentedSlave<Addr, detail::TVAttrs> Base;
   using typename Base::SegValue;
   template<SegValue Seg, uint8_t ByteOffset, uint8_t BitOffset, uint8_t BitWidth, tw::Signage Signed>
   using Register = typename Base::template Register<Seg, ByteOffset, BitOffset, BitWidth, Signed>;
   template<SegValue Seg, uint8_t ByteOffset, uint8_t BitOffset, uint8_t BitWidth>
   using UReg = Register<Seg, ByteOffset, BitOffset, BitWidth, tw::Signage::UNSIGNED>;

public:
  typedef UReg<0x03, 0x01, 0, 12> VDS_HSYNC_RST;
  typedef UReg<0x03, 0x02, 4, 11> VDS_VSYNC_RST;
  typedef UReg<0x03, 0x0d, 0, 11> VDS_VS_ST;
  typedef UReg<0x03, 0x0e, 4, 11> VDS_VS_SP;

  typedef UReg<0x00, 0x11, 4, 11> STATUS_VDS_VERT_COUNT;

  typedef UReg<0x00, 0x90, 0, 1> OSD_SW_RESET;
  typedef UReg<0x00, 0x90, 1, 3> OSD_HORIZONTAL_ZOOM;
  typedef UReg<0x00, 0x90, 4, 2> OSD_VERTICAL_ZOOM;
  typedef UReg<0x00, 0x90, 6, 1> OSD_DISP_EN;
  typedef UReg<0x00, 0x90, 7, 1> OSD_MENU_EN;
  typedef UReg<0x00, 0x91, 0, 4> OSD_MENU_ICON_SEL;
  typedef UReg<0x00, 0x91, 4, 4> OSD_MENU_MOD_SEL;
  typedef UReg<0x00, 0x92, 0, 3> OSD_MENU_BAR_FONT_FORCOR;
  typedef UReg<0x00, 0x92, 3, 3> OSD_MENU_BAR_FONT_BGCOR;
  typedef UReg<0x00, 0x92, 6, 3> OSD_MENU_BAR_BORD_COR;
  typedef UReg<0x00, 0x93, 1, 3> OSD_MENU_SEL_FORCOR;
  typedef UReg<0x00, 0x93, 4, 3> OSD_MENU_SEL_BGCOR;
  typedef UReg<0x00, 0x93, 7, 1> OSD_COMMAND_FINISH;
  typedef UReg<0x00, 0x94, 0, 1> OSD_MENU_DISP_STYLE;
  typedef UReg<0x00, 0x94, 2, 1> OSD_YCBCR_RGB_FORMAT;
  typedef UReg<0x00, 0x94, 3, 1> OSD_INT_NG_LAT;
  typedef UReg<0x00, 0x94, 4, 4> OSD_TEST_SEL;
  typedef UReg<0x00, 0x95, 0, 8> OSD_MENU_HORI_START;
  typedef UReg<0x00, 0x96, 0, 8> OSD_MENU_VER_START;
  typedef UReg<0x00, 0x97, 0, 8> OSD_BAR_LENGTH;
  typedef UReg<0x00, 0x98, 0, 8> OSD_BAR_FOREGROUND_VALUE;

  static const uint8_t OSD_ZOOM_1X = 0;
  static const uint8_t OSD_ZOOM_2X = 1;
  static const uint8_t OSD_ZOOM_3X = 2;
  static const uint8_t OSD_ZOOM_4X = 3;
  static const uint8_t OSD_ZOOM_5X = 4;
  static const uint8_t OSD_ZOOM_6X = 5;
  static const uint8_t OSD_ZOOM_7X = 6;
  static const uint8_t OSD_ZOOM_8X = 7;

  static const uint8_t OSD_MENU_DISP_STYLE_VERTICAL = 0;
  static const uint8_t OSD_MENU_DISP_STYLE_HORIZONTAL = 1;

  static const uint8_t OSD_ICON_NONE = 0;
  static const uint8_t OSD_ICON_BRIGHTNESS = 1;
  static const uint8_t OSD_ICON_CONTRAST = 2;
  static const uint8_t OSD_ICON_HUE = 3;
  static const uint8_t OSD_ICON_SOUND = 4;
  static const uint8_t OSD_ICON_UP_DOWN = 8;
  static const uint8_t OSD_ICON_LEFT_RIGHT = 9;
  static const uint8_t OSD_ICON_VERTICAL_SIZE = 10;
  static const uint8_t OSD_ICON_HORIZONTAL_SIZE = 11;
  static const uint8_t OSD_ICON_COUNT = 8;

  static inline uint8_t osdIcon(uint8_t index) {
    static const uint8_t osdIcons[8] = {
        OSD_ICON_BRIGHTNESS,
        OSD_ICON_CONTRAST,
        OSD_ICON_HUE,
        OSD_ICON_SOUND,
        OSD_ICON_UP_DOWN,
        OSD_ICON_LEFT_RIGHT,
        OSD_ICON_VERTICAL_SIZE,
        OSD_ICON_HORIZONTAL_SIZE,
    };
    return osdIcons[index];
  }

  static const uint8_t OSD_COLOR_BLACK = 0;
  static const uint8_t OSD_COLOR_BLUE = 1;
  static const uint8_t OSD_COLOR_GREEN = 2;
  static const uint8_t OSD_COLOR_CYAN = 3;
  static const uint8_t OSD_COLOR_RED = 4;
  static const uint8_t OSD_COLOR_MAGENTA = 5;
  static const uint8_t OSD_COLOR_YELLOW = 6;
  static const uint8_t OSD_COLOR_WHITE = 7;

  static const uint8_t OSD_FORMAT_YCBCR = 1;
  static const uint8_t OSD_FORMAT_RGB = 0;
};


#endif


