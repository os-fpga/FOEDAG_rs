#include "OclaHelpers.h"

static std::map<ocla_mode, std::string> ocla_mode_to_string_map = {
    {NO_TRIGGER, "disable"},
    {PRE, "pre-trigger"},
    {POST, "post-trigger"},
    {CENTER, "center-trigger"}};

static std::map<ocla_trigger_condition, std::string>
    trigger_condition_to_string_map = {
        {OR, "or"}, {AND, "and"}, {DEFAULT, "or"}, {XOR, "xor"}};

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
std::string convertOclaModeToString(ocla_mode mode, std::string defval) {
  if (ocla_mode_to_string_map.find(mode) != ocla_mode_to_string_map.end())
    return ocla_mode_to_string_map[mode];
  return defval;
}

std::string convertTriggerConditionToString(ocla_trigger_condition condition,
                                            std::string defval) {
  if (trigger_condition_to_string_map.find(condition) !=
      trigger_condition_to_string_map.end())
    return trigger_condition_to_string_map[condition];
  return defval;
}

std::string convertTriggerTypeToString(ocla_trigger_type trig_type,
                                       std::string defval) {
  if (trigger_type_to_string_map.find(trig_type) !=
      trigger_type_to_string_map.end())
    return trigger_type_to_string_map[trig_type];
  return defval;
}

std::string convertTriggerEventToString(ocla_trigger_event trig_event,
                                        std::string defval) {
  if (trigger_event_to_string_map.find(trig_event) !=
      trigger_event_to_string_map.end())
    return trigger_event_to_string_map[trig_event];
  return defval;
}

ocla_mode convertOclaMode(std::string mode_string, ocla_mode defval) {
  for (auto& [mode, str] : ocla_mode_to_string_map) {
    if (mode_string == str) return mode;
  }
  // default if not found
  return defval;
}

ocla_trigger_condition convertTriggerCondition(std::string condition_string,
                                               ocla_trigger_condition defval) {
  for (auto& [condition, str] : trigger_condition_to_string_map) {
    if (condition_string == str) return condition;
  }
  // default if not found
  return defval;
}

ocla_trigger_type convertTriggerType(std::string type_string,
                                     ocla_trigger_type defval) {
  for (auto& [type, str] : trigger_type_to_string_map) {
    if (type_string == str) return type;
  }
  // default if not found
  return defval;
}

ocla_trigger_event convertTriggerEvent(std::string event_string,
                                       ocla_trigger_event defval) {
  for (auto& [event, str] : trigger_event_to_string_map) {
    if (event_string == str) return event;
  }
  // default if not found
  return defval;
}
