#ifndef BITGEN_GEMINI_H
#define BITGEN_GEMINI_H

#include "BitGen_data_auto.h"
#include "CFGCrypto/CFGCrypto_key.h"
#include "CFGObject/CFGObject_auto.h"

class BitGen_GEMINI_BOP_HEADER_IMPL : public BitGen_GEMINI_BOP_HEADER_DATA {
 public:
  BitGen_GEMINI_BOP_HEADER_IMPL();
  static bool package(const std::string& image, std::vector<uint8_t>& data,
                      CFGCrypto_KEY*& key, const size_t aes_key_size,
                      const size_t header_size);
};

class BitGen_GEMINI_BOP_FOOTER_IMPL : public BitGen_GEMINI_BOP_FOOTER_DATA {
 public:
  BitGen_GEMINI_BOP_FOOTER_IMPL();
  static bool package(std::vector<uint8_t>& data, CFGCrypto_KEY*& key,
                      const size_t aes_key_size, const uint8_t* iv);
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
  bool generate(std::vector<uint8_t>& data, CFGCrypto_KEY*& key,
                std::vector<uint8_t>& aes_key);
  static void parse(const std::string& input_filepath,
                    const std::string& output_filepath, bool detail);

 private:
  const CFGObject_BITOBJ* m_bitobj;
};

#endif