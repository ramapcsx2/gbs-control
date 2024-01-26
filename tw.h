#ifndef TW_H_
#define TW_H_

#include <Wire.h>

namespace tw
{

    enum class Signage {
        UNSIGNED,
        SIGNED
    };

    namespace detail
    {

        // Template to pick integer type to fit a register of a particular bit
        // width and signedness
        template <uint8_t BitWidth, Signage Signed>
        struct RegValue_
        {
            // Recursively try next largest bit width if there isn't a
            // specialization
            typedef typename RegValue_<BitWidth + 1, Signed>::Type Type;
        };

        template <>
        struct RegValue_<8, Signage::UNSIGNED>
        {
            typedef uint8_t Type;
        };

        template <>
        struct RegValue_<8, Signage::SIGNED>
        {
            typedef int8_t Type;
        };

        template <>
        struct RegValue_<16, Signage::UNSIGNED>
        {
            typedef uint16_t Type;
        };

        template <>
        struct RegValue_<16, Signage::SIGNED>
        {
            typedef int16_t Type;
        };

        template <>
        struct RegValue_<32, Signage::UNSIGNED>
        {
            typedef uint32_t Type;
        };

        template <>
        struct RegValue_<32, Signage::SIGNED>
        {
            typedef int32_t Type;
        };

        template <uint8_t BitWidth, Signage Signed>
        using RegValue = typename RegValue_<BitWidth, Signed>::Type;

        inline void rawRead(uint8_t addr, uint8_t reg, uint8_t *output, uint8_t size)
        {
            Wire.beginTransmission(addr);
            Wire.write(reg);
            Wire.endTransmission();
            Wire.requestFrom(addr, size, static_cast<uint8_t>(true));
            uint8_t rcvBytes = 0;
            while (Wire.available()) {
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

        inline void rawWrite(uint8_t addr, uint8_t reg, uint8_t const *input, uint8_t size)
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

        // Number of bytes covered by a register with a particular offset and
        // width
        static constexpr uint8_t byteSize(uint8_t BitOffset, uint8_t BitWidth)
        {
            return (BitOffset + BitWidth + 7) / 8;
        }

        // Decode a register from the raw bytes it covers
        template <uint8_t BitOffset, uint8_t BitWidth>
        RegValue<BitWidth, Signage::UNSIGNED> regDecode(uint8_t *data)
        {
            RegValue<BitWidth, Signage::UNSIGNED> value;
            // For the least significant byte, just shift off the leading bits
            // we don't want
            value = data[0] >> BitOffset;
            // Append the rest of the bytes to the value.  Note the correction
            // by -BitOffset to line the positions up with the least significant
            // byte above.
            for (uint8_t i = 1; i < byteSize(BitOffset, BitWidth); ++i) {
                value |= static_cast<RegValue<BitWidth, Signage::UNSIGNED>>(data[i]) << (8 * i - BitOffset);
            }
            // Mask off extraneous bits from the most significant byte
            value &= (1 << BitWidth) - 1;
            return value;
        }

        template <uint8_t BitOffset, uint8_t BitWidth>
        RegValue<BitWidth, Signage::UNSIGNED> regRead(uint8_t addr, uint8_t offset)
        {
            static const uint8_t bs = byteSize(BitOffset, BitWidth);
            uint8_t data[bs];
            rawRead(addr, offset, data, bs);
            return regDecode<BitOffset, BitWidth>(data);
        }

        // This silences a compiler warning because GCC can't tell that a
        // negative result only occurs when the if branch isn't taken
        static inline uint8_t rightShift(uint8_t size, uint8_t offset)
        {
            return 8 * (size - 1) - offset;
        }

        // Encode a register into the raw bytes it covers
        template <uint8_t BitOffset, uint8_t BitWidth>
        void regEncode(RegValue<BitWidth, Signage::UNSIGNED> value, uint8_t *data)
        {
            static const uint8_t bs = byteSize(BitOffset, BitWidth);
            // Special case when only one byte is covered
            if (bs == 1) {
                // Create a mask of the specified width and position
                static const uint8_t mask = static_cast<uint8_t>(((1u << BitWidth) - 1) << BitOffset);
                // Move value into position, mask it, and combine with existing
                // data
                data[0] = (data[0] & ~mask) | ((value << BitOffset) & mask);
            } else {
                // For the least significant byte, create a mask from the bit
                // offset up
                static const uint8_t mask = static_cast<uint8_t>(0xFFu << BitOffset);
                // Move value into position, mask it, and combine with existing
                // data
                data[0] = (data[0] & ~mask) | ((value << BitOffset) & mask);
                // For remaining bytes other than the most significant, the entire
                // byte is covered, so simply shift into position and truncate
                for (uint8_t i = 1; i < bs - 1; ++i) {
                    data[i] = value >> (8 * i - BitOffset);
                }
                // For the most significant byte, create a mask for the covered
                // lower bits
                static const uint8_t mask2 = (1 << (BitWidth + BitOffset - (bs - 1) * 8)) - 1;
                // Apply it.  rightShift is really (8 * (bs - 1) - BitOffset), but
                // gcc doesn't realize that bs - 1 can't be negative in this
                // branch and warns about a negative shift value.  The helper
                // function silences the warning.
                data[bs - 1] = (data[bs - 1] & ~mask2) | (value >> rightShift(bs, BitOffset));
            }
        }

        template <uint8_t BitOffset, uint8_t BitWidth>
        void regWrite(uint8_t addr, uint8_t offset, RegValue<BitWidth, Signage::UNSIGNED> value)
        {
            static const uint8_t bs = byteSize(BitOffset, BitWidth);
            uint8_t data[bs];
            if (BitOffset == 0 && BitWidth % 8 == 0)
                memset(data, 0, sizeof(data));
            else
                rawRead(addr, offset, data, bs);
            regEncode<BitOffset, BitWidth>(value, data);
            rawWrite(addr, offset, data, bs);
        }

        // Template to compute the range of byte offsets covered by a list of
        // registers
        template <class... Regs>
        struct RegRange
        {
        };

        template <>
        struct RegRange<>
        {
            static const uint8_t start = 0xFF;
            static const uint8_t end = 0x00;
        };

        template <class Reg, class... Tail>
        struct RegRange<Reg, Tail...>
        {
        private:
            static const uint8_t tailStart = RegRange<Tail...>::start;
            static const uint8_t tailEnd = RegRange<Tail...>::end;
            static const uint8_t regEnd = Reg::byteOffset + byteSize(Reg::bitOffset, Reg::bitWidth);

        public:
            static const uint8_t start = Reg::byteOffset < tailStart ? Reg::byteOffset : tailStart;
            static const uint8_t end = regEnd > tailEnd ? regEnd : tailEnd;
        };

        // Template to check whether a list of segmented registers are all in
        // the same segment.
        template <class SegValue, class... Regs>
        struct SegCompatible
        {
        };

        template <class SegValue, class Reg>
        struct SegCompatible<SegValue, Reg>
        {
            static const bool compatible = true;
            static constexpr SegValue segment = Reg::segment;
        };

        template <class SegValue, class Reg1, class Reg2, class... Tail>
        struct SegCompatible<SegValue, Reg1, Reg2, Tail...>
        {
            static const bool compatible = Reg1::segment == Reg2::segment && SegCompatible<SegValue, Reg2, Tail...>::compatible;
            static constexpr SegValue segment = Reg1::segment;
        };

    } // namespace detail

    template <uint8_t Addr>
    class Slave
    {
    protected:
        template <uint8_t ByteOffset, uint8_t BitOffset, uint8_t BitWidth, Signage Signed>
        class Register
        {
            static_assert(BitWidth <= 32, "Register too wide");
            static_assert(BitOffset < 8, "Register bit offset too large");

        public:
            static const uint8_t byteOffset = ByteOffset;
            static const uint8_t bitOffset = BitOffset;
            static const uint8_t bitWidth = BitWidth;
            static const Signage signage = Signed;

            typedef detail::RegValue<BitWidth, Signed> Value;

            static inline Value read(void)
            {
                return detail::regRead<BitOffset, BitWidth>(Addr, ByteOffset);
            }

            static inline void write(Value value)
            {
                detail::regWrite<BitOffset, BitWidth>(Addr, ByteOffset, value);
            }
        };

        // Ties several registers together so they can be
        // read/written/modified in a single transaction.
        template <class... Regs>
        class Tie
        {
        public:
            static void read(typename Regs::Value &... values)
            {
                static const uint8_t start = detail::RegRange<Regs...>::start;
                static const uint8_t end = detail::RegRange<Regs...>::end;
                static const uint8_t size = end - start;
                uint8_t data[size];
                detail::rawRead(Addr, start, data, size);
                // We really want a regDecode statement for each register, but
                // template parameter packs can only be expanded in expression
                // list contexts such as function parameters or initializer
                // lists.  We turn each call into an int expression with the
                // comma operator and capture the results into a dummy array.
                // The attribute prevents gcc from warning about an unused
                // variable.
                int dummy[] __attribute__((unused)) = {
                    (values = detail::regDecode<Regs::bitOffset, Regs::bitWidth>(data + Regs::byteOffset - start), 0)...};
            }

            static void write(typename Regs::Value... values)
            {
                static const uint8_t start = detail::RegRange<Regs...>::start;
                static const uint8_t end = detail::RegRange<Regs...>::end;
                static const uint8_t size = end - start;
                uint8_t data[size];
                // FIXME: we can avoid this if registers are contiguous and
                // aligned to byte boundaries at both start and end.  The
                // template logic for determining this would be a bit complex
                // since we would need to sort the register list first.
                detail::rawRead(Addr, start, data, size);
                int dummy[] __attribute__((unused)) = {
                    (detail::regEncode<Regs::bitOffset, Regs::bitWidth>(values, data + Regs::byteOffset - start), 0)...};
                detail::rawWrite(Addr, start, data, size);
            }
        };
    };

    template <uint8_t Addr, class Attrs>
    class SegmentedSlave : public Slave<Addr>
    {
    private:
        typedef tw::Slave<Addr> Base;
        template <uint8_t ByteOffset, uint8_t BitOffset, uint8_t BitWidth, Signage Signed>
        using BaseReg = typename Base::template Register<ByteOffset, BitOffset, BitWidth, Signed>;
        typedef BaseReg<Attrs::SegByteOffset, Attrs::SegBitOffset, Attrs::SegBitWidth, Signage::UNSIGNED> Segment;

    protected:
        typedef typename Segment::Value SegValue;

    private:
        static void setSeg(SegValue seg)
        {
            static SegValue curSeg = Attrs::SegInitial;
            if (curSeg != seg) {
                Segment::write(seg);
                curSeg = seg;
            }
        }

    public:
        template <SegValue Seg, uint8_t ByteOffset, uint8_t BitOffset, uint8_t BitWidth, Signage Signed>
        class Register : public BaseReg<ByteOffset, BitOffset, BitWidth, Signed>
        {
        private:
            typedef BaseReg<ByteOffset, BitOffset, BitWidth, Signed> Base;

        public:
            typedef typename Base::Value Value;
            static const SegValue segment = Seg;

            static Value read(void)
            {
                setSeg(Seg);
                return Base::read();
            }

            static void write(Value value)
            {
                setSeg(Seg);
                Base::write(value);
            }
        };

        template <class... Regs>
        class Tie : public Base::template Tie<Regs...>
        {
        private:
            static_assert(detail::SegCompatible<SegValue, Regs...>::compatible, "Tied registers must all be in the same segment");
            static const SegValue segment = detail::SegCompatible<SegValue, Regs...>::segment;
            typedef typename Base::template Tie<Regs...> BaseTie;

        public:
            static void read(typename Regs::Value &... values)
            {
                setSeg(segment);
                BaseTie::read(values...);
            }

            static void write(typename Regs::Value... values)
            {
                setSeg(segment);
                BaseTie::write(values...);
            }
        };

        static void read(SegValue seg, uint8_t offset, uint8_t *output, uint8_t size)
        {
            setSeg(seg);
            detail::rawRead(Addr, offset, output, size);
        }

        static uint8_t read(SegValue seg, uint8_t offset)
        {
            uint8_t value;
            read(seg, offset, &value, sizeof(value));
            return value;
        }

        static void write(SegValue seg, uint8_t offset, uint8_t const *input, uint8_t size)
        {
            setSeg(seg);
            detail::rawWrite(Addr, offset, input, size);
        }

        static void write(SegValue seg, uint8_t offset, uint8_t value)
        {
            write(seg, offset, &value, sizeof(value));
        }
    };

} // namespace tw

#endif
