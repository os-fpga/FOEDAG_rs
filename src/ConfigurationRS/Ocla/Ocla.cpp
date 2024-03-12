#include "Ocla.h"

#include <algorithm>
#include <sstream>

#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "OclaHelpers.h"
#include "OclaIP.h"
#include "OclaJtagAdapter.h"
#include "OclaSession.h"
#include "OclaWaveformWriter.h"

#define OCLA_MAX_INSTANCE_COUNT (15)
#define OCLA1_ADDR (0x03000000)
#define OCLA2_ADDR (0x04000000)
#define OCLA3_ADDR (0x05000000)
#define OCLA_TYPE ("OCLA")
#define OCLA_MAX_PROBE (1024)
#define OCLA_MIN_TRIGGER_COUNT (4)

static std::map<uint32_t, uint32_t> ocla_base_address = {
    {1, OCLA1_ADDR}, {2, OCLA2_ADDR}, {3, OCLA3_ADDR}};

OclaIP Ocla::get_ocla_instance(uint32_t instance) {
  CFG_ASSERT(ocla_base_address.find(instance) != ocla_base_address.end());
  OclaIP ocla_ip{m_adapter, ocla_base_address[instance]};
  CFG_ASSERT(ocla_ip.get_type() == OCLA_TYPE);
  return ocla_ip;
}

std::map<uint32_t, OclaIP> Ocla::detect_ocla_instances() {
  std::map<uint32_t, OclaIP> list;

  for (auto& [i, baseaddr] : ocla_base_address) {
    OclaIP ocla_ip{m_adapter, baseaddr};
    if (ocla_ip.get_type() == OCLA_TYPE) {
      auto trigger_count = ocla_ip.get_trigger_count();
      if (trigger_count < OCLA_MIN_TRIGGER_COUNT) {
        CFG_POST_WARNING("Invalid trigger count %d detected for instance %d",
                         trigger_count, i);
      } else {
        list.insert(std::pair<uint32_t, OclaIP>(i, ocla_ip));
      }
    }
  }

  return list;
}

bool Ocla::get_instance_info(uint32_t base_addr,
                             Ocla_INSTANCE_INFO& instance_info, uint32_t& idx) {
  for (uint32_t i = 0; i < m_session->get_instance_count(); i++) {
    auto ocla = m_session->get_instance_info(i);
    if (ocla.base_addr == base_addr) {
      instance_info = ocla;
      idx = i;
      return true;
    }
  }
  return false;
}

std::vector<Ocla_PROBE_INFO> Ocla::get_probe_info(uint32_t base_addr) {
  Ocla_INSTANCE_INFO inf{};
  uint32_t idx;
  if (get_instance_info(base_addr, inf, idx)) {
    return m_session->get_probe_info(idx);
  }
  return {};
}

std::map<uint32_t, Ocla_PROBE_INFO> Ocla::find_probe_info_by_name(
    uint32_t base_addr, std::string probe_name) {
  std::map<uint32_t, Ocla_PROBE_INFO> list{};
  uint32_t offset = 0;

  for (const auto& probe_inf : get_probe_info(base_addr)) {
    if (probe_inf.signal_name.find(probe_name) == 0) {
      list.insert(std::make_pair(offset, probe_inf));
    }
    offset += probe_inf.bitwidth;
  }

  return list;
}

bool Ocla::find_probe_info_by_offset(uint32_t base_addr, uint32_t bit_offset,
                                     Ocla_PROBE_INFO& output) {
  uint32_t offset = 0;
  for (const auto& probe_inf : get_probe_info(base_addr)) {
    if (offset == bit_offset) {
      output = probe_inf;
      return true;
    }
    offset += probe_inf.bitwidth;
  }
  return false;
}

bool Ocla::validate() {
  if (m_session->is_loaded()) {
    auto instances = detect_ocla_instances();
    if (instances.size() != m_session->get_instance_count()) return false;

    for (const auto& [i, ocla_ip] : instances) {
      Ocla_INSTANCE_INFO inf{};
      uint32_t idx;
      if (!get_instance_info(ocla_ip.get_base_addr(), inf, idx) ||
          (inf.version != ocla_ip.get_version()) ||
          (inf.type != ocla_ip.get_type()) || (inf.id != ocla_ip.get_id()) ||
          (inf.num_probes != ocla_ip.get_number_of_probes()) ||
          (inf.depth != ocla_ip.get_memory_depth())) {
        return false;
      }
    }
  }
  return true;
}

Ocla::Ocla(OclaJtagAdapter* adapter, OclaSession* session,
           OclaWaveformWriter* writer)
    : m_adapter(adapter), m_session(session), m_writer(writer) {
  CFG_ASSERT(m_adapter != nullptr);
  CFG_ASSERT(m_session != nullptr);
  CFG_ASSERT(m_writer != nullptr);
}

void Ocla::configure(uint32_t instance, std::string mode, std::string boolcomp,
                     uint32_t sample_size) {
  if (!validate()) {
    CFG_POST_ERR("OCLA info not matched with the detected OCLA IP");
    return;
  }

  auto ocla_ip = get_ocla_instance(instance);
  auto depth = ocla_ip.get_memory_depth();
  if (sample_size > depth) {
    CFG_POST_ERR(
        "Invalid sample size parameter (Sample size should be beween 0 and %d)",
        depth);
    return;
  }

  ocla_config cfg;

  std::transform(boolcomp.begin(), boolcomp.end(), boolcomp.begin(), ::toupper);
  cfg.fns = sample_size > 0 ? true : false;
  cfg.ns = sample_size;
  cfg.mode = convert_ocla_trigger_mode(mode);
  cfg.boolcomp = convert_trigger_bool_comp(boolcomp);

  ocla_ip.configure(cfg);
}

void Ocla::configure_channel(uint32_t instance, uint32_t channel,
                             std::string type, std::string event,
                             uint32_t value, uint32_t value_compare_width,
                             std::string probe) {
  if (!validate()) {
    CFG_POST_ERR("OCLA info not matched with the detected OCLA IP");
    return;
  }

  if (type == "edge") {
    if (event != "rising" && event != "falling" && event != "either") {
      CFG_POST_ERR("Invalid event parameter for edge trigger");
      return;
    }
  } else if (type == "value_compare") {
    if (event != "equal" && event != "lesser" && event != "greater") {
      CFG_POST_ERR("Invalid event parameter for value compare trigger");
      return;
    }
  } else if (type == "level") {
    if (event != "high" && event != "low") {
      CFG_POST_ERR("Invalid event parameter for level trigger");
      return;
    }
  }

  auto ocla_ip = get_ocla_instance(instance);
  bool status = false;
  uint64_t probe_num = CFG_convert_string_to_u64(probe, false, &status);
  if (!status) {
    if (!m_session->is_loaded()) {
      CFG_POST_ERR("Probe name cannot be used when no user design loaded");
      return;
    }
    // translate probe name to probe index
    auto probe_list = find_probe_info_by_name(ocla_ip.get_base_addr(), probe);
    if (probe_list.empty()) {
      CFG_POST_ERR("Invalid probe name '%s'", probe.c_str());
      return;
    }
    probe_num = probe_list.begin()->first;
    CFG_POST_MSG("Selected probe '%s' at bit pos %u",
                 probe_list.begin()->second.signal_name.c_str(), probe_num);
  }

  uint32_t max_probes = ocla_ip.get_number_of_probes();
  if (max_probes > OCLA_MAX_PROBE) {
    max_probes = OCLA_MAX_PROBE;
  }

  if (probe_num >= max_probes) {
    CFG_POST_ERR(
        "Invalid probe parameter (Probe number should be between 0 and %d)",
        max_probes - 1);
    return;
  }

  ocla_trigger_config trig_cfg;

  trig_cfg.probe_num = (uint32_t)probe_num;
  trig_cfg.type = convert_trigger_type(type);
  trig_cfg.event = convert_trigger_event(event);
  trig_cfg.value = value;
  trig_cfg.value_compare_width = value_compare_width;

  ocla_ip.configure_channel(channel - 1, trig_cfg);
}

bool Ocla::start(uint32_t instance, uint32_t timeout,
                 std::string output_filepath) {
  if (!validate()) {
    CFG_POST_ERR("OCLA info not matched with the detected OCLA IP");
    return false;
  }

  uint32_t countdown = timeout;
  auto ocla_ip = get_ocla_instance(instance);
  ocla_ip.start();

  while (true) {
    // wait for 1 sec
    CFG_sleep_ms(1000);

    if (ocla_ip.get_status() == DATA_AVAILABLE) {
      ocla_data data = ocla_ip.get_data();
      m_writer->set_width(data.width);
      m_writer->set_depth(data.depth);
      if (m_session->is_loaded()) {
        m_writer->set_signals(generate_signal_descriptor(
            get_probe_info(ocla_ip.get_base_addr())));
      } else {
        m_writer->set_signals(generate_signal_descriptor(data.width));
      }
      m_writer->write(data.values, output_filepath);
      break;
    }

    if (timeout > 0) {
      if (countdown == 0) {
        CFG_POST_ERR("Timeout error");
        return false;
      }
      --countdown;
    }
  }
  return true;
}

std::string Ocla::show_info() {
  std::ostringstream ss;
  int count = 0;
  bool is_valid = false;

  if (m_session->is_loaded()) {
    is_valid = validate();
    if (!is_valid)
      CFG_POST_ERR("OCLA info not matched with the detected OCLA IP");
    ss << "User design loaded   : " << m_session->get_bitasm_filepath()
       << std::endl;
  }

  for (auto& [idx, ocla_ip] : detect_ocla_instances()) {
    uint32_t depth = ocla_ip.get_memory_depth();
    uint32_t base_addr = ocla_ip.get_base_addr();
    ss << "OCLA " << idx << std::setfill('0') << std::hex << std::endl
       << "  Base address       : 0x" << std::setw(8) << base_addr << std::endl
       << "  ID                 : 0x" << std::setw(8) << ocla_ip.get_id()
       << std::endl
       << "  Type               : '" << ocla_ip.get_type() << "'" << std::endl
       << "  Version            : 0x" << std::setw(8) << ocla_ip.get_version()
       << std::endl
       << "  Probes             : " << std::dec
       << ocla_ip.get_number_of_probes() << std::endl
       << "  Memory depth       : " << depth << std::endl
       << "  DA status          : " << ocla_ip.get_status() << std::endl;

    auto cfg = ocla_ip.get_config();

    uint32_t ns = cfg.fns ? cfg.ns : depth;

    ss << "  No. of samples     : " << ns << std::endl
       << "  Trigger mode       : "
       << convert_ocla_trigger_mode_to_string(cfg.mode) << std::endl
       << "  Multi-trigger bool : "
       << convert_trigger_bool_comp_to_string(cfg.boolcomp) << std::endl
       << "  Trigger " << std::endl
       << std::setfill(' ');

    for (uint32_t ch = 1; ch < 5; ch++) {
      auto trig_cfg = ocla_ip.get_channel_config(ch - 1);

      std::string probe_name = std::to_string(trig_cfg.probe_num);
      if (m_session->is_loaded()) {
        // try translate probe index to probe name
        Ocla_PROBE_INFO probe_inf{};
        if (find_probe_info_by_offset(ocla_ip.get_base_addr(),
                                      trig_cfg.probe_num, probe_inf)) {
          probe_name += "(" + probe_inf.signal_name + ")";
        }
      }

      switch (trig_cfg.type) {
        case EDGE:
        case LEVEL:
          ss << "    Channel " << ch << "        : "
             << "probe=" << probe_name << "; "
             << "mode=" << convert_trigger_event_to_string(trig_cfg.event)
             << "_" << convert_trigger_type_to_string(trig_cfg.type)
             << std::endl;
          break;
        case VALUE_COMPARE:
          ss << "    Channel " << ch << "        : "
             << "probe=" << probe_name << "; "
             << "mode=" << convert_trigger_type_to_string(trig_cfg.type)
             << "; compare_operator="
             << convert_trigger_event_to_string(trig_cfg.event)
             << "; compare_value=0x" << std::hex << trig_cfg.value << std::dec
             << "; compare_value_width=" << trig_cfg.value_compare_width
             << std::endl;
          break;
        case TRIGGER_NONE:
          ss << "    Channel " << ch << "        : "
             << convert_trigger_type_to_string(trig_cfg.type) << std::endl;
          break;
      }
    }

    if (m_session->is_loaded() && is_valid) {
      ss << "  Probe Table" << std::endl;

      auto probes = get_probe_info(base_addr);
      if (!probes.empty()) {
        ss << "  "
              "+-------+-------------------------------------+--------------+--"
              "------------+"
           << std::endl
           << "  | Index | Probe name                          | Bit pos      "
              "| No. of bits  |"
           << std::endl
           << "  "
              "+-------+-------------------------------------+--------------+--"
              "------------+"
           << std::endl;

        uint32_t i = 1;
        uint32_t bitoffset = 0;

        for (auto p : probes) {
          ss << "  | " << std::left << std::setw(5) << i << " | "
             << std::setw(35) << p.signal_name << " | " << std::setw(12)
             << bitoffset << " | " << std::setw(12) << p.bitwidth << " | "
             << std::right << std::endl;
          ++i;
          bitoffset += p.bitwidth;
        }

        ss << "  "
              "+-------+-------------------------------------+--------------+--"
              "------------+"
           << std::endl;
      } else {
        ss << "   No probe info" << std::endl;
      }
    }
    ++count;
  }

  if (count == 0) ss << "No OCLA IP detected" << std::endl;
  return ss.str();
}

std::string Ocla::show_session_info() {
  std::ostringstream ss;

  if (!m_session->is_loaded()) {
    CFG_POST_ERR("No session is loaded");
    return "";
  }

  ss << "Session Info" << std::endl;

  for (uint32_t i = 0; i < m_session->get_instance_count(); i++) {
    auto ocla = m_session->get_instance_info(i);
    ss << "  OCLA " << (i + 1) << std::endl
       << "    base_addr " << std::hex << std::showbase << ocla.base_addr
       << std::endl
       << "    type '" << ocla.type << "'" << std::endl
       << "    version " << std::hex << std::showbase << ocla.version
       << std::endl
       << "    id " << std::hex << std::showbase << ocla.id << std::endl
       << "    depth " << std::dec << ocla.depth << std::endl
       << "    num_probes " << std::dec << ocla.num_probes << std::endl
       << "    num_trigger_inputs " << std::dec << ocla.num_trigger_inputs
       << std::endl
       << "    num_probe_width " << std::dec << ocla.probe_width << std::endl
       << "    axi_data_width " << std::dec << ocla.axi_addr_width << std::endl
       << "    axi_data_width " << std::dec << ocla.axi_addr_width << std::endl
       << "    Probes" << std::endl;
    auto probes = m_session->get_probe_info(i);
    uint32_t n = 1;
    for (const auto& p : probes) {
      ss << "      Probe " << n << std::endl
         << "        name = '" << p.signal_name << "'" << std::endl
         << "        bitwidth = " << p.bitwidth << std::endl
         << "        value = " << p.value << std::endl
         << "        type = " << p.type << std::endl;
      ++n;
    }
  }

  return ss.str();
}

std::string Ocla::dump_registers(uint32_t instance) {
  std::map<uint32_t, std::string> regs = {
      {OCSR, "OCSR"},
      //{TCUR0, "TCUR0"},
      {TMTR, "TMTR"},
      //{TDCR, "TDCR"},
      // {TCUR1, "TCUR1"},
      {IP_TYPE, "IP_TYPE"},
      {IP_VERSION, "IP_VERSION"},
      {IP_ID, "IP_ID"},
  };

  OclaIP ocla_ip = get_ocla_instance(instance);
  std::ostringstream ss;
  char buffer[100];

  for (auto const& [offset, name] : regs) {
    uint32_t regaddr = ocla_ip.get_base_addr() + offset;
    snprintf(buffer, sizeof(buffer), "%-10s (0x%08x) = 0x%08x\n", name.c_str(),
             regaddr, m_adapter->read(regaddr));
    ss << buffer;
  }

  return ss.str();
}

std::string Ocla::dump_samples(uint32_t instance, bool dumpText,
                               bool generate_waveform) {
  OclaIP ocla_ip = get_ocla_instance(instance);
  std::ostringstream ss;
  char buffer[100];
  auto data = ocla_ip.get_data();

  if (dumpText) {
    ss << "width " << data.width << " depth " << data.depth << " num_reads "
       << data.num_reads << " length " << data.values.size() << std::endl;
    for (auto& value : data.values) {
      snprintf(buffer, sizeof(buffer), "0x%08x\n", value);
      ss << buffer;
    }
  }

  if (generate_waveform) {
    std::string filepath = "/tmp/ocla_debug.fst";
    m_writer->set_width(data.width);
    m_writer->set_depth(data.depth);
    m_writer->set_signals(generate_signal_descriptor(data.width));
    m_writer->write(data.values, filepath.c_str());
    ss << "Waveform written to " << filepath << std::endl;
  }

  return ss.str();
}

void Ocla::debug_start(uint32_t instance) {
  OclaIP ocla_ip = get_ocla_instance(instance);
  ocla_ip.start();
}

std::string Ocla::show_status(uint32_t instance) {
  OclaIP ocla_ip = get_ocla_instance(instance);
  std::ostringstream ss;
  ss << (uint32_t)ocla_ip.get_status();
  return ss.str();
}

void Ocla::start_session(std::string bitasm_filepath) {
  if (m_session->is_loaded()) {
    CFG_POST_ERR("Session is already loaded");
    return;
  }

  if (!std::filesystem::exists(bitasm_filepath)) {
    CFG_POST_ERR("File %s not found", bitasm_filepath.c_str());
    return;
  }

  m_session->load(bitasm_filepath);
  uint32_t count = m_session->get_instance_count();
  if (count > OCLA_MAX_INSTANCE_COUNT) {
    CFG_POST_ERR(
        "Found %u OCLA instances in bit assember file but expect max of 2 "
        "instances",
        count);
    m_session->unload();
  }
}

void Ocla::stop_session() {
  if (!m_session->is_loaded()) {
    CFG_POST_ERR("No session is loaded");
    return;
  }
  m_session->unload();
}
