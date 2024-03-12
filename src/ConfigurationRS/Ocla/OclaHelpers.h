#ifndef __OCLAHELPERS_H__
#define __OCLAHELPERS_H__

#include <map>
#include <string>

#include "OclaIP.h"
#include "OclaSession.h"
#include "OclaWaveformWriter.h"

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
                                         ocla_trigger_event defval = NONE);

std::vector<signal_info> generate_signal_descriptor(uint32_t width);

std::vector<signal_info> generate_signal_descriptor(
    std::vector<Ocla_PROBE_INFO> probes);

#endif  //__OCLAHELPERS_H__
