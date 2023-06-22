#include "BitGen_json.h"

#include <fstream>

#include "CFGCrypto/CFGOpenSSL.h"
#include "nlohmann_json/json.hpp"

static std::string BitGen_JSON_to_string(const nlohmann::json& json) {
  CFG_ASSERT(json.is_string());
  return json;
}

static uint8_t BitGen_JSON_to_u8(const nlohmann::json& json) {
  CFG_ASSERT(json.is_number_unsigned() || json.is_string());
  if (json.is_number_unsigned()) {
    return (uint8_t)(json);
  } else {
    return (uint8_t)(CFG_convert_string_to_u64(json));
  }
}

static uint16_t BitGen_JSON_to_u16(const nlohmann::json& json) {
  CFG_ASSERT(json.is_number_unsigned() || json.is_string());
  if (json.is_number_unsigned()) {
    return (uint16_t)(json);
  } else {
    return (uint16_t)(CFG_convert_string_to_u64(json));
  }
}

static bool BitGen_JSON_to_boolean(const nlohmann::json& json) {
  CFG_ASSERT(json.is_boolean());
  return (bool)(json);
}

static uint32_t BitGen_JSON_to_u32(const nlohmann::json& json) {
  CFG_ASSERT(json.is_number_unsigned() || json.is_string());
  if (json.is_number_unsigned()) {
    return (uint32_t)(json);
  } else {
    return (uint32_t)(CFG_convert_string_to_u64(json));
  }
}

static void BitGen_JSON_get_u32s_into_u8s(const nlohmann::json& json,
                                          std::vector<uint8_t>& data) {
  CFG_ASSERT(json.is_array());
  CFG_ASSERT(json.size());
  for (auto& j : json) {
    CFG_append_u32(data, BitGen_JSON_to_u32(j));
  }
}

static int BitGen_JSON_get_array_index(nlohmann::json arrays,
                                       nlohmann::json item) {
  CFG_ASSERT(arrays.is_array());
  CFG_ASSERT(arrays.size());
  bool found(false);
  size_t index = 0;
  for (auto& iter : arrays) {
    if (iter == item) {
      found = true;
      break;
    }
    index++;
  }
  return (found ? index : -1);
}

static void BitGen_JSON_validate_dict_key(const std::string& info,
                                          const nlohmann::json& json,
                                          nlohmann::json keys) {
  // Make sure dict key only support number and string
  CFG_ASSERT_MSG(json.is_object(),
                 "JSON object should be dictionary but found %d", json.type());
  CFG_ASSERT(keys.is_array());
  CFG_ASSERT(json.size());
  CFG_ASSERT(keys.size());
  for (auto& item : json.items()) {
    nlohmann::json key = item.key();
    CFG_ASSERT(key.is_number() || key.is_string());
    CFG_ASSERT_MSG(BitGen_JSON_get_array_index(keys, key) != -1,
                   "%s dictionary should not contain key %s", info.c_str(),
                   to_string(key).c_str());
  }
}

static void BitGen_JSON_ensure_dict_key_exists(const std::string& info,
                                               const nlohmann::json& json,
                                               nlohmann::json keys) {
  CFG_ASSERT(json.is_object());
  CFG_ASSERT(keys.is_array());
  CFG_ASSERT(json.size());
  CFG_ASSERT(keys.size());
  std::vector<int> indexes;
  for (auto& item : json.items()) {
    nlohmann::json key = item.key();
    CFG_ASSERT(key.is_number() || key.is_string());
    int index = BitGen_JSON_get_array_index(keys, key);
    if (index != -1) {
      CFG_ASSERT(std::find(indexes.begin(), indexes.end(), index) ==
                 indexes.end());
      indexes.push_back(index);
    }
  }
  CFG_ASSERT(indexes.size() <= keys.size());
  if (indexes.size() < keys.size()) {
    std::string missing = "";
    for (size_t index = 0; index < keys.size(); index++) {
      if (std::find(indexes.begin(), indexes.end(), int(index)) ==
          indexes.end()) {
        missing += (" " + to_string(keys[index]));
      }
    }
    CFG_ASSERT(missing.size());
    CFG_INTERNAL_ERROR("%s dictionary is missing key(s): %s", info.c_str(),
                       missing.c_str());
  }
}

static void BitGen_JSON_parse_bitstream_bop_field(
    const nlohmann::json& json, BitGen_BITSTREAM_BOP_FIELD& field) {
  BitGen_JSON_validate_dict_key(
      "Bitstream BOP Field", json,
      {"identifier", "version", "tool", "opn", "jtag_id", "jtag_mask", "chipid",
       "checksum", "integrity", "iv"});
  BitGen_JSON_ensure_dict_key_exists("Bitstream BOP Field", json,
                                     {"identifier"});
  int index = 0;
  std::string temp = BitGen_JSON_to_string(json["identifier"]);
  CFG_ASSERT_MSG(CFG_find_string_in_vector(
                     BitGen_BITSTREAM_SUPPORTED_BOP_IDENTIFIER, temp) != -1,
                 "Bitstream BOP identifier %s is not supported", temp.c_str());
  field.identifier = temp;
  if (json.contains("version")) {
    field.version = BitGen_JSON_to_u32(json["version"]);
  }
  if (json.contains("tool")) {
    temp = BitGen_JSON_to_string(json["tool"]);
    CFG_ASSERT(temp.size() <= 32);
    field.tool = temp;
  }
  if (json.contains("opn")) {
    temp = BitGen_JSON_to_string(json["opn"]);
    CFG_ASSERT(temp.size() <= 16);
    field.opn = temp;
  }
  if (json.contains("jtag_mask")) {
    field.jtag_id = BitGen_JSON_to_u32(json["jtag_id"]);
  }
  if (json.contains("jtag_mask")) {
    field.jtag_mask = BitGen_JSON_to_u32(json["jtag_mask"]);
  }
  if (json.contains("chipid")) {
    field.chipid = BitGen_JSON_to_u8(json["chipid"]);
  }
  if (json.contains("checksum")) {
    temp = BitGen_JSON_to_string(json["checksum"]);
    index = CFG_find_string_in_vector({"flecther32"}, temp);
    CFG_ASSERT(index != -1);
    field.checksum = (uint8_t)(0x10 | index);
  } else {
    field.checksum = 0x10;
  }
  if (json.contains("integrity")) {
    temp = BitGen_JSON_to_string(json["integrity"]);
    index = CFG_find_string_in_vector({"sha256", "sha384", "sha512"}, temp);
    CFG_ASSERT(index != -1);
    field.integrity = (uint8_t)(0x10 | index);
  } else {
    field.integrity = 0x10;
  }
  if (json.contains("iv")) {
    temp = BitGen_JSON_to_string(json["iv"]);
    std::vector<uint8_t> iv = CFG_convert_hex_string_to_bytes(temp, true);
    CFG_ASSERT(iv.size());
    if (iv.size() > sizeof(field.iv)) {
      iv.resize(sizeof(field.iv));
    }
    memcpy(field.iv, &iv[0], iv.size());
    memset(&iv[0], 0, iv.size());
    iv.clear();
  } else {
    CFGOpenSSL::generate_iv(field.iv, false);
  }
}

static void BitGen_JSON_parse_bitstream_bop_generic_action(
    const nlohmann::json& json,
    std::vector<BitGen_BITSTREAM_ACTION*>& actions) {
  BitGen_JSON_validate_dict_key(
      "Bitstream Generic Action", json,
      {"action", "cmd", "checksum", "field", "iv", "payload"});
  BitGen_JSON_ensure_dict_key_exists("Bitstream Generic Action", json,
                                     {"action", "cmd"});
  std::string temp = BitGen_JSON_to_string(json["action"]);
  CFG_ASSERT(temp == "generic");
  BitGen_BITSTREAM_ACTION* action = CFG_MEM_NEW(
      BitGen_BITSTREAM_ACTION, BitGen_JSON_to_u16(json["cmd"]) & 0xFFF);
  if (json.contains("field")) {
    BitGen_JSON_get_u32s_into_u8s(json["field"], action->field);
  }
  if (json.contains("payload")) {
    CFG_read_binary_file(BitGen_JSON_to_string(json["payload"]),
                         action->payload);
    CFG_ASSERT(action->payload.size());
    CFG_ASSERT((action->payload.size() % 4) == 0);
    if (json.contains("checksum") && BitGen_JSON_to_boolean(json["checksum"])) {
      action->has_checksum = true;
    }
    if (json.contains("iv") &&
        (!json["iv"].is_boolean() || BitGen_JSON_to_boolean(json["iv"]))) {
      action->iv.resize(16);
      memset(&action->iv[0], 0, action->iv.size());
      if (json["iv"].is_boolean()) {
        CFGOpenSSL::generate_iv(&action->iv[0], false);
      } else {
        temp = BitGen_JSON_to_string(json["iv"]);
        std::vector<uint8_t> iv = CFG_convert_hex_string_to_bytes(temp, true);
        CFG_ASSERT(iv.size());
        if (iv.size() > 16) {
          iv.resize(16);
        }
        memcpy(&action->iv[0], &iv[0], iv.size());
        memset(&iv[0], 0, iv.size());
        iv.clear();
      }
    }
  }
  actions.push_back(action);
}

static void BitGen_JSON_parse_bitstream_bop_action(
    const nlohmann::json& json,
    std::vector<BitGen_BITSTREAM_ACTION*>& actions) {
  CFG_ASSERT(json.is_object());
  CFG_ASSERT(json.size());
  CFG_ASSERT(json.contains("action"));
  std::string action = BitGen_JSON_to_string(json["action"]);
  if (action == "generic") {
    BitGen_JSON_parse_bitstream_bop_generic_action(json, actions);
  } else {
    CFG_INTERNAL_ERROR("Action %s is not supported", action.c_str());
  }
}

static void BitGen_JSON_parse_bitstream_bop_actions(
    const nlohmann::json& json,
    std::vector<BitGen_BITSTREAM_ACTION*>& actions) {
  CFG_ASSERT(json.is_array());
  CFG_ASSERT(json.size());
  for (auto& action : json) {
    BitGen_JSON_parse_bitstream_bop_action(action, actions);
  }
}

static BitGen_BITSTREAM_BOP* BitGen_JSON_parse_bitstream_bop(
    const nlohmann::json& json) {
  CFG_ASSERT(json.is_object());
  BitGen_BITSTREAM_BOP* bop = CFG_MEM_NEW(BitGen_BITSTREAM_BOP);
  // We only support fields and actions
  BitGen_JSON_validate_dict_key("Bitstream BOP", json, {"fields", "actions"});
  BitGen_JSON_ensure_dict_key_exists("Bitstream BOP", json,
                                     {"fields", "actions"});
  BitGen_JSON_parse_bitstream_bop_field(json["fields"], bop->field);
  BitGen_JSON_parse_bitstream_bop_actions(json["actions"], bop->actions);
  return bop;
}

void BitGen_JSON::parse_bitstream(const std::string& filepath,
                                  std::vector<BitGen_BITSTREAM_BOP*>& bops) {
  std::ifstream file(filepath.c_str());
  CFG_ASSERT(file.is_open() && file.good());
  nlohmann::json bitstream = nlohmann::json::parse(file);
  file.close();
  CFG_ASSERT(bitstream.is_array());
  CFG_ASSERT(bitstream.size());
  for (auto& bop : bitstream) {
    bops.push_back(BitGen_JSON_parse_bitstream_bop(bop));
  }
}