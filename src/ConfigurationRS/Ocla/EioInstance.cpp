#include "EioInstance.h"

#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "OclaHelpers.h"

EioInstance::EioInstance(uint32_t baseaddr, uint32_t idx)
    : m_baseaddr(baseaddr), m_index(idx) {}

EioInstance::~EioInstance() {}

uint32_t EioInstance::get_baseaddr() const { return m_baseaddr; }

uint32_t EioInstance::get_index() const { return m_index; };

void EioInstance::add_probe(eio_probe_t &probe) { m_probes.push_back(probe); }

std::vector<eio_probe_t> EioInstance::get_probes(eio_probe_type_t type) const {
  std::vector<eio_probe_t> probes{};
  for (auto &p : m_probes) {
    if (p.type == type) {
      probes.push_back(p);
    }
  }
  return probes;
}
