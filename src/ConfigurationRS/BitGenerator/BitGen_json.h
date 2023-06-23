#ifndef BITGEN_JSON_H
#define BITGEN_JSON_H

#include "BitGen_packer.h"
#include "CFGCommonRS/CFGCommonRS.h"
#include "nlohmann_json/json.hpp"

class BitGen_JSON {
 public:
  static void zeroize_array_numbers(nlohmann::json& json);
  static void parse_bitstream(const std::string& filepath,
                              std::vector<BitGen_BITSTREAM_BOP*>& bops);
  static BitGen_BITSTREAM_ACTION* gen_firmware_loading_action(
      const nlohmann::json& json);
  static BitGen_BITSTREAM_ACTION* gen_fcb_config_action(
      const nlohmann::json& json);
  static BitGen_BITSTREAM_ACTION* gen_icb_config_action(
      const nlohmann::json& json);
  static BitGen_BITSTREAM_ACTION* gen_pcb_config_action(
      const nlohmann::json& json);

 protected:
  static BitGen_BITSTREAM_ACTION* gen_standard_action(
      const nlohmann::json& json, const std::string& info,
      const std::string name, uint16_t cmd, bool has_checksum,
      bool has_payload);
};

#endif
