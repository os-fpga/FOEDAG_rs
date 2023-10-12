#include "Ocla.h"

#include <sstream>

#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "OclaHelpers.h"
#include "OclaIP.h"
#include "OclaJtagAdapter.h"
#include "WaveformWriter.h"

OclaIP Ocla::getOclaInstance(uint32_t instance) {
  OclaIP objIP{m_adapter, instance == 1 ? OCLA1_ADDR : OCLA2_ADDR};
  CFG_ASSERT(objIP.getType() == OCLA_TYPE);
  return objIP;
}

std::map<uint32_t, OclaIP> Ocla::detectOclaInstances() {
  std::map<uint32_t, OclaIP> list;
  uint32_t i = 1;

  for (auto& baseaddr : {OCLA1_ADDR, OCLA2_ADDR}) {
    OclaIP objIP{m_adapter, baseaddr};
    if (objIP.getType() == OCLA_TYPE) {
      list.insert(std::pair<uint32_t, OclaIP>(i, objIP));
    }
    ++i;
  }

  return list;
}

Ocla::Ocla(OclaJtagAdapter* adapter, WaveformWriter* writer)
    : m_adapter(adapter), m_writer(writer) {}

void Ocla::configure(uint32_t instance, std::string mode, std::string condition,
                     uint32_t sample_size) {
  auto objIP = getOclaInstance(instance);

  auto depth = objIP.getMemoryDepth();
  CFG_ASSERT_MSG(
      sample_size <= depth,
      ("Invalid sample size parameter (Sample size should be beween 0 and " +
       std::to_string(depth) + ")")
          .c_str());

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
  if (type == "edge") {
    CFG_ASSERT_MSG(event == "rising" || event == "falling" || event == "either",
                   "Invalid event parameter for edge trigger");
  } else if (type == "value_compare") {
    CFG_ASSERT_MSG(event == "equal" || event == "lesser" || event == "greater",
                   "Invalid event parameter for value compare trigger");
  } else if (type == "level") {
    CFG_ASSERT_MSG(event == "high" || event == "low",
                   "Invalid event parameter for level trigger");
  }

  bool status = false;
  uint64_t probe_num = CFG_convert_string_to_u64(probe, false, &status);
  if (!status) {
    // todo: translate probe name to probe index
    CFG_ASSERT_MSG(false, "Probe by name is not supported for now");
  }

  auto objIP = getOclaInstance(instance);
  uint32_t max_probes = objIP.getNumberOfProbes();
  if (max_probes > OCLA_MAX_PROBE) {
    max_probes = OCLA_MAX_PROBE;
  }

  CFG_ASSERT_MSG(
      probe_num < max_probes,
      ("Invalid probe parameter (Probe number should be between 0 and " +
       std::to_string(max_probes - 1) + ")")
          .c_str());

  ocla_trigger_config trig_cfg;

  trig_cfg.probe_num = (uint32_t)probe_num;
  trig_cfg.type = convertTriggerType(type);
  trig_cfg.event = convertTriggerEvent(event);
  trig_cfg.value = value;

  objIP.configureChannel(channel - 1, trig_cfg);
}

void Ocla::start(uint32_t instance, uint32_t timeout,
                 std::string outputfilepath) {
  uint32_t countdown = timeout;

  CFG_ASSERT(m_writer != nullptr);
  auto objIP = getOclaInstance(instance);
  objIP.start();

  while (true) {
    // wait for 1 sec
    CFG_sleep_ms(1000);

    if (objIP.getStatus() == DATA_AVAILABLE) {
      ocla_data data = objIP.getData();
      m_writer->setWidth(data.width);
      m_writer->setDepth(data.depth);
      m_writer->setSignals(generateSignalDescriptor(data.width));
      m_writer->write(data.values, outputfilepath);
      break;
    }

    if (timeout > 0) {
      CFG_ASSERT_MSG(countdown > 0, "Timeout error");
      --countdown;
    }
  }
}

std::string Ocla::showInfo() {
  std::ostringstream ss;

  for (auto& [index, objIP] : detectOclaInstances()) {
    uint32_t depth = objIP.getMemoryDepth();
    ss << "OCLA " << index << std::setfill('0') << std::hex << std::endl
       << "  Base address       : 0x" << std::setw(8) << objIP.getBaseAddr()
       << std::endl
       << "  ID                 : 0x" << std::setw(8) << objIP.getId()
       << std::endl
       << "  Type               : 0x" << std::setw(8) << objIP.getType()
       << std::endl
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
    CFG_ASSERT(m_writer != nullptr);
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
  CFG_ASSERT_MSG(false, "Not implemented");
}

void Ocla::stopSession() { CFG_ASSERT_MSG(false, "Not implemented"); }
