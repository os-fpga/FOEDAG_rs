#ifndef BITGEN_GEMINI_H
#define BITGEN_GEMINI_H

#include "BitGen_data_auto.h"
#include "CFGObject/CFGObject_auto.h"

class BitGen_GEMINI_FCB_IMPL : public BitGen_GEMINI_FCB_DATA {
 public:
  BitGen_GEMINI_FCB_IMPL();
};

class BitGen_GEMINI_ICB_PACKET_IMPL : public BitGen_GEMINI_ICB_PACKET_DATA {
 public:
  BitGen_GEMINI_ICB_PACKET_IMPL();
};

class BitGen_GEMINI {
 public:
  BitGen_GEMINI(const CFGObject_BITOBJ* bitobj);
  bool generate(std::vector<uint8_t>& data);
  static uint64_t parse(std::ofstream& file, const uint8_t* data,
                        uint64_t total_bit_size, uint64_t& bit_index,
                        std::string space, const bool detail);

 private:
  const CFGObject_BITOBJ* m_bitobj;
};

#endif