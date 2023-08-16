#include "BitGen_json.h"

#include <fstream>

#include "CFGCrypto/CFGOpenSSL.h"

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

static uint64_t BitGen_JSON_to_u64(const nlohmann::json& json) {
  CFG_ASSERT(json.is_number_integer() || json.is_number_unsigned() ||
             json.is_string());
  if (json.is_number_integer() || json.is_number_unsigned()) {
    return (uint64_t)(json);
  } else {
    return CFG_convert_string_to_u64(json);
  }
}

static uint32_t BitGen_JSON_to_u32(const nlohmann::json& json) {
  return (uint32_t)(BitGen_JSON_to_u64(json));
}

static void BitGen_JSON_get_u32s_into_u8s(const nlohmann::json& json,
                                          std::vector<uint8_t>& data) {
  CFG_ASSERT(json.is_array());
  CFG_ASSERT(json.size());
  for (auto& j : json) {
    CFG_append_u32(data, BitGen_JSON_to_u32(j));
  }
}

static void BitGen_JSON_get_u8s_into_u8s(const nlohmann::json& json,
                                         std::vector<uint8_t>& data) {
  CFG_ASSERT(json.is_array());
  CFG_ASSERT(json.size());
  for (auto& j : json) {
    CFG_append_u8(data, BitGen_JSON_to_u8(j));
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
  CFG_ASSERT_MSG(BitGen_PACKER::find_supported_bop_identifier(temp) != -1,
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
  BitGen_JSON_validate_dict_key("Bitstream Generic Action", json,
                                {"action", "cmd", "original_payload_size",
                                 "checksum", "field", "iv", "payload"});
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
    if (json.contains("original_payload_size") &&
        BitGen_JSON_to_boolean(json["original_payload_size"])) {
      action->has_original_payload_size = true;
    }
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

typedef std::map<const std::string, const std::pair<uint32_t, uint32_t>>
    BitGen_JSON_ACTION_FIELD;
const std::map<const std::string, const BitGen_JSON_ACTION_FIELD>
    BitGen_JSON_ACTION_DATABASE = {
        {"firmware_loading",
         {{"load_address", std::make_pair(0, 32)},
          {"entry_address", std::make_pair(32, 32)}}},
        {"fcb_config",
         {{"bitline_byte_size", std::make_pair(0, 16)},
          {"readback", std::make_pair(16, 1)}}},
        {"old_fcb_config",
         {{"cfg_cmd", std::make_pair(0, 2)},
          {"bit_twist_shift_reg", std::make_pair(2, 1)},
          {"bit_chain_connection", std::make_pair(3, 1)},
          {"parallel_chains_count", std::make_pair(32, 32)}}},
        {"icb_config",
         {{"cfg_cmd", std::make_pair(0, 2)},
          {"bit_twist", std::make_pair(2, 1)},
          {"byte_twist", std::make_pair(3, 1)},
          {"is_data_or_not_cmd", std::make_pair(4, 1)},
          {"update", std::make_pair(5, 1)},
          {"capture", std::make_pair(6, 1)}}},
        {"pcb_config",
         {{"ram_block_count", std::make_pair(0, 16)},
          {"pl_ctl_skew", std::make_pair(16, 2)},
          {"pl_ctl_parity", std::make_pair(18, 1)},
          {"pl_ctl_even", std::make_pair(19, 1)},
          {"pl_ctl_split", std::make_pair(20, 2)},
          {"pl_select_offset", std::make_pair(32, 12)},
          {"pl_select_row", std::make_pair(44, 10)},
          {"pl_select_col", std::make_pair(54, 10)},
          {"pl_row_offset", std::make_pair(64, 10)},
          {"pl_row_stride", std::make_pair(80, 10)},
          {"pl_col_offset", std::make_pair(96, 10)},
          {"pl_col_stride", std::make_pair(112, 10)},
          {"pl_extra_w32", std::make_pair(128, 1)},
          {"pl_extra_w33", std::make_pair(129, 1)},
          {"pl_extra_w34", std::make_pair(130, 1)},
          {"pl_extra_w35", std::make_pair(131, 1)}}}};

static const BitGen_JSON_ACTION_FIELD* BitGen_JSON_get_action_database(
    const std::string& action) {
  const BitGen_JSON_ACTION_FIELD* database = nullptr;
  for (auto& iter : BitGen_JSON_ACTION_DATABASE) {
    if (iter.first == action) {
      database = &iter.second;
      break;
    }
  }
  CFG_ASSERT(database != nullptr);
  return database;
}

static void BitGen_JSON_validate_action(const std::string& info,
                                        const nlohmann::json& json,
                                        const BitGen_JSON_ACTION_FIELD* fields,
                                        bool has_payload) {
  CFG_ASSERT(fields != nullptr);
  std::vector<std::string> keys;
  keys.push_back("action");
  for (auto& iter : *fields) {
    keys.push_back(iter.first);
  }
  if (has_payload) {
    keys.push_back("payload");
  }
  BitGen_JSON_validate_dict_key(info, json, keys);
  BitGen_JSON_ensure_dict_key_exists(info, json, keys);
}

static std::vector<uint8_t> BitGen_JSON_gen_action_field(
    const nlohmann::json& json, const BitGen_JSON_ACTION_FIELD* fields) {
  CFG_ASSERT(fields != nullptr);
  CFG_ASSERT(json.is_object());
  std::vector<uint8_t> data;
  std::vector<uint8_t> mask;
  uint32_t start_index = 0;
  uint32_t size = 0;
  uint32_t need_byte_size = 0;
  for (auto& iter : *fields) {
    CFG_ASSERT_MSG(json.contains(iter.first),
                   "Expect key \"%s\" in the object but it is not found",
                   iter.first.c_str());
    std::vector<uint8_t> u8s;
    if (json[iter.first].is_array()) {
      BitGen_JSON_get_u8s_into_u8s(json[iter.first], u8s);
    } else {
      uint64_t u64 = BitGen_JSON_to_u64(json[iter.first]);
      CFG_append_u64(u8s, u64);
    }
    start_index = iter.second.first;
    size = iter.second.second;
    CFG_ASSERT(size);
    if (!json[iter.first].is_array()) {
      CFG_ASSERT(size <= 64);
    }
    CFG_ASSERT(size <= (uint32_t)(u8s.size() * 8));
    for (uint32_t i = 0; i < size; i++, start_index++) {
      need_byte_size = (start_index / 8) + 1;
      CFG_ASSERT(need_byte_size);
      // limit the field -- if too much, does not make sense
      CFG_ASSERT(need_byte_size <= 128);
      while (data.size() < need_byte_size) {
        data.push_back(0);
        mask.push_back(0);
      }
      // Make sure no collision
      CFG_ASSERT((mask[start_index >> 3] & (1 << (start_index & 7))) == 0);
      mask[start_index >> 3] |= (1 << (start_index & 7));
      // Set bit
      if (u8s[i >> 3] & (1 << (i & 7))) {
        data[start_index >> 3] |= (1 << (start_index & 7));
      }
    }
  }
  CFG_ASSERT(data.size() <= 128);
  while (data.size() % 4) {
    data.push_back(0);
  }
  return data;
}

static void BitGen_JSON_gen_bitstream_bop_payload(
    const nlohmann::json& json, std::vector<uint8_t>& payload) {
  if (json.is_array()) {
    if (payload.size()) {
      memset(&payload[0], 0, payload.size());
    }
    payload.clear();
    BitGen_JSON_get_u8s_into_u8s(json, payload);
  } else {
    CFG_read_binary_file(BitGen_JSON_to_string(json), payload);
  }
}

static BitGen_BITSTREAM_ACTION* BitGen_JSON_gen_bitstream_bop_action(
    const nlohmann::json& json, uint16_t cmd, bool has_payload,
    bool has_original_payload_size, bool has_checksum,
    const BitGen_JSON_ACTION_FIELD* fields) {
  CFG_ASSERT(fields != nullptr);
  CFG_ASSERT(has_payload || (!has_original_payload_size && !has_checksum));
  BitGen_BITSTREAM_ACTION* action = CFG_MEM_NEW(BitGen_BITSTREAM_ACTION, cmd);
  action->has_original_payload_size = has_original_payload_size;
  action->has_checksum = has_checksum;
  action->field = BitGen_JSON_gen_action_field(json, fields);
  CFG_ASSERT((action->field.size() % 4) == 0);
  if (has_payload) {
    printf("BitGen_JSON_gen_bitstream_bop_payload\n");
    BitGen_JSON_gen_bitstream_bop_payload(json["payload"], action->payload);
    CFG_ASSERT(action->payload.size());
  } else {
    CFG_ASSERT(action->payload.size() == 0);
  }
  CFG_ASSERT((action->payload.size() % 4) == 0);
  return action;
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
  } else if (action == "firmware_loading") {
    actions.push_back(BitGen_JSON::gen_firmware_loading_action(json));
  } else if (action == "fcb_config") {
    actions.push_back(BitGen_JSON::gen_fcb_config_action(json));
  } else if (action == "old_fcb_config") {
    actions.push_back(BitGen_JSON::gen_old_fcb_config_action(json));
  } else if (action == "icb_config") {
    actions.push_back(BitGen_JSON::gen_icb_config_action(json));
  } else if (action == "pcb_config") {
    actions.push_back(BitGen_JSON::gen_pcb_config_action(json));
  } else if (action == "authentication_key_otp_programming") {
    actions.push_back(BitGen_JSON::gen_auth_key_otp_programming_action(json));
  } else if (action == "aes_key_otp_programming") {
    actions.push_back(BitGen_JSON::gen_aes_key_otp_programming_action(json));
  } else if (action == "otp_programming") {
    actions.push_back(BitGen_JSON::gen_otp_programming_action(json));
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

void BitGen_JSON::zeroize_array_numbers(nlohmann::json& json) {
  CFG_ASSERT(json.is_array());
  CFG_ASSERT(json.size());
  for (auto& j : json) {
    j = 0;
  }
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

BitGen_BITSTREAM_ACTION* BitGen_JSON::gen_standard_action(
    const nlohmann::json& json, const std::string& info, const std::string name,
    uint16_t cmd, bool has_payload, bool has_original_payload_size,
    bool has_checksum) {
  CFG_ASSERT(json.is_object());
  CFG_ASSERT(json.contains("action"));
  std::string action_name = BitGen_JSON_to_string(json["action"]);
  CFG_ASSERT_MSG(action_name == name, "%s == %s", action_name.c_str(),
                 name.c_str());
  const BitGen_JSON_ACTION_FIELD* fields =
      BitGen_JSON_get_action_database(action_name);
  CFG_ASSERT(fields != nullptr);
  BitGen_JSON_validate_action(info, json, fields, has_payload);
  return BitGen_JSON_gen_bitstream_bop_action(
      json, cmd, has_payload, has_original_payload_size, has_checksum, fields);
}

BitGen_BITSTREAM_ACTION* BitGen_JSON::gen_firmware_loading_action(
    const nlohmann::json& json) {
  return gen_standard_action(json, "Bitstream Firmware Loading Action",
                             "firmware_loading", 0x001, true, false, true);
}

BitGen_BITSTREAM_ACTION* BitGen_JSON::gen_fcb_config_action(
    const nlohmann::json& json) {
  return gen_standard_action(json, "Bitstream FCB Config Action", "fcb_config",
                             0x002, true, false, true);
}

BitGen_BITSTREAM_ACTION* BitGen_JSON::gen_old_fcb_config_action(
    const nlohmann::json& json) {
  return gen_standard_action(json, "Bitstream OLD FCB Config Action",
                             "old_fcb_config", 0x802, true, false, true);
}

BitGen_BITSTREAM_ACTION* BitGen_JSON::gen_icb_config_action(
    const nlohmann::json& json) {
  return gen_standard_action(json, "Bitstream ICB Config Action", "icb_config",
                             0x003, true, true, true);
}

BitGen_BITSTREAM_ACTION* BitGen_JSON::gen_pcb_config_action(
    const nlohmann::json& json) {
  return gen_standard_action(json, "Bitstream PCB Config Action", "pcb_config",
                             0x004, true, false, false);
}

BitGen_BITSTREAM_ACTION* BitGen_JSON::gen_auth_key_otp_programming_action(
    const nlohmann::json& json) {
  std::string info = "Authentication Key OTP Programming";
  BitGen_JSON_ACTION_FIELD field = {{"type", std::make_pair(0, 1)},
                                    {"hash", std::make_pair(1, 1)},
                                    {"key_file", std::make_pair(2, 1)}};
  BitGen_JSON_validate_action(info, json, &field, false);
  std::string type_str = BitGen_JSON_to_string(json["type"]);
  CFG_ASSERT_MSG(type_str == "FSBL" || type_str == "ACPU" || type_str == "FPGA",
                 "Authentication Key OTP Programming action support type "
                 "FSBL|ACPU|FPGA, %s is not supported",
                 type_str.c_str());
  uint32_t hash_size = BitGen_JSON_to_u32(json["hash"]);
  CFG_ASSERT_MSG(hash_size == 256 || hash_size == 384 || hash_size == 512,
                 "Authentication Key OTP Programming action support hash "
                 "256|384|512, %d is not supported",
                 hash_size);
  CFGCrypto_KEY authen_key(BitGen_JSON_to_string(json["key_file"]), "", false);
  std::vector<uint8_t> public_key;
  authen_key.get_public_key(public_key, 4);
  CFG_ASSERT(public_key.size());
  CFG_ASSERT(public_key.size() <= 272);
  std::vector<uint8_t> hash;
  hash.resize(hash_size / 8);
  CFGOpenSSL::sha(hash_size / 8, &public_key[0], public_key.size(), &hash[0]);
  // Convert it to basic json
  nlohmann::json action_json;
  action_json["action"] = json["action"];
  action_json["type"] = type_str == "FSBL" ? 1 : (type_str == "ACPU" ? 2 : 3);
  action_json["key_type"] =
      authen_key.get_bitstream_signing_algo() & 0xF0 | ((hash_size / 128) - 1);
  action_json["key"] = nlohmann::json(hash_size / 8, 0);
  for (uint32_t i = 0; i < (hash_size / 8); i++) {
    action_json["key"][i] = hash[i];
  }
  BitGen_JSON_ACTION_FIELD action_field = {
      {"type", std::make_pair(0, 8)},
      {"key_type", std::make_pair(8, 8)},
      {"key", std::make_pair(32, hash_size)}};
  BitGen_JSON_validate_action(info, action_json, &action_field, false);
  return BitGen_JSON_gen_bitstream_bop_action(action_json, 0x101, false, false,
                                              false, &action_field);
}

BitGen_BITSTREAM_ACTION* BitGen_JSON::gen_aes_key_otp_programming_action(
    const nlohmann::json& json) {
  std::string info = "AES Key OTP Programming";
  BitGen_JSON_ACTION_FIELD field = {{"type", std::make_pair(0, 1)},
                                    {"key_file", std::make_pair(1, 1)}};
  BitGen_JSON_validate_action(info, json, &field, false);
  std::string type_str = BitGen_JSON_to_string(json["type"]);
  CFG_ASSERT_MSG(
      type_str == "FSBL" || type_str == "ACPU" || type_str == "FPGA",
      "AES Key OTP Programming action support type FSBL|ACPU|FPGA, %s "
      "is not supported",
      type_str.c_str());
  std::string aes_key_file = BitGen_JSON_to_string(json["key_file"]);
  std::vector<uint8_t> aes_key;
  CFG_read_binary_file(aes_key_file, aes_key);
  CFG_ASSERT_MSG(
      aes_key.size() == 16 || aes_key.size() == 24 || aes_key.size() == 32,
      "AES Key should be 16, 24 or 32 Bytes. But found %ld Bytes in %s",
      aes_key.size(), aes_key_file.c_str());
  // Convert it to basic json
  nlohmann::json action_json;
  action_json["action"] = json["action"];
  action_json["type"] = type_str == "FSBL" ? 1 : (type_str == "ACPU" ? 2 : 3);
  action_json["key_type"] = 0x80 | ((aes_key.size() / 8) - 1);
  action_json["key"] = nlohmann::json(aes_key.size(), 0);
  for (uint32_t i = 0; i < (uint32_t)(aes_key.size()); i++) {
    action_json["key"][i] = aes_key[i];
  }
  BitGen_JSON_ACTION_FIELD action_field = {
      {"type", std::make_pair(0, 8)},
      {"key_type", std::make_pair(8, 8)},
      {"key", std::make_pair(32, (uint32_t)(aes_key.size() * 8))}};
  BitGen_JSON_validate_action(info, action_json, &action_field, false);
  return BitGen_JSON_gen_bitstream_bop_action(action_json, 0x101, false, false,
                                              false, &action_field);
}

BitGen_BITSTREAM_ACTION* BitGen_JSON::gen_otp_programming_action(
    const nlohmann::json& json) {
  std::string info = "OTP Programming";
  BitGen_JSON_ACTION_FIELD field = {{"type", std::make_pair(0, 1)},
                                    {"byte_size", std::make_pair(1, 1)},
                                    {"data", std::make_pair(2, 1)}};
  BitGen_JSON_validate_action(info, json, &field, false);
  std::string type_str = BitGen_JSON_to_string(json["type"]);
  CFG_ASSERT_MSG(
      type_str == "Lifecycle Bits" || type_str == "Chip ID",
      "Key OTP Programming action support type \"Lifecycle Bits|Chip ID\", %s "
      "is not supported",
      type_str.c_str());
  uint32_t byte_size = BitGen_JSON_to_u32(json["byte_size"]);
  CFG_ASSERT(byte_size);
  CFG_ASSERT((byte_size % 4) == 0);
  CFG_ASSERT(byte_size <= 16);
  std::vector<uint8_t> otp;
  if (json["data"].is_number_integer() || json["data"].is_number_unsigned() ||
      json["data"].is_string()) {
    CFG_ASSERT(byte_size <= 8);
    uint64_t u64 = BitGen_JSON_to_u64(json["data"]);
    if (byte_size == 4) {
      CFG_append_u32(otp, (uint32_t)(u64 & 0xFFFFFFFF));
    } else {
      CFG_append_u64(otp, u64);
    }
  } else {
    CFG_ASSERT(json["data"].is_array());
    BitGen_JSON_get_u32s_into_u8s(json["data"], otp);
    if (otp.size() < byte_size) {
      otp.push_back(0);
    }
  }
  // Convert it to basic json
  nlohmann::json action_json;
  action_json["action"] = json["action"];
  action_json["type"] = type_str == "Lifecycle Bits" ? 1 : 2;
  action_json["byte_size"] = byte_size;
  action_json["otp"] = nlohmann::json(byte_size, 0);
  for (uint32_t i = 0; i < byte_size; i++) {
    action_json["otp"][i] = otp[i];
  }
  BitGen_JSON_ACTION_FIELD action_field = {
      {"type", std::make_pair(0, 8)},
      {"byte_size", std::make_pair(16, 16)},
      {"otp", std::make_pair(32, byte_size * 8)}};
  BitGen_JSON_validate_action(info, action_json, &action_field, false);
  return BitGen_JSON_gen_bitstream_bop_action(action_json, 0x102, false, false,
                                              false, &action_field);
}