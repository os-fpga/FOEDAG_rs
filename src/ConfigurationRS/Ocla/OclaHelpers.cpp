#include "OclaHelpers.h"

#include <algorithm>
#include <cctype>
#include <regex>

#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"

static std::map<ocla_trigger_mode, std::string>
    ocla_trigger_mode_to_string_map = {{CONTINUOUS, "disable"},
                                       {PRE, "pre-trigger"},
                                       {POST, "post-trigger"},
                                       {CENTER, "center-trigger"}};

static std::map<ocla_trigger_condition, std::string>
    trigger_condition_to_string_map = {
        {OR, "OR"}, {AND, "AND"}, {DEFAULT, "OR"}, {XOR, "XOR"}};

static std::map<ocla_trigger_type, std::string> trigger_type_to_string_map = {
    {TRIGGER_NONE, "disable"},
    {EDGE, "edge"},
    {LEVEL, "level"},
    {VALUE_COMPARE, "value_compare"}};

static std::map<ocla_trigger_event, std::string> trigger_event_to_string_map = {
    {EDGE_NONE, "edge_none"},   {RISING, "rising"}, {FALLING, "falling"},
    {EITHER, "either"},         {LOW, "low"},       {HIGH, "high"},
    {VALUE_NONE, "value_none"}, {EQUAL, "equal"},   {LESSER, "lesser"},
    {GREATER, "greater"}};

std::string CFG_toupper(const std::string& str) {
  std::string result = str;
  for (char& c : result) {
    c = std::toupper(c);
  }
  return result;
}

bool CFG_type_event_sanity_check(std::string& type, std::string& event) {
  static std::map<std::string, std::vector<std::string>> s_check_table{
      {"edge", {"rising", "falling", "either"}},
      {"level", {"high", "low"}},
      {"value_compare", {"equal", "lesser", "greater"}}};

  auto vec = s_check_table[type];
  auto it = std::find(vec.begin(), vec.end(), event);

  if (it != vec.end()) {
    return true;
  }

  return false;
}

uint32_t CFG_reverse_byte_order_u32(uint32_t value) {
  return (value >> 24) | ((value >> 8) & 0xff00) | ((value << 8) & 0xff0000) |
         (value << 24);
}

void CFG_set_bitfield_u32(uint32_t& value, uint8_t pos, uint8_t width,
                          uint32_t data) {
  CFG_ASSERT(width > 0);
  CFG_ASSERT(width + pos <= 32);
  uint32_t mask = (~0u >> (32 - width)) << pos;
  value &= ~mask;
  value |= (data << pos) & mask;
}

uint32_t CFG_read_bit_vec32(uint32_t* data, uint32_t pos) {
  // callee should make sure data ptr not out of bound
  CFG_ASSERT(data != nullptr);
  return data[pos / 32] >> (pos % 32) & 1;
}

void CFG_write_bit_vec32(uint32_t* data, uint32_t pos, uint32_t value) {
  // callee should make sure data ptr not out of bound
  CFG_ASSERT(data != nullptr);
  uint32_t mask = 1u << (pos % 32);
  data[pos / 32] &= ~mask;
  if (value) {
    data[pos / 32] |= mask;
  }
}

void CFG_copy_bits_vec32(uint32_t* data, uint32_t pos, uint32_t* output,
                         uint32_t output_pos, uint32_t nbits) {
  // callee should make sure data ptr not out of bound
  CFG_ASSERT(data != nullptr);
  CFG_ASSERT(output != nullptr);
  // naive implementation that does the copying bit by bit.
  // this is still ok for small dataset.
  for (uint32_t i = 0; i < nbits; i++) {
    CFG_write_bit_vec32(output, output_pos + i,
                        CFG_read_bit_vec32(data, pos + i));
  }
}

std::vector<uint32_t> CFG_convert_u64_to_vec_u32(uint64_t value) {
  return {uint32_t(value), uint32_t(value >> 32)};
}

uint64_t CFG_convert_vec_u32_to_u64(std::vector<uint32_t> values) {
  CFG_ASSERT(values.size() >= 2);
  return uint64_t(values[0]) | (uint64_t(values[1]) << 32);
}

uint32_t CFG_parse_signal(std::string& signal_str, std::string& name,
                          uint32_t& bit_start, uint32_t& bit_end,
                          uint32_t& bit_width, uint32_t& value) {
  static std::map<uint32_t, std::string> patterns = {
      {OCLA_SIGNAL_PATTERN_1, R"((\w+) *\[ *(\d+) *: *(\d+)\ *])"},
      {OCLA_SIGNAL_PATTERN_2, R"((\d+)'([01]+))"},
      {OCLA_SIGNAL_PATTERN_3, R"((\w+) *\[ *(\d+)\ *])"},
      {OCLA_SIGNAL_PATTERN_4, R"(^(\d+)$)"},
      {OCLA_SIGNAL_PATTERN_5, R"(^(\w+)$)"}};

  uint32_t patid = 0;
  std::cmatch m;

  for (const auto& [i, pat] : patterns) {
    if (std::regex_search(signal_str.c_str(), m,
                          std::regex(pat, std::regex::icase)) == true) {
      patid = i;
      break;
    }
  }

  switch (patid) {
    case OCLA_SIGNAL_PATTERN_1:  // pattern 1: counter[13:2]
    {
      name = m[1];
      bit_start = (uint32_t)std::stoul(m[3]);
      bit_end = (uint32_t)std::stoul(m[2]);
      bit_width = 0;
      value = 0;
      break;
    }
    case OCLA_SIGNAL_PATTERN_2:  // pattern 2: 4'0000
    {
      name = signal_str;
      bit_start = 0;
      bit_end = 0;
      bit_width = (uint32_t)std::stoul(m[1]);
      value = (uint32_t)CFG_convert_string_to_u64("b" + (std::string)m[2]);
      break;
    }
    case OCLA_SIGNAL_PATTERN_3:  // pattern 3: s_axil_awprot[0]
    {
      name = m[1];
      bit_start = (uint32_t)std::stoul(m[2]);
      bit_end = (uint32_t)std::stoul(m[2]);
      bit_width = 0;
      value = 0;
      break;
    }
    case OCLA_SIGNAL_PATTERN_4:  // pattern 5: 3
    {
      name = m[0];
      bit_start = 0;
      bit_end = 0;
      bit_width = 0;
      value = (uint32_t)std::stoul(m[0]);
      break;
    }
    case OCLA_SIGNAL_PATTERN_5:  // pattern 5: s_axil_bready
    {
      name = m[0];
      bit_start = 0;
      bit_end = 0;
      bit_width = 0;
      value = 0;
      break;
    }
    default:  // unknown pattern
    {
      name = "";
      bit_start = 0;
      bit_end = 0;
      bit_width = 0;
      value = 0;
      break;
    }
  }

  return patid;
}

// helpers to convert enum to string and vice versa
std::string convert_ocla_trigger_mode_to_string(ocla_trigger_mode mode,
                                                std::string defval) {
  if (ocla_trigger_mode_to_string_map.find(mode) !=
      ocla_trigger_mode_to_string_map.end())
    return CFG_toupper(ocla_trigger_mode_to_string_map[mode]);
  return defval;
}

std::string convert_trigger_condition_to_string(
    ocla_trigger_condition condition, std::string defval) {
  if (trigger_condition_to_string_map.find(condition) !=
      trigger_condition_to_string_map.end())
    return trigger_condition_to_string_map[condition];
  return defval;
}

std::string convert_trigger_type_to_string(ocla_trigger_type trig_type,
                                           std::string defval) {
  if (trigger_type_to_string_map.find(trig_type) !=
      trigger_type_to_string_map.end())
    return trigger_type_to_string_map[trig_type];
  return defval;
}

std::string convert_trigger_event_to_string(ocla_trigger_event trig_event,
                                            std::string defval) {
  if (trigger_event_to_string_map.find(trig_event) !=
      trigger_event_to_string_map.end())
    return trigger_event_to_string_map[trig_event];
  return defval;
}

ocla_trigger_mode convert_ocla_trigger_mode(std::string mode_string,
                                            ocla_trigger_mode defval) {
  for (auto& [mode, str] : ocla_trigger_mode_to_string_map) {
    if (mode_string == str) return mode;
  }
  // default if not found
  return defval;
}

ocla_trigger_condition convert_trigger_condition(
    std::string condition_string, ocla_trigger_condition defval) {
  for (auto& [condition, str] : trigger_condition_to_string_map) {
    if (condition_string == str) return condition;
  }
  // default if not found
  return defval;
}

ocla_trigger_type convert_trigger_type(std::string type_string,
                                       ocla_trigger_type defval) {
  for (auto& [type, str] : trigger_type_to_string_map) {
    if (type_string == str) return type;
  }
  // default if not found
  return defval;
}

ocla_trigger_event convert_trigger_event(std::string event_string,
                                         ocla_trigger_event defval) {
  for (auto& [event, str] : trigger_event_to_string_map) {
    if (event_string == str) return event;
  }
  // default if not found
  return defval;
}
