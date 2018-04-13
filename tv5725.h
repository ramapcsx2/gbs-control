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
  typedef UReg<0x03, 0x01, 0, 11> VDS_HSYNC_RST;
  typedef UReg<0x03, 0x02, 4, 11> VDS_VSYNC_RST;
  typedef UReg<0x03, 0x0d, 0, 11> VDS_VS_ST;
  typedef UReg<0x03, 0x0e, 4, 11> VDS_VS_SP;
};


#endif


