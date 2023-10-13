#include "MemorySession.h"

MemorySession::MemorySession() {}

MemorySession::~MemorySession() {}

void MemorySession::load(std::string bitasmfile) {}

void MemorySession::unload() {}

uint32_t MemorySession::get_instance_count() { return 0; }

Ocla_INSTANCE_INFO MemorySession::get_instance_info(uint32_t instance) {
  return Ocla_INSTANCE_INFO();
}

std::vector<Ocla_PROBE_INFO> MemorySession::get_probe_info(uint32_t instance) {
  return std::vector<Ocla_PROBE_INFO>();
}
