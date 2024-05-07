#include "EioInstance.h"

#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "OclaHelpers.h"

EioInstance::EioInstance(uint32_t baseaddr, uint32_t idx)
    : m_baseaddr(baseaddr), m_index(idx) {}

EioInstance::~EioInstance() {}

uint32_t EioInstance::get_baseaddr() const { return m_baseaddr; }

uint32_t EioInstance::get_index() const { return m_index; };

void EioInstance::add_probe(eio_probe_t& probe) { m_probes.push_back(probe); }

std::vector<eio_probe_t>& EioInstance::get_probes() { return m_probes; }

uint32_t EioInstance::get_total_probe_width(eio_probe_type_t type) const {
  uint32_t width = 0;
  for (auto& probe : m_probes) {
    if (probe.type == type) {
      width += probe.probe_width;
    }
  }
  return width;
}
