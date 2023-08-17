#ifndef BITGEN_UBI_H
#define BITGEN_UBI_H

#include "BitGen_packer.h"

struct BitGen_UBI_HEADER {
  uint16_t header_version = 0;
  uint16_t header_size = 0;
  uint32_t product_id = 0;
  uint32_t customer_id = 0;
  uint32_t image_version = 0;
  uint32_t size = 0;
  uint32_t fsbl_size = 0;
  uint8_t image_type = 0;
  uint8_t package_count = 0;
  uint16_t crc16 = 0;
};

class BitGen_UBI {
 public:
  static void package(BitGen_UBI_HEADER& header,
                      std::vector<std::string>& input_filepaths,
                      const std::string& output_filepath);

 private:
  static bool is_supported_bop_identifier(const std::string& identifier);
  static uint32_t get_u32(const uint8_t* data);
  static int parse_old_bop(std::vector<uint8_t>& data, bool is_last_file,
                           uint32_t* fsbl_size);
};

#endif