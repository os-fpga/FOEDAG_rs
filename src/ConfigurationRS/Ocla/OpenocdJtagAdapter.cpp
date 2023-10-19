#include "OpenocdJtagAdapter.h"

#include <cassert>
#include <iomanip>
#include <regex>
#include <sstream>
#include <vector>

#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"

OpenocdJtagAdapter::OpenocdJtagAdapter(std::string filepath,
                                       ExecFuncType cmdexec)
    : m_filepath(filepath), m_cmdexec(cmdexec), m_device{}, m_taplist{} {
  CFG_ASSERT(m_cmdexec != nullptr);
}

OpenocdJtagAdapter::~OpenocdJtagAdapter() {}

void OpenocdJtagAdapter::write(uint32_t addr, uint32_t data) {
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
     << std::showbase << i << " 32 " << data << " 2 0x0;" << std::dec
     << std::noshowbase << "irscan ocla" << i << ".tap 0x08;"
     << "drscan ocla" << i << ".tap 32 0x0 2 0x0;\"";

  CFG_ASSERT_MSG(execute_command(ss.str(), output) == 0, "cmdexec error: %s",
                 output.c_str());
  parse(output);
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

std::string OpenocdJtagAdapter::build_command(const std::string &cmd) {
  std::ostringstream ss;

  ss << " -l /dev/stdout"  //<-- not windows friendly
     << " -d2";

  // setup cable configuration
  if (m_device.cable.cable_type == FTDI) {
    ss << " -c \"adapter driver ftdi;"
       << "ftdi vid_pid " << std::hex << std::showbase
       << m_device.cable.vendor_id << " " << m_device.cable.product_id << ";"
       << std::noshowbase << std::dec << "ftdi layout_init 0x0c08 0x0f1b;\"";
  } else if (m_device.cable.cable_type == JLINK) {
    ss << " -c \"adapter driver jlink;\"";
  }

  // setup general cable configuration
  ss << " -c \"adapter speed " << m_device.cable.speed << ";"
     << "transport select jtag;"
     << "telnet_port disabled;"
     << "gdb_port disabled;\"";

  // setup tap configuration
  if (!m_taplist.empty()) {
    ss << " -c \"";
    for (const auto &tap : m_taplist) {
      ss << "jtag newtap ocla" << tap.index << " tap"
         << " -irlen " << tap.irlength << " -expected-id " << std::hex
         << std::showbase << tap.idcode << ";" << std::noshowbase << std::dec;
    }
    ss << "\"";
  }

  // setup target configuration
  ss << " -c \"target create ocla testee -chain-position ocla"
     << m_device.tap.index << ".tap;\"";

  ss << " -c \"init\"" << cmd << " -c \"exit\"";

  return ss.str();
}

int OpenocdJtagAdapter::execute_command(const std::string &cmd,
                                        std::string &output) {
  std::atomic<bool> stop = false;
  int res =
      m_cmdexec("OPENOCD_DEBUG_LEVEL=-3 " + m_filepath + build_command(cmd),
                output, nullptr, stop);
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
      CFG_ASSERT_MSG(stoi(matches[2], nullptr, 16) == 0, "ocla error");
      values.push_back((uint32_t)stoul(matches[1], nullptr, 16));
    }
  }

  CFG_ASSERT_MSG(values.empty() == false, "empty result");
  return values;
}

void OpenocdJtagAdapter::set_target_device(Device device,
                                           std::vector<Tap> taplist) {
  m_device = device;
  m_taplist = taplist;
};
