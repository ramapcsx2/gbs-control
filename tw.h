#ifndef TW_H_
#define TW_H_

#include <Wire.h>

namespace tw {

enum class Signage {
  UNSIGNED,
  SIGNED
};

namespace detail {

template <uint8_t BitWidth, Signage Signed>
struct RegValue_ {
  // Recursively try next largest bit width if there isn't a specialization
  typedef typename RegValue_<BitWidth + 1, Signed>::Type Type;
};

template<>
struct RegValue_<8, Signage::UNSIGNED> {
  typedef uint8_t Type;
};

template<>
struct RegValue_<8, Signage::SIGNED> {
  typedef int8_t Type;
};

template<>
struct RegValue_<16, Signage::UNSIGNED> {
  typedef uint16_t Type;
};

template<>
struct RegValue_<16, Signage::SIGNED> {
  typedef int16_t Type;
};

template<>
struct RegValue_<32, Signage::UNSIGNED> {
  typedef uint32_t Type;
};

template<>
struct RegValue_<32, Signage::SIGNED> {
  typedef int32_t Type;
};

template<uint8_t BitWidth, Signage Signed>
using RegValue = typename RegValue_<BitWidth, Signed>::Type;

void rawRead(uint8_t addr, uint8_t reg, uint8_t* output, uint8_t size)
{
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom(addr, size, static_cast<uint8_t>(true));
  uint8_t rcvBytes = 0;
  while (Wire.available())
  {
    output[rcvBytes++] = Wire.read();
  }

#if 0
  Serial.print("READ "); Serial.print(addr, HEX); Serial.print("@"); Serial.print(reg, HEX); Serial.print(": ");
  for (uint8_t i = 0; i < size; ++i) {
    Serial.print(output[i] >> 4, HEX); Serial.print(output[i] & 0xF, HEX);
  }
  Serial.println();
#endif
}

void rawWrite(uint8_t addr, uint8_t reg, uint8_t const* input, uint8_t size)
{
#if 0
  Serial.print("WRITE "); Serial.print(addr, HEX); Serial.print("@"); Serial.print(reg, HEX); Serial.print(": ");
  for (uint8_t i = 0; i < size; ++i) {
    Serial.print(input[i] >> 4, HEX); Serial.print(input[i] & 0xF, HEX);
  }
  Serial.println();
#endif
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(input, size);
  Wire.endTransmission();
}

template<uint8_t BitOffset, uint8_t BitWidth>
RegValue<BitWidth, Signage::UNSIGNED> regRead(uint8_t addr, uint8_t offset) {
  static const uint8_t ByteSize = (BitOffset + BitWidth + 7) / 8;
  RegValue<BitWidth, Signage::UNSIGNED> value;
  uint8_t data[ByteSize];

  rawRead(addr, offset, data, ByteSize);
  value = data[0] >> BitOffset;
  for (uint8_t i = 1; i < ByteSize; ++i) {
    value |= static_cast<RegValue<BitWidth, Signage::UNSIGNED>>(data[i]) << (8 * i - BitOffset);
  }
  value &= (1 << BitWidth) - 1;
  return value;
}

// This silences a compiler warning because GCC can't tell that a negative result
// only occurs when the if branch isn't taken
static inline uint8_t rightShift(uint8_t size, uint8_t offset) {
  return 8 * (size - 1) - offset;
}

template<uint8_t BitOffset, uint8_t BitWidth>
void regWrite(uint8_t addr, uint8_t offset, RegValue<BitWidth, Signage::UNSIGNED> value){
  static const uint8_t ByteSize = (BitOffset + BitWidth + 7) / 8;
  uint8_t data[ByteSize];
  if (BitOffset == 0 && BitWidth % 8 == 0)
    memset(data, 0, sizeof(data));
  else
    rawRead(addr, offset, data, ByteSize);
  if (ByteSize == 1) {
    static const uint8_t mask = static_cast<uint8_t>(((1u << BitWidth) - 1) << BitOffset);
    data[0] = (data[0] & ~mask) | ((value << BitOffset) & mask);
  } else {
    static const uint8_t mask = static_cast<uint8_t>(0xFFu << BitOffset);
    data[0] = (data[0] & ~mask) | ((value << BitOffset) & mask);
    for (uint8_t i = 1; i < ByteSize - 1; ++i) {
      data[i] = value >> (8 * i - BitOffset);
    }
    static const uint8_t mask2 = (1 << (BitWidth + BitOffset - (ByteSize - 1) * 8)) - 1;
    data[ByteSize - 1] = (data[ByteSize - 1] & ~mask2) | (value >> rightShift(ByteSize, BitOffset));
  }
  rawWrite(addr, offset, data, ByteSize);
}

}

template <uint8_t Addr>
class Slave {
protected:
  template <uint8_t ByteOffset, uint8_t BitOffset, uint8_t BitWidth, Signage Signed>
  class Register {
    static_assert(BitWidth <= 32, "Register too wide");
    static_assert(BitOffset < 8, "Register bit offset too large");

  public:
    typedef detail::RegValue<BitWidth, Signed> Value;

    static inline Value read(void) {
      return detail::regRead<BitOffset, BitWidth>(Addr, ByteOffset);
    }

    static inline void write(Value value) {
      detail::regWrite<BitOffset, BitWidth>(Addr, ByteOffset, value);   
    }
  };
};

template<uint8_t Addr, class Attrs>
class SegmentedSlave : public Slave<Addr> {
private:
   typedef tw::Slave<Addr> Base;
   template<uint8_t ByteOffset, uint8_t BitOffset, uint8_t BitWidth, Signage Signed>
   using BaseReg = typename Base::template Register<ByteOffset, BitOffset, BitWidth, Signed>;
   typedef BaseReg<Attrs::SegByteOffset, Attrs::SegBitOffset, Attrs::SegBitWidth, Signage::UNSIGNED> Segment;

protected:
   typedef typename Segment::Value SegValue;

private:
   static void setSeg(SegValue seg) {
     static SegValue curSeg = Attrs::SegInitial;
     if (curSeg != seg) {
       Segment::write(seg);
       curSeg = seg;
     }
   }

public:
   template<SegValue Seg, uint8_t ByteOffset, uint8_t BitOffset, uint8_t BitWidth, Signage Signed>
   class Register : public BaseReg<ByteOffset, BitOffset, BitWidth, Signed> {
   private:
     typedef BaseReg<ByteOffset, BitOffset, BitWidth, Signed> Base;
     typedef typename Base::Value Value;

   public:
     static Value read(void) {
       setSeg(Seg);
       return Base::read();
     }

     static void write(Value value) {
       setSeg(Seg);
       Base::write(value);
     }
   };

   static void read(SegValue seg, uint8_t offset, uint8_t* output, uint8_t size) {
     setSeg(seg);
     detail::rawRead(Addr, offset, output, size);
   }

   static void write(SegValue seg, uint8_t offset, uint8_t const* input, uint8_t size) {
     setSeg(seg);
     detail::rawWrite(Addr, offset, input, size);
   }
};

}

#endif


