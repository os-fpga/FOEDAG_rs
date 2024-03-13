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

  ss << " -c \"set addr [format 0x%08x " << addr << "];"
     << "set data [format 0x%08x " << data << "];"
     << "irscan tap" << i << ".tap 0x04;"
     << "drscan tap" << i << ".tap 1 0x1 1 0x1 32 \\$addr 32 \\$data 2 0x0;"
     << "irscan tap" << i << ".tap 0x08;"
     << "set res [drscan tap" << i << ".tap 32 0x0 2 0x0];"
     << "echo \\\"\\$addr \\$res\\\";\"";

  CFG_ASSERT_MSG(execute_command(ss.str(), output) == 0, "cmdexec error: %s",
                 output.c_str());
  parse(output);
}

uint32_t OclaOpenocdAdapter::read(uint32_t addr) {
  auto result = read(addr, 1);
  return result[0].data;
}

std::vector<jtag_read_result> OclaOpenocdAdapter::read(uint32_t base_addr,
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

  ss << build_tcl_proc(m_device.index) << " -c \"ocla_read " << std::hex
     << std::showbase << base_addr << std::dec << std::noshowbase << " "
     << num_reads << " " << increase_by << ";\"";

  CFG_ASSERT_MSG(execute_command(ss.str(), output) == 0, "cmdexec error: %s",
                 output.c_str());
  auto values = parse(output);
  CFG_ASSERT_MSG(values.size() == num_reads,
                 "values size is not equal to read requests");
  return values;
}

std::string OclaOpenocdAdapter::build_tcl_proc(uint32_t tap_index) {
  std::ostringstream ss;
  ss << " -c \"proc ocla_read {addr {n 1} {c 0}} { for {set i 0} {\\$i < \\$n} "
        "{incr i} {; set addr [format 0x%08x \\$addr]; irscan tap"
     << tap_index << ".tap 0x04; drscan tap" << tap_index
     << ".tap 1 0x1 1 0x0 32 \\$addr 32 0x0 2 0x0; irscan tap" << tap_index
     << ".tap 0x08; set res [drscan tap" << tap_index
     << ".tap 32 0x0 2 0x0]; echo \\\"\\$addr \\$res\\\"; incr addr \\$c; }; "
        "};\"";
  return ss.str();
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
  ss << cmd;
  ss << " -c \"exit\"";

  int res = CFG_execute_cmd("OPENOCD_DEBUG_LEVEL=-3 " + m_openocd + ss.str(),
                            output, nullptr, stop);
  return res;
}

std::vector<jtag_read_result> OclaOpenocdAdapter::parse(
    const std::string &output) {
  std::stringstream ss(output);
  std::string s;
  std::cmatch matches;
  std::vector<jtag_read_result> values;

  while (std::getline(ss, s)) {
    // look for text format (in hex): 0xNNNNNNNN xxxxxxxx yy
    if (std::regex_search(
            s.c_str(), matches,
            std::regex("^0x([0-9A-F]{8}) ([0-9A-F]{8}) ([0-9A-F]{2})$",
                       std::regex::icase)) == true) {
      jtag_read_result res{};
      res.address = (uint32_t)stoul(matches[1], 0, 16);
      res.data = (uint32_t)stoul(matches[2], 0, 16);
      res.status = (uint32_t)stoul(matches[3], 0, 16);
      values.push_back(res);
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
