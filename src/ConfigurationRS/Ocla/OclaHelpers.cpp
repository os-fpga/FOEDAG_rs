#include "OclaHelpers.h"

static std::map<ocla_mode, std::string> ocla_mode_to_string_map = {
    {NO_TRIGGER, "disable"},
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

// helpers to convert enum to string and vice versa
std::string convert_ocla_mode_to_string(ocla_mode mode, std::string defval) {
  if (ocla_mode_to_string_map.find(mode) != ocla_mode_to_string_map.end())
    return ocla_mode_to_string_map[mode];
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

ocla_mode convert_ocla_mode(std::string mode_string, ocla_mode defval) {
  for (auto& [mode, str] : ocla_mode_to_string_map) {
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

std::vector<signal_info> generate_signal_descriptor(uint32_t width) {
  std::vector<signal_info> signals;
  for (uint32_t i = 0; i < width; i++) {
    signals.push_back({"s" + std::to_string(i), 1});
  }
  return signals;
}

std::vector<signal_info> generate_signal_descriptor(
    std::vector<Ocla_PROBE_INFO> probes) {
  std::vector<signal_info> signals;
  for (const auto& p : probes) {
    signals.push_back({p.signal_name, p.bitwidth});
  }
  return signals;
}
