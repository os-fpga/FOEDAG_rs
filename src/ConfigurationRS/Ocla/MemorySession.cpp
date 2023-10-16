#include "MemorySession.h"

#include <regex>

#include "BitAssembler/BitAssembler_mgr.h"
#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "nlohmann_json/json.hpp"

std::map<uint32_t, Ocla_INSTANCE_INFO> MemorySession::m_instances{};
std::map<uint32_t, std::vector<Ocla_PROBE_INFO>> MemorySession::m_probes{};
bool MemorySession::m_loaded = false;

MemorySession::MemorySession() {}

MemorySession::~MemorySession() {}

void MemorySession::load(std::string bitasmfile) {
  std::string ocla_json = BitAssembler_MGR::get_ocla_design(bitasmfile);
  CFG_ASSERT_MSG(!ocla_json.empty(), "No OCLA info");
  parse(ocla_json);
  m_loaded = true;
}

void MemorySession::unload() {
  m_instances.clear();
  m_probes.clear();
  m_loaded = false;
}

uint32_t MemorySession::get_instance_count() { return m_instances.size(); }

Ocla_INSTANCE_INFO MemorySession::get_instance_info(uint32_t instance) {
  CFG_ASSERT(instance < m_instances.size());
  return m_instances[instance];
}

std::vector<Ocla_PROBE_INFO> MemorySession::get_probe_info(uint32_t instance) {
  CFG_ASSERT(instance < m_probes.size());
  return m_probes[instance];
}

void MemorySession::parse(std::string ocla_json) {
  uint32_t total_bitwidth = 0;
  uint32_t id = 0;
  try {
    nlohmann::json json = nlohmann::json::parse(ocla_json);
    nlohmann::json ocla_array = json.at("ocla");
    for (const auto& ocla : ocla_array) {
      Ocla_INSTANCE_INFO inf{};
      inf.type = ocla.at("IP_TYPE");
      inf.version = ocla.at("IP_VERSION");
      inf.id = ocla.at("IP_ID");
      inf.axi_addr_width = ocla.at("AXI_ADDR_WIDTH");
      inf.axi_data_width = ocla.at("AXI_DATA_WIDTH");
      inf.base_addr = ocla.at("addr");
      inf.depth = ocla.at("MEM_DEPTH");
      inf.num_probes = ocla.at("NO_OF_PROBES");
      inf.num_trigger_inputs = ocla.at("NO_OF_TRIGGER_INPUTS");
      inf.probe_width = ocla.at("PROBE_WIDHT");  //<-- RTL typo
      m_instances.insert(std::make_pair(id, inf));
      // probe info
      std::vector<Ocla_PROBE_INFO> probes{};
      for (const auto& probe : ocla.at("probes")) {
        const auto& probe_inf = parse_probe((std::string)probe);
        probes.push_back(probe_inf);
        total_bitwidth += probe_inf.bitwidth;
      }
      CFG_ASSERT_MSG(total_bitwidth == inf.num_probes,
                     "Ocla %d total probe width is %u but expect %u", id,
                     total_bitwidth, inf.num_probes);
      m_probes.insert(std::make_pair(id, probes));
      total_bitwidth = 0;
      ++id;
    }
  } catch (const nlohmann::detail::exception& e) {
    CFG_ASSERT_MSG(false, e.what());
  }
}

Ocla_PROBE_INFO MemorySession::parse_probe(std::string probe) {
  static std::map<uint32_t, std::string> patterns = {
      {1, R"((\w+) *\[ *(\d+) *: *(\d+)\ *])"},
      {2, R"((\d+)'([01]+))"},
      {3, R"((\w+) *\[ *(\d+)\ *])"},
      {4, R"(^(\w+)$)"}};

  uint32_t patid = 0;
  std::cmatch m;

  for (const auto& [i, pat] : patterns) {
    if (std::regex_search(probe.c_str(), m,
                          std::regex(pat, std::regex::icase)) == true) {
      patid = i;
      break;
    }
  }

  CFG_ASSERT_MSG(patid > 0, "Invalid probe '%s'", probe.c_str());

  Ocla_PROBE_INFO probe_inf{};

  switch (patid) {
    case 1U:  // pattern 1: count[13:2]
    {
      uint64_t bit_start = CFG_convert_string_to_u64(m[3].str());
      uint64_t bit_end = CFG_convert_string_to_u64(m[2].str());
      CFG_ASSERT_MSG(bit_end >= bit_start, "Invalid bit position '%s'",
                     probe.c_str());
      probe_inf.signal_name = m[1].str();
      probe_inf.type = SIGNAL;
      probe_inf.value = 0;
      probe_inf.bitwidth = static_cast<uint32_t>(bit_end - bit_start) + 1;
      break;
    }
    case 2U:  // pattern 2: 4'0000
    {
      probe_inf.signal_name = "_";
      probe_inf.type = PLACEHOLDER;
      probe_inf.value =
          static_cast<uint32_t>(CFG_convert_string_to_u64("b" + m[2].str()));
      probe_inf.bitwidth =
          static_cast<uint32_t>(CFG_convert_string_to_u64(m[1].str()));
      break;
    }
    case 3U:  // pattern 3: s_axil_awprot[0]
    case 4U:  // pattern 4: s_axil_bready
    {
      probe_inf.signal_name = m[1].str();
      probe_inf.type = SIGNAL;
      probe_inf.value = 0;
      probe_inf.bitwidth = 1;
      break;
    }
    default:
      break;
  }

  return probe_inf;
}
