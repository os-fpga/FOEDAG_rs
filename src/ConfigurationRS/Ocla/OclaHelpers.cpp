#include "OclaHelpers.h"

#include <algorithm>
#include <cctype>

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
  uint32_t mask = (~0u >> (32 - width)) << pos;
  value &= ~mask;
  value |= (data & ((1u << width) - 1)) << pos;
}

uint32_t CFG_read_bit_vec32(uint32_t* data, uint32_t pos) {
  // callee should make sure data ptr not out of bound
  CFG_ASSERT(data != nullptr);
  return data[pos / 32] >> (pos % 32) & 1;
}

void CFG_write_bit_vec32(uint32_t* data, uint32_t pos, uint32_t value) {
  // callee should make sure data ptr not out of bound
  CFG_ASSERT(data != nullptr);
  data[pos / 32] |= value << (pos % 32);
}

void CFG_copy_bits_vec32(uint32_t* data, uint32_t pos, uint32_t* output,
                         uint32_t output_pos, uint32_t nbits) {
  // callee should make sure data ptr not out of bound
  CFG_ASSERT(data != nullptr);
  CFG_ASSERT(output != nullptr);
  // naive implementation that does the copying bit by bit.
  // this is till ok for small dataset.
  for (uint32_t i = 0; i < nbits; i++) {
    CFG_write_bit_vec32(output, output_pos + i,
                        CFG_read_bit_vec32(data, pos + i));
  }
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
