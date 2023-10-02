#include "OpenocdJtagAdapter.h"

#include <cassert>
#include <iomanip>
#include <regex>
#include <sstream>
#include <vector>

OpenocdJtagAdapter::OpenocdJtagAdapter(std::string filepath,
                                       ExecFuncType cmdexec, Cable *cable)
    : m_filepath(filepath),
      m_cmdexec(cmdexec),
      m_id(0x10000db3),
      m_irlen(5),
      m_speedKhz(1000),
      m_cable(cable) {}

void OpenocdJtagAdapter::write(uint32_t addr, uint32_t data) {
  // ocla jtag write via openocd command
  // -----------------------------------
  // irscan ocla 0x04
  // drscan ocla 1 0x1 1 0x1 32 <addr> 32 <data> 2 0x0
  // irscan ocla 0x08
  // drscan ocla 32 0x0 2 0x0

  std::string output;
  std::stringstream ss;

  ss << " -c \"irscan ocla.tap 0x04\""
     << " -c \"drscan ocla.tap 1 0x1 1 0x1 32 " << std::hex << std::showbase
     << addr << " 32 " << data << " 2 0x0\""
     << " -c \"irscan ocla.tap 0x08\""
     << " -c \"drscan ocla.tap 32 0x0 2 0x0\"";

  if (executeCommand(ss.str(), output) != 0) {
    throw std::runtime_error("write(), cmdexec error: " + output);
  }

  try {
    parse(output);
  } catch (const std::exception &e) {
    throw std::runtime_error(std::string("write(), ") + e.what());
  }
}

uint32_t OpenocdJtagAdapter::read(uint32_t addr) {
  auto values = read(addr, 1);
  return values[0];
}

std::vector<uint32_t> OpenocdJtagAdapter::read(uint32_t base_addr,
                                               uint32_t num_reads,
                                               uint32_t increase_by) {
  // ocla jtag read via openocd command
  // ----------------------------------
  // irscan ocla 0x04
  // drscan ocla 1 0x1 1 0x0 32 <addr> 32 0x0 2 0x0
  // irscan ocla 0x08
  // drscan ocla 32 <data> 2 0x0

  std::string output;
  std::stringstream ss;

  for (uint32_t i = 0; i < num_reads; i++) {
    ss << " -c \"irscan ocla.tap 0x04\""
       << " -c \"drscan ocla.tap 1 0x1 1 0x0 32 " << std::hex << std::showbase
       << base_addr << " 32 0x0 2 0x0\""
       << " -c \"irscan ocla.tap 0x08\""
       << " -c \"drscan ocla.tap 32 0x0 2 0x0\"";
    base_addr += increase_by;
  }

  if (executeCommand(ss.str(), output) != 0) {
    throw std::runtime_error("read(), cmdexec error: " + output);
  }

  try {
    auto values = parse(output);
    if (values.size() != num_reads) {
      throw std::runtime_error("values size is not equal to read requests");
    }
    return values;
  } catch (const std::exception &e) {
    throw std::runtime_error(std::string("read(), ") + e.what());
  }
}

void OpenocdJtagAdapter::setId(uint32_t id) { m_id = id; }

void OpenocdJtagAdapter::setSpeedKhz(uint32_t speedKhz) {
  m_speedKhz = speedKhz;
}

std::stringstream OpenocdJtagAdapter::buildCommand(const std::string &cmd) {
  std::stringstream ss;

  ss << " -l /dev/stdout"  //<-- not windows friendly
     << " -d2"
     << " -c \"adapter driver ftdi\""
     << " -c \"ftdi vid_pid 0x0403 0x6014\""
     << " -c \"ftdi layout_init 0x0c08 0x0f1b\""
     << " -c \"adapter speed " << m_speedKhz << "\""
     << " -c \"transport select jtag\""
     << " -c \"jtag newtap ocla tap -irlen " << m_irlen << " -expected-id "
     << std::hex << std::showbase << m_id << "\""
     << " -c \"target create ocla testee -chain-position ocla.tap\""
     << " -c \"init\"" << cmd << " -c \"exit\"";

  return ss;
}

int OpenocdJtagAdapter::executeCommand(const std::string &cmd,
                                       std::string &output) {
  std::atomic<bool> stop = false;
  int res = m_cmdexec(
      "OPENOCD_DEBUG_LEVEL=-3 " + m_filepath + buildCommand(cmd).str(), output,
      nullptr, stop);
  return res;
}

std::vector<uint32_t> OpenocdJtagAdapter::parse(const std::string &output) {
  std::stringstream ss(output);
  std::string s;
  std::cmatch matches;
  std::vector<uint32_t> values;

  while (std::getline(ss, s)) {
    // look for text format (in hex): xxxxxxxx yy
    if (std::regex_search(s.c_str(), matches,
                          std::regex("^([0-9A-F]{8}) ([0-9A-F]{2})$",
                                     std::regex::icase)) == true) {
      int status = stoi(matches[2], nullptr, 16);
      if (status != 0) {
        throw std::runtime_error("ocla error");
      }
      values.push_back((uint32_t)stoul(matches[1], nullptr, 16));
    }
  }

  if (values.empty()) {
    throw std::runtime_error("empty result");
  }

  return values;
}
