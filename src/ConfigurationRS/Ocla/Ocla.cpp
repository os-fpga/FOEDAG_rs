#include "Ocla.h"

#include <sstream>

#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "JtagAdapter.h"
#include "OclaIP.h"

OclaIP Ocla::getOclaInstance(uint32_t instance) {
  OclaIP objIP{m_adapter, instance == 1 ? OCLA1_ADDR : OCLA2_ADDR};
  CFG_ASSERT(objIP.getType() == OCLA_TYPE);
  return objIP;
}

std::map<uint32_t, OclaIP> Ocla::detect() {
  CFG_ASSERT_MSG(false, "Not implemented");
  return std::map<uint32_t, OclaIP>{};
}

void Ocla::configure(uint32_t instance, std::string mode, std::string cond,
                     std::string sample_size) {
  CFG_ASSERT_MSG(false, "Not implemented");
}

void Ocla::configureChannel(uint32_t instance, uint32_t channel,
                            std::string type, std::string event, uint32_t value,
                            std::string probe) {
  CFG_ASSERT_MSG(false, "Not implemented");
}

void Ocla::start(uint32_t instance, uint32_t timeout,
                 std::string outputfilepath) {
  CFG_ASSERT_MSG(false, "Not implemented");
}

std::stringstream Ocla::showInfo() {
  CFG_ASSERT_MSG(false, "Not implemented");
  return std::stringstream{};
}

std::stringstream Ocla::dumpRegisters(uint32_t instance) {
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
  std::stringstream ss;
  char buffer[100];

  for (auto const& [offset, name] : regs) {
    uint32_t regaddr = objIP.getBaseAddr() + offset;
    sprintf(buffer, "%-10s (0x%08x) = 0x%08x\n", name.c_str(), regaddr,
            m_adapter->read(regaddr));
    ss << buffer;
  }

  return ss;
}

std::stringstream Ocla::dumpSamples(uint32_t instance) {
  OclaIP objIP = getOclaInstance(instance);
  std::stringstream ss;
  char buffer[100];
  auto data = objIP.getData();

  ss << "width " << data.width << " depth " << data.depth << " num_reads "
     << data.num_reads << " length " << data.values.size() << std::endl;

  for (auto& value : data.values) {
    sprintf(buffer, "0x%08x\n", value);
    ss << buffer;
  }

  return ss;
}

std::stringstream Ocla::showStatus(uint32_t instance) {
  CFG_ASSERT_MSG(false, "Not implemented");
  return std::stringstream{};
}

void Ocla::startSession(std::string bitasmFilepath) {
  CFG_ASSERT_MSG(false, "Not implemented");
}

void Ocla::stopSession() { CFG_ASSERT_MSG(false, "Not implemented"); }
