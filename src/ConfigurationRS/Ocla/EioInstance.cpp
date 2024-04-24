#include "EioInstance.h"

#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "OclaHelpers.h"

EioInstance::EioInstance(uint32_t baseaddr) : m_baseaddr(baseaddr) {}

EioInstance::~EioInstance() {}

std::vector<eio_probe_t> EioInstance::get_input_probes() const {
  return get_probes(eio_probe_type_t::INPUT);
}

std::vector<eio_probe_t> EioInstance::get_output_probes() const {
  return get_probes(eio_probe_type_t::OUTPUT);
}

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
