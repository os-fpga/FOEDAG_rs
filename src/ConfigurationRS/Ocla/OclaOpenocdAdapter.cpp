#include "OclaOpenocdAdapter.h"

#include <cassert>
#include <iomanip>
#include <regex>
#include <sstream>
#include <vector>

#include "Configuration/HardwareManager/OpenocdHelper.h"
#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"

OclaOpenocdAdapter::OclaOpenocdAdapter(std::string openocd)
    : FOEDAG::OpenocdAdapter(openocd), m_openocd(openocd) {}

OclaOpenocdAdapter::~OclaOpenocdAdapter() {}

void OclaOpenocdAdapter::write(uint32_t addr, uint32_t data) {
  // ocla jtag write via openocd command
  // -----------------------------------
  // irscan ocla 0x04
  // drscan ocla 1 0x1 1 0x1 32 <addr> 32 <data> 2 0x0
  // irscan ocla 0x08
  // drscan ocla 32 0x0 2 0x0

  std::string output;
  std::stringstream ss;
  uint32_t i = m_device.tap.index;

  ss << " -c \"irscan ocla" << i << ".tap 0x04;"
     << "drscan ocla" << i << ".tap 1 0x1 1 0x1 32 " << std::hex
     << std::showbase << addr << " 32 " << data << " 2 0x0;" << std::dec
     << std::noshowbase << "irscan ocla" << i << ".tap 0x08;"
     << "drscan ocla" << i << ".tap 32 0x0 2 0x0;\"";

  CFG_ASSERT_MSG(execute_command(ss.str(), output) == 0, "cmdexec error: %s",
                 output.c_str());
  parse(output);
}

uint32_t OclaOpenocdAdapter::read(uint32_t addr) {
  auto values = read(addr, 1);
  return values[0];
}

std::vector<uint32_t> OclaOpenocdAdapter::read(uint32_t base_addr,
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
  uint32_t j = m_device.tap.index;

  for (uint32_t i = 0; i < num_reads; i++) {
    ss << " -c \"irscan ocla" << j << ".tap 0x04;"
       << "drscan ocla" << j << ".tap 1 0x1 1 0x0 32 " << std::hex
       << std::showbase << base_addr << " 32 0x0 2 0x0;" << std::dec
       << std::noshowbase << "irscan ocla" << j << ".tap 0x08;"
       << "drscan ocla" << j << ".tap 32 0x0 2 0x0;\"";
    base_addr += increase_by;
  }

  CFG_ASSERT_MSG(execute_command(ss.str(), output) == 0, "cmdexec error: %s",
                 output.c_str());
  auto values = parse(output);
  CFG_ASSERT_MSG(values.size() == num_reads,
                 "values size is not equal to read requests");
  return values;
}

int OclaOpenocdAdapter::execute_command(const std::string &cmd,
                                        std::string &output) {
  std::atomic<bool> stop = false;
  std::ostringstream ss;

  ss << " -l /dev/stdout"  //<-- not windows friendly
     << " -d2";

  ss << build_cable_config(m_device.cable) << build_tap_config(m_taplist)
     << build_target_config(m_device);
  ss << " -c \"init\"";
  ss << " -c \"" << cmd << "\"";
  ss << " -c \"exit\"";

  int res = CFG_execute_cmd("OPENOCD_DEBUG_LEVEL=-3 " + m_openocd + cmd, output,
                            nullptr, stop);
  return res;
}

std::vector<uint32_t> OclaOpenocdAdapter::parse(const std::string &output) {
  std::stringstream ss(output);
  std::string s;
  std::cmatch matches;
  std::vector<uint32_t> values;

  while (std::getline(ss, s)) {
    // look for text format (in hex): xxxxxxxx yy
    if (std::regex_search(s.c_str(), matches,
                          std::regex("^([0-9A-F]{8}) ([0-9A-F]{2})$",
                                     std::regex::icase)) == true) {
      CFG_ASSERT_MSG(stoi(matches[2], nullptr, 16) == 0, "ocla error");
      values.push_back((uint32_t)stoul(matches[1], nullptr, 16));
    }
  }

  CFG_ASSERT_MSG(values.empty() == false, "empty result");
  return values;
}

void OclaOpenocdAdapter::set_target_device(FOEDAG::Device device,
                                           std::vector<FOEDAG::Tap> taplist) {
  m_device = device;
  m_taplist = taplist;
}
