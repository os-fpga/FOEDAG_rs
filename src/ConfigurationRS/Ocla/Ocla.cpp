#include "Ocla.h"

#include "JtagAdapter.h"
#include "OclaException.h"
#include "OclaIP.h"

OclaIP Ocla::getOclaInstance(uint32_t instance) {
  throw OclaException(NOT_IMPLEMENTED);
}

std::map<uint32_t, OclaIP> Ocla::detect() {
  throw OclaException(NOT_IMPLEMENTED);
}

void Ocla::configure(uint32_t instance, std::string mode, std::string cond,
                     std::string sample_size) {
  throw OclaException(NOT_IMPLEMENTED);
}

void Ocla::configureChannel(uint32_t instance, uint32_t channel,
                            std::string type, std::string event, uint32_t value,
                            std::string probe) {
  throw OclaException(NOT_IMPLEMENTED);
}

void Ocla::start(uint32_t instance, uint32_t timeout,
                 std::string outputfilepath) {
  throw OclaException(NOT_IMPLEMENTED);
}

void Ocla::showInfo(std::stringstream& ss) {
  throw OclaException(NOT_IMPLEMENTED);
}

void Ocla::showStatus(uint32_t instance, std::stringstream& ss) {
  throw OclaException(NOT_IMPLEMENTED);
}

void Ocla::startSession(std::string bitasmFilepath) {
  throw OclaException(NOT_IMPLEMENTED);
}

void Ocla::stopSession() { throw OclaException(NOT_IMPLEMENTED); }
