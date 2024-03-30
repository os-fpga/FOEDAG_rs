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

std::vector<OclaDomain>& OclaDebugSession::get_clock_domains() { return m_clock_domains; }

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

void OclaDebugSession::load(std::string filepath) {
    if (m_loaded) {
        // force unload
        unload();
    }

    std::string ocla_str = BitAssembler_MGR::get_ocla_design(filepath);
    CFG_ASSERT_MSG(!ocla_str.empty(), "No OCLA info found");
    parse(ocla_str);
    m_filepath = filepath;
    m_loaded = true;
}

void OclaDebugSession::unload() {
    m_clock_domains.clear();
    m_filepath.clear();
    m_loaded = false;
}

void OclaDebugSession::parse(std::string ocla_str) {
    try {
        nlohmann::json json = nlohmann::json::parse(ocla_str);
        nlohmann::json ocla_debug_subsystem = json.at("ocla_debug_subsystem");
        nlohmann::json ocla_list = json.at("ocla");
        std::vector<OclaDomain> deferred_list{};
        bool single = false;

        if (ocla_debug_subsystem.at("Sampling_Clk") == "SINGLE") {
            m_clock_domains.push_back(OclaDomain{oc_domain_type_t::NATIVE, 1});
            single = true;
        }

        for (const auto& ocla : ocla_list) {
            // parse instance info
            OclaInstance instance{ocla.at("IP_TYPE"), ocla.at("IP_VERSION"), ocla.at("IP_ID"), ocla.at("MEM_DEPTH"), 
                ocla.at("NO_OF_PROBES"), ocla.at("addr"), ocla.at("INDEX")};

            // parse signal info
            std::vector<OclaSignal> signals{};
            uint32_t sig_pos = 0;
            for (auto const &elem : ocla.at("probes")) {
                auto sig = parse_signal(elem);
                sig.set_pos(sig_pos);
                sig_pos += sig.get_bitwidth();
                signals.push_back(sig);
            }

            // parse probe info
            if (ocla.at("probe_info").size() > 0) {
                // native probe
                uint32_t signal_index = 1;

                // create domain object for multiple clock domain ocla
                if (!single) {
                    m_clock_domains.push_back(OclaDomain{oc_domain_type_t::NATIVE, (uint32_t)m_clock_domains.size() + 1});
                }

                // get target domain object to work on
                OclaDomain &domain = m_clock_domains[m_clock_domains.size() - 1];
                domain.add_instance(instance);

                // create probes
                for (auto const &elem : ocla.at("probe_info")) {
                    uint32_t probe_index = elem.at("index");
                    uint32_t offset = elem.at("offset");
                    uint32_t width = elem.at("width");
                    OclaProbe probe{probe_index + 1};
                    probe.set_instance_index(instance.get_index());
                    for (auto &sig : signals) {
                        // add signals
                        if (sig.get_pos() >= offset && sig.get_pos() < (offset + width)) {
                            sig.set_index(signal_index++);
                            probe.add_signal(sig);
                        }
                    }
                    domain.add_probe(probe);
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
                // defer adding axi domains as it should always stay at the bottom of the clock domain list 
                deferred_list.push_back(domain);
            }
        }

        // insert deferred clock domains (axi domain)
        if (!deferred_list.empty()) {
            for (auto &elem : deferred_list) {
                elem.set_index((uint32_t)m_clock_domains.size() + 1);
                m_clock_domains.push_back(elem);
            }
        }
    } catch (const nlohmann::detail::exception& e) {
        CFG_ASSERT_MSG(false, e.what());
    }
}

OclaSignal OclaDebugSession::parse_signal(std::string signal_str) {

  static std::map<uint32_t, std::string> patterns = {
      {1, R"((\w+) *\[ *(\d+) *: *(\d+)\ *])"},
      {2, R"((\d+)'([01]+))"},
      {3, R"((\w+) *\[ *(\d+)\ *])"},
      {4, R"(^(\w+)$)"}};

  uint32_t patid = 0;
  std::cmatch m;

  for (const auto& [i, pat] : patterns) {
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
      sig.set_name(m[0].str());
      sig.set_type(oc_signal_type_t::SIGNAL);
      sig.set_value(0);
      sig.set_bitwidth(static_cast<uint32_t>(bit_end - bit_start) + 1);
      break;
    }
    case 2U:  // pattern 2: 4'0000
    {
      sig.set_name(m[0].str());
      sig.set_type(oc_signal_type_t::PLACEHOLDER);
      sig.set_value(static_cast<uint32_t>(CFG_convert_string_to_u64("b" + m[2].str())));
      sig.set_bitwidth(static_cast<uint32_t>(CFG_convert_string_to_u64(m[1].str())));
      break;
    }
    case 3U:  // pattern 3: s_axil_awprot[0]
    case 4U:  // pattern 4: s_axil_bready
    {
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
