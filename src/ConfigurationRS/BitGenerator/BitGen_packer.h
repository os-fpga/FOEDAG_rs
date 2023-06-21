#ifndef BITGEN_PACKER_H
#define BITGEN_PACKER_H

#include "CFGCommonRS/CFGCommonRS.h"
#include "CFGCrypto/CFGCrypto_key.h"

#define BitGen_BITSTREAM_BLOCK_SIZE (2048)

const std::vector<std::string> BitGen_BITSTREAM_SUPPORTED_BOP_IDENTIFIER = {
    "FSBL", "FCB", "ICB", "PCB"};

struct BitGen_BITSTREAM_BOP_FIELD {
  ~BitGen_BITSTREAM_BOP_FIELD();
  std::string identifier = "";
  uint32_t version = 0;
  std::string tool = "";
  std::string opn = "";
  uint32_t jtag_id = 0;
  uint32_t jtag_mask = 0;
  uint8_t chipid = 0;
  uint8_t checksum = 0;
  uint8_t integrity = 0;
  uint8_t iv[16] = {0};
};

struct BitGen_BITSTREAM_ACTION {
  BitGen_BITSTREAM_ACTION(uint16_t a);
  BitGen_BITSTREAM_ACTION(const std::string& a);
  ~BitGen_BITSTREAM_ACTION();
  uint16_t action = 0;
  bool has_checksum = false;
  std::vector<uint8_t> field = {};
  std::vector<uint8_t> iv = {};
  std::vector<uint8_t> payload = {};
};

struct BitGen_BITSTREAM_BOP {
  ~BitGen_BITSTREAM_BOP();
  BitGen_BITSTREAM_BOP_FIELD field;
  std::vector<BitGen_BITSTREAM_ACTION*> actions;
};

enum BitGen_BITSTREAM_BLOCK_TYPE {
  BitGen_BITSTREAM_HEADER_BLOCK,
  BitGen_BITSTREAM_HASH_BLOCK,
  BitGen_BITSTREAM_ACTION_BLOCK,
  BitGen_BITSTREAM_DATA_BLOCK
};

struct BitGen_BITSTREAM_BLOCK {
  BitGen_BITSTREAM_BLOCK(BitGen_BITSTREAM_BLOCK_TYPE t);
  ~BitGen_BITSTREAM_BLOCK();
  const BitGen_BITSTREAM_BLOCK_TYPE type;
  std::vector<uint8_t> data;
  uint8_t* hash_block_hash_data_ptr = nullptr;
};

class BitGen_PACKER {
 public:
  static void generate_bitstream(std::vector<BitGen_BITSTREAM_BOP*>& bops,
                                 std::vector<uint8_t>& data, bool compress,
                                 std::vector<uint8_t>& aes_key,
                                 CFGCrypto_KEY*& key);
  static void update_bitstream_ending_size(uint8_t* const data,
                                           uint64_t ending_size,
                                           bool is_last_bop);
  static std::string get_feature_enum_string(uint8_t feature_enum,
                                             std::vector<std::string> features,
                                             bool& status,
                                             bool allow_none = false);
  static void obscure(uint8_t* obscure_addr, const uint8_t* hash_addr);
  static uint64_t calc_checksum(std::vector<uint8_t>& data, uint8_t type,
                                uint32_t& checksum_size);
};

#endif