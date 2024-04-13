#include "OclaDebugSession.h"

#include <regex>
#include <string>

#include "BitAssembler/BitAssembler_mgr.h"
#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "nlohmann_json/json.hpp"

OclaDebugSession::OclaDebugSession() : m_loaded(false) {}

OclaDebugSession::~OclaDebugSession() {}

std::vector<OclaInstance> OclaDebugSession::get_instances() {
  std::vector<OclaInstance> instances{};

  for (auto &domain : m_clock_domains) {
    auto list = domain.get_instances();
    instances.insert(instances.end(), list.begin(), list.end());
  }

  return instances;
}

std::vector<OclaDomain> &OclaDebugSession::get_clock_domains() {
  return m_clock_domains;
}

std::string OclaDebugSession::get_filepath() const { return m_filepath; }

bool OclaDebugSession::is_loaded() const { return m_loaded; }

std::vector<OclaProbe> OclaDebugSession::get_probes(uint32_t instance_index) {
  std::vector<OclaProbe> probes{};

  for (auto &domain : m_clock_domains) {
    for (auto &probe : domain.get_probes()) {
      if (probe.get_instance_index() == instance_index) {
        probes.push_back(probe);
      }
    }
  }

  return probes;
}

bool OclaDebugSession::load(std::string filepath,
                            std::vector<std::string> &error_messages) {
  if (m_loaded) {
    // force unload
    unload();
  }

  std::string ocla_str = BitAssembler_MGR::get_ocla_design(filepath);
  if (ocla_str.empty()) {
    error_messages.push_back("No OCLA debug information found");
    return false;
  }

  if (!parse(ocla_str, error_messages)) {
    return false;
  }

  m_filepath = filepath;
  m_loaded = true;
  return true;
}

void OclaDebugSession::unload() {
  m_clock_domains.clear();
  m_filepath.clear();
  m_loaded = false;
}

bool OclaDebugSession::parse(std::string ocla_str,
                             std::vector<std::string> &error_messages) {
  bool res = true;
  try {
    nlohmann::json json = nlohmann::json::parse(ocla_str);
    nlohmann::json ocla_debug_subsystem = json.at("ocla_debug_subsystem");
    nlohmann::json ocla_list = json.at("ocla");
    std::vector<OclaDomain> deferred_list{};
    std::map<uint32_t, int> indexes{};
    uint32_t probes_sum = 0;
    bool single = false;

    if (ocla_debug_subsystem.at("Sampling_Clk") == "SINGLE") {
      // create one single domain object for all ocla instances for single
      // sampling clock setting
      m_clock_domains.push_back(OclaDomain{oc_domain_type_t::NATIVE, 1});
      single = true;
    }

    for (const auto &ocla : ocla_list) {
      // sanity check: duplicate instance index
      if (indexes.find(ocla.at("INDEX")) != indexes.end()) {
        error_messages.push_back("Duplicate instance index " +
                                 std::to_string((uint32_t)ocla.at("INDEX")));
        res = false;
        break;
      }

      // parse instance info
      OclaInstance instance{ocla.at("IP_TYPE"),      ocla.at("IP_VERSION"),
                            ocla.at("IP_ID"),        ocla.at("MEM_DEPTH"),
                            ocla.at("NO_OF_PROBES"), ocla.at("addr"),
                            ocla.at("INDEX")};

      // insert into index map for duplicate check
      indexes[instance.get_index()] = 1;

      // accumulate no of probes for all instances
      probes_sum += instance.get_num_of_probes();

      // parse signal info
      std::vector<OclaSignal> signals{};
      uint32_t bitpos = 0;
      uint32_t total_bitwidth = 0;
      for (auto const &elem : ocla.at("probes")) {
        auto sig = parse_signal(elem);
        sig.set_bitpos(bitpos);
        bitpos += sig.get_bitwidth();
        total_bitwidth += sig.get_bitwidth();
        signals.push_back(sig);
      }

      // sanity check: total width of probes equals to no. of probes of the
      // instance
      if (total_bitwidth != instance.get_num_of_probes()) {
        error_messages.push_back(
            "Instance " + std::to_string(instance.get_index()) +
            " total probes width mismatched (expect=" +
            std::to_string(instance.get_num_of_probes()) +
            ", actual=" + std::to_string(total_bitwidth) + ")");
        res = false;
        break;
      }

      // parse probe info
      if (ocla.at("probe_info").size() > 0) {
        // native probe
        uint32_t signal_index = 1;

        if (!single) {
          // create a domain object for each ocla instance for multiple sampling
          // clock setting
          m_clock_domains.push_back(OclaDomain{
              oc_domain_type_t::NATIVE, (uint32_t)m_clock_domains.size() + 1});
        }

        // get target domain object to work on
        OclaDomain &domain = m_clock_domains[m_clock_domains.size() - 1];
        domain.add_instance(instance);

        // create probes
        uint32_t total_probe_width = 0;
        for (auto const &elem : ocla.at("probe_info")) {
          uint32_t probe_index = elem.at("index");
          uint32_t offset = elem.at("offset");
          uint32_t width = elem.at("width");
          total_probe_width += width;
          OclaProbe probe{probe_index + 1};
          probe.set_instance_index(instance.get_index());
          for (auto &sig : signals) {
            // add signals
            if (sig.get_bitpos() >= offset &&
                sig.get_bitpos() < (offset + width)) {
              sig.set_index(signal_index++);
              probe.add_signal(sig);
            }
          }
          domain.add_probe(probe);
        }

        // sanity check: total width of probe_info equals to no. of probes of
        // the instance
        if (total_probe_width != instance.get_num_of_probes()) {
          error_messages.push_back(
              "Instance " + std::to_string(instance.get_index()) +
              " total probe_info width mismatched (expect=" +
              std::to_string(instance.get_num_of_probes()) +
              ", actual=" + std::to_string(total_probe_width) + ")");
          res = false;
          break;
        }
      } else {
        // axi probe
        uint32_t signal_index = 1;
        OclaDomain domain{oc_domain_type_t::AXI};
        domain.add_instance(instance);
        OclaProbe probe{1};
        probe.set_instance_index(instance.get_index());
        for (auto &sig : signals) {
          // add signals
          sig.set_index(signal_index++);
          probe.add_signal(sig);
        }
        domain.add_probe(probe);
        // defer adding axi domains as it should always stay at the bottom of
        // the clock domain list
        deferred_list.push_back(domain);
      }
    }

    if (res) {
      // sanity check: total no. of probes equals to Probes_Sum in debug info
      if (probes_sum != ocla_debug_subsystem.at("Probes_Sum")) {
        error_messages.push_back(
            "Total sum of all probes mismatched (expect=" +
            std::to_string((uint32_t)ocla_debug_subsystem.at("Probes_Sum")) +
            ", actual=" + std::to_string(probes_sum) + ")");
        res = false;
      }
    }

    if (res) {
      // insert deferred clock domains (axi domain)
      if (!deferred_list.empty()) {
        for (auto &elem : deferred_list) {
          elem.set_index((uint32_t)m_clock_domains.size() + 1);
          m_clock_domains.push_back(elem);
        }
      }
    } else {
      m_clock_domains.clear();
    }
  } catch (const nlohmann::detail::exception &e) {
    CFG_ASSERT_MSG(false, e.what());
  }

  return res;
}

OclaSignal OclaDebugSession::parse_signal(std::string signal_str) {
  static std::map<uint32_t, std::string> patterns = {
      {1, R"((\w+) *\[ *(\d+) *: *(\d+)\ *])"},
      {2, R"((\d+)'([01]+))"},
      {3, R"((\w+) *\[ *(\d+)\ *])"},
      {4, R"(^(\w+)$)"}};

  uint32_t patid = 0;
  std::cmatch m;

  for (const auto &[i, pat] : patterns) {
    if (std::regex_search(signal_str.c_str(), m,
                          std::regex(pat, std::regex::icase)) == true) {
      patid = i;
      break;
    }
  }

  CFG_ASSERT_MSG(patid > 0, "Invalid signal '%s'", signal_str.c_str());

  OclaSignal sig{};

  switch (patid) {
    case 1U:  // pattern 1: count[13:2]
    {
      uint64_t bit_start = CFG_convert_string_to_u64(m[3].str());
      uint64_t bit_end = CFG_convert_string_to_u64(m[2].str());
      CFG_ASSERT_MSG(bit_end >= bit_start, "Invalid bit position '%s'",
                     signal_str.c_str());
      sig.set_orig_name(m[0].str());
      sig.set_name(m[1].str());
      sig.set_type(oc_signal_type_t::SIGNAL);
      sig.set_value(0);
      sig.set_bitwidth(static_cast<uint32_t>(bit_end - bit_start) + 1);
      break;
    }
    case 2U:  // pattern 2: 4'0000
    {
      sig.set_orig_name(m[0].str());
      sig.set_name(m[0].str());
      sig.set_type(oc_signal_type_t::PLACEHOLDER);
      sig.set_value(
          static_cast<uint32_t>(CFG_convert_string_to_u64("b" + m[2].str())));
      sig.set_bitwidth(
          static_cast<uint32_t>(CFG_convert_string_to_u64(m[1].str())));
      break;
    }
    case 3U:  // pattern 3: s_axil_awprot[0]
    {
      sig.set_orig_name(m[0].str());
      sig.set_name(m[1].str());
      sig.set_type(oc_signal_type_t::SIGNAL);
      sig.set_value(0);
      sig.set_bitwidth(1);
      break;
    }
    case 4U:  // pattern 4: s_axil_bready
    {
      sig.set_orig_name(m[0].str());
      sig.set_name(m[0].str());
      sig.set_type(oc_signal_type_t::SIGNAL);
      sig.set_value(0);
      sig.set_bitwidth(1);
      break;
    }
    default:
      break;
  }

  return sig;
}
