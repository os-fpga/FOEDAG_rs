#include "OclaProbe.h"

OclaProbe::OclaProbe(uint32_t idx) : m_instance_index(0), m_index(idx) {}

OclaProbe::~OclaProbe() {}

void OclaProbe::add_signal(const OclaSignal& signal) {
  m_signals.push_back(signal);
}

uint32_t OclaProbe::get_instance_index() const { return m_instance_index; }

void OclaProbe::set_instance_index(uint32_t instance_index) {
  m_instance_index = instance_index;
}

uint32_t OclaProbe::get_index() const { return m_index; }

void OclaProbe::set_index(uint32_t idx) { m_index = idx; }

uint32_t OclaProbe::get_total_bitwidth() {
  uint32_t total_bitwidth = 0;
  for (auto const& sig : m_signals) {
    total_bitwidth += sig.get_bitwidth();
  }
  return total_bitwidth;
}

std::vector<OclaSignal>& OclaProbe::get_signals() { return m_signals; }
