#ifndef BITGEN_GEMINI_H
#define BITGEN_GEMINI_H

#include "BitGen_packer.h"
#include "CFGObject/CFGObject_auto.h"

class BitGen_GEMINI {
 public:
  BitGen_GEMINI(const CFGObject_BITOBJ* bitobj);
  void generate(std::vector<BitGen_BITSTREAM_BOP*>& data);

 protected:
  uint64_t convert_to(uint64_t value, uint64_t unit);
  uint64_t convert_to8(uint64_t value);
  uint64_t convert_to16(uint64_t value);
  uint64_t convert_to32(uint64_t value);
  uint64_t convert_to64(uint64_t value);
  uint64_t align(uint64_t value, uint64_t alignment);
  std::vector<uint8_t> genbits_line_by_line(
      const std::vector<uint8_t>& src_data, uint64_t line_bits,
      uint64_t total_line, uint64_t src_unit_bits, uint64_t dest_unit_bits,
      bool pad_reversed, bool unit_reversed);

 private:
  const CFGObject_BITOBJ* m_bitobj;
};

#endif