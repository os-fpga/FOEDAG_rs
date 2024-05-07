#include "OclaDebugSession.h"

#include <string>

#include "BitAssembler/BitAssembler_mgr.h"
#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "OclaHelpers.h"

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

  std::string json_str = BitAssembler_MGR::get_ocla_design(filepath);
  if (json_str.empty()) {
    error_messages.push_back("No OCLA debug information found");
    return false;
  }

  try {
    nlohmann::json json = nlohmann::json::parse(json_str);
    if (!json.contains("eio") && !json.contains("ocla")) {
      error_messages.push_back(
          "Debug information contains no OCLA nor EIO information");
      return false;
    }
    if (json.contains("ocla")) {
      if (!parse_ocla(json, error_messages)) {
        return false;
      }
    }
    if (json.contains("eio")) {
      if (!parse_eio(json, error_messages)) {
        return false;
      }
    }
  } catch (const nlohmann::detail::exception &e) {
    CFG_ASSERT_MSG(false, e.what());
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

std::vector<EioInstance> &OclaDebugSession::get_eio_instances() {
  return m_eio_instances;
}

bool OclaDebugSession::parse_eio(nlohmann::json json,
                                 std::vector<std::string> &error_messages) {
  nlohmann::json eio = json.at("eio");

  // create one eio instance. only 1 instance is supported at current version
  EioInstance instance{eio.at("addr"), 1};

  if (eio.contains("probes_in")) {
    uint32_t input_probe_width = eio.at("Input_Probe_Width");
    uint32_t idx = 1;
    uint32_t probe_width = 0;
    uint32_t bitpos = 0;

    // NOTE: create one input probe. only 1 input probe is supported at current
    // version. multiple input probes will be supported in the future.
    eio_probe_t probe{};

    // parse probe signals
    for (auto &p : eio.at("probes_in")) {
      eio_signal_t sig{};
      if (!parse_eio_signal(p, sig, error_messages)) {
        return false;
      }
      sig.idx = idx++;
      sig.bitpos = bitpos;
      probe_width += sig.bitwidth;
      bitpos += sig.bitwidth;
      probe.signal_list.push_back(sig);
    }

    probe.type = eio_probe_type_t::IO_INPUT;
    probe.probe_width = probe_width;
    probe.idx = 1;

    // sanity check to see if parsed total signal width matches
    // Input_Probe_Width
    if (probe.probe_width != input_probe_width) {
      error_messages.push_back("EIO: Input probe width mismatched (expect=" +
                               std::to_string(input_probe_width) + ", actual=" +
                               std::to_string(probe.probe_width) + ")");
      return false;
    }

    instance.add_probe(probe);
  }

  if (eio.contains("probes_out")) {
    uint32_t output_probe_width = eio.at("Output_Probe_Width");
    uint32_t idx = 1;
    uint32_t probe_width = 0;
    uint32_t bitpos = 0;

    // NOTE: create one output probe. only 1 output probe is supported at
    // current version. multiple output probes will be supported in the future.
    eio_probe_t probe{};

    // parse probe signals
    for (auto &p : eio.at("probes_out")) {
      eio_signal_t sig{};
      if (!parse_eio_signal(p, sig, error_messages)) {
        return false;
      }
      sig.idx = idx++;
      sig.bitpos = bitpos;
      probe_width += sig.bitwidth;
      bitpos += sig.bitwidth;
      probe.signal_list.push_back(sig);
    }

    probe.type = eio_probe_type_t::IO_OUTPUT;
    probe.probe_width = probe_width;
    probe.idx = 1;

    // sanity check to see if parsed total signal width matches
    // Output_Probe_Width
    if (probe.probe_width != output_probe_width) {
      error_messages.push_back("EIO: Output probe width mismatched (expect=" +
                               std::to_string(output_probe_width) +
                               ", actual=" + std::to_string(probe.probe_width) +
                               ")");
      return false;
    }

    instance.add_probe(probe);
  }

  m_eio_instances.push_back(instance);
  return true;
}

bool OclaDebugSession::parse_ocla(nlohmann::json json,
                                  std::vector<std::string> &error_messages) {
  nlohmann::json ocla_debug_subsystem = json.at("ocla_debug_subsystem");
  nlohmann::json ocla_list = json.at("ocla");
  std::vector<OclaDomain> deferred_list{};
  std::map<uint32_t, int> indexes{};
  uint32_t probes_sum = 0;
  bool single = false;
  bool res = true;

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
      OclaSignal sig{};
      if (!parse_ocla_signal(elem, sig, error_messages)) {
        res = false;
        break;
      }
      sig.set_bitpos(bitpos);
      bitpos += sig.get_bitwidth();
      total_bitwidth += sig.get_bitwidth();
      signals.push_back(sig);
    }

    // break the loop in case probes parsing failed above
    if (!res) {
      break;
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

  return res;
}

bool OclaDebugSession::parse_eio_signal(
    std::string signal_str, eio_signal_t &signal,
    std::vector<std::string> &error_messages) {
  std::string signal_name = "";
  uint32_t bit_start = 0;
  uint32_t bit_end = 0;
  uint32_t bit_width = 0;

  auto patid =
      CFG_parse_signal(signal_str, signal_name, bit_start, bit_end, bit_width);
  switch (patid) {
    case OCLA_SIGNAL_PATTERN_1:  // pattern 1: count[13:2]
    case OCLA_SIGNAL_PATTERN_3:  // pattern 3: s_axil_awprot[0]
    case OCLA_SIGNAL_PATTERN_5:  // pattern 5: s_axil_bready
      if (bit_end < bit_start) {
        error_messages.push_back("EIO: Invalid bit position '" + signal_str +
                                 "'");
        return false;
      }
      signal.orig_name = signal_str;
      signal.name = signal_name;
      signal.bitpos = bit_start;
      signal.bitwidth = bit_end - bit_start + 1;
      break;
    default:
      error_messages.push_back("EIO: Invalid signal name format '" +
                               signal_str + "'");
      return false;
  }

  return true;
}

bool OclaDebugSession::parse_ocla_signal(
    std::string signal_str, OclaSignal &signal,
    std::vector<std::string> &error_messages) {
  std::string signal_name = "";
  uint32_t bit_start = 0;
  uint32_t bit_end = 0;
  uint32_t bit_width = 0;
  uint64_t bit_value = 0;

  auto patid = CFG_parse_signal(signal_str, signal_name, bit_start, bit_end,
                                bit_width, &bit_value);
  switch (patid) {
    case OCLA_SIGNAL_PATTERN_1:  // pattern 1: count[13:2]
    {
      if (bit_end < bit_start) {
        error_messages.push_back("Invalid bit position '" + signal_str + "'");
        return false;
      }
      signal.set_orig_name(signal_str);
      signal.set_name(signal_name);
      signal.set_type(oc_signal_type_t::SIGNAL);
      signal.set_value(0);
      signal.set_bitwidth(static_cast<uint32_t>(bit_end - bit_start) + 1);
      break;
    }
    case OCLA_SIGNAL_PATTERN_2:  // pattern 2: 4'0000
    {
      if (bit_width == 0) {
        error_messages.push_back("Invalid signal name format '" + signal_str +
                                 "'");
        return false;
      }
      signal.set_orig_name(signal_str);
      signal.set_name(signal_name);
      signal.set_type(oc_signal_type_t::CONSTANT);
      signal.set_value((uint32_t)bit_value);
      signal.set_bitwidth(bit_width);
      break;
    }
    case OCLA_SIGNAL_PATTERN_3:  // pattern 3: s_axil_awprot[0]
    case OCLA_SIGNAL_PATTERN_5:  // pattern 5: s_axil_bready
    {
      signal.set_orig_name(signal_str);
      signal.set_name(signal_name);
      signal.set_type(oc_signal_type_t::SIGNAL);
      signal.set_value(0);
      signal.set_bitwidth(1);
      break;
    }
    default:
      error_messages.push_back("Invalid signal name format '" + signal_str +
                               "'");
      return false;
  }

  return true;
}
