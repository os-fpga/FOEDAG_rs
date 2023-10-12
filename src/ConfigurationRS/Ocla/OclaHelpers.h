#ifndef __OCLAHELPERS_H__
#define __OCLAHELPERS_H__

#include <map>
#include <string>

#include "OclaIP.h"
#include "WaveformWriter.h"

std::string convertOclaModeToString(ocla_mode mode,
                                    std::string defval = "(unknown)");

std::string convertTriggerConditionToString(ocla_trigger_condition condition,
                                            std::string defval = "(unknown)");

std::string convertTriggerTypeToString(ocla_trigger_type trig_type,
                                       std::string defval = "(unknown)");

std::string convertTriggerEventToString(ocla_trigger_event trig_event,
                                        std::string defval = "(unknown)");

ocla_mode convertOclaMode(std::string mode_string,
                          ocla_mode defval = NO_TRIGGER);

ocla_trigger_condition convertTriggerCondition(
    std::string condition_string, ocla_trigger_condition defval = DEFAULT);

ocla_trigger_type convertTriggerType(std::string type_string,
                                     ocla_trigger_type defval = TRIGGER_NONE);

ocla_trigger_event convertTriggerEvent(std::string event_string,
                                       ocla_trigger_event defval = NONE);

std::vector<signal_info> generateSignalDescriptor(uint32_t width);

#endif  //__OCLAHELPERS_H__
