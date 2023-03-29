#ifndef BITGEN_GEMINI_H
#define BITGEN_GEMINI_H

#include "BitGen_data_auto.h"
#include "CFGObject/CFGObject_auto.h"

class BitGen_GEMINI_BOP_IMPL : public BitGen_GEMINI_BOP_DATA {
 public:
  BitGen_GEMINI_BOP_IMPL();
  static bool package(const std::string& image, std::vector<uint8_t>& data);
};

class BitGen_GEMINI_FCB_IMPL : public BitGen_GEMINI_FCB_DATA {
 public:
  BitGen_GEMINI_FCB_IMPL();
};

class BitGen_GEMINI_ICB_PACKET_IMPL : public BitGen_GEMINI_ICB_PACKET_DATA {
 public:
  BitGen_GEMINI_ICB_PACKET_IMPL();
};

class BitGen_GEMINI_PCB_PACKET_IMPL : public BitGen_GEMINI_PCB_PACKET_DATA {
 public:
  BitGen_GEMINI_PCB_PACKET_IMPL();
};

class BitGen_GEMINI {
 public:
  BitGen_GEMINI(const CFGObject_BITOBJ* bitobj);
  bool generate(std::vector<uint8_t>& data);
  static void parse(const std::string& input_filepath,
                    const std::string& output_filepath, bool detail);

 private:
  const CFGObject_BITOBJ* m_bitobj;
};

#endif