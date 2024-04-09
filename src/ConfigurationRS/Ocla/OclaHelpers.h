#ifndef __OCLAHELPERS_H__
#define __OCLAHELPERS_H__

#include <map>
#include <string>

#include "OclaIP.h"

std::string convert_ocla_trigger_mode_to_string(
    ocla_trigger_mode mode, std::string defval = "(unknown)");

std::string convert_trigger_condition_to_string(
    ocla_trigger_condition condition, std::string defval = "(unknown)");

std::string convert_trigger_type_to_string(ocla_trigger_type trig_type,
                                           std::string defval = "(unknown)");

std::string convert_trigger_event_to_string(ocla_trigger_event trig_event,
                                            std::string defval = "(unknown)");

ocla_trigger_mode convert_ocla_trigger_mode(
    std::string mode_string, ocla_trigger_mode defval = CONTINUOUS);

ocla_trigger_condition convert_trigger_condition(
    std::string condition_string, ocla_trigger_condition defval = DEFAULT);

ocla_trigger_type convert_trigger_type(std::string type_string,
                                       ocla_trigger_type defval = TRIGGER_NONE);

ocla_trigger_event convert_trigger_event(std::string event_string,
                                         ocla_trigger_event defval = NO_EVENT);

bool CFG_type_event_sanity_check(std::string &type, std::string &event);

std::string CFG_toupper(const std::string &str);

uint32_t CFG_reverse_byte_order_u32(uint32_t value);

void CFG_set_bitfield_u32(uint32_t &value, uint8_t pos, uint8_t width,
                          uint32_t data);

uint32_t CFG_read_bit_vec32(uint32_t *data, uint32_t pos);

void CFG_write_bit_vec32(uint32_t *data, uint32_t pos, uint32_t value);

void CFG_copy_bits_vec32(uint32_t *src, uint32_t pos, uint32_t *dest,
                         uint32_t dest_pos, uint32_t nbits);

#endif  //__OCLAHELPERS_H__
