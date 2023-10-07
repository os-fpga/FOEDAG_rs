#include "Ocla.h"

#include <sstream>

#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "JtagAdapter.h"
#include "OclaIP.h"

OclaIP Ocla::getOclaInstance(uint32_t instance) {
  CFG_ASSERT_MSG(false, "Not implemented");
  return OclaIP{};
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

std::stringstream Ocla::showStatus(uint32_t instance) {
  CFG_ASSERT_MSG(false, "Not implemented");
  return std::stringstream{};
}

void Ocla::startSession(std::string bitasmFilepath) {
  CFG_ASSERT_MSG(false, "Not implemented");
}

void Ocla::stopSession() { CFG_ASSERT_MSG(false, "Not implemented"); }
