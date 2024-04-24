#include "OclaDomain.h"

OclaDomain::OclaDomain(oc_domain_type_t type, uint32_t idx)
    : m_type(type), m_index(idx) {
  m_config.mode = ocla_trigger_mode::CONTINUOUS;
  m_config.condition = ocla_trigger_condition::DEFAULT;
  m_config.sample_size = 0;
}

OclaDomain::~OclaDomain() {}

void OclaDomain::add_probe(const OclaProbe& probe) {
  m_probes.push_back(probe);
}

void OclaDomain::add_instance(const OclaInstance& instance) {
  m_instances.push_back(instance);
}

oc_domain_type_t OclaDomain::get_type() const { return m_type; }

uint32_t OclaDomain::get_index() const { return m_index; };

void OclaDomain::set_index(uint32_t idx) { m_index = idx; }

std::vector<OclaProbe>& OclaDomain::get_probes() { return m_probes; }

std::vector<OclaInstance>& OclaDomain::get_instances() { return m_instances; }

ocla_config OclaDomain::get_config() const { return m_config; }

void OclaDomain::set_config(ocla_config& config) { m_config = config; }

void OclaDomain::add_trigger(oc_trigger_t& trig) { m_triggers.push_back(trig); }

std::vector<oc_trigger_t>& OclaDomain::get_triggers() { return m_triggers; }

bool OclaDomain::get_instance(uint32_t instance_index, OclaInstance*& output) {
  for (auto& instance : m_instances) {
    if (instance.get_index() == instance_index) {
      output = &instance;
      return true;
    }
  }
  return false;
}

uint32_t OclaDomain::get_number_of_triggers(uint32_t instance_index) {
  uint32_t count = 0;
  for (auto& trig : m_triggers) {
    if (trig.instance_index == instance_index) {
      ++count;
    }
  }
  return count;
}

bool OclaDomain::get_trigger(uint32_t trigger_index, oc_trigger_t*& output) {
  if (trigger_index > 0 && trigger_index <= m_triggers.size()) {
    output = &m_triggers[trigger_index - 1];
    return true;
  }
  return false;
}

bool OclaDomain::remove_trigger(uint32_t trigger_index) {
  if (trigger_index > 0 && trigger_index <= m_triggers.size()) {
    m_triggers.erase(m_triggers.begin() + (trigger_index - 1));
    return true;
  }
  return false;
}
