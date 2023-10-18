#include "OpenocdJtagAdapter.h"

#include <cassert>
#include <iomanip>
#include <regex>
#include <sstream>
#include <vector>

#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"

OpenocdJtagAdapter::OpenocdJtagAdapter(std::string filepath,
                                       ExecFuncType cmdexec)
    : m_filepath(filepath),
      m_cmdexec(cmdexec),
      m_cable(nullptr),
      m_device(nullptr),
      m_taps{} {
  CFG_ASSERT(m_cmdexec != nullptr);
}

OpenocdJtagAdapter::~OpenocdJtagAdapter() {
  m_taps.clear();
  delete m_cable;
  delete m_device;
}

void OpenocdJtagAdapter::write(uint32_t addr, uint32_t data) {
  // ocla jtag write via openocd command
  // -----------------------------------
  // irscan ocla 0x04
  // drscan ocla 1 0x1 1 0x1 32 <addr> 32 <data> 2 0x0
  // irscan ocla 0x08
  // drscan ocla 32 0x0 2 0x0

  CFG_ASSERT(m_device != nullptr);

  std::string output;
  std::stringstream ss;
  uint32_t j = m_device->tap.index;

  ss << " -c \"irscan ocla" << j << ".tap 0x04;"
     << "drscan ocla" << j << ".tap 1 0x1 1 0x1 32 " << std::hex
     << std::showbase << addr << " 32 " << data << " 2 0x0;" << std::dec
     << std::noshowbase << "irscan ocla" << j << ".tap 0x08;"
     << "drscan ocla" << j << ".tap 32 0x0 2 0x0;\"";

  CFG_ASSERT_MSG(
      execute_command(ss.str(), output, m_cable, m_device, m_taps) == 0,
      "cmdexec error: %s", output.c_str());
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

  CFG_ASSERT(m_device != nullptr);

  std::string output;
  std::stringstream ss;
  uint32_t j = m_device->tap.index;

  for (uint32_t i = 0; i < num_reads; i++) {
    ss << " -c \"irscan ocla" << j << ".tap 0x04;"
       << "drscan ocla" << j << ".tap 1 0x1 1 0x0 32 " << std::hex
       << std::showbase << base_addr << " 32 0x0 2 0x0;" << std::dec
       << std::noshowbase << "irscan ocla" << j << ".tap 0x08;"
       << "drscan ocla" << j << ".tap 32 0x0 2 0x0;\"";
    base_addr += increase_by;
  }

  CFG_ASSERT_MSG(
      execute_command(ss.str(), output, m_cable, m_device, m_taps) == 0,
      "cmdexec error: %s", output.c_str());
  auto values = parse(output);
  CFG_ASSERT_MSG(values.size() == num_reads,
                 "values size is not equal to read requests");
  return values;
}

std::string OpenocdJtagAdapter::build_command(const std::string &cmd,
                                              const Cable *cable,
                                              const Device *device,
                                              const std::vector<Tap> taps) {
  std::ostringstream ss;

  CFG_ASSERT(cable != nullptr);

  ss << " -l /dev/stdout"  //<-- not windows friendly
     << " -d2";

  // setup cable configuration
  if (cable->cable_type == FTDI) {
    ss << " -c \"adapter driver ftdi;"
       << "ftdi vid_pid " << std::hex << std::showbase << cable->vendor_id
       << " " << cable->product_id << ";"
       << "ftdi layout_init 0x0c08 0x0f1b;\"";
  } else if (cable->cable_type == JLINK) {
    ss << " -c \"adapter driver jlink;\"";
  }

  // setup general cable configuration
  ss << " -c \"adapter speed " << cable->speed << ";"
     << "transport select jtag;"
     << "telnet_port disabled;"
     << "gdb_port disabled;\"";

  // setup tap configuration
  if (!taps.empty()) {
    ss << " -c \"";
    for (const auto &tap : taps) {
      ss << "jtag newtap ocla" << tap.index << " tap"
         << " -irlen " << tap.irlength << " -expected-id " << std::hex
         << std::showbase << tap.idcode << ";" << std::dec;
    }
    ss << "\"";
  }

  // setup target configuration
  if (device != nullptr) {
    ss << " -c \"target create ocla testee -chain-position ocla"
       << device->tap.index << ".tap;\"";
  }

  ss << " -c \"init\"" << cmd << " -c \"exit\"";

  return ss.str();
}

int OpenocdJtagAdapter::execute_command(const std::string &cmd,
                                        std::string &output, const Cable *cable,
                                        const Device *device,
                                        const std::vector<Tap> taps) {
  std::atomic<bool> stop = false;
  int res = m_cmdexec("OPENOCD_DEBUG_LEVEL=-3 " + m_filepath +
                          build_command(cmd, cable, device, taps),
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

std::vector<Tap> OpenocdJtagAdapter::get_taps(const Cable &cable) {
  std::vector<Tap> taps;
  std::string line;
  std::cmatch matches;
  std::string output;

  const std::string pattern(
      R"((\d+) +(\w+.\w+) +([YN]) +(0x[0-9a-f]+) +(0x[0-9a-f]+) +(\d+) +(0x[0-9a-f]+) +(0x[0-9a-f]+))");

  // OpenOCD "scan_chain" command output text example:-
  //    TapName            Enabled IdCode     Expected   IrLen IrCap IrMask
  // -- ------------------ ------- ---------- ---------- ----- ----- ------
  //  0 omap5912.dsp          Y    0x03df1d81 0x03df1d81    38 0x01  0x03
  //  1 omap5912.arm          Y    0x0692602f 0x0692602f     4 0x01  0x0f
  //  2 omap5912.unknown      Y    0x00000000 0x00000000     8 0x01  0x03
  //  3 auto0.tap             Y    0x20000913 0x00000000     5 0x01  0x03

  CFG_ASSERT_MSG(execute_command(" -c \"scan_chain\"", output, &cable) == 0,
                 "cmdexec error: %s", output.c_str());

  std::stringstream ss(output);
  uint32_t tap_index = 1;

  while (std::getline(ss, line)) {
    if (std::regex_search(line.c_str(), matches,
                          std::regex(pattern, std::regex::icase)) == true) {
      Tap ti{tap_index++, (uint32_t)CFG_convert_string_to_u64(matches[4]), 0};
      taps.push_back(ti);
    }
  }

  return taps;
}
