#include "MemorySession.h"

#include "BitAssembler/BitAssembler_mgr.h"
#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "nlohmann_json/json.hpp"

static bool g_loaded = false;

MemorySession::MemorySession() {}

MemorySession::~MemorySession() {}

bool MemorySession::is_loaded() const { return g_loaded; }

void MemorySession::load(std::string bitasmfile) {
  std::string ocla_json = BitAssembler_MGR::get_ocla_design(bitasmfile);
  CFG_ASSERT_MSG(!ocla_json.empty(), "No OCLA detected in user design");
  parse(ocla_json);
  g_loaded = true;
}

void MemorySession::unload() { g_loaded = false; }

uint32_t MemorySession::get_instance_count() { return 0; }

Ocla_INSTANCE_INFO MemorySession::get_instance_info(uint32_t instance) {
  return Ocla_INSTANCE_INFO();
}

std::vector<Ocla_PROBE_INFO> MemorySession::get_probe_info(uint32_t instance) {
  return std::vector<Ocla_PROBE_INFO>();
}

void MemorySession::parse(std::string ocla_json) {
  CFG_POST_MSG("OCLA: %s", ocla_json.c_str());
}
