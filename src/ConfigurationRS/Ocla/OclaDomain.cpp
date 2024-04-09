#include "OclaDomain.h"

OclaDomain::OclaDomain(oc_domain_type_t type, uint32_t idx)
    : m_type(type), m_index(idx) {
  m_config.mode = ocla_trigger_mode::CONTINUOUS;
  m_config.condition = ocla_trigger_condition::DEFAULT;
  m_config.enable_fix_sample_size = false;
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
