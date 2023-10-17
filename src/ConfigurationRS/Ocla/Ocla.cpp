#include "Ocla.h"

#include <sstream>

#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "OclaHelpers.h"
#include "OclaIP.h"
#include "OclaJtagAdapter.h"
#include "OclaSession.h"
#include "OclaWaveformWriter.h"

static std::map<uint32_t, uint32_t> ocla_base_address = {{1, OCLA1_ADDR},
                                                         {2, OCLA2_ADDR}};

OclaIP Ocla::getOclaInstance(uint32_t instance) {
  CFG_ASSERT(ocla_base_address.find(instance) != ocla_base_address.end());
  OclaIP objIP{m_adapter, ocla_base_address[instance]};
  CFG_ASSERT(objIP.getType() == OCLA_TYPE);
  return objIP;
}

std::map<uint32_t, OclaIP> Ocla::detectOclaInstances() {
  std::map<uint32_t, OclaIP> list;

  for (auto& [i, baseaddr] : ocla_base_address) {
    OclaIP objIP{m_adapter, baseaddr};
    if (objIP.getType() == OCLA_TYPE) {
      list.insert(std::pair<uint32_t, OclaIP>(i, objIP));
    }
  }

  return list;
}

bool Ocla::getInstanceInfo(uint32_t base_addr,
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

std::vector<Ocla_PROBE_INFO> Ocla::getProbeInfo(uint32_t base_addr) {
  Ocla_INSTANCE_INFO inf{};
  uint32_t idx;
  if (getInstanceInfo(base_addr, inf, idx)) {
    return m_session->get_probe_info(idx);
  }
  return {};
}

bool Ocla::validate() {
  if (m_session->is_loaded()) {
    auto instances = detectOclaInstances();
    if (instances.size() != m_session->get_instance_count()) return false;

    for (const auto& [i, objIP] : instances) {
      Ocla_INSTANCE_INFO inf{};
      uint32_t idx;
      if (!getInstanceInfo(objIP.getBaseAddr(), inf, idx)) {
        return false;
      }
      if (inf.version != objIP.getVersion()) return false;
      if (inf.type != objIP.getType()) return false;
      if (inf.id != objIP.getId()) return false;
      if (inf.num_probes != objIP.getNumberOfProbes()) return false;
      if (inf.depth != objIP.getMemoryDepth()) return false;
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

void Ocla::configure(uint32_t instance, std::string mode, std::string condition,
                     uint32_t sample_size) {
  if (m_session->is_loaded()) {
    if (!validate()) {
      CFG_POST_ERR("OCLA info not matched with the detected OCLA IP");
      return;
    }
  }

  auto objIP = getOclaInstance(instance);
  auto depth = objIP.getMemoryDepth();
  if (sample_size > depth) {
    CFG_POST_ERR(
        "Invalid sample size parameter (Sample size should be beween 0 and %d)",
        depth);
    return;
  }

  ocla_config cfg;

  cfg.fns = sample_size > 0 ? ENABLED : DISABLED;
  cfg.ns = sample_size;
  cfg.mode = convertOclaMode(mode);
  cfg.condition = convertTriggerCondition(condition);

  objIP.configure(cfg);
}

void Ocla::configureChannel(uint32_t instance, uint32_t channel,
                            std::string type, std::string event, uint32_t value,
                            std::string probe) {
  if (m_session->is_loaded()) {
    if (!validate()) {
      CFG_POST_ERR("OCLA info not matched with the detected OCLA IP");
      return;
    }
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

  bool status = false;
  uint64_t probe_num = CFG_convert_string_to_u64(probe, false, &status);
  if (!status) {
    // todo: translate probe name to probe index
    CFG_POST_ERR("Probe by name is not supported for now");
    return;
  }

  auto objIP = getOclaInstance(instance);
  uint32_t max_probes = objIP.getNumberOfProbes();
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
  trig_cfg.type = convertTriggerType(type);
  trig_cfg.event = convertTriggerEvent(event);
  trig_cfg.value = value;

  objIP.configureChannel(channel - 1, trig_cfg);
}

void Ocla::start(uint32_t instance, uint32_t timeout,
                 std::string outputfilepath) {
  if (m_session->is_loaded()) {
    if (!validate()) {
      CFG_POST_ERR("OCLA info not matched with the detected OCLA IP");
      return;
    }
  }

  uint32_t countdown = timeout;
  auto objIP = getOclaInstance(instance);
  objIP.start();

  while (true) {
    // wait for 1 sec
    CFG_sleep_ms(1000);

    if (objIP.getStatus() == DATA_AVAILABLE) {
      ocla_data data = objIP.getData();
      m_writer->setWidth(data.width);
      m_writer->setDepth(data.depth);
      if (m_session->is_loaded()) {
        m_writer->setSignals(
            generateSignalDescriptor(getProbeInfo(objIP.getBaseAddr())));
      } else {
        m_writer->setSignals(generateSignalDescriptor(data.width));
      }
      m_writer->write(data.values, outputfilepath);
      break;
    }

    if (timeout > 0) {
      if (countdown == 0) {
        CFG_POST_ERR("Timeout error");
        break;
      }
      --countdown;
    }
  }
}

std::string Ocla::showInfo() {
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

  for (auto& [idx, objIP] : detectOclaInstances()) {
    uint32_t depth = objIP.getMemoryDepth();
    uint32_t base_addr = objIP.getBaseAddr();
    ss << "OCLA " << idx << std::setfill('0') << std::hex << std::endl
       << "  Base address       : 0x" << std::setw(8) << base_addr << std::endl
       << "  ID                 : 0x" << std::setw(8) << objIP.getId()
       << std::endl
       << "  Type               : '" << objIP.getType() << "'" << std::endl
       << "  Version            : 0x" << std::setw(8) << objIP.getVersion()
       << std::endl
       << "  Probes             : " << std::dec << objIP.getNumberOfProbes()
       << std::endl
       << "  Memory depth       : " << depth << std::endl
       << "  DA status          : " << objIP.getStatus() << std::endl;

    auto cfg = objIP.getConfig();

    uint32_t ns = cfg.fns == ENABLED ? cfg.ns : depth;

    ss << "  No. of samples     : " << ns << std::endl
       << "  Trigger mode       : " << convertOclaModeToString(cfg.mode)
       << std::endl
       << "  Trigger condition  : "
       << convertTriggerConditionToString(cfg.condition) << std::endl
       << "  Trigger " << std::endl
       << std::setfill(' ');

    for (uint32_t ch = 1; ch < 5; ch++) {
      auto trig_cfg = objIP.getChannelConfig(ch - 1);
      switch (trig_cfg.type) {
        case EDGE:
        case LEVEL:
          ss << "    Channel " << ch << "        : "
             << "probe=" << trig_cfg.probe_num << "; "
             << "mode=" << convertTriggerEventToString(trig_cfg.event) << "_"
             << convertTriggerTypeToString(trig_cfg.type) << std::endl;
          break;
        case VALUE_COMPARE:
          ss << "    Channel " << ch << "        : "
             << "probe=" << trig_cfg.probe_num << "; "
             << "mode=" << convertTriggerTypeToString(trig_cfg.type)
             << "; compare_operator="
             << convertTriggerEventToString(trig_cfg.event)
             << "; compare_value=0x" << std::hex << trig_cfg.value << std::dec
             << std::endl;
          break;
        case TRIGGER_NONE:
          ss << "    Channel " << ch << "        : "
             << convertTriggerTypeToString(trig_cfg.type) << std::endl;
          break;
      }
    }

    if (m_session->is_loaded() == true & is_valid == true) {
      ss << "  Probe Table" << std::endl;

      auto probes = getProbeInfo(base_addr);
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

std::string Ocla::showSessionInfo() {
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

std::string Ocla::dumpRegisters(uint32_t instance) {
  std::map<uint32_t, std::string> regs = {
      {OCSR, "OCSR"},
      {TCUR0, "TCUR0"},
      {TMTR, "TMTR"},
      {TDCR, "TDCR"},
      {TCUR1, "TCUR1"},
      {IP_TYPE, "IP_TYPE"},
      {IP_VERSION, "IP_VERSION"},
      {IP_ID, "IP_ID"},
  };

  OclaIP objIP = getOclaInstance(instance);
  std::ostringstream ss;
  char buffer[100];

  for (auto const& [offset, name] : regs) {
    uint32_t regaddr = objIP.getBaseAddr() + offset;
    snprintf(buffer, sizeof(buffer), "%-10s (0x%08x) = 0x%08x\n", name.c_str(),
             regaddr, m_adapter->read(regaddr));
    ss << buffer;
  }

  return ss.str();
}

std::string Ocla::dumpSamples(uint32_t instance, bool dumpText,
                              bool generateWaveform) {
  OclaIP objIP = getOclaInstance(instance);
  std::ostringstream ss;
  char buffer[100];
  auto data = objIP.getData();

  if (dumpText) {
    ss << "width " << data.width << " depth " << data.depth << " num_reads "
       << data.num_reads << " length " << data.values.size() << std::endl;
    for (auto& value : data.values) {
      snprintf(buffer, sizeof(buffer), "0x%08x\n", value);
      ss << buffer;
    }
  }

  if (generateWaveform) {
    std::string filepath = "/tmp/ocla_debug.fst";
    m_writer->setWidth(data.width);
    m_writer->setDepth(data.depth);
    m_writer->setSignals(generateSignalDescriptor(data.width));
    m_writer->write(data.values, filepath.c_str());
    ss << "Waveform written to " << filepath << std::endl;
  }

  return ss.str();
}

void Ocla::debugStart(uint32_t instance) {
  OclaIP objIP = getOclaInstance(instance);
  objIP.start();
}

std::string Ocla::showStatus(uint32_t instance) {
  OclaIP objIP = getOclaInstance(instance);
  std::ostringstream ss;
  ss << (uint32_t)objIP.getStatus();
  return ss.str();
}

void Ocla::startSession(std::string bitasmFilepath) {
  if (m_session->is_loaded()) {
    CFG_POST_ERR("Session is already loaded");
    return;
  }

  if (!std::filesystem::exists(bitasmFilepath)) {
    CFG_POST_ERR("File %s not found", bitasmFilepath.c_str());
    return;
  }

  m_session->load(bitasmFilepath);
  uint32_t count = m_session->get_instance_count();
  if (count > OCLA_MAX_INSTANCE_COUNT) {
    CFG_POST_ERR(
        "Found %u OCLA instances in bit assember file but expect max of 2 "
        "instances",
        count);
    m_session->unload();
  }
}

void Ocla::stopSession() {
  if (!m_session->is_loaded()) {
    CFG_POST_ERR("No session is loaded");
    return;
  }
  m_session->unload();
}
