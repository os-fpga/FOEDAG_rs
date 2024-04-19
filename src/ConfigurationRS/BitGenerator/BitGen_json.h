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
  static BitGen_BITSTREAM_ACTION* gen_old_fcb_config_action(
      const nlohmann::json& json);
  static BitGen_BITSTREAM_ACTION* gen_icb_config_action(
      const nlohmann::json& json);
  static BitGen_BITSTREAM_ACTION* gen_pcb_config_action(
      const nlohmann::json& json);
  static BitGen_BITSTREAM_ACTION* gen_pcb_config_with_parity_action(
      const nlohmann::json& json);
  static BitGen_BITSTREAM_ACTION* gen_auth_key_otp_programming_action(
      const nlohmann::json& json);
  static BitGen_BITSTREAM_ACTION* gen_aes_key_otp_programming_action(
      const nlohmann::json& json);
  static BitGen_BITSTREAM_ACTION* gen_otp_programming_action(
      const nlohmann::json& json);

 protected:
  static BitGen_BITSTREAM_ACTION* gen_standard_action(
      const nlohmann::json& json, const std::string& info,
      const std::string name, uint16_t cmd, bool has_payload,
      bool has_original_payload_size, bool has_checksum);
};

#endif
